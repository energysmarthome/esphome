#pragma once
#include "esphome/core/log.h"
#include <vector>
#include <cstring>

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/errno.h"

static const char *const TAG_STEALTH = "stealth_listen";

static int stealth_sock_ = -1;
static sockaddr_in stealth_dest_{};
static bool stealth_sent_header_ = false;
static uint32_t stealth_last_connect_try_ms_ = 0;

static inline void wr_le16_(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
}
static inline void wr_le32_(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

// WAV header (PCM 16kHz, 16-bit, mono, "streaming" size)
static void build_wav_header_(uint8_t hdr[44]) {
  std::memset(hdr, 0, 44);
  hdr[0] = 'R'; hdr[1] = 'I'; hdr[2] = 'F'; hdr[3] = 'F';
  wr_le32_(hdr + 4, 0xFFFFFFFFu);
  hdr[8] = 'W'; hdr[9] = 'A'; hdr[10] = 'V'; hdr[11] = 'E';

  hdr[12] = 'f'; hdr[13] = 'm'; hdr[14] = 't'; hdr[15] = ' ';
  wr_le32_(hdr + 16, 16u);
  wr_le16_(hdr + 20, 1u);     // PCM
  wr_le16_(hdr + 22, 1u);     // mono
  wr_le32_(hdr + 24, 16000u);
  wr_le32_(hdr + 28, 32000u); // 16000 * 2
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

static bool stealth_try_connect_() {
  // backoff: max 1 próba / 1s
  uint32_t now = (uint32_t) esphome::millis();
  if (now - stealth_last_connect_try_ms_ < 1000u) return false;
  stealth_last_connect_try_ms_ = now;

  stealth_close_();

  int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (s < 0) {
    ESP_LOGW(TAG_STEALTH, "socket(TCP) failed");
    return false;
  }

  // Ne blokkoljon hosszasan
  timeval tv{};
  tv.tv_sec = 0;
  tv.tv_usec = 200000; // 200ms send timeout
  ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  int rc = ::connect(s, (sockaddr *) &stealth_dest_, sizeof(stealth_dest_));
  if (rc != 0) {
    // még nem hallgat a go2rtc/ffmpeg -> connection refused
    ::close(s);
    return false;
  }

  stealth_sock_ = s;
  stealth_sent_header_ = false;
  ESP_LOGI(TAG_STEALTH, "TCP connected");
  return true;
}

static void stealth_send_header_if_needed_() {
  if (stealth_sock_ < 0 || stealth_sent_header_) return;

  uint8_t hdr[44];
  build_wav_header_(hdr);

  int sent = ::send(stealth_sock_, hdr, sizeof(hdr), 0);
  if (sent == (int)sizeof(hdr)) {
    stealth_sent_header_ = true;
    ESP_LOGI(TAG_STEALTH, "WAV header sent");
  } else {
    // ha nem ment, bontunk és újrapróbáljuk később
    stealth_close_();
  }
}

static void stealth_udp_begin_(const char *host, int port) {
  std::memset(&stealth_dest_, 0, sizeof(stealth_dest_));
  stealth_dest_.sin_family = AF_INET;
  stealth_dest_.sin_port = htons((uint16_t) port);

  stealth_dest_.sin_addr.s_addr = ::inet_addr(host);
  if (stealth_dest_.sin_addr.s_addr == INADDR_NONE) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo *res = nullptr;
    int err = ::getaddrinfo(host, nullptr, &hints, &res);
    if (err == 0 && res) {
      auto *addr = (sockaddr_in *) res->ai_addr;
      stealth_dest_.sin_addr = addr->sin_addr;
      ::freeaddrinfo(res);
    } else {
      ESP_LOGE(TAG_STEALTH, "DNS resolve failed for host=%s", host);
      return;
    }
  }

  stealth_last_connect_try_ms_ = 0;
  ESP_LOGI(TAG_STEALTH, "Stealth stream target -> %s:%d (TCP)", host, port);

  // Nem baj, ha még nem tud csatlakozni (go2rtc consumer még nincs),
  // majd send közben újrapróbáljuk.
  stealth_try_connect_();
  stealth_send_header_if_needed_();
}

static void stealth_udp_end_() {
  stealth_close_();
  ESP_LOGI(TAG_STEALTH, "Stealth stream stopped");
}

static void stealth_udp_send_(const std::vector<uint8_t> &payload) {
  if (payload.empty()) return;

  if (stealth_sock_ < 0) {
    if (!stealth_try_connect_()) return;
  }

  stealth_send_header_if_needed_();
  if (stealth_sock_ < 0) return;

  int sent = ::send(stealth_sock_, payload.data(), payload.size(), 0);
  if (sent < 0) {
    // kapcsolat megszakadt → újraépítjük
    stealth_close_();
  }
}