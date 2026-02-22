# Xiaozhi Ball V3 — Voice Assistant + Intercom (ESPHome)

ESP32-S3 alapú **Xiaozhi Ball V3** konfiguráció, ami egyesíti a **Home Assistant Voice Assistant** (micro_wake_word + voice_assistant) funkciót egy **teljes duplex ESP↔ESP intercom** megoldással (`intercom_api`).  
A hangút **ES8311 kodeken**, **egyetlen I2S duplex buszon** fut, **esp_aec** akusztikus visszhangszűréssel (digitális, sample-pontos referencia).

- Device: `intercom-haloszoba`
- Friendly name: `Intercom Hálószoba`
- Kijelző: **GC9A01A 240×240** kör LCD (SPI)
- Státusz LED: **WS2812** (GPIO48)

---

## Fő funkciók

### Voice Assistant (VA)
- ✅ **micro_wake_word** + **voice_assistant** integráció
- ✅ **On-device** wake word alapértelmezetten (választható: HA / on-device)
- ✅ Több VA állapotoldal képekkel/animációval:
  - initializing / idle / listening / thinking / replying / error / timer finished
- ✅ “Replying” oldalon **többoldalas, lapozódó szöveg** (pagination timer)
- ✅ **Időzítő UI** progress bar (aktív / szüneteltetett)
- ✅ LED effektek VA fázisokhoz (Slow Pulse / Fast Pulse / fix színek)
- ✅ Backlight automata időzítő (USB-n más viselkedés)

### Intercom (ESP↔ESP)
- ✅ `intercom_api` **full mode** (kontaktlista + bejövő/kimenő hívás)
- ✅ Intercom oldalak:
  - idle (kontakt kiválasztó)
  - ringing in / ringing out
  - in call
- ✅ Hívásvezérlők (HA entitások): call / next / prev / decline / auto-answer
- ✅ “Doorbell” esemény Home Assistant felé, ha a cél `"Home Assistant"`

### Hang / AEC
- ✅ **i2s_audio_duplex**: valódi full-duplex egy buszon (mic + speaker)
- ✅ **esp_aec**: echo cancellation
- ✅ **ES8311 digitális feedback AEC**:
  - Bootkor ES8311 regiszter `0x44=0x48`
  - I2S RX stereo: **L = DAC ref**, **R = mic**
  - `use_stereo_aec_reference: true` + `aec_reference_delay_ms: 10`
- ✅ ES8311 hardveres hangerő + AEC referencia szinkron (`set_aec_reference_volume`)

### “Stealth listen” (figyelem!)
- ⚠️ Opcionális funkció: **post-AEC mic PCM UDP stream** egy cél host/portra
- `Stealth Listen` kapcsoló HA-ból indítható, **UI/LED jelzés nélkül**

---

## Hardver / Pinout

| Funkció | GPIO |
|---|---:|
| I2C SDA / SCL (ES8311) | 15 / 14 |
| I2S LRCLK / BCLK / MCLK | 45 / 9 / 16 |
| I2S DIN / DOUT | 10 / 8 |
| Speaker amp enable | 46 |
| Backlight (LEDC, inverted) | 42 |
| WS2812 LED | 48 |
| Main button | 0 |
| Touch input | 12 |
| SPI CLK / MOSI | 4 / 2 |
| Display CS / DC / RST | 5 / 47 / 38 |
| Battery ADC | 1 |

---

## go2rtc konfiguráció (HA oldalon)

A go2rtc `go2rtc.yaml` példa:

```yaml


log:
  level: debug
  ffmpeg: debug
  streams: debug
  webrtc: debug
  api: debug

api:
  listen: ":1984"

webrtc:
  listen: ":8555"

rtsp:
  listen: ":8554"

ffmpeg:
  bin: ffmpeg
  global: "-hide_banner -loglevel warning -v debug"
  udp_pcm16k: "-f s16le -ar 16000 -ac 1 -i {input}"
  tcp_wav16k: "-f wav -i {input}"





streams:
  haloszoba:
    - "ffmpeg:tcp://0.0.0.0:12345?listen=1#audio=pcma/16000"

  konyha_ha_voice_stealth:
    - "ffmpeg:tcp://0.0.0.0:12346?listen=1#input=tcp_wav16k#audio=pcma/16000"


```




---




## Hálózat / elérés

- ESPHome min: `2025.5.0`
- Framework: `esp-idf`
- Statikus IP: `192.168.22.189`
- Time/NTP: `Europe/Budapest` (+ helyi NTP: `192.168.22.203`)
- API: csatlakozáskor state publish + redraw

---

## UI módok (VA ↔ Intercom)

A rendszer két “módot” kezel **csak UI-szinten**:

- `current_mode = 0` → Voice Assistant UI
- `current_mode = 1` → Intercom UI

A wake word **folyamatosan aktív** (a módváltás a kijelzőt és gomb-logikát érinti).

### Fő gomb (GPIO0) logika
- **Rövid nyomás**
  - ha timer csörög: kikapcsolja
  - intercom oldalakon: `Call` (toggle: call/answer/hangup)
  - VA oldalon: VA start/stop (ha nem fut → start, ha fut → stop)
- **Dupla katt**
  - intercom ringing in oldalon: `Decline`
  - intercom idle oldalon: `Next contact`
- **Hosszú nyomás (1–3s)**
  - VA ↔ Intercom mód váltás (`switch_to_va` / `switch_to_intercom`)

Touch (GPIO12) is tud módot váltani dupla kattal + backlight ébresztés.

---

## Audio topológia (mixer)

A hardver hangszóró (`hw_speaker`) elé egy mixer kerül:

- `va_speaker` (TTS / VA)
- `intercom_speaker` (intercom stream)
→ `audio_mixer` → `hw_speaker`

Így a VA hang és az intercom ugyanarra a kimenetre tud menni, ütközés nélkül (timeout-olt csatornákkal).

---

## Home Assistant integráció

### Kontaktlista (broker)
A kontaktok a HA-ból jönnek:
- `sensor.intercom_active_devices` → CSV
- ESPHome `ha_active_devices` text_sensor → `intercom_api.set_contacts()`

### Doorbell event
Kimenő híváskor, ha destination `"Home Assistant"`:
- event: `esphome.intercom_call`
- data: `{ caller, destination, type: "doorbell" }`

---

## Backlight viselkedés

`backlight_timer`:
- USB-n: marad 100%
- Akkun: 100% → 30s után 60% → +120s után OFF

---

## Stealth listen (UDP) beállítás

Substitutions:
- `stealth_stream_host: 192.168.22.203`
- `stealth_stream_port: 12345`

Működés:
- `Stealth Listen` switch ON → `stealth_listen_start`:
  - `stealth_udp_begin_(host, port)`
  - `microphone.capture: mic_stealth`
- `mic_stealth on_data` → `stealth_udp_send_(x)` (PCM chunk)
- OFF → capture stop + `stealth_udp_end_()`

Szükséges include:
- `includes: stealth_udp.h`

---

## Projekt struktúra javaslat