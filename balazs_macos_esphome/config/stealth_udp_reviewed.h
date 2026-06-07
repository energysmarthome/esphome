#pragma once
#include "esphome/core/log.h"
#include <algorithm>
#include <cstring>
#include <cstdint>

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/errno.h"
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>

static const char *const TAG_STEALTH = "stealth_listen";

static int stealth_sock_ = -1;
static sockaddr_in stealth_dest_{};
static bool stealth_sent_header_ = false;
static uint32_t stealth_last_connect_try_ms_ = 0;
static bool stealth_configured_ = false;

static inline void wr_le16_(uint8_t *p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

static inline void wr_le32_(uint8_t *p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
  p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

// WAV header: PCM 16 kHz, 16-bit, mono, streaming/unknown size.
static void build_wav_header_(uint8_t hdr[44]) {
  std::memset(hdr, 0, 44);
  hdr[0] = 'R'; hdr[1] = 'I'; hdr[2] = 'F'; hdr[3] = 'F';
  wr_le32_(hdr + 4, 0xFFFFFFFFu);
  hdr[8] = 'W'; hdr[9] = 'A'; hdr[10] = 'V'; hdr[11] = 'E';

  hdr[12] = 'f'; hdr[13] = 'm'; hdr[14] = 't'; hdr[15] = ' ';
  wr_le32_(hdr + 16, 16u);
  wr_le16_(hdr + 20, 1u);
  wr_le16_(hdr + 22, 1u);
  wr_le32_(hdr + 24, 16000u);
  wr_le32_(hdr + 28, 32000u);
  wr_le16_(hdr + 32, 2u);
  wr_le16_(hdr + 34, 16u);

  hdr[36] = 'd'; hdr[37] = 'a'; hdr[38] = 't'; hdr[39] = 'a';
  wr_le32_(hdr + 40, 0xFFFFFFFFu);
}

static void stealth_close_() {
  if (stealth_sock_ >= 0) {
    ::close(stealth_sock_);
    stealth_sock_ = -1;
  }
  stealth_sent_header_ = false;
}

static bool stealth_set_nonblocking_(int fd) {
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0) return false;
  return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

static bool stealth_wait_connected_(int fd, uint32_t timeout_ms) {
  fd_set wfds;
  FD_ZERO(&wfds);
  FD_SET(fd, &wfds);
  timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  int rc = ::select(fd + 1, nullptr, &wfds, nullptr, &tv);
  if (rc <= 0) return false;

  int so_error = 0;
  socklen_t len = sizeof(so_error);
  if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len) != 0) return false;
  return so_error == 0;
}

static bool stealth_try_connect_() {
  if (!stealth_configured_) return false;

  uint32_t now = static_cast<uint32_t>(esphome::millis());
  if (now - stealth_last_connect_try_ms_ < 1000u) return false;
  stealth_last_connect_try_ms_ = now;

  stealth_close_();

  int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (s < 0) {
    ESP_LOGW(TAG_STEALTH, "socket(TCP) failed");
    return false;
  }

  if (!stealth_set_nonblocking_(s)) {
    ESP_LOGW(TAG_STEALTH, "failed to set non-blocking socket");
    ::close(s);
    return false;
  }

  int rc = ::connect(s, reinterpret_cast<sockaddr *>(&stealth_dest_), sizeof(stealth_dest_));
  if (rc != 0) {
    int err = errno;
    if (err != EINPROGRESS && err != EWOULDBLOCK && err != EALREADY) {
      ::close(s);
      return false;
    }
    // Keep this short: this may be called from microphone on_data.
    if (!stealth_wait_connected_(s, 10)) {
      ::close(s);
      return false;
    }
  }

  stealth_sock_ = s;
  stealth_sent_header_ = false;
  ESP_LOGI(TAG_STEALTH, "TCP connected");
  return true;
}

