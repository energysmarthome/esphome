# BoneIO ESP Cover (ESPHome) — README

**Languages:** [Magyar](#magyar) • [English](#english)

---

## Magyar

Ez a projekt egy **16 csatornás redőny-/árnyékolóvezérlő** ESPHome konfiguráció BoneIO hardverhez (ESP32 + LAN8720 Ethernet + I²C bővítők). Tartalmaz
- időalapú (time_based) **nyers cover** entitásokat,
- egy **nemlineáris pozíció-megjelenítést** és -vezérlést (töréspont/„ablak alja”),
- **PCF8574/PCF8575** I/O expanderre kötött fizikai bemeneteket és reléket **interlock** védelemmel,
- **OLED kijelzőt** (status oldalak + képernyőkímélő),
- hőmérséklet, feszültség és fogyasztás mérését (**LM75B**, **INA219**, ADC bemenetek),
- **Ethernetes** kapcsolódást, **OTA** frissítést és beépített **webszervert**.

> **Nyelv/környezet:** Europe/Budapest időzóna; a konfiguráció ESP-IDF frameworkre épül és `nodemcu-32s` alapú ESP32-vel készült.

---

## Tartalomjegyzék

1. [Hardver & követelmények](#hardver--követelmények)  
2. [Bekötés, buszok és címek](#bekötés-buszok-és-címek)  
3. [Telepítés & feltöltés](#telepítés--feltöltés)  
4. [Első indítás & OLED](#első-indítás--oled)  
5. [Entitások áttekintése (HA)](#entitások-áttekintése-ha)  
6. [Kalibráció és működés](#kalibráció-és-működés)  
7. [Bemenetek és relék kiosztása](#bemenetek-és-relék-kiosztása)  
8. [Mérések (INA219, LM75B, ADC-k)](#mérések-ina219-lm75b-adc-k)  
9. [Hálózat, API, OTA, web](#hálózat-api-ota-web)  
10. [Testreszabás](#testreszabás)  
11. [Hibaelhárítás](#hibaelhárítás)  
12. [Köszönet, licenc-megjegyzések](#köszönet-licenc-megjegyzések)

---

## Hardver & követelmények

- **MCU:** ESP32 (board: `nodemcu-32s`)  
- **Ethernet PHY:** LAN8720 (külső órajel: GPIO0, `CLK_EXT_IN`, PHY addr: 1, táp: GPIO16)  
- **I²C busz:** SDA GPIO17, SCL GPIO33 @ 400 kHz  
- **I/O expander(ek):** PCF8574/PCF8575 (bemenet/relé meghajtás)  
- **Kijelző:** SH1106 128×64 OLED, I²C címe 0x3C  
- **Hőmérő:** LM75B (külső komponens)  
- **Fogyasztásmérő:** INA219 (0x40)  
- **Bemenetek:** max. 35 (IN_01…IN_35)  
- **Kimenetek/relék:** 32 (16× nyit/zár)  
- **Csengő/Buzzer:** GPIO2  
- **Időforrás:** Home Assistant (`time:` HA integráció, `Europe/Budapest`)

> A konfiguráció **csak Ethernetet** használ (Wi-Fi nincs konfigurálva). Az első feltöltéshez USB szükséges.

---

## Bekötés, buszok és címek

**I²C**  
- Busz: `i2c_bus` — SDA **GPIO17**, SCL **GPIO33**, `scan: true`, 400 kHz  
- PCF8574/8575 címek:
  - Bemenetek: `0x20` (1–14), `0x21` (15–28), `0x22` (28–35 + menü)  
  - Relék (bal oldal): `0x23`  
  - Relék (jobb oldal): `0x24`  
- OLED (SH1106): `0x3C`  
- INA219: `0x40`  
- LM75B: (gyári alapértelmezett; külső komponens kezeli)

**Ethernet (LAN8720)**  
- MDC: **GPIO23**, MDIO: **GPIO18**, CLK: **GPIO0** (külső óra)  
- PHY addr: **1**, táp: **GPIO16**

> A PCF-ek aktív alacsony logikán futnak (`inverted: true`). A relékpárok **interlock** alatt vannak, így nem tud egyszerre a „fel” és „le” irány bekapcsolni.

---

## Telepítés & feltöltés

1. **ESPHome** telepítése (HA add-on vagy CLI).  
2. Helyezd el a mellékelt YAML-t saját projektedben (pl. `boneio-cover.yaml`).  
3. Cseréld a `substitutions` értékeit igény szerint:
   - `friendly_name`, `name`  
   - `nyitasiIdoMinimum` / `nyitasiIdoMaximum` (alap: 5…500 s)  
   - `ALSO_PONT_KIJELZETT_NYITOTTSAG_PC` (alap: 10%)  
4. **Első flash USB-n** (ESPHome: Install → Plug into this computer).  
5. **Ethernet** kábelt csatlakoztasd; az eszköz IP-t fog kapni (lásd OLED vagy HA).

> A `framework: esp-idf`. Ha PlatformIO/CLI-t használsz, győződj meg róla, hogy az ESP-IDF toolchain rendben van.

---

## Első indítás & OLED

Az eszköz bootkor:
- betölti az **első oldalt** (IP cím, uptime, hőmérséklet),
- frissíti az IP cím szenzort, elindítja a **képernyőkímélőt** (30 s inaktivitás).  
A **BoneIO gomb** (`boneIO_button`, IN a menü PCF-en) rövid nyomásra lapoz a kijelzőoldalak között; képernyőkímélőből „felébreszt”.

**Oldalak:**
- **first_page:** IP, Uptime, LM75B hőmérséklet  
- (köztes oldalak: bővíthető)  
- **last_page:** INA219 áram, teljesítmény, busz és shunt feszültség  
- **screensaver:** fekete képernyő

---

## Entitások áttekintése (HA)

**Cover-ek**
- `cover.shutter_01` … `cover.shutter_16`: **logikai** cover-ek, pozícióval  
- Minden logikai cover mögött egy **belső** `time_based` nyers cover van (`shutter_raw_xx`), amely ténylegesen a reléket vezérli.

**Number (konfiguráció) — minden redőnyhöz 3 db**
- `Redőny XX FEL idő (s)` → `up_time_s_xx`  
- `Redőny XX LE idő (s)` → `down_time_s_xx`  
- `Redőny XX töréspont – nyitottság (%)` → `knee_raw_open_pc_xx`  
> Ezek **mentésre kerülnek** (restore), és bootkor a nyers cover időzítéseire automatikusan betöltődnek.

**Binary sensorok (fizikai kapcsolók/gombok)**
- `IN_01` … `IN_32`: 16 redőnyhöz **FEL/LE** (gyárilag „on press” logika: **irányváltás = stop**, ismételt nyomás irányba = mozgatás)  
- `IN_33`, `IN_34`, `IN_35`: vezérlőmenü (jelen YAML-ban funkció nélkül)  
- `boneIO_button`: OLED lapozáshoz / képernyőkímélő reset

**Szenzorok**
- `A1 0-5V` (GPIO39), `A2 0-10V` (GPIO36), `A3 0-25V` (GPIO35) — lineáris kalibrációval  
- `INA219 Current/Power/Bus Voltage/Shunt Voltage`  
- `LM75B Temperature`  
- `Uptime` (formázott), `Serial No.` (MAC alapú), `IP Address`

**Switch-ek**
- `Buzzer` (GPIO2)  
- 32× relé: `RedőnyN relé1` (FEL), `RedőnyN relé2` (LE) — **interlock** védett párok

---

## Kalibráció és működés

### 1) Időalapú kalibráció
Minden redőnyre állítsd be a **FEL/LE időt** (másodpercben, 0.1 lépésköz, min…max substitutions szerint).  
Lépések (példa egy redőnyre):
1. Húzd le **zárt** állapotba (0%).  
2. Mérd meg stopperrel a teljes **fel** futási időt → írd be a `Redőny XX FEL idő (s)` mezőbe.  
3. Mérd meg a teljes **le** futási időt → írd be a `Redőny XX LE idő (s)` mezőbe.  
4. Végezz néhány 0↔100% parancsot és ellenőrizd, hogy a pozíciók **nem csúsznak**.

> A YAML a HA-ban beállított értékeket **bootkor betölti**: `on_boot → lambda → set_open/close_duration(...)`.

### 2) „Töréspont” és „ablak alja” (nemlineáris mapping)
Valós életben a „vizuálisan 10% nyitva” nem feltétlenül **időben 10%** futást jelent (pl. lamellák).  
Ezt a YAML két paraméterrel tudja korrigálni:

- **Nyers töréspont**: `Redőny XX töréspont – nyitottság (%)` (`knee_raw_open_pc_xx`)  
  – itt vált a leíró görbe meredeksége a **nyers időalapú 0…100%** tartományban.  
- **Megjelenített „ablak alja”**: `ALSO_PONT_KIJELZETT_NYITOTTSAG_PC` (globális substitution, **alap 10%**)  
  – a felhasználónak mutatott görbe ezen a ponton érje el a „vizuális alját”.

**Elv (egyszerűen):**  
- Ha a **nyers** nyitottság `raw_open` ≤ `knee_raw`, akkor a kijelzett pozíció:  
  `disp = raw_open * (knee_disp / knee_raw)`  
- Ha `raw_open` > `knee_raw`:  
  `disp = knee_disp + (raw_open - knee_raw) * ((1 - knee_disp) / (1 - knee_raw))`

> **Példa:** ha a lamellák már **nyers 30%**-nál „kinyílt ablak aljára” érnek, állítsd a `töréspontot` ~30%-ra, és hagyd a megjelenített „ablak alját” 10%-on. Ekkor a HA-ban a **10%** már valóban a „vizuális 10%”.

### 3) Fizikai gombok logikája
A bináris bemeneteknél a YAML **irány-emlékezettel** dolgozik (`moving_dir_xx` globális int: 1 = FEL, −1 = LE, 0 = áll):  
- Ugyanarra az irányra **ismét** nyomva → **STOP**  
- Ellentétes irány gombja → ha mozgás van, előbb **STOP**, kis késleltetés, majd indul az új irány

---

## Bemenetek és relék kiosztása

### Fizikai **bemenetek** (IN_xx) redőnyönként
| Redőny | FEL (open) | LE (close) |
|---|---|---|
| 1 | IN_01 | IN_02 |
| 2 | IN_03 | IN_04 |
| 3 | IN_05 | IN_06 |
| 4 | IN_07 | IN_08 |
| 5 | IN_09 | IN_10 |
| 6 | IN_11 | IN_12 |
| 7 | IN_13 | IN_14 |
| 8 | IN_15 | IN_16 |
| 9 | IN_17 | IN_18 |
| 10 | IN_19 | IN_20 |
| 11 | IN_21 | IN_22 |
| 12 | IN_23 | IN_24 |
| 13 | IN_25 | IN_26 |
| 14 | IN_27 | IN_28 |
| 15 | IN_29 | IN_30 |
| 16 | IN_31 | IN_32 |

> **IN_33–IN_35**: menü gombok; **boneIO_button** (menü PCF 0x22, #7) az OLED lapozáshoz.

### **Relé** kimenetek (interlock párok)

| Redőny | „FEL” relé (név → PCF/port) | „LE” relé (név → PCF/port) |
|---|---|---|
| 1 | `cover_open_01_out01` → **pcf_left** #15 | `cover_close_01_out02` → **pcf_left** #14 |
| 2 | `cover_open_02_out03` → pcf_left #13 | `cover_close_02_out04` → pcf_left #12 |
| 3 | `cover_open_03_out05` → pcf_left #11 | `cover_close_03_out06` → pcf_left #10 |
| 4 | `cover_open_04_out07` → pcf_left #9 | `cover_close_04_out08` → pcf_left #8 |
| 5 | `cover_open_05_out09` → **pcf_right** #15 | `cover_close_05_out10` → pcf_right #14 |
| 6 | `cover_open_06_out11` → pcf_right #13 | `cover_close_06_out12` → pcf_right #12 |
| 7 | `cover_open_07_out13` → pcf_right #11 | `cover_close_07_out14` → pcf_right #10 |
| 8 | `cover_open_08_out15` → pcf_right #9 | `cover_close_08_out16` → pcf_right #8 |
| 9 | `cover_open_09_out17` → **pcf_left** #0 | `cover_close_09_out18` → pcf_left #1 |
| 10 | `cover_open_10_out19` → pcf_left #2 | `cover_close_10_out20` → pcf_left #3 |
| 11 | `cover_open_11_out21` → pcf_left #4 | `cover_close_11_out22` → pcf_left #5 |
| 12 | `cover_open_12_out23` → pcf_left #6 | `cover_close_12_out24` → pcf_left #7 |
| 13 | `cover_open_13_out25` → **pcf_right** #0 | `cover_close_13_out26` → pcf_right #1 |
| 14 | `cover_open_14_out27` → pcf_right #2 | `cover_close_14_out28` → pcf_right #3 |
| 15 | `cover_open_15_out29` → pcf_right #4 | `cover_close_15_out30` → pcf_right #5 |
| 16 | `cover_open_16_out31` → pcf_right #6 | `cover_close_16_out32` → pcf_right #7 |

- **Interlock:** minden pár egymás interlockja (`interlock: [open, close]`, `interlock_wait_time: 5ms`, `restore_mode: always off`).

---

## Mérések (INA219, LM75B, ADC-k)

**INA219 @ 0x40**
- Shunt: **0.1 Ω**, max 32 V, 3.2 A; frissítés: 30 s  
- Entitások: `INA219 Current`, `INA219 Power`, `INA219 Bus Voltage`, `INA219 Shunt Voltage`

**LM75B hőmérő**
- Külső komponens (lásd `external_components`), entitás: `LM75B Temperature` (30 s)

**ADC-k** (1 s, 12 dB, 2 tizedes)
- `A1 0-5V` (GPIO39): 0…3.30 V → **0…5.00 V** kalibráció  
- `A2 0-10V` (GPIO36): 0…3.30 V → **0…10.00 V**  
- `A3 0-25V` (GPIO35): 0…3.30 V → **0…25.00 V**

---

## Hálózat, API, OTA, web

- **Ethernet**: `eth` interfész; az IP cím szenzor (`text_sensor.ip_address`) percenként frissül.  
- **API (HA)**: `reboot_timeout: 0s` → HA hiba esetén **nem** indul újra.  
- **OTA**: két platform (ESPHome + web_server).  
- **Beépített web**: `web_server: port 80, local: true` (csak helyi hálózat).  
- **Naplózás**: `logger:` engedélyezve.  
- **Mentések**: `preferences.flash_write_interval: 5s` (figyelj a túl gyakori írásokra automatizmusoknál).

---

## Testreszabás

**Substitutions**
```yaml
substitutions:
  name: boneio-cover
  friendly_name: "BoneIO ESP Cover"
  serial_prefix: "esp"
  nyitasiIdoMinimum: "5"
  nyitasiIdoMaximum: "500"
  ALSO_PONT_KIJELZETT_NYITOTTSAG_PC: "10"
```
- `name_add_mac_suffix: true` → a hostnévhez automatikusan hozzácsatolja a MAC‐szuffixet.  
- A `Serial No.` szenzor a `serial_prefix` + MAC fél stringjéből készül.

**Modbus / Dallas**  
- A YAML tartalmaz **kommentelt** sablont Modbus és 1-Wire hőmérők számára; igény szerint aktiválható és testreszabható.

**Buzzer**  
- `switch.buzzer` → tetszőleges visszajelzéshez (pl. gombnyomás, hiba).

---

## Hibaelhárítás

- **Nincs IP az OLED-en / HA-ban?**  
  Ellenőrizd az Ethernet bekötést (MDC/MDIO/CLK/PHY addr/táp), a hálózati switch DHCP-t, és hogy az ESP32-n a LAN8720 tápja (GPIO16) aktív-e.

- **OLED üres / „Unknown device”**  
  I²C címlista: nézd meg az ESPHome logot (I²C scan). Cím: 0x3C, busz: GPIO17/33. Kábelezés/árnyékolás rendben?

- **Bemenetek „inverz” viselkednek**  
  A PCF bemenetek `inverted: true`. Ha a kapcsolódás logikája más, változtasd a hardveren vagy a beállításon.

- **Egyszerre két relé kapcsolna**  
  Lehetetlennek kell lennie az **interlock** miatt. Ha mégis furcsaság van, ellenőrizd, hogy nem módosítottad az interlock párokat.

- **Pozíció „elmászik” idővel**  
  Finomhangold a **FEL/LE időket**, és ellenőrizd a mechanikai csúszást. Szükség esetén növeld kicsit a hosszabbik irány idejét.

- **Nemlineáris görbe nem jó**  
  Emeld/süllyeszd a **töréspontot** (`knee_raw_open_pc_xx`), illetve a globális **„ablak alja”** százalékot, amíg a kijelzett és a vizuális pozíció egybeesik.

---

## Köszönet, licenc-megjegyzések

- **LM75** komponens forrás: `energysmarthome/esphome-LM75` (köszönet az eredeti szerzőnek / BTomalának a megoldásért).  
- A YAML kommentjei és elnevezései a BoneIO ökoszisztémát követik.

---

## Gyors parancsok (példák)

- **Félig fel**: `cover.set_cover_position` → `{"entity_id": "cover.shutter_01", "position": 50}`  
- **Stop**: `cover.stop_cover`  
- **Teljesen fel/le**: `cover.open_cover` / `cover.close_cover`

---

## Változások / bővíthetőség ötletek

- További OLED oldalak (pl. minden cover állapot rövid listája).  
- Szenzorok frissítési idejének paraméterezése substitutions-szal.  
- Modbus/1-Wire szakasz aktiválása és eszközök felvétele.  
- Csoportos vezérlés (HA oldali scene/automation).

---

## English

This project is a **16-channel shutter/blind controller** ESPHome configuration for BoneIO hardware (ESP32 + LAN8720 Ethernet + I²C expanders). It includes
- time-based (`time_based`) **raw cover** entities,
- a **non-linear position display and control** (knee point / “bottom of window”),
- physical inputs and relays via **PCF8574/PCF8575** I/O expanders with **interlock** protection,
- an **OLED display** (status pages + screensaver),
- temperature, voltage and power measurements (**LM75B**, **INA219**, ADC inputs),
- **Ethernet** connectivity, **OTA** updates and a built-in **web server**.

> **Language/environment:** Europe/Budapest time zone; the configuration is built on the ESP-IDF framework and was made with an ESP32 based on `nodemcu-32s`.

---

## Table of Contents

1. [Hardware & Requirements](#hardware--requirements)  
2. [Wiring, Buses and Addresses](#wiring-buses-and-addresses)  
3. [Installation & Flashing](#installation--flashing)  
4. [First Boot & OLED](#first-boot--oled)  
5. [Entities Overview (HA)](#entities-overview-ha)  
6. [Calibration & Operation](#calibration--operation)  
7. [Inputs and Relay Mapping](#inputs-and-relay-mapping)  
8. [Measurements (INA219, LM75B, ADCs)](#measurements-ina219-lm75b-adcs)  
9. [Network, API, OTA, Web](#network-api-ota-web)  
10. [Customization](#customization)  
11. [Troubleshooting](#troubleshooting)  
12. [Credits & License Notes](#credits--license-notes)

---

## Hardware & Requirements

- **MCU:** ESP32 (board: `nodemcu-32s`)  
- **Ethernet PHY:** LAN8720 (external clock: GPIO0, `CLK_EXT_IN`, PHY addr: 1, power: GPIO16)  
- **I²C bus:** SDA GPIO17, SCL GPIO33 @ 400 kHz  
- **I/O expander(s):** PCF8574/PCF8575 (inputs/relay drive)  
- **Display:** SH1106 128×64 OLED, I²C address 0x3C  
- **Thermometer:** LM75B (external component)  
- **Power monitor:** INA219 (0x40)  
- **Inputs:** up to 35 (IN_01…IN_35)  
- **Outputs/relays:** 32 (16× up/down)  
- **Bell/Buzzer:** GPIO2  
- **Time source:** Home Assistant (`time:` HA integration, `Europe/Budapest`)

> The configuration **uses Ethernet only** (Wi-Fi is not configured). USB is required for the first flash.

---

## Wiring, Buses and Addresses

**I²C**  
- Bus: `i2c_bus` — SDA **GPIO17**, SCL **GPIO33**, `scan: true`, 400 kHz  
- PCF8574/8575 addresses:
  - Inputs: `0x20` (1–14), `0x21` (15–28), `0x22` (28–35 + menu)  
  - Relays (left side): `0x23`  
  - Relays (right side): `0x24`  
- OLED (SH1106): `0x3C`  
- INA219: `0x40`  
- LM75B: (factory default; handled by external component)

**Ethernet (LAN8720)**  
- MDC: **GPIO23**, MDIO: **GPIO18**, CLK: **GPIO0** (external clock)  
- PHY addr: **1**, power: **GPIO16**

> The PCFs operate with active-low logic (`inverted: true`). Relay pairs are **interlocked**, so “up” and “down” cannot be on at the same time.

---

## Installation & Flashing

1. Install **ESPHome** (HA add-on or CLI).  
2. Place the provided YAML in your project (e.g. `boneio-cover.yaml`).  
3. Adjust `substitutions` as needed:
   - `friendly_name`, `name`  
   - `nyitasiIdoMinimum` / `nyitasiIdoMaximum` (default: 5…500 s)  
   - `ALSO_PONT_KIJELZETT_NYITOTTSAG_PC` (default: 10%)  
4. **First flash over USB** (ESPHome: Install → Plug into this computer).  
5. Connect the **Ethernet** cable; the device will obtain an IP (see OLED or HA).

> `framework: esp-idf`. If you use PlatformIO/CLI, ensure the ESP-IDF toolchain is set up correctly.

---

## First Boot & OLED

On boot the device:
- loads the **first page** (IP address, uptime, temperature),
- updates the IP address sensor and starts the **screensaver** (after 30 s inactivity).  
The **BoneIO button** (`boneIO_button`, on the menu PCF) short-press cycles OLED pages; it also wakes the screen from the screensaver.

**Pages:**
- **first_page:** IP, Uptime, LM75B temperature  
- (intermediate pages: extendable)  
- **last_page:** INA219 current, power, bus and shunt voltage  
- **screensaver:** black screen

---

## Entities Overview (HA)

**Covers**
- `cover.shutter_01` … `cover.shutter_16`: **logical** covers with position  
- Each logical cover is backed by an **internal** `time_based` raw cover (`shutter_raw_xx`) that actually drives the relays.

**Number (configuration) — 3 per shutter**
- `Shutter XX UP time (s)` → `up_time_s_xx`  
- `Shutter XX DOWN time (s)` → `down_time_s_xx`  
- `Shutter XX knee – openness (%)` → `knee_raw_open_pc_xx`  
> These are **persisted** (restore) and are automatically loaded into the raw cover timings at boot.

**Binary sensors (physical switches/buttons)**
- `IN_01` … `IN_32`: **UP/DOWN** for 16 shutters (default “on press” logic: **direction change = stop**, pressing again in the direction = move)  
- `IN_33`, `IN_34`, `IN_35`: control menu (no function in this YAML)  
- `boneIO_button`: for OLED paging / screensaver reset

**Sensors**
- `A1 0-5V` (GPIO39), `A2 0-10V` (GPIO36), `A3 0-25V` (GPIO35) — linear calibration  
- `INA219 Current/Power/Bus Voltage/Shunt Voltage`  
- `LM75B Temperature`  
- `Uptime` (formatted), `Serial No.` (MAC-based), `IP Address`

**Switches**
- `Buzzer` (GPIO2)  
- 32× relays: `ShutterN relay1` (UP), `ShutterN relay2` (DOWN) — **interlock** protected pairs

---

## Calibration & Operation

### 1) Time-based calibration
Set **UP/DOWN times** for each shutter (in seconds, step 0.1, min…max via substitutions).  
Steps (example for one shutter):
1. Drive it fully **closed** (0%).  
2. Measure total **up** run time with a stopwatch → enter into `Shutter XX UP time (s)`.  
3. Measure total **down** run time → enter into `Shutter XX DOWN time (s)`.  
4. Execute a few 0↔100% commands and verify positions **don’t drift**.

> The YAML **loads** HA-set values **at boot**: `on_boot → lambda → set_open/close_duration(...)`.

### 2) “Knee point” and “bottom of window” (non-linear mapping)
In real life, “visually 10% open” isn’t necessarily **10% in time** (e.g., slats).  
The YAML compensates with two parameters:

- **Raw knee:** `Shutter XX knee – openness (%)` (`knee_raw_open_pc_xx`)  
  – slope switch point on the **raw, time-based 0…100%** domain.  
- **Displayed “bottom of window”:** `ALSO_PONT_KIJELZETT_NYITOTTSAG_PC` (global substitution, **default 10%**)  
  – the user-facing curve reaches the visual bottom at this point.

**Idea (simply):**  
- If **raw** openness `raw_open` ≤ `knee_raw`, displayed position:  
  `disp = raw_open * (knee_disp / knee_raw)`  
- If `raw_open` > `knee_raw`:  
  `disp = knee_disp + (raw_open - knee_raw) * ((1 - knee_disp) / (1 - knee_raw))`

> **Example:** if slats already reach the “bottom of window” at **raw 30%**, set the `knee` to ~30% and keep the displayed bottom at 10%. Then **10%** in HA corresponds to the **visual 10%**.

### 3) Physical button logic
Binary inputs use **direction memory** in the YAML (`moving_dir_xx` global int: 1 = UP, −1 = DOWN, 0 = idle):  
- Pressing **again** in the same direction → **STOP**  
- Pressing the opposite direction → if moving, first **STOP**, short delay, then start the new direction

---

## Inputs and Relay Mapping

### Physical **inputs** (IN_xx) per shutter
| Shutter | UP (open) | DOWN (close) |
|---|---|---|
| 1 | IN_01 | IN_02 |
| 2 | IN_03 | IN_04 |
| 3 | IN_05 | IN_06 |
| 4 | IN_07 | IN_08 |
| 5 | IN_09 | IN_10 |
| 6 | IN_11 | IN_12 |
| 7 | IN_13 | IN_14 |
| 8 | IN_15 | IN_16 |
| 9 | IN_17 | IN_18 |
| 10 | IN_19 | IN_20 |
| 11 | IN_21 | IN_22 |
| 12 | IN_23 | IN_24 |
| 13 | IN_25 | IN_26 |
| 14 | IN_27 | IN_28 |
| 15 | IN_29 | IN_30 |
| 16 | IN_31 | IN_32 |

> **IN_33–IN_35**: menu buttons; **boneIO_button** (menu PCF 0x22, #7) for OLED paging.

### **Relay** outputs (interlock pairs)

| Shutter | “UP” relay (name → PCF/port) | “DOWN” relay (name → PCF/port) |
|---|---|---|
| 1 | `cover_open_01_out01` → **pcf_left** #15 | `cover_close_01_out02` → **pcf_left** #14 |
| 2 | `cover_open_02_out03` → pcf_left #13 | `cover_close_02_out04` → pcf_left #12 |
| 3 | `cover_open_03_out05` → pcf_left #11 | `cover_close_03_out06` → pcf_left #10 |
| 4 | `cover_open_04_out07` → pcf_left #9 | `cover_close_04_out08` → pcf_left #8 |
| 5 | `cover_open_05_out09` → **pcf_right** #15 | `cover_close_05_out10` → pcf_right #14 |
| 6 | `cover_open_06_out11` → pcf_right #13 | `cover_close_06_out12` → pcf_right #12 |
| 7 | `cover_open_07_out13` → pcf_right #11 | `cover_close_07_out14` → pcf_right #10 |
| 8 | `cover_open_08_out15` → pcf_right #9 | `cover_close_08_out16` → pcf_right #8 |
| 9 | `cover_open_09_out17` → **pcf_left** #0 | `cover_close_09_out18` → pcf_left #1 |
| 10 | `cover_open_10_out19` → pcf_left #2 | `cover_close_10_out20` → pcf_left #3 |
| 11 | `cover_open_11_out21` → pcf_left #4 | `cover_close_11_out22` → pcf_left #5 |
| 12 | `cover_open_12_out23` → pcf_left #6 | `cover_close_12_out24` → pcf_left #7 |
| 13 | `cover_open_13_out25` → **pcf_right** #0 | `cover_close_13_out26` → pcf_right #1 |
| 14 | `cover_open_14_out27` → pcf_right #2 | `cover_close_14_out28` → pcf_right #3 |
| 15 | `cover_open_15_out29` → pcf_right #4 | `cover_close_15_out30` → pcf_right #5 |
| 16 | `cover_open_16_out31` → pcf_right #6 | `cover_close_16_out32` → pcf_right #7 |

- **Interlock:** every pair is each other’s interlock (`interlock: [open, close]`, `interlock_wait_time: 5ms`, `restore_mode: always off`).

---

## Measurements (INA219, LM75B, ADCs)

**INA219 @ 0x40**
- Shunt: **0.1 Ω**, max 32 V, 3.2 A; update: 30 s  
- Entities: `INA219 Current`, `INA219 Power`, `INA219 Bus Voltage`, `INA219 Shunt Voltage`

**LM75B thermometer**
- External component (see `external_components`), entity: `LM75B Temperature` (30 s)

**ADCs** (1 s, 12 dB, 2 decimals)
- `A1 0-5V` (GPIO39): 0…3.30 V → **0…5.00 V** calibration  
- `A2 0-10V` (GPIO36): 0…3.30 V → **0…10.00 V**  
- `A3 0-25V` (GPIO35): 0…3.30 V → **0…25.00 V**

---

## Network, API, OTA, Web

- **Ethernet:** `eth` interface; IP address sensor (`text_sensor.ip_address`) updates every minute.  
- **API (HA):** `reboot_timeout: 0s` → **does not** reboot on HA error.  
- **OTA:** two platforms (ESPHome + web_server).  
- **Built-in web:** `web_server: port 80, local: true` (local network only).  
- **Logging:** `logger:` enabled.  
- **Persistence:** `preferences.flash_write_interval: 5s` (beware of overly frequent writes in automations).

---

## Customization

**Substitutions**
```yaml
substitutions:
  name: boneio-cover
  friendly_name: "BoneIO ESP Cover"
  serial_prefix: "esp"
  nyitasiIdoMinimum: "5"
  nyitasiIdoMaximum: "500"
  ALSO_PONT_KIJELZETT_NYITOTTSAG_PC: "10"
```
- `name_add_mac_suffix: true` → automatically appends the MAC suffix to the hostname.  
- The `Serial No.` sensor is built from `serial_prefix` + half of the MAC string.

**Modbus / Dallas**  
- The YAML includes a **commented** template for Modbus and 1-Wire thermometers; enable and customize as needed.

**Buzzer**  
- `switch.buzzer` → for arbitrary feedback (e.g., button press, error).

---

## Troubleshooting

- **No IP on OLED / in HA?**  
  Check Ethernet wiring (MDC/MDIO/CLK/PHY addr/power), your switch/DHCP, and whether the LAN8720 power (GPIO16) is active on the ESP32.

- **OLED blank / “Unknown device”**  
  I²C address list: check ESPHome logs (I²C scan). Address: 0x3C, bus: GPIO17/33. Cabling/shielding OK?

- **Inputs behave “inverted”**  
  PCF inputs are `inverted: true`. If your wiring logic differs, change it in hardware or in the settings.

- **Two relays appear to switch at once**  
  Should be impossible due to **interlock**. If you still see oddities, verify you didn’t alter the interlock pairs.

- **Position “drifts” over time**  
  Fine-tune **UP/DOWN times** and check for mechanical slip. If needed, slightly increase the longer direction time.

- **Non-linear curve feels off**  
  Raise/lower the **knee** (`knee_raw_open_pc_xx`) and/or the global **“bottom of window”** percentage until displayed and visual position match.

---

## Credits & License Notes

- **LM75** component source: `energysmarthome/esphome-LM75` (thanks to the original author / BTomala for the solution).  
- YAML comments and naming follow the BoneIO ecosystem.

---

## Quick Commands (examples)

- **Half open:** `cover.set_cover_position` → `{"entity_id": "cover.shutter_01", "position": 50}`  
- **Stop:** `cover.stop_cover`  
- **Fully open/close:** `cover.open_cover` / `cover.close_cover`

---

## Ideas for Changes / Extensibility

- More OLED pages (e.g., compact list of all cover states).  
- Parameterize sensor update intervals via substitutions.  
- Enable Modbus/1-Wire section and add devices.  
- Group control (scenes/automations in HA).
