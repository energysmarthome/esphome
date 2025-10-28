# boneio-32x10 – ESPHome konfiguráció / ESPHome configuration

> **HU ➜** Először magyar összefoglaló, utána angol verzió (EN).  
> **Megjegyzés:** Ez a README *csak* azt írja le, hogy **mit tartalmaz** a konfiguráció – nem tartalmaz telepítési vagy használati útmutatót.

---

## 🇭🇺 Magyar összefoglaló

### Miről szól ez a repo?
ESPHome YAML konfiguráció **ESP32 (NodeMCU-32S) + LAN8720 Ethernet** alapú **boneio-32x10** IO vezérlőhöz.  
A kód 32 relékimenetet, 35+ digitális bemenetet, 3 analóg bemenetet, OLED kijelzőt, több szenzort és opcionális (kommentelt) Modbus-t definiál.

### Főbb funkciók
- **32 relékimenet** (`out_01`…`out_32`) két I²C IO‑expanderen:
  - `pcf_left` @ **0x23** (PCF8575) – kimenetek 1–16
  - `pcf_right` @ **0x24** (PCF8575) – kimenetek 17–32
  - Mindhez tartozik egy-egy ESPHome `switch` entitás (`switch_01`…`switch_32`).
- **Digitális bemenetek** (nyomógombok/érintkezők) I²C expander(ek)en:
  - `pcf_inputs_1to14` @ **0x20** (PCF8575) – `in_01`…`in_14`
  - `pcf_inputs_15to28` @ **0x21** (PCF8575) – `in_15`…`in_28`
  - `pcf_inputs_28to35_menu` @ **0x22** (PCF8574) – `in_29`…`in_32` + **IN_33…IN_35** és egy **menü gomb** (`boneIO_button`)
- **Bemenet → relé link kapcsolók** (`link_01`…`link_32`): ha az adott link **ON**, a megfelelő bemenet **toggle-olja** a hozzárendelt relét.  
  Állapot‑megőrzés: `RESTORE_DEFAULT_ON`.
- **Analóg bemenetek (ADC)** feszültségméréssel és skálázással:
  - **A1**: GPIO39 → *0–5 V* (kalibráció: 0–3.30 V → 0–5.00 V)
  - **A2**: GPIO36 → *0–10 V*
  - **A3**: GPIO35 → *0–25 V*
- **Szenzorok**
  - **INA219** @ **0x40**: áram, teljesítmény, busz‑ és söntfeszültség
  - **LM75B** (külső komponens): hőmérséklet
  - **Uptime** (formázott szöveges és másodperc alapú)
  - **IP‑cím** és **sorozatszám** (MAC‑cím utótagból, `serial_prefix: "esp"`)
- **OLED kijelző** (I²C, **SH1106 128×64** @ **0x3C**) két információs oldallal + **képernyőkímélő**:
  - **Első oldal**: IP‑cím, uptime, LM75B hőmérséklet
  - **Utolsó oldal**: INA219 mérési adatok
  - **Screensaver** 30 s inaktivitás után (újraindul gombnyomásra)
- **Buzzer**: GPIO2‑re kötött kimenetként deklarálva
- **Ethernet (LAN8720)**: MDC=GPIO23, MDIO=GPIO18, CLK=GPIO0 (külső órajel), PHY_ADDR=1, POWER=GPIO16
- **Idő és hálózat**
  - `time:` Home Assistant idő (id: `homeassistant_time`, zóna: **Europe/Budapest**)
  - **API**, **OTA** (ESPHome + web_server), **web_server** (v3, port 80, `local: true`), **logger**
- **Előkészített, de kikommentezett** részek:
  - **Dallas 1‑Wire** példa szenzorok (a busz a GPIO32‑n aktív)
  - **UART + Modbus** minta beállítások (GPIO14/15)
- **Indításkori logika** (`on_boot`):
  - Kijelző: első oldal megjelenítése, IP és kijelző frissítése, képernyőkímélő script indítása

### I²C busz és eszközök
- I²C busz: **SDA=GPIO17**, **SCL=GPIO33**, 400 kHz, buszszkennelés engedélyezve
- Címek összefoglalója:
  | Cím | Eszköz / szerep                         |
  |-----|-----------------------------------------|
  | 0x20 | PCF8575 – digitális bemenetek 1–14     |
  | 0x21 | PCF8575 – digitális bemenetek 15–28    |
  | 0x22 | PCF8574 – bemenetek 29–35 + menü gomb  |
  | 0x23 | PCF8575 – relé kimenetek 1–16 (left)   |
  | 0x24 | PCF8575 – relé kimenetek 17–32 (right) |
  | 0x3C | SH1106 OLED                            |
  | 0x40 | INA219                                 |

### Külső komponensek
- **LM75**: `external_components` a GitHub‑ról (`energysmarthome/esphome-LM75`), köszönet megjegyzéssel.