static bool stealth_send_all_nonblocking_(const uint8_t *data, size_t len) {
  if (stealth_sock_ < 0 || data == nullptr || len == 0) return false;

  size_t sent_total = 0;
  while (sent_total < len) {
    int sent = ::send(stealth_sock_, data + sent_total, len - sent_total, MSG_DONTWAIT);
    if (sent > 0) {
      sent_total += static_cast<size_t>(sent);
      continue;
    }
    // Avoid blocking the audio path. Reconnect on next chunk.
    stealth_close_();
    return false;
  }
  return true;
}

static bool stealth_send_header_if_needed_() {
  if (stealth_sock_ < 0) return false;
  if (stealth_sent_header_) return true;

  uint8_t hdr[44];
  build_wav_header_(hdr);
  if (stealth_send_all_nonblocking_(hdr, sizeof(hdr))) {
    stealth_sent_header_ = true;
    ESP_LOGI(TAG_STEALTH, "WAV header sent");
    return true;
  }
  return false;
}

static void stealth_udp_begin_(const char *host, int port) {
  stealth_close_();
  stealth_configured_ = false;

  if (host == nullptr || host[0] == '\0' || port <= 0 || port > 65535) {
    ESP_LOGE(TAG_STEALTH, "invalid target host/port");
    return;
  }

  std::memset(&stealth_dest_, 0, sizeof(stealth_dest_));
  stealth_dest_.sin_family = AF_INET;
  stealth_dest_.sin_port = htons(static_cast<uint16_t>(port));

  stealth_dest_.sin_addr.s_addr = ::inet_addr(host);
  if (stealth_dest_.sin_addr.s_addr == INADDR_NONE) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo *res = nullptr;
    int err = ::getaddrinfo(host, nullptr, &hints, &res);
    if (err == 0 && res != nullptr) {
      auto *addr = reinterpret_cast<sockaddr_in *>(res->ai_addr);
      stealth_dest_.sin_addr = addr->sin_addr;
      ::freeaddrinfo(res);
    } else {
      ESP_LOGE(TAG_STEALTH, "DNS resolve failed for host=%s", host);
      return;
    }
  }

  stealth_configured_ = true;
  stealth_last_connect_try_ms_ = 0;
  ESP_LOGI(TAG_STEALTH, "Stealth stream target -> %s:%d (TCP)", host, port);

  stealth_try_connect_();
  stealth_send_header_if_needed_();
}

static void stealth_udp_end_() {
  stealth_close_();
  stealth_configured_ = false;
  ESP_LOGI(TAG_STEALTH, "Stealth stream stopped");
}

static void stealth_udp_send_raw_(const uint8_t *payload, size_t len) {
  if (!stealth_configured_ || payload == nullptr || len == 0) return;

  if (stealth_sock_ < 0 && !stealth_try_connect_()) return;
  if (!stealth_send_header_if_needed_()) return;

  stealth_send_all_nonblocking_(payload, len);
}

// Convert ESPHome's 32-bit stereo little-endian I2S microphone chunks to
// 16-bit mono little-endian PCM and stream in small stack buffers. This avoids
// heap allocation in the microphone callback.
static void stealth_udp_send_s32le_stereo_as_s16le_mono_(const uint8_t *data, size_t len) {
  if (!stealth_configured_ || data == nullptr || len < 8) return;

  constexpr size_t OUT_BYTES = 512;
  uint8_t out[OUT_BYTES];
  size_t out_pos = 0;
  const size_t frames = len / 8;
  const uint8_t *p = data;

  for (size_t i = 0; i < frames; i++) {
    int32_t l = static_cast<int32_t>(
        static_cast<uint32_t>(p[0]) |
        (static_cast<uint32_t>(p[1]) << 8) |
        (static_cast<uint32_t>(p[2]) << 16) |
        (static_cast<uint32_t>(p[3]) << 24));
    int16_t s = static_cast<int16_t>(l >> 16);
    out[out_pos++] = static_cast<uint8_t>(s & 0xFF);
    out[out_pos++] = static_cast<uint8_t>((s >> 8) & 0xFF);
    p += 8;

    if (out_pos >= OUT_BYTES) {
      stealth_udp_send_raw_(out, out_pos);
      out_pos = 0;
      if (stealth_sock_ < 0) return;
    }
  }

  if (out_pos > 0) stealth_udp_send_raw_(out, out_pos);
}
