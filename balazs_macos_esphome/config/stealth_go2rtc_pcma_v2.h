#pragma once

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/tcp.h"

static const char *const TAG_STEALTH = "stealth_listen";

// Historical YAML function names are kept for compatibility:
//   stealth_udp_begin_(), stealth_udp_send_(), stealth_udp_end_()
//
// Transport format for go2rtc config:
//   haloszoba:
//     - "ffmpeg:tcp://0.0.0.0:12345?listen=1#audio=pcma/16000"
//
// This helper sends RAW G.711 A-law (PCMA) at 16 kHz over TCP.
// It does NOT send a WAV header and does NOT send raw PCM, because the go2rtc
// line above expects pcma/16000, not pcm_s16le and not wav.
//
// The microphone callback only converts/copies into a PSRAM ring buffer. A
// background task handles TCP connection and sending, so Stealth Listen should
// not block voice assistant, wake word, media playback, or intercom.

static constexpr uint32_t STEALTH_SAMPLE_RATE_ = 16000;
static constexpr size_t STEALTH_RING_SIZE_ = 16 * 1024;      // ~1 second @ PCMA 16 kHz mono; keep latency bounded
static constexpr size_t STEALTH_SEND_CHUNK_ = 1024;
static constexpr uint32_t STEALTH_RECONNECT_MS_ = 1500;
static constexpr uint32_t STEALTH_CONNECT_TIMEOUT_MS_ = 350;

static int stealth_sock_ = -1;
static sockaddr_in stealth_dest_{};
static bool stealth_dest_valid_ = false;
static volatile bool stealth_active_ = false;
static uint32_t stealth_last_connect_try_ms_ = 0;
static uint32_t stealth_last_warn_ms_ = 0;
static uint32_t stealth_dropped_bytes_since_log_ = 0;

static uint8_t *stealth_ring_ = nullptr;
static size_t stealth_ring_head_ = 0;
static size_t stealth_ring_tail_ = 0;
static size_t stealth_ring_used_ = 0;
static portMUX_TYPE stealth_ring_mux_ = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t stealth_task_handle_ = nullptr;

static inline uint8_t stealth_linear_to_alaw_(int16_t sample) {
  // ITU G.711 A-law companding. Input: signed 16-bit PCM. Output: 8-bit PCMA.
  int pcm = sample;
  int mask;

  if (pcm >= 0) {
    mask = 0xD5;
  } else {
    mask = 0x55;
    pcm = -pcm - 1;
    if (pcm < 0) pcm = 32767;
  }

  // A-law uses a 13-bit dynamic range. Convert 16-bit PCM to 13-bit scale.
  pcm >>= 3;
  if (pcm > 0x0FFF) pcm = 0x0FFF;

  int seg = 0;
  int tmp = pcm >> 4;
  while (tmp > 0x0F) {
    seg++;
    tmp >>= 1;
  }

  uint8_t aval;
  if (seg >= 8) {
    aval = 0x7F;
  } else {
    aval = static_cast<uint8_t>((seg << 4) | ((pcm >> (seg + 1)) & 0x0F));
  }

  return static_cast<uint8_t>(aval ^ mask);
}

static void stealth_close_socket_() {
  if (stealth_sock_ >= 0) {
    ::shutdown(stealth_sock_, SHUT_RDWR);
    ::close(stealth_sock_);
  }
  stealth_sock_ = -1;
}

