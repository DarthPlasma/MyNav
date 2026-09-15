# MyNAV

Fork personale di INAV basato sul tag `9.1.0`. Aggiunge il display ADS-B avanzato portato da MyTAflight
(selezione del velivolo in avvicinamento, avviso critico, cono di avvicinamento e conteggio del traffico)
e i target delle schede MyTAflight che INAV non ha con lo stesso nome.
Il comportamento visto dal pilota è descritto in [docs/ADSB.md](docs/ADSB.md), sezione MyNAV.

## Cosa cambia rispetto a INAV 9.1.0

| Area | File |
| --- | --- |
| Selezione della minaccia e geometria del cono (funzioni pure, con unit test) | `src/main/io/adsb_threat.c/.h`, `src/test/unit/adsb_threat_unittest.cc` |
| Elementi OSD 169 `OSD_ADSB_CRITICAL_WARNING`, 170 `OSD_ADSB_CONE`, 171 `OSD_ADSB_STATUS` | `src/main/io/osd.c`, `src/main/io/osd.h` |
| CLI `osd_adsb_detection_cone`, `osd_adsb_aircraft_toa`, `adsb_max_vehicles` | `src/main/fc/settings.yaml`, `docs/Settings.md` (rigenerato) |
| Lista velivoli configurabile fino a 12, nessuno scarto oltre 64 km | `src/main/io/adsb.c/.h`, `src/main/target/common.h`, `src/main/fc/fc_msp.c` |
| Target aggiunti o ritoccati (vedi sotto) | `src/main/target/SPEEDYBEEF405AIO`, `FLYWOOF745`, `TMOTORF7`, `FOXEERF745AIO` |
| Strumenti | `mynav-tools/` |

## Decisioni prese con l'utente

- Il cono è del velivolo in arrivo: vertice nella sua posizione attuale, asse lungo la sua rotta.
- Freccia = noi adesso. Crosshair = noi dopo il ToA, nello **stesso cono attuale**, proiettando solo la **nostra**
  velocità: il velivolo non viene spostato. MyTAflight spostava anche il velivolo (`src/main/io/adsb.c:384-386` di
  quel repo), e con quel calcolo il crosshair finisce quasi sempre fuori dal cono.
- Segni visti da chi guarda il velivolo arrivare: spostandoci verso la nostra destra, freccia e crosshair vanno a destra.
- ToA = distanza / velocità del velivolo, come MyTAflight. Con traffico lento in frontale la proiezione può superare
  il vertice e l'angolo si ribalta: lasciato così.
- Per essere una minaccia il velivolo deve anche segnalare heading e velocità validi (flag MAVLink).
- `osd_adsb_detection_cone` è in gradi, larghezza totale (default 20 = ±10°).
- L'elemento x/y usa il simbolo ADS-B del font INAV (`SYM_ADSB`) al posto della lettera A.
- `PG_ADSB_CONFIG` = 1100, lontano dalla sequenza upstream (in 10.x gli id 1045-1047 sono già usati).

## Da sapere al flash

`PG_OSD_CONFIG` passa da 15 a 0 (le versioni hanno 4 bit) e `PG_OSD_LAYOUTS_CONFIG` da 3 a 4: flashando MyNAV sopra
INAV la configurazione OSD e i layout tornano ai default. Salvare il `diff` prima e reincollarlo dopo.

## Target delle schede MyTAflight

Il build tool propone tutti i target INAV, ma mette in cima le schede MyTAflight: i 23 target che INAV ha con lo
stesso nome (KAKUTEH7 copre anche la Kakute H7 V1.3) più le schede qui sotto, abbinate confrontando con le config
Betaflight pin, sensori, bus SPI, ingressi ADC e orientamento del gyro.

| Scheda (nome Betaflight) | Target | Note |
| --- | --- | --- |
| SPEEDYBEE_F745_AIO | `SPEEDYBEEF745AIO` | stessa scheda |
| FLYWOOF722PROV2 | `FLYWOOF722PRO` | il target INAV ha già il gyro della V2 (ICM42688P, CW270) |
| FOXEERF745V4_AIO | `FOXEERF745AIO` | aggiunto il baro DPS310 montato sulla V4 |
| SPEEDYBEEF405AIOV2 | `SPEEDYBEEF405AIOV2`, variante MyNAV | gyro CW270, scala corrente 88, PINIO1 (PC14) = BEC 9V sul modo USER1 |
| FLYWOOF745AIOV2 | `FLYWOOF745AIOV2`, variante MyNAV | pin di `FLYWOOF745`, gyro CW90 |
| TMOTORF7_AIO | `TMOTORF7_AIO`, variante MyNAV | pin di `TMOTORF7`, gyro famiglia MPU6500, baro DPS310 su SPI |