### Entitás elnevezések
- Relék: `relay1Name`…`relay32Name` (substitutions) – alapértelmezés: `relay1`…`relay32`
- Link kapcsolók: `IN-><relayXName> link`
- Bemenetek: `in_01`…`in_32`, továbbá `IN_33`…`IN_35`
- Kijelzőhöz kapcsolódó: `boneIO_button`
- Szenzorok/szöveges szenzorok: LM75B, INA219, Uptime, IP Address, Serial No.

---

## 🇬🇧 English summary

### What is this repository?
An ESPHome YAML configuration for an **ESP32 (NodeMCU‑32S) + LAN8720 Ethernet** based **boneio‑32x10** IO controller.  
It defines **32 relay outputs**, **35+ digital inputs**, **3 analog inputs**, an **OLED display**, multiple **sensors**, and optional (commented) **Modbus** blocks.

### Key capabilities
- **32 relay outputs** (`out_01`…`out_32`) via two I²C IO expanders:
  - `pcf_left` @ **0x23** (PCF8575) – outputs 1–16
  - `pcf_right` @ **0x24** (PCF8575) – outputs 17–32
  - Each has a corresponding ESPHome `switch` entity (`switch_01`…`switch_32`).
- **Digital inputs** on I²C expanders:
  - `pcf_inputs_1to14` @ **0x20** (PCF8575) – `in_01`…`in_14`
  - `pcf_inputs_15to28` @ **0x21** (PCF8575) – `in_15`…`in_28`
  - `pcf_inputs_28to35_menu` @ **0x22** (PCF8574) – `in_29`…`in_32` + **IN_33…IN_35** and a **menu button** (`boneIO_button`)
- **Input → relay link switches** (`link_01`…`link_32`): when a link is **ON**, the paired input **toggles** its relay.  
  State retention: `RESTORE_DEFAULT_ON`.
- **Analog inputs (ADC)** with scaling:
  - **A1**: GPIO39 → *0–5 V* (calibration: 0–3.30 V → 0–5.00 V)
  - **A2**: GPIO36 → *0–10 V*
  - **A3**: GPIO35 → *0–25 V*
- **Sensors**
  - **INA219** @ **0x40**: current, power, bus & shunt voltage
  - **LM75B** (external component): temperature
  - **Uptime** (pretty‑formatted + raw seconds)
  - **IP address** and **Serial No.** (derived from MAC suffix, `serial_prefix: "esp"`)
- **OLED display** (I²C, **SH1106 128×64** @ **0x3C**) with two info pages + **screensaver**:
  - **First page**: IP address, uptime, LM75B temperature
  - **Last page**: INA219 readings
  - **Screensaver** after 30 s idle (restarts on button press)
- **Buzzer**: declared as a GPIO output on GPIO2
- **Ethernet (LAN8720)**: MDC=GPIO23, MDIO=GPIO18, CLK=GPIO0 (external), PHY_ADDR=1, POWER=GPIO16
- **Time & networking**
  - `time:` from Home Assistant (id `homeassistant_time`, zone **Europe/Budapest**)
  - **API**, **OTA** (ESPHome + web_server), **web_server** (v3, port 80, `local: true`), **logger**
- **Prepared but commented** sections:
  - **Dallas 1‑Wire** example sensors (bus is active on GPIO32)
  - **UART + Modbus** example config (GPIO14/15)
- **On‑boot logic** (`on_boot`):
  - Display: show first page, update IP & display, start the screensaver script

### I²C bus and devices
- I²C bus: **SDA=GPIO17**, **SCL=GPIO33**, 400 kHz, bus scan enabled
- Address summary:
  | Address | Device / role                         |
  |---------|---------------------------------------|
  | 0x20    | PCF8575 – digital inputs 1–14        |
  | 0x21    | PCF8575 – digital inputs 15–28       |
  | 0x22    | PCF8574 – inputs 29–35 + menu button |
  | 0x23    | PCF8575 – relay outputs 1–16 (left)  |
  | 0x24    | PCF8575 – relay outputs 17–32 (right)|
  | 0x3C    | SH1106 OLED                           |
  | 0x40    | INA219                                |

### External components
- **LM75**: pulled via `external_components` from GitHub (`energysmarthome/esphome-LM75`) with a thank‑you note.

### Entity naming
- Relays: `relay1Name`…`relay32Name` (substitutions) – defaults: `relay1`…`relay32`
- Link switches: `IN-><relayXName> link`
- Inputs: `in_01`…`in_32`, plus `IN_33`…`IN_35`
- Display‑related: `boneIO_button`
- Sensors/text sensors: LM75B, INA219, Uptime, IP Address, Serial No.

---

*This README intentionally avoids setup/usage steps as requested; it only documents what the configuration contains.*
