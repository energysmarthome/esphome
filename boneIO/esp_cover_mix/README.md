# BoneIO ESP Cover Mix — ESPHome konfiguráció
**Bilingual README — Magyar & English**

> Ez a README a megosztott ESPHome konfigurációhoz készült.  
> This README accompanies the provided ESPHome configuration.

---

## Tartalomjegyzék (HU)

- [Áttekintés](#áttekintés)
- [Fő funkciók](#fő-funkciók)
- [Hardver és kompatibilitás](#hardver-és-kompatibilitás)
- [Bekötési és modul-információk](#bekötési-és-modul-információk)
- [Telepítés és feltöltés](#telepítés-és-feltöltés)
- [Konfigurálható helyettesítések (substitutions)](#konfigurálható-helyettesítések-substitutions)
- [Entitások és működés](#entitások-és-működés)
  - [Redőnyök (8 csatorna)](#redőnyök-8-csatorna)
  - [NO relé kimenetek (16 csatorna)](#no-relé-kimenetek-16-csatorna)
  - [Bemenetek (IN\_01…IN\_35)](#bemenetek-in_01in_35)
  - [Kijelző](#kijelző)
  - [Érzékelők](#érzékelők)
- [Home Assistant integráció](#home-assistant-integráció)
- [Hibaelhárítás](#hibaelhárítás)
- [Biztonság](#biztonság)
- [Köszönetnyilvánítás](#köszönetnyilvánítás)

---

## Áttekintés

A projekt egy **ESP32 (NodeMCU-32S)** alapú, **Ethernetes (LAN8720)** vezérlő BoneIO hardverhez/IO panelhez, mely **8 redőnycsatornát** és **16 NO relé kimenetet** kezel. A konfiguráció **PCF8574/PCF8575** IO-bővítőket, **INA219** fogyasztásmérőt, **LM75B** hőmérőt, valamint egy **SH1106 128×64** I2C OLED kijelzőt használ. Működik **Home Assistant**-tal, tartalmaz **OTA**-t és beépített **web szervert** is.

---

## Fő funkciók

- **8 redőny** (időalapú vezérlés, pozíció-számítás törésponttal)
- **16 NO relé kimenet** (out\_17…out\_32)
- **35 darab bemenet** (gombok/érintkezők), dedikált redőny és NO relé kimenet funkciókkal
- **I2C érzékelők**: INA219 (áram, feszültség, teljesítmény), LM75B (hőmérséklet)
- **3× ADC bemenet** (0–5 V, 0–10 V, 0–25 V — kalibrálással)
- **OLED kijelző oldalakkal + képernyővédő** (30 s inaktivitás után)
- **Ethernet (LAN8720)** + **OTA frissítés** + **integrált web\_server**
- **Home Assistant idő** és **időzóna** támogatás
- **Interlock a redőnymotorokhoz**, hogy egyszerre csak egy irány legyen aktív

---

## Hardver és kompatibilitás

- **MCU**: ESP32, `board: nodemcu-32s` (ESP-IDF framework)
- **Ethernet PHY**: LAN8720  
  - MDC: **GPIO23**
  - MDIO: **GPIO18**
  - CLK (külső): **GPIO0** (`CLK_EXT_IN`)
  - PHY cím: **1**
  - Power pin: **GPIO16**
- **I2C busz**: SDA **GPIO17**, SCL **GPIO33**, 400 kHz
- **OLED**: SH1106 128×64 @ **0x3C**
- **INA219**: @ **0x40**, sönt: **0.1 Ω**
- **LM75(B)**: külső komponens, I2C érzékelő
- **PCF857x címek**:
  - Bemenetek 1–14: **0x20** (PCF8575, `pcf_inputs_1to14`)
  - Bemenetek 15–28: **0x21** (PCF8575, `pcf_inputs_15to28`)
  - Bemenetek 28–35 menü: **0x22** (PCF8574, `pcf_inputs_28to35_menu`)
  - **NO relé kimenetek** bal oldal: **0x23** (PCF8575, `pcf_left`)
  - **NO relé kimenetek** jobb oldal: **0x24** (PCF8575, `pcf_right`)

> **Megjegyzés:** A fenti kiosztás a konfigurációból következik; igazítsd a valós panel-bekötésedhez.

---

## Bekötési és modul-információk

- **Buzzer**: GPIO2 (aktív magas)
- **Redőny relék** (interlock): minden redőnyhöz 2 relé kimenet (pl. `cover_open_01_out01` és `cover_close_01_out02`).  
  A relék a `pcf_left`/`pcf_right` expanderre vannak kötve (inverz logika).
- **NO relé kimenetek**: `out_17 … out_32` a `pcf_left`/`pcf_right` expander portjain.
- **Bemenetek**: `IN_01 … IN_35` a három input expanderen.

**ADC bemenetek:**
- **GPIO39** — „A1 0-5V” (3.30 V → 5.00 V skálázás)
- **GPIO36** — „A2 0-10V” (3.30 V → 10.00 V skálázás)
- **GPIO35** — „A3 0-25V” (3.30 V → 25.00 V skálázás)

---

## Telepítés és feltöltés

1. **Előkészületek**
   - ESPHome Dashboard vagy CLI telepítve
   - Home Assistant (ajánlott), hálózati elérés
   - Első feltöltéshez USB-sorosos kapcsolat az ESP32-höz

2. **Konfiguráció másolása**
   - Mentsd a `.yaml` fájlt az ESPHome projekt könyvtáradba.
   - Szükség esetén módosítsd a **Substitutions** értékeit (lásd lent).

3. **Fordítás és feltöltés**
   - Első alkalommal **USB-n** töltsd fel (ESPHome „Install” → „Plug into this computer”).
   - Sikeres indulás után használhatod az **OTA frissítést** (a configban engedélyezve).

4. **Hálózat**
   - Ethernet automatikusan indul a LAN8720 beállításokkal.
   - A beépített **web szerver** a **80-as porton** fut (`local: true`).

---

## Konfigurálható helyettesítések (substitutions)

A konfiguráció tetején lévő értékek gyors testreszabást adnak:

```yaml
substitutions:
  name: boneio-cover-mix
  friendly_name: BoneIO ESP Cover Mix
  serial_prefix: esp   # Don't change it.
  nyitasiIdoMinimum: "5"
  nyitasiIdoMaximum: "500"
  ALSO_PONT_KIJELZETT_NYITOTTSAG_PC: "10"

  # Entitásnevek (NO relé kimenetek és redőnyök)
  light1Name: Light1
  ...
  cover8Name: Cover8
```

- **ALSO_PONT_KIJELZETT_NYITOTTSAG_PC**: a kijelzett „alsó pont” (pl. ablak alja) százaléka.
- **nyitasiIdoMinimum / nyitasiIdoMaximum**: a redőny fel/le időtartam számmezők korlátai (másodperc).

---

## Entitások és működés

### Redőnyök (8 csatorna)

- Nyers vezérlők: `shutter_raw_01` … `shutter_raw_08` (time_based, **belső**)
- Felhasználói entitások: `shutter_01` … `shutter_08` (template, **pozícióképes**)
- **Időtartamok** (fel/le) a Home Assistantban állíthatók a **Number** entitásokkal:
  - „Redőny NN FEL idő (s)” (`up_time_s_NN`)
  - „Redőny NN LE idő (s)” (`down_time_s_NN`)
- **Töréspont – nyitottság (%)** (`knee_raw_open_pc_NN`): a lineárisítás két szakaszának váltási pontja.
- **Térképzés**: a kijelzett pozíciót a konfiguráció a `knee` és a globális `ALSO_...` alapján számítja.
- **Interlock**: motor védelme — egy irányban aktív relé mellett a másik blokkolt.

> **Bootkor** a `screensaver_script` mellett egy `lambda` a **beállított Number értékekből** frissíti a nyers redőnyök `open_duration`/`close_duration` mezőit.

### NO relé kimenetek (16 csatorna)

- Bináris (ki/be) **NO relé kimenetek**: `light_17` … `light_32`  
- Kimenetek: `out_17` … `out_32` (PCF-re kötve, invertált logika)

### Bemenetek (IN_01…IN_35)

- `IN_01`/`IN_02` → **Redőny 1** fel/le (okos stop-logikával)
- `IN_03`/`IN_04` → **Redőny 2** … egészen `IN_15`/`IN_16` → **Redőny 8**
- `IN_17`…`IN_32` → **NO relé kimenet 17…32** kapcsolás (toggle)
- `IN_33`…`IN_35` → jelenleg **nincs akció** konfigurálva
- `boneIO_button` (`IN_35` busz 0x22, pin 7): kijelző oldalváltás / képernyővédő visszaébresztés

> Minden redőny két bemenettel rendelkezik (fel/le). Ha az aktuális irányban már mozog, rövid nyomás **STOP**-ot küld; ellenirány esetén **Stop → kis késleltetés → új irány**.

### Kijelző

- **Első oldal**: IP cím, Uptime, LM75B hőmérséklet
- **Utolsó oldal**: INA219 áram, teljesítmény, busz- és söntfeszültség
- **Képernyővédő**: 30 s inaktivitás után fekete képernyő
- **Oldalváltás**: a `boneIO_button` bemenet rövid nyomására

### Érzékelők

- **INA219**: `ina_current`, `ina_power`, `ina_bus_voltage`, `ina_shunt_voltage`
- **LM75B**: `boneIO_temp`
- **ADC**: „A1 0-5V” (GPIO39), „A2 0-10V” (GPIO36), „A3 0-25V” (GPIO35), kalibrált skálákkal
- **Uptime**: formázott szöveg és másodperc alapú szenzor

---

## Home Assistant integráció

- **API** engedélyezett, `reboot_timeout: 0s`
- **Idő** a Home Assistantból jön (`Europe/Budapest` időzóna)
- Az entitások automatikusan megjelennek (cover, NO relé kimenet, number, sensor, text_sensor).
- **OTA** és **web\_server** is aktív (web: port 80, `local: true`).

---

## Hibaelhárítás

- **Nincs Ethernet/IP**: ellenőrizd a LAN8720 tápját (**GPIO16**), a kristály/órajelet (CLK **GPIO0**), valamint a kábelezést (MDC **GPIO23**, MDIO **GPIO18**).
- **Redőny rossz irányba megy**: cseréld fel a két relé kimenetet az adott csatornán, vagy a motorvezetékeket a relén.
- **Pozíció pontatlan**: mérd ki a valós fel/le időket és állítsd be a megfelelő „FEL/LE idő (s)” Number entitásokat; finomhangold a **töréspont** értékét is.
- **Kijelző üres**: várd meg a 30 s képernyővédőt, majd nyomd meg a `boneIO_button`-t; ellenőrizd az I2C címet (**0x3C**).
- **LM75/INA219 nem látszik**: ellenőrizd az I2C címeket és a busz kapcsolatait; az LM75 komponens külső forrásból érkezik (lásd config).

---

## Biztonság

- A redőnymotorok és relék **hálózati feszültséggel** dolgozhatnak. Csak **szakképzett személy** végezzen szerelést.
- Kapcsold le a hálózati feszültséget a bekötés **minden módosítása előtt**.
- Használj **megfelelő biztosítékot** és védelmet a motorszabályozáshoz.

---

## Köszönetnyilvánítás

- **LM75** külső komponens: `github://energysmarthome/esphome-LM75@main` (köszönet BTomala munkájáért a forrásban megjelölt megjegyzés szerint).
- BoneIO & ESPHome közösség inspirációi és példái.

---

# BoneIO ESP Cover Mix — ESPHome configuration (EN)

## Table of Contents (EN)

- [Overview](#overview-en)
- [Key Features](#key-features-en)
- [Hardware & Compatibility](#hardware--compatibility-en)
- [Wiring & Modules](#wiring--modules-en)
- [Installation & Flashing](#installation--flashing-en)
- [Configurable substitutions](#configurable-substitutions-en)
- [Entities & Operation](#entities--operation-en)
  - [Covers (8 channels)](#covers-8-channels-en)
  - [NO relay outputs (16 channels)](#no-relay-outputs-16-channels-en)
  - [Inputs (IN\_01…IN\_35)](#inputs-in_01in_35-en)
  - [Display](#display-en)
  - [Sensors](#sensors-en)
- [Home Assistant integration](#home-assistant-integration-en)
- [Troubleshooting](#troubleshooting-en)
- [Safety](#safety-en)
- [Credits](#credits-en)

---

## Overview (EN) {#overview-en}

This project is an **ESP32 (NodeMCU-32S)** based **Ethernet (LAN8720)** controller for BoneIO/IO panels, managing **8 shutter channels** and **16 NO relay outputs**. It uses **PCF8574/PCF8575** IO expanders, an **INA219** power monitor, an **LM75B** temperature sensor, and an **SH1106 128×64** I2C OLED display. It integrates with **Home Assistant**, provides **OTA** updates, and includes a built-in **web server**.

---

## Key Features (EN) {#key-features-en}

- **8 shutters** (time-based control with two-segment position mapping)
- **16 NO relay outputs** (out\_17…out\_32)
- **Up to 35 inputs** with dedicated shutter/NO relay output actions
- **I2C sensors**: INA219 (current/voltage/power), LM75B (temperature)
- **3× ADC inputs** (0–5 V, 0–10 V, 0–25 V with calibration)
- **OLED display with pages + screensaver** (after 30 s)
- **Ethernet (LAN8720)** + **OTA** + **web\_server**
- **HA time** & **timezone** support
- **Interlock** for motor direction safety

---

## Hardware & Compatibility (EN) {#hardware--compatibility-en}

- **MCU**: ESP32, `board: nodemcu-32s` (ESP-IDF)
- **Ethernet PHY**: LAN8720  
  - MDC: **GPIO23**
  - MDIO: **GPIO18**
  - CLK (external): **GPIO0** (`CLK_EXT_IN`)
  - PHY address: **1**
  - Power pin: **GPIO16**
- **I2C**: SDA **GPIO17**, SCL **GPIO33**, 400 kHz
- **OLED**: SH1106 128×64 @ **0x3C**
- **INA219**: @ **0x40**, shunt: **0.1 Ω**
- **LM75(B)**: external ESPHome component
- **PCF857x addresses**:
  - Inputs 1–14: **0x20** (PCF8575, `pcf_inputs_1to14`)
  - Inputs 15–28: **0x21** (PCF8575, `pcf_inputs_15to28`)
  - Inputs 28–35 menu: **0x22** (PCF8574, `pcf_inputs_28to35_menu`)
  - **NO relay outputs** left side: **0x23** (PCF8575, `pcf_left`)
  - **NO relay outputs** right side: **0x24** (PCF8575, `pcf_right`)

> Adjust to your actual wiring; the above is derived from the YAML configuration.

---

## Wiring & Modules (EN) {#wiring--modules-en}

- **Buzzer**: GPIO2 (active high)
- **Shutter relays**: two relays per shutter (open/close) on PCF expanders (inverted logic)
- **NO relay outputs**: `out_17 … out_32` on PCF expanders
- **Inputs**: `IN_01 … IN_35` across three PCF input expanders

**ADC inputs:**
- **GPIO39** — “A1 0-5V” (3.30 V → 5.00 V scaling)
- **GPIO36** — “A2 0-10V” (3.30 V → 10.00 V scaling)
- **GPIO35** — “A3 0-25V” (3.30 V → 25.00 V scaling)

---

## Installation & Flashing (EN) {#installation--flashing-en}

1. **Prerequisites**
   - ESPHome Dashboard or CLI, Home Assistant (recommended)
   - USB serial access for the **first** flash

2. **Copy the configuration**
   - Save the `.yaml` into your ESPHome project directory.
   - Tweak **substitutions** as needed (see below).

3. **Build & flash**
   - First install via **USB** from ESPHome.
   - Afterwards use **OTA** updates.

4. **Networking**
   - Ethernet comes up automatically with LAN8720 settings.
   - Built-in **web server** on port **80** (`local: true`).

---

## Configurable substitutions (EN) {#configurable-substitutions-en}

```yaml
substitutions:
  name: boneio-cover-mix
  friendly_name: BoneIO ESP Cover Mix
  serial_prefix: esp   # Don't change it.
  nyitasiIdoMinimum: "5"
  nyitasiIdoMaximum: "500"
  ALSO_PONT_KIJELZETT_NYITOTTSAG_PC: "10"
  # NO relay output and Cover display names...
```

- **ALSO_PONT_KIJELZETT_NYITOTTSAG_PC**: displayed “lower edge” percentage in UI mapping.
- **nyitasiIdoMinimum / nyitasiIdoMaximum**: min/max bounds for per-shutter up/down time Numbers (seconds).

---

## Entities & Operation (EN) {#entities--operation-en}

### Covers (8 channels) {#covers-8-channels-en}

- Raw covers: `shutter_raw_01 … _08` (time_based, **internal**)
- User covers: `shutter_01 … _08` (template, **position capable**)
- Set per-shutter **up/down times** via HA Numbers:
  - “Redőny NN FEL idő (s)” / “Redőny NN LE idő (s)”
- Set per-shutter **knee** (% open) via Number:
  - “Redőny NN töréspont – nyitottság (%)”
- **Boot-time lambda** syncs durations from Numbers to raw covers.
- **Interlock** ensures only one direction relay is active.

### NO relay outputs (16 channels) {#no-relay-outputs-16-channels-en}

- Binary **NO relay outputs**: `light_17 … light_32` driving `out_17 … out_32`

### Inputs (IN_01…IN_35) {#inputs-in_01in_35-en}

- `IN_01/02` → Shutter 1 up/down (smart stop logic), … up to `IN_15/16` → Shutter 8
- `IN_17 … 32` → toggle **NO relay output** 17 … 32
- `IN_33 … 35` → no action defined
- `boneIO_button` (PCF 0x22 pin 7) → display page cycling / wake screensaver

### Display (EN) {#display-en}

- **First page**: IP address, uptime, LM75B temperature
- **Last page**: INA219 current, power, bus & shunt voltages
- **Screensaver** after 30 s of inactivity

### Sensors (EN) {#sensors-en}

- **INA219** (`ina_current`, `ina_power`, `ina_bus_voltage`, `ina_shunt_voltage`)
- **LM75B** (`boneIO_temp`)
- **ADC**: “A1 0-5V” (GPIO39), “A2 0-10V” (GPIO36), “A3 0-25V” (GPIO35) with linear calibration
- **Uptime** as formatted text & seconds sensor

---

## Home Assistant integration (EN) {#home-assistant-integration-en}

- **API** enabled, `reboot_timeout: 0s`
- **Time** from Home Assistant (`Europe/Budapest` timezone)
- Entities auto-discovered (covers, NO relay outputs, numbers, sensors, text_sensors).
- **OTA** and **web\_server** active (port 80, `local: true`).

---

## Troubleshooting (EN) {#troubleshooting-en}

- **No Ethernet/IP**: check LAN8720 power (**GPIO16**), external clock (CLK **GPIO0**), and MDC (**GPIO23**) / MDIO (**GPIO18**) wiring.
- **Shutter direction wrong**: swap the two relays or the motor leads.
- **Position inaccurate**: measure real up/down durations and set the Number entities; tune the **knee** value.
- **Display blank**: wake from screensaver using `boneIO_button`; verify I2C address (**0x3C**).
- **LM75/INA219 missing**: verify I2C wiring/addresses; LM75 uses an external component source.

---

## Safety (EN) {#safety-en}

- Shutter motors/relays may involve **mains voltage** — only qualified personnel should handle wiring.
- Disconnect mains power **before** making changes.
- Use proper **fusing** and protection.

---

## Credits (EN) {#credits-en}

- External LM75 component: `github://energysmarthome/esphome-LM75@main` (thanks to BTomala as noted in source).
- Thanks to the BoneIO & ESPHome communities.

---

**License**: This README is provided “as is” for your project docs. Add your project’s license information here.
