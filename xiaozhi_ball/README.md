# Xiaozhi Ball – ESPHome Intercom Config

Ez a repó a **Xiaozhi Ball** projekthez tartozó **ESPHome intercom konfigurációkat** és a kapcsolódó **Intercom API** csomagot tartalmazza.

## Tartalom

A repó 3 mappára van bontva:

### 1) `intercom/`
**Sima intercom ESPHome config**  
Ebben a mappában találod a “csak intercom” működéshez szükséges ESPHome YAML konfigurációt.

### 2) `intercom api/`
**Komplett Intercom API repó**  
Itt van minden, ami az intercom API-hoz kell, többek között:
- leírások / dokumentáció
- fájlok, amiket be kell másolni a megfelelő helyre
- Lovelace kártya(ák)
- Home Assistant integráció
- egyéb kiegészítők és beállítások

### 3) `intercom-voice_assistant/`
**Intercom + Voice Assistant ESPHome config**  
Ebben a mappában az a konfiguráció található, ami **intercomként is működik**, és **voice assistant** funkciót is tartalmaz.

## Használat (röviden)

1. Válaszd ki, melyik ESPHome konfigurációra van szükséged:
   - csak intercom → `intercom/`
   - intercom + voice assistant → `intercom-voice_assistant/`
2. Az API + Home Assistant oldalhoz nézd meg a `intercom api/` mappát, és kövesd az ottani leírásokat.

## Megjegyzés

A mappákban található konfigurációk és leírások együtt alkotják a teljes rendszert (ESPHome eszköz + API + Home Assistant/Lovelace oldal).