static bool stealth_alloc_ring_() {
  if (stealth_ring_ != nullptr) return true;
  stealth_ring_ = static_cast<uint8_t *>(heap_caps_malloc(STEALTH_RING_SIZE_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (stealth_ring_ == nullptr) {
    stealth_ring_ = static_cast<uint8_t *>(heap_caps_malloc(STEALTH_RING_SIZE_, MALLOC_CAP_8BIT));
  }
  if (stealth_ring_ == nullptr) {
    ESP_LOGE(TAG_STEALTH, "Failed to allocate %u byte PCMA stream buffer", static_cast<unsigned>(STEALTH_RING_SIZE_));
    return false;
  }
  ESP_LOGI(TAG_STEALTH, "Allocated %u byte PCMA stream buffer", static_cast<unsigned>(STEALTH_RING_SIZE_));
  return true;
}

static void stealth_ring_clear_() {
  portENTER_CRITICAL(&stealth_ring_mux_);
  stealth_ring_head_ = 0;
  stealth_ring_tail_ = 0;
  stealth_ring_used_ = 0;
  stealth_dropped_bytes_since_log_ = 0;
  portEXIT_CRITICAL(&stealth_ring_mux_);
}

static void stealth_ring_write_(const uint8_t *data, size_t len) {
  if (stealth_ring_ == nullptr || data == nullptr || len == 0) return;

  if (len > STEALTH_RING_SIZE_) {
    data += (len - STEALTH_RING_SIZE_);
    len = STEALTH_RING_SIZE_;
  }

  portENTER_CRITICAL(&stealth_ring_mux_);

  size_t free_space = STEALTH_RING_SIZE_ - stealth_ring_used_;
  if (len > free_space) {
    // Drop oldest PCMA frames to keep latency bounded.
    size_t drop = len - free_space;
    if (drop > stealth_ring_used_) drop = stealth_ring_used_;
    stealth_ring_tail_ = (stealth_ring_tail_ + drop) % STEALTH_RING_SIZE_;
    stealth_ring_used_ -= drop;
    stealth_dropped_bytes_since_log_ += static_cast<uint32_t>(drop);
  }

  size_t first = STEALTH_RING_SIZE_ - stealth_ring_head_;
  if (first > len) first = len;
  std::memcpy(stealth_ring_ + stealth_ring_head_, data, first);
  if (len > first) {
    std::memcpy(stealth_ring_, data + first, len - first);
  }
  stealth_ring_head_ = (stealth_ring_head_ + len) % STEALTH_RING_SIZE_;
  stealth_ring_used_ += len;

  portEXIT_CRITICAL(&stealth_ring_mux_);

  if (stealth_task_handle_ != nullptr) {
    xTaskNotifyGive(stealth_task_handle_);
  }
}

static size_t stealth_ring_read_(uint8_t *out, size_t max_len) {
  if (stealth_ring_ == nullptr || out == nullptr || max_len == 0) return 0;

  portENTER_CRITICAL(&stealth_ring_mux_);
  size_t n = stealth_ring_used_;
  if (n > max_len) n = max_len;
  if (n > 0) {
    size_t first = STEALTH_RING_SIZE_ - stealth_ring_tail_;
    if (first > n) first = n;
    std::memcpy(out, stealth_ring_ + stealth_ring_tail_, first);
    if (n > first) {
      std::memcpy(out + first, stealth_ring_, n - first);
    }
    stealth_ring_tail_ = (stealth_ring_tail_ + n) % STEALTH_RING_SIZE_;
    stealth_ring_used_ -= n;
  }
  portEXIT_CRITICAL(&stealth_ring_mux_);
  return n;
}

static uint32_t stealth_take_dropped_bytes_() {
  portENTER_CRITICAL(&stealth_ring_mux_);
  uint32_t dropped = stealth_dropped_bytes_since_log_;
  stealth_dropped_bytes_since_log_ = 0;
  portEXIT_CRITICAL(&stealth_ring_mux_);
  return dropped;
}

static void stealth_set_nonblocking_(int s, bool nonblocking) {
  int flags = ::fcntl(s, F_GETFL, 0);
  if (flags < 0) return;
  if (nonblocking) flags |= O_NONBLOCK;
  else flags &= ~O_NONBLOCK;
  ::fcntl(s, F_SETFL, flags);
}

static bool stealth_resolve_host_(const char *host) {
  stealth_dest_.sin_addr.s_addr = ::inet_addr(host);
  if (stealth_dest_.sin_addr.s_addr != INADDR_NONE) return true;

  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo *res = nullptr;
  int err = ::getaddrinfo(host, nullptr, &hints, &res);
  if (err == 0 && res != nullptr) {
    auto *addr = reinterpret_cast<sockaddr_in *>(res->ai_addr);
    stealth_dest_.sin_addr = addr->sin_addr;
    ::freeaddrinfo(res);
    return true;
  }
  if (res != nullptr) ::freeaddrinfo(res);
  ESP_LOGE(TAG_STEALTH, "DNS resolve failed for host=%s", host);
  return false;
}

static bool stealth_connect_socket_() {
  if (!stealth_active_ || !stealth_dest_valid_) return false;
  if (stealth_sock_ >= 0) return true;

  const uint32_t now = static_cast<uint32_t>(esphome::millis());
  if (now - stealth_last_connect_try_ms_ < STEALTH_RECONNECT_MS_) return false;
  stealth_last_connect_try_ms_ = now;

  int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (s < 0) {
    if (now - stealth_last_warn_ms_ > 10000u) {
      stealth_last_warn_ms_ = now;
      ESP_LOGW(TAG_STEALTH, "socket(TCP) failed, errno=%d", errno);
    }
    return false;
  }

  int one = 1;
  ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  ::setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));

  int sndbuf = 32768;
  ::setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

  timeval send_timeout{};
  send_timeout.tv_sec = 0;
  send_timeout.tv_usec = 250000;
  ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));

  stealth_set_nonblocking_(s, true);
  int rc = ::connect(s, reinterpret_cast<sockaddr *>(&stealth_dest_), sizeof(stealth_dest_));
  if (rc != 0 && errno != EINPROGRESS && errno != EALREADY && errno != EWOULDBLOCK) {
    if (now - stealth_last_warn_ms_ > 10000u) {
      stealth_last_warn_ms_ = now;
      ESP_LOGW(TAG_STEALTH, "TCP connect failed, errno=%d", errno);
    }
    ::close(s);
    return false;
  }

  fd_set wfds;
  FD_ZERO(&wfds);
  FD_SET(s, &wfds);
  timeval tv{};
  tv.tv_sec = 0;
  tv.tv_usec = STEALTH_CONNECT_TIMEOUT_MS_ * 1000;
  int sel = ::select(s + 1, nullptr, &wfds, nullptr, &tv);
  if (sel <= 0) {
    ::close(s);
    return false;
  }

  int so_error = 0;
  socklen_t slen = sizeof(so_error);
  if (::getsockopt(s, SOL_SOCKET, SO_ERROR, &so_error, &slen) != 0 || so_error != 0) {
    if (now - stealth_last_warn_ms_ > 10000u) {
      stealth_last_warn_ms_ = now;
      ESP_LOGW(TAG_STEALTH, "TCP connect failed, so_error=%d", so_error);
    }
    ::close(s);
    return false;
  }

  stealth_set_nonblocking_(s, false);
  stealth_sock_ = s;
  ESP_LOGI(TAG_STEALTH, "TCP connected, sending raw PCMA/16000");
  return true;
}

