# ESPHome ESP32-S3 Intercom (Xiaozhi Ball V3) — Broker-based, Display + AEC

ESP32-S3 alapú **kétirányú intercom** (ESP↔ESP) konfiguráció **Home Assistant “broker”** közvetítéssel, **kör kijelzővel (GC9A01A)**, **ES8311 kodekkel**, valamint **WS2812 státusz LED-del**.  
A projekt az `intercom_api` komponenst használja, és a hangút egy **egyetlen I2S duplex buszon** fut **digitális AEC (echo cancellation)** referenciával.

> Eszköznév a YAML-ben: `intercom-gyerekszoba`  
> Friendly name: `Intercom Gyerekszoba`

---

## Funkciók

- ✅ **ESP↔ESP intercom hívás** Home Assistant brokeren keresztül (`intercom_api`, `mode: full`)
- ✅ **Bejövő / kimenő hívás állapotgép** (Idle → Ringing → Streaming → Idle)
- ✅ **Kijelző oldalak** állapotok szerint: Idle / Ringing In / Calling / In Call
- ✅ **WS2812 státusz LED effektek** (Ringing / Calling + fix szín hívásban)
- ✅ **Hardveres hangerő** ES8311 DAC-on + **AEC referencia szinkron**
- ✅ **Digitális feedback AEC**: ES8311 stereo RX (L=DAC ref, R=ADC mic)
- ✅ **“Okos gomb”** (GPIO0):
  - 1x: hívás / felvesz / lerak
  - 2x: idle-ben következő kontakt, csörgésben elutasít
  - 3x: kontaktok frissítése HA-ból
- ✅ Web UI (`web_server: 80`), OTA, API encryption, statikus IP

---

## Hardver

- **MCU:** ESP32-S3 DevKitC-1 (16MB Flash, Octal PSRAM)
- **Audio codec:** ES8311 (I2C vezérlés + I2S duplex)
- **Kijelző:** GC9A01A 240×240 kör LCD (SPI)
- **LED:** WS2812 (GPIO48)
- **Gomb:** GPIO0 (multi-click logika)
- **Erősítő engedély:** GPIO46
- **Háttérvilágítás:** GPIO42 (LEDC, invertált)

---

## Szoftver komponensek

- ESPHome min.: `2024.12.0`
- Framework: **esp-idf**
- Extra: `espressif/esp-sr^2.3.0` (AEC / esp_aec támogatáshoz)
- Külső komponensek (local):
  - `intercom_api`
  - `i2s_audio_duplex`
  - `esp_aec`

> A kijelző frissítés **nem fut időzítőből** (`update_interval: never`), csak állapotváltáskor, SPI race condition elkerülésére.

---

## Hálózat / Elérés

- ESPHome API: titkosított (encryption key a YAML-ben)
- Web server: `http://<ip>/` (port 80)
- OTA: engedélyezett (jelszóval)
- Wi-Fi: statikus IP
  - `192.168.22.188`
  - gateway: `192.168.22.1`

---

## Home Assistant integráció

### Kontaktok (Broker / HA oldal)
A kontaktlista a Home Assistantból érkezik:
- HA entity: `sensor.intercom_active_devices`
- ESPHome text_sensor: `ha_active_devices`
- `on_value` → `intercom_api.set_contacts(contacts_csv: x)`

**Elvárt formátum:** CSV (vesszővel elválasztva).  
A kijelző Idle oldalon a kontaktok számát a CSV vesszőinek száma alapján számolja.

### Doorbell / event (Home Assistant cél esetén)
Kimenő híváskor, ha a destination `"Home Assistant"`, akkor esemény megy HA felé:
- event: `esphome.intercom_call`
- data:
  - `caller`: az ESP friendly name-je
  - `destination`: `"Home Assistant"`
  - `type`: `"doorbell"`

Ez jól használható automations/notification célra.

---

## AEC (Echo Cancellation) – hogyan működik itt?

A konfiguráció lényege, hogy az AEC referencia **digitális úton** és **sample-pontosan** érkezik:

1. Bootkor I2C-n át beállítja az ES8311 **0x44 regiszterét**:
   - ADCDAT_SEL = `4` → **ASDOUT stereo**:
     - **Left:** DAC loopback (AEC ref)
     - **Right:** ADC mic
2. Az `i2s_audio_duplex` `use_stereo_aec_reference: true` módban a bal csatornát referenciának használja.
3. A hangerő állításnál a rendszer **mindig szinkronban tartja**:
   - ES8311 DAC vol
   - `i2s_duplex.set_aec_reference_volume(...)`

Ezzel elkerülhetők a tipikus “ring buffer timing” késések, és javul az echo-cancel minősége.

---

## Kezelés (gombok és entitások)

### Fizikai gomb (GPIO0)
- **1x katt:** `Call` (call/answer/hangup toggle)
- **2x katt:**
  - ha csörög → `Decline`
  - ha idle → `Next Contact`
- **3x katt:** `Refresh Contacts`

### ESPHome / HA entitások (fontosabbak)
**Buttonok**
- `Call`
- `Next Contact`
- `Previous Contact`
- `Decline`
- `Refresh Contacts`
- `Restart`

**Switchek**
- `Auto Answer` (intercom_api)
- `Echo Cancellation` (i2s_audio_duplex)

**Number**
- `Mic Gain` (intercom_api)
- `Speaker Volume` (template → ES8311 DAC + AEC ref sync)

**Szenzorok**
- WiFi dBm + %
- Uptime
- CPU temperature

**Display backlight**
- `Display Backlight` (monochromatic light, ALWAYS_ON)

---

## Kijelző oldalak

- **IDLE**: státusz, destination (peer_name), kontaktok száma, WiFi%, HA kapcsolat
- **RINGING (incoming)**: hívó neve + “Press button to answer”
- **CALLING (outgoing)**: hívott kontakt + “Waiting for answer…”
- **IN CALL**: connected to + “Press button to hang up”

Oldalváltás kizárólag az `intercom_api` callbackekből és az `update_display_page` scriptből történik.

---

## Pinout összefoglaló

| Funkció | GPIO |
|---|---:|
| I2C SDA / SCL (ES8311) | 15 / 14 |
| I2S LRCLK / BCLK / MCLK | 45 / 9 / 16 |
| I2S DIN / DOUT | 10 / 8 |
| Speaker amp enable | 46 |
| Backlight (LEDC, inverted) | 42 |
| WS2812 status LED | 48 |
| Main button | 0 |
| “Touch” input (pullup) | 12 |
| SPI CLK / MOSI (GC9A01A) | 4 / 2 |
| Display CS / DC / RST | 5 / 47 / 38 |

---

## Projekt struktúra javaslat