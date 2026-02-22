#pragma once
#include "esphome/core/log.h"
#include <vector>
#include <cstring>
#include <cstdint>
#include <algorithm>

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"

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

// WAV header: PCM 16kHz, 16-bit, mono, "streaming" size
static void build_wav_header_(uint8_t hdr[44]) {
  std::memset(hdr, 0, 44);
  hdr[0] = 'R'; hdr[1] = 'I'; hdr[2] = 'F'; hdr[3] = 'F';
  wr_le32_(hdr + 4, 0xFFFFFFFFu);          // unknown size (stream)
  hdr[8]  = 'W'; hdr[9]  = 'A'; hdr[10] = 'V'; hdr[11] = 'E';

  hdr[12] = 'f'; hdr[13] = 'm'; hdr[14] = 't'; hdr[15] = ' ';
  wr_le32_(hdr + 16, 16u);                 // PCM fmt chunk size
  wr_le16_(hdr + 20, 1u);                  // AudioFormat = 1 (PCM)
  wr_le16_(hdr + 22, 1u);                  // NumChannels = 1
  wr_le32_(hdr + 24, 16000u);              // SampleRate
  wr_le32_(hdr + 28, 32000u);              // ByteRate = 16000 * 1 * 2
  wr_le16_(hdr + 32, 2u);                  // BlockAlign = 2
  wr_le16_(hdr + 34, 16u);                 // BitsPerSample = 16

  hdr[36] = 'd'; hdr[37] = 'a'; hdr[38] = 't'; hdr[39] = 'a';
  wr_le32_(hdr + 40, 0xFFFFFFFFu);         // unknown data size (stream)
}

static void stealth_close_() {
  if (stealth_sock_ >= 0) {
    ::close(stealth_sock_);
    stealth_sock_ = -1;
  }
  stealth_sent_header_ = false;
}

static bool stealth_try_connect_() {
  uint32_t now = (uint32_t) esphome::millis();
  if (now - stealth_last_connect_try_ms_ < 1000u) return false;
  stealth_last_connect_try_ms_ = now;

  stealth_close_();

  int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (s < 0) {
    ESP_LOGW(TAG_STEALTH, "socket(TCP) failed");
    return false;
  }

  int rc = ::connect(s, (sockaddr *) &stealth_dest_, sizeof(stealth_dest_));
  if (rc != 0) {
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
  if (sent == (int) sizeof(hdr)) {
    stealth_sent_header_ = true;
    ESP_LOGI(TAG_STEALTH, "WAV header sent");
  } else {
    ESP_LOGW(TAG_STEALTH, "WAV header send failed");
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
  ESP_LOGI(TAG_STEALTH, "Stealth target -> %s:%d (TCP)", host, port);

  stealth_try_connect_();
  stealth_send_header_if_needed_();
}

static void stealth_udp_end_() {
  stealth_close_();
  ESP_LOGI(TAG_STEALTH, "Stealth stopped");
}

static inline int16_t sat16_(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t) v;
}

/**
 * Convert input audio buffer to 16-bit mono PCM @16kHz and send via TCP.
 *
 * This is designed to work with Home Assistant Voice Preview (PE) where
 * microphone capture is commonly 32-bit stereo (2 channels interleaved).
 *
 * Supported input layouts:
 * - 32-bit stereo interleaved: [L(int32), R(int32), L, R, ...]
 * - 32-bit mono:              [S(int32), S, ...]  (will downshift)
 * - 16-bit mono:              [S(int16), S, ...]  (passed through)
 *
 * If the layout is unknown, it will attempt a reasonable fallback.
 */
static void stealth_udp_send_(const std::vector<uint8_t> &payload) {
  if (payload.empty()) return;

  if (stealth_sock_ < 0) {
    if (!stealth_try_connect_()) return;
  }

  stealth_send_header_if_needed_();
  if (stealth_sock_ < 0) return;

  // Heuristics to infer format
  const size_t n = payload.size();

  // Output buffer: 16-bit mono
  static std::vector<uint8_t> out;
  out.clear();

  // If looks like 32-bit stereo (multiple of 8 bytes)
  if ((n % 8) == 0 && n >= 8) {
    const size_t frames = n / 8;
    out.resize(frames * 2);
    const int32_t *in = reinterpret_cast<const int32_t *>(payload.data());
    int16_t *o = reinterpret_cast<int16_t *>(out.data());
    for (size_t i = 0; i < frames; i++) {
      int32_t l = in[i * 2 + 0];
      int32_t r = in[i * 2 + 1];
      // average then downshift to 16-bit
      int32_t m = (l >> 16) + (r >> 16);
      m /= 2;
      o[i] = sat16_(m);
    }
  }
  // If looks like 32-bit mono (multiple of 4 bytes)
  else if ((n % 4) == 0 && n >= 4) {
    const size_t frames = n / 4;
    out.resize(frames * 2);
    const int32_t *in = reinterpret_cast<const int32_t *>(payload.data());
    int16_t *o = reinterpret_cast<int16_t *>(out.data());
    for (size_t i = 0; i < frames; i++) {
      o[i] = sat16_(in[i] >> 16);
    }
  }
  // If looks like 16-bit mono (multiple of 2 bytes)
  else if ((n % 2) == 0 && n >= 2) {
    // already 16-bit PCM, forward as-is
    out.assign(payload.begin(), payload.end());
  }
  // Fallback: drop
  else {
    return;
  }

  // Send converted PCM
  int sent = ::send(stealth_sock_, out.data(), out.size(), 0);
  if (sent < 0) {
    stealth_close_();
  }
}