Come i target INAV originali, le varianti non riportano la rotazione della scheda delle config Betaflight
(`DEFAULT_ALIGN_BOARD_YAW`: +45° sulla Flywoo F745 AIO V2). Prima del primo volo verificare l'orientamento nella tab
Setup del Configurator ed eventualmente impostare `align_board_yaw`.

Senza un equivalente in INAV, perché l'hardware è diverso: HGLRC_H743_LITE, BETAFPVH743, TMOTORH743, BETAFPVF405,
TMOTORVELOXF7SE, SPEEDYBEEF405V5, ZEEZWHOOP, AIRBRAINH743, IFLIGHT_BLITZ_F405, IFLIGHT_F745_AIO, IFLIGHT_SUCCEX_E_F7,
GEPRC_TAKER_H743V2, IFLIGHT_H743_AIO. Andrebbero scritti come target nuovi.

## Build (macOS)

- cmake 3.31 in un venv: `python3 -m venv tools/cmake-venv && tools/cmake-venv/bin/pip install cmake==3.31.10`
  (`tools/` è ignorata da git).
- Toolchain: la arm-none-eabi-gcc 13.2.1 che INAV si aspetta. Il primo configure la scarica da solo in
  `tools/arm-gnu-toolchain-13.2.rel1` (circa 1 GB). Un'altra versione va passata nel PATH con `-DCOMPILER_VERSION_CHECK=OFF`.
- Riga di comando: `PATH=$PWD/tools/cmake-venv/bin:$PATH cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`,
  poi `cmake --build build --target DAKEFPVH743`. Il `.hex` finisce in `build/`.
- Oppure: `python3 mynav-tools/build-tool.py` e poi http://localhost:8792. Con una toolchain diversa da quella di INAV
  aggiungere `--toolchain-bin <toolchain>/bin` al primo avvio; dopo il tool la legge da `build/CMakeCache.txt`.
- Il solo configure genera circa 1 GB in `build/`; `Release` evita i simboli di debug, il firmware è identico.
- Durante una build CMake può rifare il configure da solo: se in quel momento non trova nel PATH lo stesso compilatore
  cancella la cache, scarica la toolchain e torna a una build di debug che può riempire il disco. Il build tool mette
  nel PATH il compilatore della cache e non parte con meno di 1,5 GB liberi.
- I `.hex` compilati prima della pulizia del 2026-09-15 sono in `release/` (ignorata da git).

## Unit test

In CI (Linux) `adsb_threat_unittest` gira insieme agli altri test INAV (`cmake -DTOOLCHAIN= ..` e target `check`).
Su macOS con Apple clang 17 l'infrastruttura di test di INAV non parte: il generatore `src/utils/settings.rb`
fallisce anche con il `settings.yaml` originale di 9.1.0 ("templates must have C++ linkage").

## Strumenti

- `mynav-tools/osd-layout.html`: editor visuale dei layout, legge e scrive righe `osd_layout <layout> <item> <col> <row> <V|H>`.
  La tabella degli elementi viene da `src/main/io/osd.h`: dopo modifiche all'enum eseguire `python3 mynav-tools/update-osd-items.py`.
- `mynav-tools/build-tool.py`: scelta del target, build e download del `.hex`.

## Aperto

- Segni e crosshair da validare a terra e poi in volo (DVR), con l'iniettore ESP32 di MyTAflight.
- Le varianti di target sono verificate solo sulla carta e in compilazione: nessuna è stata provata su una scheda vera.
- Rebase su 10.x: conflitti attesi in `adsb.c/.h`, `osd.c/.h`, `settings.yaml`, `fc_msp.c`. La PR upstream #11346 (CPA)
  sostituisce la selezione del velivolo. Se upstream aggiunge elementi OSD dopo `OSD_THROTTLE_GAUGE`, gli indici
  169-171 cambiano e le righe `osd_layout` salvate vanno aggiornate.
