# Home Assistant Voice PE – Stealth Listen (láthatatlan mikrofonhallgatás) go2rtc-vel

Ez a repo egy **Home Assistant Voice Preview / Voice PE** ESPHome konfigurációt tartalmaz, kibővítve egy **Stealth Listen** funkcióval.


A megoldás úgy készült, hogy a Voice PE alapcsomag (wake word, VA, zenelejátszás, `sendspin`, stb.) működése megmaradjon.

---

## Hogyan működik?

1. ESPHome oldalon a Voice PE eredeti `i2s_mics` mikrofon streamjére ráakasztunk egy `on_data` hookot.
2. Ha a **Stealth Listen** switch ON:
   - az ESP **TCP-n csatlakozik** a HA (go2rtc) felé
   - elküld egy **WAV headert** (PCM 16kHz / 16bit / mono)
   - majd folyamatosan küldi a mikrofonadatot
3. go2rtc oldalon egy **TCP listen** forrás fogadja a streamet, és WebRTC-vel lejátszható.

### Miért TCP + WAV?

- UDP-n a WAV header “újracsomagolása” kattogást és instabil csatlakozást okozhat.
- TCP-n a WAV header biztosan az elején érkezik, stabil és “koppanásmentes”.

---

## Követelmények

- Home Assistant (HA OS is jó)
- ESPHome 2026.2.x (tesztelve: 2026.2.1)
- Voice PE (Home Assistant Voice Preview) eszköz
- go2rtc (add-on vagy más go2rtc példány)

---

## Fájlok

- `home-assistant-voice-09d0d3.yaml`  
  A fő eszköz YAML. Ez **csak** a helyi package-et include-olja.

- `packages/home-assistant-voice-local.yaml`  
  A Voice PE eredeti csomag **helyi másolata**, minimális kiegészítésekkel:
  - `includes: stealth_tcp_wav.h`
  - `globals: stealth_listen_enabled`
  - `microphone: i2s_mics` → `on_data` → stealth küldés
  - `script: stealth_listen_start/stop`
  - `switch: stealth_listen_switch`

- `stealth_tcp_wav.h`  
  A TCP kliens + WAV header + 32bit stereo → 16bit mono konverzió.

---

## Telepítés

### 1) Másold be a fájlokat az ESPHome config mappába

ESPHome-ban (HA OS esetén tipikusan: `/config/esphome/`):

```
/config/esphome/
├── home-assistant-voice-09d0d3.yaml
├── stealth_tcp_wav.h
└── packages/
    └── home-assistant-voice-local.yaml
```

> **Fontos:** a local package YAML-ban a behúzások számítanak (YAML). Ne “tördeld” át szerkesztővel.

---

### 2) go2rtc konfiguráció (HA oldalon)

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

### 3) ESPHome substitutions (IP/port)

A local package vagy a fő YAML substitutions részében ezek legyenek:

```yaml
substitutions:
  stealth_stream_host: "192.168.22.203"
  stealth_stream_port: "12345"
```

Megjegyzések:
	•	stealth_stream_host = a HA/go2rtc gép IP-je
	•	stealth_stream_port = ugyanaz a port, mint a go2rtc configban

---

### 4) Feltöltés / Flash

ESPHome:
	•	Validate
	•	Compile
	•	Upload

---

## Használat

1.	Home Assistantban az eszköznél megjelenik egy új switch: Stealth Listen
2.	Kapcsold ON.
3.	go2rtc Web UI-ban (vagy ahol használod): indítsd a stealth_mic streamet WebRTC-vel.

✅ Nincs jelzés az eszközön (nem villog, nem változik LED, nincs kijelzőmódosítás).

---

## Gyakori hibák / hibaelhárítás

**“Too many candidates found for id type microphone”**

Ez akkor történik, ha második mikrofont hozol létre (stealth_mic) és a Voice PE csomagban van microphone.mute/unmute: {}.

Megoldás: ebben a repóban ezért csak az i2s_mics létezik, nincs második microphone komponens.

---

**go2rtc: “Invalid data found when processing input”**

Ez tipikusan akkor volt, amikor raw PCM ment UDP-n és az ffmpeg autodetektált.

Megoldás: a TCP+WAV megoldás ezt elkerüli.

---

**Néma / rossz hang**

A Voice PE mikrofon sokszor 32bit stereo streamet ad.

Megoldás: a stealth_tcp_wav.h ezt konvertálja 16bit mono-ra, hogy a WAV headerrel egyezzen.

---

## Biztonság / adatvédelem

A Stealth Listen funkció “láthatatlan” az eszközön.
Csak saját hálózaton, saját eszközökre használd, és csak olyan helyen, ahol ehhez minden érintett hozzájárult.

---

## Licenc / köszönet

	•	Voice PE csomag: esphome/home-assistant-voice-pe
	•	go2rtc: AlexxIT/go2rtc
	•	ESPHome: esphome/esphome