static bool stealth_send_all_blocking_(const uint8_t *data, size_t len) {
  if (stealth_sock_ < 0 || data == nullptr || len == 0) return false;

  size_t off = 0;
  while (stealth_active_ && off < len) {
    int sent = ::send(stealth_sock_, data + off, len - off, 0);
    if (sent > 0) {
      off += static_cast<size_t>(sent);
      continue;
    }
    if (sent < 0 && errno == EINTR) continue;

    stealth_close_socket_();
    return false;
  }
  return off == len;
}

static void stealth_sender_task_(void *) {
  uint8_t chunk[STEALTH_SEND_CHUNK_];

  for (;;) {
    if (!stealth_active_) {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
      continue;
    }

    uint32_t dropped = stealth_take_dropped_bytes_();
    if (dropped > 0) {
      ESP_LOGW(TAG_STEALTH, "PCMA stream buffer overrun: dropped %u bytes", static_cast<unsigned>(dropped));
    }

    if (!stealth_connect_socket_()) {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
      continue;
    }

    size_t n = stealth_ring_read_(chunk, sizeof(chunk));
    if (n == 0) {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
      continue;
    }

    if (!stealth_send_all_blocking_(chunk, n)) {
      // Socket broke or stalled. The ring buffer keeps the newest audio while
      // the next loop reconnects.
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

static bool stealth_ensure_task_() {
  if (stealth_task_handle_ != nullptr) return true;
  BaseType_t ok = xTaskCreatePinnedToCore(
      stealth_sender_task_,
      "stealth_pcma",
      4096,
      nullptr,
      1,
      &stealth_task_handle_,
      0);
  if (ok != pdPASS) {
    ESP_LOGE(TAG_STEALTH, "Failed to create sender task");
    stealth_task_handle_ = nullptr;
    return false;
  }
  return true;
}

static void stealth_udp_begin_(const char *host, int port) {
  stealth_active_ = false;
  stealth_close_socket_();
  stealth_ring_clear_();

  if (!stealth_alloc_ring_()) return;
  if (!stealth_ensure_task_()) return;

  std::memset(&stealth_dest_, 0, sizeof(stealth_dest_));
  stealth_dest_.sin_family = AF_INET;
  stealth_dest_.sin_port = htons(static_cast<uint16_t>(port));
  stealth_dest_valid_ = stealth_resolve_host_(host);
  if (!stealth_dest_valid_) return;

  stealth_last_connect_try_ms_ = 0;
  stealth_last_warn_ms_ = 0;
  stealth_active_ = true;

  ESP_LOGI(TAG_STEALTH, "Stealth stream target -> %s:%d (TCP raw PCMA/16000)", host, port);
  xTaskNotifyGive(stealth_task_handle_);
}

static void stealth_udp_end_() {
  stealth_active_ = false;
  stealth_close_socket_();
  stealth_ring_clear_();
  if (stealth_task_handle_ != nullptr) xTaskNotifyGive(stealth_task_handle_);
  ESP_LOGI(TAG_STEALTH, "Stealth stream stopped");
}

static void stealth_udp_send_(const std::vector<uint8_t> &payload) {
  if (!stealth_active_ || payload.size() < 2) return;

  // Do not buffer while disconnected. go2rtc/ffmpeg closes the TCP connection
  // immediately when its input line is wrong (for example missing -f alaw).
  // Buffering during that state only creates stale audio and overrun spam.
  if (stealth_sock_ < 0) return;

  // Convert PCM S16LE mono from ESPHome microphone callback to raw PCMA/16000.
  // Keep the callback fast: no network calls here.
  const size_t samples = payload.size() / 2;
  if (samples == 0) return;

  uint8_t stack_buf[512];
  std::vector<uint8_t> heap_buf;
  uint8_t *out = stack_buf;

  if (samples > sizeof(stack_buf)) {
    heap_buf.resize(samples);
    out = heap_buf.data();
  }

  const uint8_t *p = payload.data();
  for (size_t i = 0; i < samples; i++) {
    int16_t s = static_cast<int16_t>(static_cast<uint16_t>(p[i * 2]) | (static_cast<uint16_t>(p[i * 2 + 1]) << 8));
    out[i] = stealth_linear_to_alaw_(s);
  }

  stealth_ring_write_(out, samples);
}
