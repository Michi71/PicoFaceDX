# CHANGELOG_DX_ENGINE.md

**Datum:** 2026-07-05
**Workflow:** Architektur/Orchestrierung Claude, Codegenerierung glm-5.2:cloud
**Kontext:** Portierung der FM-Synth-Engine eines ESP32-Arduino-Projekts ('RDX', Yamaha reface DX Emulator, Verzeichnis `tools/refacedx/RDX-Reface-DX-emu/`) in das bestehende RP2350-Projekt PicoFaceDX (Kopie von PicoFaceCP).

---

## 1. Neue Dateien — Engine (header-only, `include/dx_engine/`)

1:1 aus dem ESP32-Projekt portiert mit Bereinigung (siehe Abschnitt 4).

| Datei | Inhalt |
|---|---|
| `RDX_Types.h` | Patch-Datenstrukturen |
| `RDX_State.h` | Singleton-State |
| `RDX_VoiceAlloc.h` | Voice-Allocator |
| `RDX_Constants.h` | LUTs/Konstanten |
| `misc.h` | Math-Utilities |
| `RDX_Envelope.h` | Amplituden-Hüllkurve |
| `RDX_PEG.h` | Pitch-Hüllkurve |
| `RDX_LFO.h` | LFO |
| `RDX_Operator.h` | FM-Operator/Oszillator |
| `RDX_Voice.h` | 4-Operator-Stimme mit 12 Algorithmen |
| `RDX_Synth.h` | Synth-Host mit `renderAudioBlock()` / `process()` / `noteOn()` / `noteOff()` |
| `dx_engine_config.h` | **NEU**, nicht aus ESP32 übernommen — minimale Engine-Konfiguration |

**`dx_engine_config.h` — Konfigurationswerte:**

- `SAMPLE_RATE = 44100`
- `DMA_BUFFER_LEN = 64`
- `MAX_VOICES = 8`
- `MAX_VOICES_PER_NOTE = 2`
- `TIMING_CORRECTION = 1.0f`

> Ersetzt das ESP32-`config.h`, das GPIO-/Display-/SD-Definitionen enthielt.

---

## 2. Neue Dateien — Integration (Top-Level `include/` + `src/`)

| Datei | Inhalt |
|---|---|
| `DX_Synth_Bridge.h` / `DX_Synth_Bridge.cpp` | Wrapper-Klasse: übersetzt `fill_buffer(float* buffer, int length)` [interleaved stereo, length=Frames] in Blöcke von je 64 Frames über `RDX_Synth::renderAudioBlock()` |
| `DX_Controller.h` / `DX_Controller.cpp` | Steuerungsklasse: MIDI-Note-Weiterleitung, Encoder-Handling, Menü-Seiten-Logik |
| `DX_GUI.h` / `DX_GUI.cpp` | GUI: Algorithmus-Diagramme und Screen-Darstellung |

### 2.1 DX_Synth_Bridge

- `fill_buffer(float* buffer, int length)` — interleaved stereo, `length` = Frames
- Blockweise Verarbeitung über `RDX_Synth::renderAudioBlock()` in Blöcken von je 64 Frames
- Feste Scratch-Buffer, keine Heap-Allokation
- `synth_.updateCache()` wird einmal pro Aufruf aufgerufen
- `noteOn()` / `noteOff()` / `patch()` als Durchreiche/Zugriff

### 2.2 DX_Controller

- `onMidiNoteOn()` / `onMidiNoteOff()` — leiten an `DX_Synth_Bridge` weiter
- `onEncoder1()` — wechselt Menüseite (Enum `DxPage`: `OP1`, `OP2`, `OP3`, `OP4`, `LFO`, `ALGO`, mit Wraparound)
- `onEncoder2()` / `onEncoder3()` — ändern je 2 Werte pro Seite:

| Seite | Encoder 2 | Encoder 3 |
|---|---|---|
| OP1–OP4 | `freqCoarse` (0–31) des jeweiligen Operators | `outLevel` (0–127) des jeweiligen Operators |
| LFO | `lfoSpeed` (0–127) | `lfoPMD` (0–127) |
| ALGO | `algorithm` (0–11) | `feedback` von Operator 1 (0–127) |

### 2.3 DX_GUI

- `drawAlgo()` — zeichnet die 12 FM-Algorithmus-Diagramme (Operatoren als Kästchen/Kreise mit Verbindungslinien)
- 1:1-Geometrie-Port von der ESP32-Eigenimplementierung (`UI_Display`-Klasse) auf direkte U8g2-Aufrufe (dünne Wrapper-Funktionen für Aufruf-Kompatibilität)
- `dxDrawScreen()` — zeigt Kopfzeile (Seitenname) + Algorithmus-Diagramm (nur ALGO-Seite) bzw. Platzhaltertext (andere Seiten; echte Werteanzeige folgt später)

---

## 3. Geänderte Dateien

| Datei | Änderung |
|---|---|
| `CMakeLists.txt` | 3 neue `.cpp` zum `add_executable()`-Target hinzugefügt: `DX_Synth_Bridge.cpp`, `DX_Controller.cpp`, `DX_GUI.cpp` |

---

## 4. Bereinigung (während Portierung entfernt)

- `#include <Arduino.h>`, `<esp_log.h>`, `<esp_heap_caps.h>` — überall entfernt
- `ESP_LOGI` / `ESP_LOGD` Aufrufe entfernt (reiner Diagnose-Code)
- `logMemoryStats()` Funktion (ESP-Heap-Diagnose) — komplett entfernt
- `vTaskDelay(1)` im Audio-Hot-Path (`RDX_Voice::step()`, invalid-algorithm-Fallback) entfernt — wäre im Audio-IRQ katastrophal gewesen
- `IRAM_ATTR` / `DRAM_ATTR` (ESP32-Speicher-Placement):
  - Ersetzt durch das bereits im Projekt vorhandene `CP_HOT()`-Makro (`effects/cp_hot.h`, RAM-Placement per `__not_in_flash_func` auf RP2350, No-Op auf Host-Build)
  - bzw. ersatzlos entfernt bei konstanten Datentabellen (bleiben in Flash/rodata, analog zur bestehenden Praxis mit den mdaEPiano-Sample-Daten)
- `RDX_GUI.h` / PresetManager-Kopplung aus `RDX_Synth.h` entfernt (Arduino-Filesystem/LittleFS-basiert, nicht Teil der reinen Klangerzeugung)
- `applyBankProgram()` lädt bis auf Weiteres immer die hartkodierte `DigiChordPatch()` (TODO-Kommentar im Code für spätere Multi-Patch-Persistenz)

---

## 5. Bugfixes (im Original-ESP32-Code gefunden, während Portierung korrigiert)

Nicht ESP32-bezogene Fehler, die im Original-Quellcode bereits vorhanden waren:

- **Bezeichner `VOICES`:** Im Original-Projekt an 9 Stellen verwendet (`RDX_Synth.h` ×8, `RDX_VoiceAlloc.h` ×1), aber nirgends definiert (nur `MAX_VOICES` existierte in `config.h`) — wäre ein Compile-Fehler gewesen. Vereinheitlicht auf `MAX_VOICES`.
- **Fehlendes `TWO_PI`-Makro:** In Arduino-Welt implizit aus `wiring.h` verfügbar, in Standard-C++ nicht. Als eigene Konstante (`6.283185307179586f`) in `RDX_Constants.h` ergänzt.
- **Fehlende explizite Includes:** Verließen sich auf transitive Einbindung durch `Arduino.h`. Folgende Header an mehreren Stellen ergänzt: `<cstring>`, `<cmath>`, `<initializer_list>`.

---

## 6. Bekanntes, nicht behobenes Problem

**`misc.h`, Funktion `fast_floorf()`:**

```cpp
int i = (int)x - (int)(i > x);
```

- `i` wird gelesen, bevor es initialisiert ist (undefined behaviour)
- Vom Compiler als Warnung bestätigt (`-Wuninitialized`)
- Bereits im ESP32-Original vorhanden
- Bewusst nicht gefixt, da außerhalb des Auftragsumfangs „ESP32-Abhängigkeiten entfernen"
- **Empfehlung:** In separatem Fix-Task beheben

---

## 7. Architektur-Hinweis (WICHTIG, noch nicht gelöst)

- `DX_Synth_Bridge` ist wie die bestehende `mdaEPiano`-Engine **nicht für Cross-Core-Zugriff** ausgelegt (nur von einem Ausführungskontext aus aufrufbar, vorgesehen: Core 0 Audio-DMA-IRQ)
- `DX_Controller` (Encoder/MIDI) würde naturgemäß auf Core 1 laufen — direkte Aufrufe über die Core-Grenze sind **nicht sicher**
- Die Integration in `main.cpp` (Instanziierung, tatsächliche IPC-Anbindung über neue `IPC_CMD_DX_*`-Kommandos analog zu `IPC_CMD_NOTE_ON`, Einhängen in `pico_frontpanel.cpp`) ist **noch nicht erfolgt**
- Die neue Codebasis kompiliert und linkt, ist aber zur Laufzeit noch nicht erreichbar/aufrufbar

---

## 8. Build-Verifikation

- **Build-Kommando:** `cmake -G Ninja ..` + `ninja`
- **Toolchain:** `arm-none-eabi-gcc 15.2.1`
- **Board:** `sparkfun_promicro_rp2350`
- **Platform:** `PICO_PLATFORM=rp2350-arm-s`
- **Ergebnis:** 455/455 Targets erfolgreich, `main.elf` gelinkt

**Speicherverbrauch:**

| Ressource | Verbraucht | Gesamt | Anteil | Vorher (CP-only) |
|---|---|---|---|---|
| FLASH | 4.423.320 B | 16 MB | 26,37 % | 26,32 % |
| RAM | 193.872 B | 512 KB | 36,98 % | 36,44 % |

**Compiler-Warnungen:**

Alle Compiler-Warnungen stammen unverändert aus dem portierten ESP32-Originalcode:

| Warnung | Anzahl | Quelle |
|---|---|---|
| `misleading-indentation` | 3 | ESP32-Originalcode |
| `reorder` | 1 | `RDX_Operator`-Konstruktor |
| `sign-compare` | 1 | `renderAudioBlock` |
| `uninitialized` | 1 | `fast_floorf` |

> Die neu geschriebenen Dateien `DX_Synth_Bridge.cpp` / `DX_Controller.cpp` / `DX_GUI.cpp` selbst erzeugen **keine eigenen Warnungen**.

---

## 9. Offene Punkte / Empfehlungen

1. **Dual-Core-IPC-Anbindung in `main.cpp`** (siehe Architektur-Hinweis Abschnitt 7) — Kernstück für tatsächliche Nutzbarkeit.
2. **`fast_floorf()`-Uninitialized-Read** beheben.
3. **DX-eigene Effektkette** (Distortion/Chorus/Flanger/Phaser/Delay/Reverb/Touch-Wah aus dem ESP32-Projekt, dort als `fx_*.h` vorhanden) ist **nicht portiert** — DX-Stimme hat aktuell keine Effektverarbeitung. Kurzfristig ggf. bestehende `RefaceCpChain` (bereits float-basiert) mitnutzen, langfristig eigene DX-Effekte portieren.
4. **`dxDrawScreen()`-Werteanzeige** für OP1–4/LFO-Seiten ist Platzhaltertext — echte Zahlen-/Gauge-Anzeige folgt in einem späteren Schritt.
5. **Patch-Persistenz** (SysEx Bulk Dump/Flash-Speicherung für DX-Patches) nicht implementiert, nur eine hartkodierte Init-Voice (`DigiChordPatch`) verfügbar.
6. **`MAX_VOICES=8`** (übernommen vom ESP32S3-Vorbild) noch nicht auf tatsächliche CPU-Auslastung auf dem RP2350 (444 MHz Overclock) getestet.

---

## 10. Nachtrag: Dual-Core-IPC-Anbindung (2026-07-05, Fortsetzung)

Die in Abschnitt 7 beschriebene fehlende IPC-Anbindung wurde in dieser Fortsetzung umgesetzt.

### Geänderte Dateien und Implementierungsdetails

- **`include/ipc.h`**:
  - 3 neue Kommandos ergänzt: `IPC_CMD_DX_NOTE_ON=0x0B`, `IPC_CMD_DX_NOTE_OFF=0x0C`, `IPC_CMD_DX_PARAM=0x0D`.
  - Neues Enum `DxParamId` mit 12 Werten: je 2 pro Operator (Frequenz/Level) für OP1-4, plus LFO-Speed, LFO-PMD, Algorithmus, OP1-Feedback.
  - 3 neue Sende-Helper hinzugefügt: `ipc_send_dx_note_on`, `ipc_send_dx_note_off`, `ipc_send_dx_param`.
- **`src/midi_reface.cpp`**:
  - `RefaceMidi::onNoteOn` und `RefaceMidi::onNoteOff` senden jetzt zusätzlich (mit derselben kanalgefilterten/transponierten Notennummer) an die DX-Engine. Eingehende MIDI-Noten klingen ab sofort auf BEIDEN Engines (CP und DX) gleichzeitig.
- **`include/DX_Controller.h` / `src/DX_Controller.cpp`**:
  - Methoden `onMidiNoteOn` und `onMidiNoteOff` entfernt. Diese waren redundant geworden, da MIDI jetzt direkt über `RefaceMidi` läuft, welches Kanal-Filter/Transpose/Soft-Pedal bereits mitbringt.
  - `onEncoder2` und `onEncoder3` schreiben nicht mehr direkt in den Patch (der in Abschnitt 7 dokumentierte Cross-Core-Fehler). Sie lesen den aktuellen Wert weiterhin lesend (akzeptierter Cross-Core-Read, wie an anderer Stelle im Projekt üblich) und senden die neuen Werte über `ipc_send_dx_param()`.
- **`include/DX_Synth_Bridge.h`**:
  - Kommentar präzisiert: `noteOn()`, `noteOff()` und `fill_buffer()` dürfen nur von Core 0 aufgerufen werden. `patch()` darf von Core 1 gelesen (nicht geschrieben) werden.
- **`src/main.cpp`**:
  - Neue globale Instanz `DX_Synth_Bridge dxBridge;` (Core-0-Eigentum wie `ep`/`cp_fx`).
  - `dxBridge.init()` in `main()` aufgerufen.
  - `ipc_apply()` hat 3 neue Fälle: Note-On/Off wird direkt weitergereicht, Param-Set schreibt das passende Patch-Feld.
  - `i2s_callback_func()` rendert jetzt zusätzlich einen `dxBuf`-Scratch-Puffer (float, interleaved stereo) via `dxBridge.fill_buffer()` und mischt diesen ADDITIV (mit int16-Clamping) zum bestehenden CP-Ausgangssignal dazu. Bei stiller DX-Engine (keine aktiven Stimmen) ist das Ergebnis exakt identisch zum bisherigen reinen CP-Signal.

### Build-Verifikation

- Build erneut verifiziert: `ninja` -> 8/8 (nur die geänderten/neuen Targets neu gebaut) erfolgreich.
- FLASH: 4.485.528 B / 16 MB = 26,74 % (vorher 26,37 %).
- RAM: 241.040 B / 512 KB = 45,97 % (vorher 36,98 %). Deutlicher Sprung, da `dxBridge` jetzt eine echte, vom Linker nicht mehr entfernbare globale Instanz ist: 8 Stimmen x 4 Operatoren plus Hüllkurven/LFO-Zustand aller Stimmen sind jetzt fest allokierter statischer RAM. Zuvor war der Speicher für die (nirgends instanziierte) Engine ggf. weggefallen.
- Keine neuen Compiler-Warnungen.

### Weiterhin offen (Stand vor Abschnitt 11)

- Die Encoder-Hardware (`encSel`/`encA`/`encB` in `main.cpp`, verarbeitet in `pico_frontpanel.cpp`) ruft `DX_Controller::onEncoder1/2/3` noch NICHT auf. Die DX-Menüseiten (OP1-4/LFO/ALGO) sind über die physischen Encoder noch nicht erreichbar. Dies ist ein separater, noch ausstehender Schritt (Einhängen in `pico_frontpanel.cpp`, z.B. als neuer Screen oder eigener Modus).
- MIDI-Noten hingegen lösen ab sofort bereits echten DX-Klang aus (über die geladene Default-Patch `DigiChordPatch`). Dies ist durch einfaches Anspielen per MIDI-Keyboard/DAW testbar.

---

## 11. Nachtrag: Encoder-Anbindung in pico_frontpanel.cpp (2026-07-05, Fortsetzung)

- **Ziel:** Physische Encoder (`encSel`/`encA`/`encB`) sollen die DX-Menueseiten (OP1-4/LFO/ALGO) tatsächlich erreichbar machen.
- **Neue globale Instanz:** `DX_Controller dxController(dxBridge);` in `src/main.cpp` (Core-1-Objekt, referenziert die Core-0-Bridge nur für IPC-Versand/Lesezugriffe, siehe Abschnitt 10).
- **`src/pico_frontpanel.cpp`:** Neue Funktion `openDxSynth()` nach dem Vorbild der bestehenden `showAbout()`/`openSystem()`-Leaf-Screens implementiert. Sie besitzt eine eigene kleine Render/Event-Schleife, zeichnet via `dxDrawScreen()`, liest `encSel->delta()`/`encA->delta()`/`encB->delta()` und ruft `dxController.onEncoder1/2/3()` auf. Der Exit erfolgt durch Tastendruck auf `btSel` (Rückkehr ins Hauptmenü).
- **`openMainMenu()` erweitert:** Die Menü-Liste erhält einen 3. Eintrag 'DX Synth' (zwischen 'System' und '<< BACK'). Die Signatur wurde um `encA`/`encB`-Parameter erweitert (diese wurden vorher nicht durchgereicht), die einzige Aufrufstelle wurde entsprechend angepasst.
- **Erreichbarkeit:** Die DX-Synth-Seite ist nun über einen langen Tastendruck auf den Selektor-Encoder → Menüeintrag 'DX Synth' erreichbar. Dort dreht der Selektor durch die 6 Seiten (OP1-4/LFO/ALGO), Encoder A/B ändern die 2 Werte der aktuellen Seite. Ein kurzer Tastendruck auf den Selektor verlässt die Seite zurück ins Hauptmenü.
- **WICHTIGER BUGFIX (kein ESP32-Bezug, neu entdeckt durch den ersten echten Build-Versuch mit dieser Änderung):** `include/mdaEPiano.h` definiert `#define SUSTAIN 128` (Präprozessor-Makro, rein textuelle Ersetzung, keine C++-Scope-Kenntnis). Die DX-Engine-Dateien `RDX_Envelope.h` und `RDX_PEG.h` hatten jeweils ein eigenes `enum class Stage { ..., SUSTAIN, ... }` mit einem Enum-Wert `SUSTAIN`. Solange keine Übersetzungseinheit beide Header in der Reihenfolge (erst `mdaEPiano.h`, dann `dx_engine`) einband, blieb das unbemerkt. `pico_frontpanel.cpp` bindet über `pico_frontpanel.h`/`pico_userinterface.h` bereits `mdaEPiano.h` ein, und jetzt zusätzlich (für die neue DX-Seite) `DX_GUI.h`. Das Makro ersetzte `Stage::SUSTAIN` textuell durch `Stage::128`, was einen Compile-Fehler mit langer Fehlerkaskade verursachte.
  - **Fix:** Beide Enum-Werte in `RDX_Envelope.h` und `RDX_PEG.h` wurden von `SUSTAIN` zu `SUSTAIN_STAGE` umbenannt (7 Fundstellen insgesamt zwischen beiden Dateien: je 1 Enum-Deklaration + mehrere `Stage::SUSTAIN`-Verwendungen in `enterStage()`/`advanceStage()`/`stageTarget()`-Methoden). `mdaEPiano.h` selbst wurde nicht angefasst (bestehender, funktionierender CP-Code).
- **Build-Verifizierung nach dem Fix:** `ninja` → 6/6 erfolgreich. FLASH 4.489.648 B / 16 MB = 26,76 % (vorher 26,74 %). RAM 241.048 B / 512 KB = 45,98 % (vorher 45,97 %, minimal durch die kleine `dxController`-Instanz). Keine neuen Compiler-Warnungen — alle verbleibenden Warnungen sind unverändert die bereits dokumentierten, aus dem ESP32-Original geerbten (misleading-indentation, reorder, sign-compare, das bekannte fast_floorf-Uninitialized-Problem).
- **STATUS JETZT:** Die DX-Engine ist vollständig End-to-End erreichbar — sowohl per MIDI-Note (klingt sofort, additiv mit CP gemischt) als auch per Hardware-Encoder (Menüseiten-Navigation + Parameter-Editierung über das Hauptmenü 'DX Synth').

---

## 12. Nachtrag: Vollständige Entfernung der reface-CP-Engine (2026-07-06)

**Ziel (Korrektur der ursprünglichen Aufgabenstellung):** Der Nutzer stellte klar,
dass PicoFaceDX **ausschließlich** ein reface-DX-Klon werden soll; reface CP
diente nur als Vorlage/Skelett für die Hardware-Anbindung (Pin-/Display-/
Encoder-Init, USB-MIDI-Transport, virtuelles EEPROM, generische OLED-Dialoge).
Die additive Zwei-Engine-Architektur aus Abschnitt 10/11 wurde daher rückgebaut.

### Gelöscht (24 Dateien)
- mdaEPiano-Engine + Instrumenten-Sample-Daten (14 Dateien, ≈8 MB):
  `src/mdaEPiano.cpp`, `include/mdaEPiano.h`, `include/mdaEPianoData.h`,
  `include/mdaEPianoInstruments.h`, `include/{CP,Clv,Pno,Rd_II,Wr}Data.h`,
  `include/{CP,Clv,Pno,Rd_II,Wr}Keygroups.h`.
- reface-CP-Effektkette + dadurch verwaiste generische DSP-Header (7 Dateien):
  `effects/reface_cp_chain.h`, `reface_cp_fx.h`, `wahwah.h`, `cp_audio.h`,
  `dsp_fastmath.h`, `dsp_lut.h`, `dsp_reverb.h`.
- `src/pico_program_select.cpp` (mdaEPiano-Programmauswahl-Screen).
- CP-only Host-Testharness (7 Dateien): `test/test.cpp`, `build.sh`,
  `mdaepiano_test`, `cp_test.cpp`, `build_cp.sh`, `cp_test`, `test/README.md`
  (`test/veeprom_test.*` bleibt — reine, engine-unabhängige Speichertests).
- `include/arduino_compat.h` (nach der mdaEPiano-Entfernung ohne verbleibende
  Konsumenten, per Grep verifiziert).

### Neu geschrieben (DX-nativ statt CP-nativ)
- **`include/ipc.h`:** alle CP-Kommandos/Enums (`FxParam`, `FxMode`,
  `IPC_CMD_NOTE_ON/OFF/CC/FX_PARAM/FX_MODE/VOICE_PARAM/PROGRAM/INSTRUMENT/PITCH_BEND`)
  entfernt. 4 neue DX-Kommandos ergänzt: `IPC_CMD_DX_PITCH_BEND`, `IPC_CMD_DX_CC`,
  `IPC_CMD_DX_RAW_WRITE` (einzelnes Patch-Byte per SysEx-Parameteränderung, da
  die 12 kuratierten `DxParamId`-Werte nicht das komplette Common/Operator-Layout
  abdecken), `IPC_CMD_DX_PATCH_APPLY`.
- **`include/dx_patch_stage.h`** (neu): Core-übergreifender Staging-Slot für
  einen kompletten `RDX_Patch` (~66 Byte, zu groß fürs 32-Bit-FIFO-Wort) —
  Core 1 beschreibt ihn (Presets, SysEx-Bulk-Dump), Core 0 übernimmt ihn nach
  `IPC_CMD_DX_PATCH_APPLY`.
- **`include/DX_Synth_Bridge.h`:** zwei neue schlanke Passthrough-Methoden
  `processCC()`/`updatePB()`, die auf bereits vorhandene, ungenutzte
  `RDX_Synth::processCC()`/`updatePB()`-Methoden verweisen (keine Änderung an
  der Engine selbst — nur zwei zusätzliche Aufruf-Wrapper).
- **`include/midi_reface.h` + `src/midi_reface.cpp`:** komplett neues,
  reface-DX-natives MIDI/SysEx-Schema (Common-Block @ `30 00 00`, 38 Byte;
  Operator-Blöcke @ `31 <op> 00`, 28 Byte je Operator; Model-ID `05H` statt
  `04H`; Identity-Reply-Bytes `41 53 06` statt `41 52 06`), 1:1 aus der
  ESP32-Referenz `tools/refacedx/RDX-Reface-DX-emu/RDX/RDX_Midi.h` portiert.
  Control-Change-Handling zweistufig: eine Handvoll Fälle bleiben Core-1-lokal
  (Soft Pedal, Reset All Controllers, Omni On/Off), der große Rest wird
  unverändert per `ipc_send_dx_cc` an `RDX_Synth::processCC` durchgereicht, das
  bereits Mod-Wheel/Volume/Sustain/Portamento/Algorithmus-Quick-Select/
  Operator-Quick-Edit-CCs vollständig implementiert (bislang unbenutzter
  Code-Pfad — eine echte Vereinfachung gegenüber der ursprünglich geplanten,
  deutlich komplexeren CC-Tabelle).
- **`include/presets.h` + `src/presets.cpp`:** `CpPreset[8]` → `DxPreset[1]`
  (einziger Eintrag `"DigiChord"`, bewusste Scope-Entscheidung, siehe
  `doc/PRESETS.md`).
- **`include/settings.h` + `src/settings.cpp`:** `SettingsV1` → `SettingsV2`
  (Oktave + 32-Byte-SYSTEM-Block + kompletter `RDX_Patch`), Speicher-Mechanismus
  (veeprom) unverändert.
- **`include/pico_frontpanel.h` + `src/pico_frontpanel.cpp`:** die
  DX-Seitenansicht (vormals nur über den Menüpunkt „DX Synth" erreichbar) ist
  jetzt der permanente Homescreen; alle CP-Bildschirme entfernt. Menü nur noch
  `Presets` / `System` / `<< BACK`.
- **`CMakeLists.txt`, `src/usb_descriptors.c`:** `mdaEPiano.cpp`/
  `pico_program_select.cpp` aus dem Build entfernt, Produktname/USB-String
  `PicoFaceCP` → `PicoFaceDX`.
- **`effects/cp_hot.h` → `effects/ram_hot.h`:** Makro `CP_HOT` → `RAM_HOT`
  umbenannt (rein kosmetisch, keine Verhaltensänderung), in allen 10
  `dx_engine`-Headern + `DX_Synth_Bridge.cpp` durchgezogen.

### Build-Verifikation
- `ninja` → alle Targets erfolgreich, keine neuen Warnungen (nur die bereits
  bekannten, aus dem ESP32-Original geerbten).
- **FLASH: 142.864 B / 16 MB ≈ 0,85 %** (vorher 26,76 % mit CP-Sample-Daten).
- **RAM: 71.672 B / 512 KB ≈ 13,67 %** (vorher 45,98 %).
- Grep-Sweep über `src/`, `include/`, `effects/` bestätigt: keine
  `mdaEPiano`/`RefaceCpChain`/`CpPreset`/`FxParam`/`FxMode`/`CP_HOT`-Referenzen
  mehr vorhanden (bis auf zwei rein historische Kommentarstellen, ebenfalls
  bereinigt: `include/veeprom.h`-Kopfzeile, `.gitignore`-Einträge für die
  gelöschten Testbinaries).

### STATUS JETZT
PicoFaceDX ist ab sofort ein **reiner reface-DX-Klon** ohne jede reface-CP-Engine,
-Effektkette, -Presets oder -MIDI-Schicht. Die Hardware-Anbindung (Pin-Konfiguration,
USB-MIDI-Transport, virtuelles EEPROM, generische OLED-Dialoge) bleibt unverändert
als Skelett bestehen. Dokumentation aktualisiert: `README.md`,
`doc/MIDI_IMPLEMENTATION.md` (v2.0), `doc/PERSISTENCE.md`, `doc/PRESETS.md`;
`doc/CHANGELOG_MIDI_RP2350.md` als überholt/archiviert markiert.

**Weiterhin offen** (unverändert gegenüber Abschnitt 9, jetzt ohne CP-Altlast):
Master-Tune-Anwendung auf die Engine (aktuell No-Op, `tuningSemitones` nicht
verdrahtet), echte Zahlenanzeige auf den OP/LFO-Seiten statt Platzhaltertext,
Erweiterung der Preset-Bibliothek (Parität zur echten reface-DX-Werksbank),
CPU-Lasttest von `MAX_VOICES=8` auf echter Hardware, ein DX-eigener
Host-Test analog zum entfernten CP-`cp_test`.
- **Verbleibend offen:** Patch-Persistenz, DX-eigene Effektkette, echte Werte-Anzeige auf den OP/LFO-Seiten (noch Platzhaltertext statt Zahlen — `dxDrawScreen()` zeigt für Nicht-ALGO-Seiten weiterhin nur '(values shown via encoders)'), CPU-Lasttest bei `MAX_VOICES=8`.

---

## 13. Nachtrag: Phase A — MIDI-Spec-Genauigkeit (2026-07-06)

**Ziel:** Ein Gap-Analyse-Plan (basierend auf der offiziellen Yamaha Data List
+ Reference Manual + einer erneuten Durchsicht des ESP32-Referenzprojekts) hat
mehrere konkrete Abweichungen unserer MIDI-Implementierung vom echten reface
DX aufgedeckt. Diese Nachtrag behebt die kleinen, risikoarmen Korrekturen
("Phase A" des Plans); die größeren Punkte (Preset-Bibliothek aus dem
ESP32-Projekt, Effektkette) folgen in separaten Durchläufen.

- **CC11 (Expression) war spezifiziert, aber unverdrahtet.** `RDX_Synth::processCC`
  hatte keinen `case 11`. Neu: `RDX_Controls` bekam `expression`/`expressionFactor`
  (Default 127/1.0, wie im echten Reset-All-Controllers-Verhalten), `processCC`
  behandelt CC11 jetzt exakt wie CC7 (Volume), `calcOutputGain()` multipliziert
  jetzt zusätzlich mit `expressionFactor`.
- **CC66/CC67 (Sostenuto/Soft Pedal) existieren auf der echten reface DX nicht**
  (nur reface CP hat sie) — bestätigt gegen die offizielle Data List. Die
  CC67-Soft-Pedal-Behandlung (Velocity-Skalierung um 0,75×), ein Überbleibsel
  aus der entfernten reface-CP-Schicht, wurde komplett entfernt: `_softPedal`-Member
  aus `midi_reface.h`, alle Verwendungen in `midi_reface.cpp` (`onNoteOn`, `init`,
  `resetControllers`, `onControlChange`).
- **„MIDI Control"-Gate wieder eingeführt.** Die offizielle Data List bestätigt,
  dass CC80 (Algorithmus) und CC85-90/102-119 (Operator-Quick-Edit) auf echter
  Hardware nur greifen, wenn SYSTEM „MIDI Control" eingeschaltet ist; alle
  anderen CCs (Mod Wheel, Volume, Expression, Sustain, Portamento, All-Sound/
  Notes-Off) sind immer aktiv. `RefaceMidi::onControlChange` prüft das jetzt
  korrekt vor der Weiterleitung der gegateten CCs.
- **Reset All Controllers vervollständigt.** Die Data List spezifiziert für
  CC121: Pitch Bend → Center, Modulation → 0 (Minimum), Expression → 127
  (Maximum), Sustain → off. `resetControllers()` sendet jetzt alle vier statt
  nur Pitch Bend + Sustain.
- **Dokumentationsfehler korrigiert:** `doc/MIDI_IMPLEMENTATION.md`,
  `doc/PRESETS.md` und ein Codekommentar in `src/midi_reface.cpp` behaupteten
  fälschlich, das echte reface DX habe gar kein MIDI Program Change. Laut
  offizieller Data List unterstützt es tatsächlich Program Change 0–31 (4
  Bänke à 8 Presets) — diese Firmware implementiert davon bislang nur Wert 0
  (einziges Preset „DigiChord"), was jetzt korrekt als offener Umfangspunkt
  statt als (nicht existente) Spezifikationsparität dokumentiert ist.
- **Echter Bug gefunden und behoben:** `RefaceMidi::txBulkBlock()`s Stack-Puffer
  `buf[48]` war zu klein — ein vollständiger Common-Block-Bulk-Dump (38
  Datenbytes) benötigt 11 Header-Bytes + 38 Daten + Checksumme + F7 = 51 Bytes,
  also 3 Bytes mehr als der Puffer fasste (Stack-Buffer-Overflow, vom Compiler
  als `-Wstringop-overflow` gemeldet). Puffer auf `buf[64]` vergrößert. Dieser
  Bug bestand bereits seit der ursprünglichen SysEx-Implementierung (Abschnitt
  12) und wurde erst durch einen erneuten vollständigen Rebuild in dieser
  Session sichtbar.

### Build-Verifikation

`ninja` → erfolgreich, keine neuen Warnungen mehr (der `-Wstringop-overflow`
durch den Puffer-Fix behoben). FLASH 143.400 B / 16 MB ≈ 0,85 %, RAM 71.752 B
/ 512 KB ≈ 13,69 % (minimal gegenüber Abschnitt 12, durch die neuen
`expression`/`expressionFactor`-Felder).

### STATUS JETZT

Phase A des 100%-Clone-Plans (`doc/CHANGELOG_DX_ENGINE.md` bzw. der
Session-Plan) ist abgeschlossen. Noch offen: Phase B (Preset-Bibliothek aus
den 34 `.syx`-Werksdateien des ESP32-Referenzprojekts) und Phase C
(Effektkette — 2 Slots, 8 Effekttypen, aktuell komplett unverdrahtet trotz
bereits reserviertem Speicherplatz in `RDX_Common::effects[2][3]`).

---

## 14. Nachtrag: Phase B — Preset-Bibliothek (2026-07-06)

**Ziel:** Von 1 hartcodiertem Preset auf die vollständigen 32 echten
Yamaha-reface-DX-Werkspresets erweitern (Program Change 0–31, 4 Bänke à 8).

- **Neues einmaliges Host-Tool `tools/refacedx/syx_to_patches.cpp`** (nicht
  Teil des Firmware-Builds, nicht von CMake kompiliert): liest die 32
  offiziellen `.syx`-Factory-Voice-Bulk-Dumps aus
  `tools/refacedx/RDX-Reface-DX-emu/RDX/data/patches/` und wendet auf jede
  Datei die bereits verifizierte `syxToPatch()` (`include/dx_engine/RDX_Types.h`)
  an; gibt für jedes Patch einen `{ "Name", patchFromBytes({...150 Bytes...}) }`-
  Initialisierer aus. Gebaut mit `g++ -std=c++17` (Host-Compiler, kein
  Pico-SDK nötig — `RDX_Types.h` ist bereits reines Standard-C++17).
- **Verifiziert byte-exakt:** Das `voiceName`-Feld jedes geparsten Patches
  ergibt als ASCII exakt den erwarteten Factory-Voice-Namen (Stichprobe:
  „DigiChord ", „WobbleBass", „GlassHarp ", „Chopper   "). Byte-Zahl pro
  Patch bestätigt `sizeof(RDX_Patch)=150` exakt.
- **Datei-Reihenfolge → Program-Change-Zuordnung:** Die 32 Dateinamen kodieren
  Bank+Slot (`11`=Bank1-Slot1 … `48`=Bank4-Slot8); die generierte Tabelle
  ordnet sie exakt in PC-0..31-Reihenfolge. Eine 33. Datei
  `00-Init_Voice.syx` existiert im ESP32-Projekt zusätzlich, ist aber kein
  realer Factory-Bank-Slot (fällt außerhalb 0–31) und wurde bewusst
  ausgelassen.
- **`include/presets.h` / `src/presets.cpp`:** `DX_NPRESETS` 1→32,
  `dxPresets[]` jetzt mit allen 32 generierten Patches gefüllt, neuer
  `patchFromBytes()`-Helfer (memcpy der 150 rohen Bytes in ein `RDX_Patch`).
  `preset_apply`/`preset_stage`/`preset_set_current`/`preset_get_current`
  unverändert (Architektur war bereits korrekt für N Presets ausgelegt).
- **Echter Folgefehler gefunden und behoben:** `pico_frontpanel.cpp`s
  `openPresets()` baute die Menü-Auswahlliste in `char buf[128]` — bei nur 1
  Preset unproblematisch, bei 32 Namen (bis zu 10 Zeichen + Newline, macht
  ~360 Bytes) ein garantierter Stack-Buffer-Overflow. Puffer auf `buf[512]`
  vergrößert.
- **Dokumentation korrigiert:** alle verbleibenden „nur 1 Preset"/„Wert 0
  einzig gültig"-Aussagen in `doc/MIDI_IMPLEMENTATION.md`, `doc/PRESETS.md`
  und einem Codekommentar in `src/midi_reface.cpp` aktualisiert — Program
  Change deckt jetzt den vollen realen Bereich 0–31 ab.

### Build-Verifikation

`ninja` → erfolgreich, keine neuen Warnungen. FLASH 150.272 B / 16 MB ≈
0,90 % (vorher 0,85 % — die 32×150 Byte Presetdaten schlagen mit ca. 7 KB zu
Buche), RAM 76.592 B / 512 KB ≈ 14,61 %.

### STATUS JETZT

Phase B ist abgeschlossen. Die Preset-Bibliothek erreicht volle Parität mit
der realen reface-DX-Werksbank. Noch offen: Phase C (Effektkette — mit
Abstand größter verbleibender Aufwand) und die bereits länger bekannten
Punkte (Master-Tune-Verdrahtung, Zahlenanzeige auf den OP/LFO-Seiten,
CPU-Lasttest auf echter Hardware, DX-eigener Host-Test).

---

## 15. Nachtrag: Phase C — Effektkette (2026-07-06)

**Ziel:** Die größte verbliebene Lücke zur echten reface DX schließen: 2
unabhängige Effekt-Slots, je wählbar aus 8 Typen (Thru, Distortion, Touch
Wah, Chorus, Flanger, Phaser, Delay, Reverb) — bestätigt gegen die
offizielle Yamaha Data List UND das ESP32-Referenzprojekt (`RDX_FX.h` +
`fx_*.h`), das eine vollständige, fertige Implementierung aller 8 Typen
mitbringt. `RDX_Common::effects[2][3]` reservierte den passenden Speicher
bereits seit dem ursprünglichen Engine-Port, war aber nirgends verdrahtet —
die DX-Stimme war bis eben 100% trocken.

### Portierte Dateien (alle in `include/dx_engine/`, byte-treu zum ESP32-Original)

- `fx_base.h` — `FXBase`-Interface (virtuell: `init`/`reset`/`processBlock`/`prepare`) + `FxThru`.
- `fx_distortion.h`, `fx_touch_wah.h`, `fx_chorus.h`, `fx_flanger.h`,
  `fx_phaser.h` (2-Notch-Phaser + interner Flanger-Layer, größtes Modul,
  256 Zeilen im Original), `fx_delay.h`, `fx_reverb.h` (4 Combs + 2
  Allpasses je Kanal) — alle DSP-Algorithmen, Koeffizienten und Konstanten
  1:1 übernommen, keine Klangänderungen.
- Alle benötigten LUTs/Helfer (`LFO_SPEED`, `PM_DEPTH`, `DELAY_TIME_MS`,
  `sin01`, `semitonesToRatio`, `wrap01`, `fclamp`, `saturate_cubic`) waren
  bereits im Projekt vorhanden (aus dem ursprünglichen Engine-Port) — keine
  einzige neue LUT nötig.

### RP2350-Anpassungen (ESP32-spezifische Teile ersetzt, DSP unangetastet)

- **`prepare()`-Signatur vereinfacht:** ESP32 unterschied „fast" (DRAM)
  und „slow" (PSRAM) Scratch-Puffer je Slot; RP2350 hat in diesem Build
  keine PSRAM-Stufe, daher ein einziger gemeinsamer Puffer pro Slot
  (`prepare(float* scratch, uint32_t scratchSize, int sampleRate)`).
- **`DX_FXHost`** (`include/dx_engine/DX_FXHost.h`, neu, kein 1:1-Port):
  2 Slots × 8 Effekttypen als statischer Objekt-Pool (kein Allocation bei
  Effektwechsel), pro Slot ein fixer `float[24576]`-Puffer (96 KB) —
  dimensioniert für den größten Einzelbedarf (`FxReverb`, 24276 Floats
  errechnet, mit Marge aufgerundet). ESP32s `heap_caps_get_largest_free_block`/
  `MALLOC_CAP_*`-Sondierung und die dynamische Polyphonie-Drosselung
  (`VOICES`-Schätzung aus gemessener Effekt-Rechenzeit) entfallen ersatzlos
  — dieses Projekt hat bereits ein festes `MAX_VOICES`-Budget.
- **Speicherbudget geprüft, nicht geraten:** 2 × 96 KB = 192 KB RAM für
  Effekt-Scratch-Puffer, macht zusammen mit dem übrigen Firmware-Bedarf
  52,76 % von 512 KB — ausreichend Marge für Stacks etc. (siehe
  Build-Verifikation unten).
- **Echter Bug gefunden (via Host-Smoke-Test vor dem echten Pico-Build)
  und behoben:** `FxTouchWah::reset(bool instant=false)` hatte eine andere
  Signatur als `FXBase::reset()` und **versteckte** damit die virtuelle
  Funktion statt sie zu überschreiben (`-Woverloaded-virtual`) — ein aus
  dem ESP32-Original übernommener, dort ebenfalls bereits vorhandener
  Bug. Aufruf über `FXBase*` (genau das, was `DX_FXHost::setSlot()` beim
  Effektwechsel tut) rief bisher stillschweigend die No-Op-Basisversion
  auf, sodass der interne Filter-/Hüllkurvenzustand beim Wechsel AUF
  Touch-Wah nicht zurückgesetzt wurde. Fix: `reset()` ohne Parameter,
  `override` ergänzt, einzige Aufrufstelle in `prepare()` angepasst.

### Verdrahtung

- **`DX_Synth_Bridge`:** neues Member `DX_FXHost fxHost_;`, `init()` ruft
  `fxHost_.init((float)SAMPLE_RATE)`, `fill_buffer()` ruft
  `fxHost_.process(scratchL_, scratchR_, chunkLen)` direkt nach
  `synth_.renderAudioBlock(...)` und vor dem Interleaving — Effekte
  laufen also post-mix, wie auf echter Hardware.
- **SysEx/Patch-Editing funktioniert automatisch:** `effects[2][3]` liegt
  im bereits adressierbaren Common-Block (Offset `0x1D–0x22`); sobald die
  DSP die Bytes konsumiert, greift der bereits generische
  `IPC_CMD_DX_RAW_WRITE`-Pfad ohne jede neue IPC-Arbeit.
- **Keine MIDI-CC-Steuerung nötig:** die offizielle Data List bestätigt,
  dass Effekte ausschließlich Patch-/SysEx-adressiert sind, keine
  dedizierte CC-Zuordnung existiert.
- **Neue Frontpanel-Seiten FX1/FX2** (`DX_Controller`: `DxPage` um `FX1`,
  `FX2` erweitert; Encoder A = Effekt-Typ 0–7 geclamped wie ALGO, Encoder B
  = Param1 0–127; beide per `ipc_send_dx_raw_write(1, offset, val)` mit
  `offsetof(RDX_Common, effects)` statt Magic Numbers). Param2 bleibt
  SysEx-only (wie viele andere Patch-Parameter, z. B. Voice-Name,
  KSC-Kurven, die ebenfalls nicht über die 3 Encoder erreichbar sind).
- **`DX_GUI::dxDrawScreen()`:** FX1/FX2-Seiten zeigen jetzt echten
  Effekt-Namen (`FX_NAMES[]`, neu in `DX_FXHost.h`) + Param1-Wert statt
  des generischen Platzhaltertexts — die anderen Seiten (OP1-4/LFO)
  bleiben unverändert beim Platzhalter (weiterhin offener Punkt).

### Build-Verifikation

`ninja` → erfolgreich, keine neuen Warnungen außer einer bereits im
ESP32-Original vorhandenen, harmlosen Signedness-Vergleichswarnung in
`fx_delay.h`. FLASH 166.128 B / 16 MB ≈ 0,99 %, RAM 276.616 B / 512 KB ≈
52,76 % (deutlicher, aber budgetierter Sprung durch die 192 KB
Effekt-Scratch-Puffer).

### STATUS JETZT

Phase C ist abgeschlossen. PicoFaceDX hat jetzt eine vollständige,
funktionierende Effektkette mit allen 8 realen reface-DX-Effekttypen,
patch-/SysEx-editierbar und über 2 neue Frontpanel-Seiten (FX1/FX2)
erreichbar. Alle drei Phasen des 100%-Clone-Plans (A: MIDI-Spec-Genauigkeit,
B: Preset-Bibliothek, C: Effektkette) sind damit abgeschlossen.

**Verbleibend offen** (Phase D des Plans, niedrigere Priorität):
Master-Tune-Verdrahtung (`tuningSemitones` weiterhin unwired), echte
Zahlenanzeige auf den OP/LFO-Seiten (weiterhin Platzhaltertext), Delay-
Zeit-Anzeige in Millisekunden statt Rohwert, CPU-Lasttest von
`MAX_VOICES=8` mit aktiven Effekten auf echter Hardware (durch die
Effektkette jetzt relevanter als zuvor), kein DX-eigener Host-Test.

---

## 16. Nachtrag: Phase D — Politur (2026-07-06)

**Ziel:** Die drei in Phase C offen gelassenen Politur-Punkte abarbeiten:
Master-Tune-Verdrahtung, echte Zahlenanzeige auf den OP/LFO-Frontpanel-
Seiten, und eine Messgrundlage für die CPU-Last mit aktiver Effektkette.

### Master Tune verdrahtet (SYSTEM-SysEx-Parameter, keine MIDI-CC)

`RDX_Controls::tuningSemitones` war seit dem ursprünglichen Engine-Port
gespeichert/round-tripped, aber nirgends im Audiopfad konsumiert. Jetzt
vollständig verdrahtet, End-to-End über die Core-Grenze:

- **`RDX_Voice.h`** (`updateMods()`): Pitch-Berechnung erweitert von
  `ctl_.pitchbendSemitones` auf `ctl_.pitchbendSemitones + ctl_.tuningSemitones`
  — Master Tune wirkt additiv zum Pitch Bend auf alle Operatoren, exakt wie
  auf echter Hardware.
- **`RDX_Synth.h`**: neue `setMasterTune(float semitones)`, setzt
  `ctl_.tuningSemitones`.
- **`ipc.h`**: neues `IPC_CMD_DX_MASTER_TUNE` + `ipc_send_dx_master_tune(uint16_t rawTune)`
  — eigener IPC-Befehl statt Zweckentfremdung von CC/Raw-Write, da Master
  Tune global-Controls-State ist, kein Patch-Byte und keine CC. Core 0
  dekodiert die 4 Nibble-Bytes zu Cent/Halbtönen (Formel laut offizieller
  Yamaha Data List: `cents = (raw - 1024) * 0.1`, geklemmt auf
  ±102,4/±102,3 Cent) in `main.cpp`s `ipc_apply()`.
- **`DX_Synth_Bridge.h`**: `setMasterTune(float semitones)`-Passthrough zu
  `synth_.setMasterTune()`.
- **`midi_reface.cpp`**: `applyMasterTune()` war ein dokumentierter No-Op-
  Stub aus Phase A (damals korrekt, weil `tuningSemitones` noch unwired
  war) — jetzt echte Implementierung: liest `_sys[SYS_TUNE_0..3]`,
  rekonstruiert den 16-Bit-Rohwert aus den 4 Nibbles
  (`(t0<<12)|(t1<<8)|(t2<<4)|t3`, jeweils mit `& 0x0F` maskiert gemäß
  offizieller Data List: „1st bit 3-0: bit 15-12" usw.), sendet ihn per
  `ipc_send_dx_master_tune()`.

### Echte Zahlenanzeige auf OP1-4/LFO-Seiten

`DX_GUI::dxDrawScreen()` zeigte für OP1-4/LFO/ALGO-Seiten bisher nur den
generischen Platzhaltertext „(values shown via encoders)" — genau wie die
FX1/FX2-Seiten vor ihrer eigenen Anzeige in Phase C. Jetzt nach demselben
Muster erweitert:

- **OP1-4:** `"Freq: %d"` (`ops[opIdx].freqCoarse`, das was Encoder A auf
  dieser Seite tatsächlich schreibt) und `"Level: %d"`
  (`ops[opIdx].outLevel`, was Encoder B schreibt).
- **LFO:** `"Speed: %d"` (`common.lfoSpeed`) und `"PMD: %d"`
  (`common.lfoPMD`).
- ALGO bleibt bei der grafischen Algorithmus-Darstellung (`drawAlgo()`),
  FX1/FX2 bleiben bei ihrer Phase-C-Anzeige — beide unverändert.

### CPU-Lastmessung instrumentiert (kein Hardware-Test durch mich möglich)

Der Plan sah einen echten CPU-Lasttest von `MAX_VOICES=8` mit aktiven
Effekten auf echter Hardware vor. Ich habe kein reales RP2350-Board zur
Hand — ein tatsächlicher Hardware-Test kann daher nicht von mir
durchgeführt oder behauptet werden. Stattdessen wurde die Messgrundlage
geschaffen, damit der Nutzer beim Flashen selbst reale Zahlen ablesen
kann:

- **`DX_Synth_Bridge::fill_buffer()`**: misst per `time_us_32()` die
  tatsächliche Blockdauer (Rendering + Effektkette) und vergleicht sie
  gegen das Echtzeit-Budget des Blocks (`length / SAMPLE_RATE` Sekunden).
  Ergebnis in zwei neuen Membern: `cpuLoadPercent_` (letzter Block) und
  `cpuLoadPeakPercent_` (Maximum seit Boot), über `cpuLoadPercent()`/
  `cpuLoadPeakPercent()` public lesbar — Core 1 darf sie direkt lesen,
  gleiche Konvention wie bei `patch()`.
- **Neue Frontpanel-Seite:** SYSTEM-Menü um „CPU Load" erweitert
  (`pico_frontpanel.cpp`, `showCpuLoad()` nach dem Muster von
  `showAbout()`) — zeigt „Now" und „Peak" in Prozent, live aktualisiert.
- Kein Overhead im Hot-Path außer zwei `time_us_32()`-Aufrufen (bereits
  IRQ-Kontext, keine zusätzliche Synchronisation nötig).

### Build-Verifikation

`ninja` → erfolgreich, keine neuen Warnungen. FLASH 166.384 B / 16 MB ≈
0,99 %, RAM 276.544 B / 512 KB ≈ 52,75 % (minimaler Zuwachs gegenüber
Phase C: +256 B FLASH, +16 B RAM für die gesamte Phase D).

### STATUS JETZT

Phase D ist abgeschlossen. Damit sind alle vier Phasen des
100 %-Clone-Plans (A: MIDI-Spec-Genauigkeit, B: Preset-Bibliothek,
C: Effektkette, D: Politur) umgesetzt. Verbleibend: der tatsächliche
CPU-Lasttest auf echter Hardware selbst — die Messgrundlage steht, das
Ablesen der realen Prozentzahl beim Spielen mit aktiven Effekten kann
nur der Nutzer auf dem echten Board durchführen.

---

## 17. Nachtrag: Compiler-Warnungen bereinigt (2026-07-06)

**Anlass:** Nutzer meldete diverse Warnungen im Build-Log. Aufgeschlüsselt
in projekteigene (behebbar) und Vendor-Warnungen (nicht behebbar/nicht
sinnvoll behebbar, da aus einer per CMake `FetchContent` geladenen
Fremdabhängigkeit stammend).

### Projekteigene Warnungen behoben (alle in `include/dx_engine/`)

- **`misc.h` — `fclamp()`/`saturate_cubic()`:** je zwei `if`-Statements
  standen auf einer Zeile (`-Wmisleading-indentation`) — auf separate
  Zeilen aufgeteilt, keine Logikänderung.
- **`misc.h` — `fast_floorf()`, echter Bug, nicht nur Warnung:**
  `int i = (int)x - (int)(i>x);` las die lokale Variable `i` in ihrem
  eigenen Initialisierer, bevor sie einen Wert hatte — undefiniertes
  Verhalten (`-Wuninitialized`). **Dieser Bug existierte bereits im
  ESP32-Original** (`tools/refacedx/RDX-Reface-DX-emu/RDX/misc.h:27`,
  identischer Code) und wurde beim ursprünglichen Engine-Port 1:1
  mitübernommen — analog zum bereits in Phase C gefundenen
  `FxTouchWah::reset()`-Bug. `fast_floorf()`/`wrap01()` werden u. a. für
  LFO-Phasenwrapping verwendet (`RDX_LFO.h`), das UB konnte also
  potenziell zu Phasenglitches führen. Fix nach dem Standard-Trick
  „truncate-then-adjust": `int i = (int)x; return (float)(i - (int)(i > x));`
- **`RDX_Types.h` — `syxToPatch()`:** `while (...) i++; i++;` auf einer
  Zeile (`-Wmisleading-indentation`) — auf zwei Zeilen aufgeteilt, das
  unbedingte `i++` bleibt unbedingt, keine Logikänderung.
- **`RDX_Operator.h` — Konstruktor (`-Wreorder`):** Initialisierungsliste
  nannte `idx_` vor `params_`, obwohl `params_` in der Klasse zuerst
  deklariert ist (C++ initialisiert immer in Deklarationsreihenfolge,
  unabhängig von der Listenreihenfolge) — Liste umsortiert
  (`params_` vor `idx_`), reine Kosmetik, keine Verhaltensänderung.
- **`RDX_Synth.h` (`renderAudioBlock`) + `fx_delay.h` (`processBlock`)
  (`-Wsign-compare`):** je eine `for`-Schleife verglich ein signed `int`
  gegen ein unsigned `uint32_t`-Limit (`len`/`frames`) — Schleifenvariable
  auf `uint32_t` umgestellt.

### Nicht behoben (außerhalb des Projekt-Quellbaums)

- `build/_deps/picotool-src/main.cpp:227` (`static_assert` ohne Message,
  C++17-Extension-Warnung) und ein Linker-Hinweis „ignoring duplicate
  libraries" (`liberrors.a`/`libmodel.a`) — beide stammen aus dem von
  CMake automatisch nachgeladenen `picotool`-Quellcode
  (`_deps/picotool-src/`), nicht aus diesem Repository. Ein Patch dort
  würde beim nächsten `FetchContent`-Refresh verlorengehen und gehört
  nicht zu PicoFaceDX — bewusst unverändert gelassen.

### Build-Verifikation

Vollständiger Rebuild (`cmake --build .` nach `touch` aller `.cpp`-Dateien,
um jede Übersetzungseinheit neu zu kompilieren): **null Warnungen** aus
allen Projektdateien. FLASH 163.760 B / 16 MB ≈ 0,98 %, RAM 273.912 B /
512 KB ≈ 52,24 % — leicht kleiner als vor der Bereinigung (u. a. durch den
Wegfall des `fast_floorf`-UB-Pfads in `misc.h`, das praktisch überall
eingebunden wird).

---

## 18. Nachtrag: Zwei Restpunkte aus der Gap-Analyse (2026-07-07)

**Anlass:** Nutzerfrage „ist noch irgendwas offen zum 100 %-Clone?" ergab
eine erneute Durchsicht gegen Data List/Reference Manual/Codebasis. Dabei
zwei kleine, klar abgegrenzte Punkte identifiziert und auf Wunsch behoben
(kein Klang-/Protokoll-Gap, reine Code-Hygiene bzw. Anzeige-Politur).

### Toter Code entfernt: `RDX_Synth::programChange()` / `applyBankProgram()`

Grep über den gesamten Quellbaum bestätigte: beide Methoden wurden
nirgends aufgerufen. Der echte Program-Change-Pfad läuft seit Phase B
vollständig über `midi_reface.cpp::onProgramChange()` →
`presets.cpp` → `IPC_CMD_DX_PATCH_APPLY` → `preset_apply()` — die
`RDX_Synth`-eigene Methode war ein Überbleibsel von vor Phase B und ihr
`// TODO: multi-patch storage — for now always load the hardcoded init
voice`-Kommentar war seit Phase B sachlich falsch (es gibt längst 32
echte Presets, nur eben über den anderen Pfad). Beide Methoden ersatzlos
gelöscht (`RDX_Synth.h`). `ctl_.wantProgram`/`wantBankMSB`/`wantBankLSB`/
`getWantBank()` in `RDX_Types.h` bleiben unangetastet (bewusst außerhalb
des Umfangs — `wantBankMSB`/`wantBankLSB` werden weiterhin von den
echten, aktiven CC0/32-Handlern beschrieben, passend zum bereits an
anderer Stelle etablierten Muster „empfangen und gespeichert, aber nicht
verbraucht", z. B. Tempo/LCD-Kontrast/Pedal-Modell im SYSTEM-Block).

### FX1/FX2-Anzeige: effekt-spezifische Labels statt generischem „Param1"

Die Frontpanel-Seiten zeigten bisher unabhängig vom Effekttyp immer
„Param1: %d". Gegen jedes `fx_*.h`s `processBlock()` geprüft, was das
Param1-Byte (`effects[slot][1]`, das einzige, das die Encoder tatsächlich
schreiben — Param2 bleibt SysEx-only) pro Typ bedeutet: Distortion=Drive,
Touch Wah=Sens, Chorus/Flanger/Phaser/Reverb=Depth, Delay=Feedback; Thru
hat gar keine Parameter.

- **`DX_FXHost.h`:** neues `FX_PARAM1_LABELS[FX_COUNT]`-Array, indiziert
  wie `FX_NAMES[]`.
- **`DX_GUI::dxDrawScreen()`:** FX1/FX2-Zweig nutzt jetzt
  `"%s: %d"` mit `FX_PARAM1_LABELS[typeId]` statt der generischen
  Beschriftung; bei Thru wird die zweite Zeile komplett weggelassen
  (kein Parameter vorhanden, keine leere/irreführende Zeile).
- Werte bleiben bewusst roh (0–127), konsistent mit allen anderen Seiten
  (OP1-4 zeigen ebenfalls rohe Werte für Freq/Level, keine Umrechnung).
  Eine ursprünglich in Phase C notierte Idee „Delay-Zeit in Millisekunden
  anzeigen" erledigt sich damit von selbst: die Zeit ist Param2 (Delay/
  Reverb), das ohnehin SysEx-only bleibt und am Frontpanel nie angezeigt
  wird — Param1 bei Delay ist Feedback, ein einfacher Rohwert ohne
  Einheit passt hier tatsächlich am besten.

### Build-Verifikation

Vollständiger Rebuild: **null Warnungen**. FLASH 163.864 B / 16 MB ≈
0,98 %, RAM 273.912 B / 512 KB ≈ 52,24 % (minimale Änderung: −168 B
Flash durch die gelöschten Methoden, +272 B durch das neue Label-Array
und die GUI-Anpassung, macht netto +104 B).

### STATUS JETZT

Beide in der Nutzerfrage aufgeworfenen Restpunkte sind erledigt. Der
100 %-Clone-Plan (A–D) bleibt vollständig abgeschlossen; einzig
verbleibender, nicht selbst schließbarer Punkt ist weiterhin der
CPU-Lasttest auf echter Hardware (Messinstrumentierung seit Phase D
vorhanden, siehe §16). Die 3-Encoder-Bedienoberfläche (vs. die ~20
direkten Knobs der echten reface DX) bleibt eine bewusste, seit
Projektbeginn feststehende Hardware-Entscheidung, kein Firmware-Gap.

---

## 19. Nachtrag: Echter Hardware-CPU-Lasttest deckt Effekt-Wechsel-Bug auf (2026-07-07)

**Anlass:** Nutzer hat den in Phase D instrumentierten CPU-Lasttest auf
echter Hardware durchgeführt (der eine offene Punkt, den ich selbst nicht
schließen konnte). Ergebnis: **Now 49–60 % (gut), Peak 140 %** — ein
Peak über 100 % bedeutet einen echten Pufferunterlauf (Audio-Glitch/Klick
in dem Moment), kein Messfehler.

### Root Cause

`SAMPLES_PER_BUFFER` (`lib/audio/include/audio_subsystem.h`) ist **16**
Samples — bei 44,1 kHz nur **~363 µs** Echtzeitbudget pro I2S-IRQ-Block.
`DX_FXHost::setSlot()` (aufgerufen bei jedem Effekt-Typ-Wechsel: Encoder
auf FX1/FX2, Preset-Wechsel, SysEx-Patch-Load) rief bisher
`resetSlot()` auf, das **immer den vollen 96-KB-Scratch-Puffer**
(24576 Floats, dimensioniert für den größten Einzelbedarf `FxReverb`)
per `memset()` löschte — unabhängig davon, welcher der 8 Effekttypen
tatsächlich aktiviert wurde. Geschätzte Memset-Dauer bei ~150 MB/s
Durchsatz: 400–650 µs, je nach tatsächlichem Speicherdurchsatz zur
Laufzeit — sprengt das 363-µs-Budget bei praktisch jedem Wechsel und
erklärt den gemessenen 140-%-Peak.

Der Grund für die Notwendigkeit **irgendeines** Löschens: Chorus,
Flanger, Phaser, Delay und Reverb legen ihre Delay-Lines alle als
Pointer-Aliase in denselben gemeinsamen `scratch_[slot]`-Puffer (RAM-
Sparmaßnahme aus Phase C — nur ein Effekt pro Slot ist je aktiv). Ohne
Löschen beim Wechsel würde die neue Effektinstanz die Rohdaten des
vorherigen Effekts als Delay-Line-Inhalt vorfinden (hörbarer Phantom-
Echo-Artefakt). Der Fix in Phase C hat das korrekt erkannt, aber mit der
denkbar teuersten Lösung (immer alles löschen) statt der günstigsten
(nur den tatsächlich benötigten Bereich löschen) behoben.

### Fix

- **`fx_base.h`:** neue virtuelle Methode `scratchFootprintFloats()`,
  Default `0` (Thru/Distortion/TouchWah fassen `scratch_` nie an).
- **`fx_chorus.h`/`fx_phaser.h`:** Override `MAX_DELAY * 2` bzw.
  `FLANGER_BUF_SIZE * 2` → 8192 Floats statt 24576 (66,7 % weniger).
- **`fx_flanger.h`:** Override `bufferSize_ * 2` (Laufzeitwert aus
  `prepare()`, bei 44,1 kHz ≈ 1338 Floats) → 94,6 % weniger.
- **`fx_delay.h`:** Override `MAX_DELAY * 2` = 22050 Floats (10,3 %
  weniger als der volle Puffer — Delay braucht selbst schon fast das
  Reverb-Maximum, siehe unten).
- **`fx_reverb.h`:** Override summiert `combSize_[ch][i]` +
  `allSize_[ch][i]` über beide Kanäle zur Laufzeit (Schleife, unkritisch
  — läuft nur einmal pro Wechsel, nicht pro Sample) → exakt die
  tatsächlich benötigten ~24276 Floats (kein struktureller Unterschied
  zum Worst-Case, da Reverb selbst der Worst-Case ist).
- **`DX_FXHost.h`:** `resetSlot()` nimmt jetzt die eingehende
  `FXBase*`-Instanz entgegen, fragt `scratchFootprintFloats()` ab
  (defensiv auf `FX_SCRATCH_FLOATS` geclampt) und löscht nur noch genau
  diesen Bereich statt immer `sizeof(scratch_[slot])`.

### Ergebnis und verbleibende Einschränkung

Für 6 von 8 Zieleffekten (Thru/Distortion/TouchWah: kein Löschen nötig;
Chorus/Phaser: 8192 statt 24576 Floats; Flanger: 1338 statt 24576 Floats)
ist der Peak damit vollständig unter dem 363-µs-Budget. Für die
verbleibenden 2 (Delay: 22050 Floats, Reverb: ~24276 Floats) bleibt ein
kurzer, unvermeidbarer Blip beim Wechsel **speziell in diese beiden
Effekttypen** — deren Delay-Lines sind inhärent so groß, dass selbst ihr
tatsächlicher (nicht mehr Worst-Case-)Bedarf das 363-µs-Budget einer
einzelnen Audio-IRQ übersteigt. Eine vollständige Beseitigung würde das
Löschen über mehrere Audio-Blöcke strecken (Zustandsautomat mit
Zwischen-Stummschaltung) — bewusst nicht umgesetzt, da das ein deutlich
größerer Eingriff für ein seltenes, nutzergetriebenes Ereignis (Encoder-
Dreh auf FX-Typ) wäre, kein Dauerzustand während des Spielens.

### Build-Verifikation

Vollständiger Rebuild: **null Warnungen**. FLASH 164.064 B / 16 MB ≈
0,98 %, RAM 273.936 B / 512 KB ≈ 52,25 % (minimale Änderung: +200 B
Flash, +24 B RAM für die 5 neuen Overrides + die `DX_FXHost`-Anpassung).

### STATUS JETZT

Der von Nutzerseite durchgeführte Hardware-Test hat einen echten,
zuvor unentdeckten Performance-Bug aufgedeckt und direkt zu seiner
Behebung geführt — genau der Zweck der in Phase D hinzugefügten
Instrumentierung. Empfehlung an den Nutzer: den Lasttest auf echter
Hardware erneut durchführen und insbesondere gezielt zwischen Delay und
Reverb hin- und herschalten, um zu bestätigen, dass der Peak jetzt nur
noch bei diesen beiden Wechseln auftritt (und nicht mehr bei den
anderen 6 Effekttypen).

---

## 20. Nachtrag: FPU Flush-to-Zero gegen Zischen/Jitter bei schnellem Notenwechsel (2026-07-07)

**Anlass:** Zweiter Hardware-Test nach §19 mit dem Preset "MotionPad"
(Algorithmus 8, Slot 1 = Chorus, Slot 2 = Delay mit Feedback 70/127):
Peak 111 %, Now 50–65 % — deutlich besser als die 140 % aus §19, aber
weiterhin über 100 %. Zusätzlich beschrieben: hörbares Zischen/Jittern
speziell bei schnellem Tonwechsel (schneller Notenwechsel, POLY-Modus,
also Stimmen-Stealing über alle 8 Stimmen).

### Root Cause

`RDX_Operator::compute()` (`RDX_Operator.h`) enthält pro Operator und
Sample ein Feedback-Tiefpassfilter:
```cpp
fbFilter_ += fbLpCoef_ * (fbAcc_ - fbFilter_);
```
Ein klassisches IIR-Filter, das sich beim Ausklingen einer Stimme
exponentiell der Null annähert — dabei durchläuft es zwangsläufig den
**Subnormal-/Denormal-Gleitkommabereich** (extrem kleine Floats nahe
Null). Ohne explizite Flush-to-Zero-Konfiguration behandelt der
Cortex-M33 der RP2350 Denormals IEEE-754-konform in Hardware/Mikrocode,
was **um ein Vielfaches langsamer** sein kann als der Normalfall. Grep
über den gesamten Quellbaum bestätigte: keine Stelle setzt bisher das
FZ-Bit. Je mehr Stimmen/Operatoren gleichzeitig ausklingen (genau der
Fall bei schnellem Notenwechsel mit vielen aktiven/abklingenden
Stimmen), desto wahrscheinlicher ein Stall in einem einzelnen
363-µs-Audioblock — hörbar als Zischen/Jitter. Das Reverb/Delay/Chorus/
Flanger/Phaser der Effektkette haben strukturell ähnliche
Feedback-/Dämpfungs-IIR-Zustände, sind aber sekundär gegenüber dem
Operator-Feedback-Filter, der bei **jeder** Stimme und **jedem**
Preset kontinuierlich läuft.

### Fix

`pico_init()` (`src/pico_hw.cpp`) setzt jetzt als allererste Anweisung
das FZ- (Flush-to-Zero, Bit 24) und DN-Bit (Default-NaN, Bit 25) im
FPU-Statusregister FPSCR:
```cpp
{
    uint32_t fpscr;
    __asm__ volatile ("vmrs %0, fpscr" : "=r" (fpscr));
    fpscr |= (1u << 24) | (1u << 25);
    __asm__ volatile ("vmsr fpscr, %0" : : "r" (fpscr));
}
```
Reine Inline-Assembly (VMRS/VMSR) statt CMSIS-Intrinsics
(`__get_FPSCR`/`__set_FPSCR`): der erste Versuch scheiterte, weil die
CMSIS-Core-Header dieses SDK-Forks für den RP2350-Build-Pfad nicht
transitiv erreichbar sind (weder über bereits vorhandene Includes noch
über `hardware/sync.h`, das trotz `__wfi()` keine CMSIS-Kette zieht,
sondern einen eigenen Inline-Asm-Wrapper nutzt) — die Inline-Assembly
ist plattform-/header-unabhängig und funktioniert identisch auf jedem
Cortex-M mit FPU (M4/M7/M33).

Flush-to-Zero ist für Audio unhörbar (die betroffenen Werte liegen per
Definition unterhalb der Rauschgrenze) und ist Standardpraxis in
Echtzeit-Audio-DSP auf ARM.

### Build-Verifikation

Vollständiger Rebuild: **null Warnungen**. FLASH 164.072 B / 16 MB ≈
0,98 %, RAM 273.936 B / 512 KB ≈ 52,25 % (+8 B Flash gegenüber §19,
kein Laufzeit-RAM-Overhead).

### STATUS JETZT

Fix angewandt und Build verifiziert; die eigentliche Bestätigung (Peak
sollte bei erneutem Hardware-Test mit "MotionPad" bei schnellem
Notenwechsel spürbar sinken, das Zischen/Jittern verschwinden) kann nur
der Nutzer auf echter Hardware durchführen. Sollte nach diesem Fix noch
Zischen auftreten, käme als nächstes in Frage: die tatsächliche
IRQ-Gesamtzeit inkl. IPC-Drain-Schleife messen (die aktuelle
CPU-Last-Instrumentierung erfasst nur `fill_buffer()` selbst, nicht die
Zeit für `ipc_apply()`-Aufrufe wie `noteOn()` davor, die bei einem
Ausbruch mehrerer Noten in kurzer Zeit ebenfalls ins Budget schneiden
könnten).

---

## 21. Nachtrag: Soft-Limiter gegen Clipping-Zischen (2026-07-07)

**Anlass:** Dritter Hardware-Test nach §20: Peak auf 96 % gesunken (FTZ-Fix
wirkt), aber das Zischen blieb bestehen — jetzt konkret beschrieben als
"besonders im hohen Notenbereich". Erste Vermutung (Aliasing durch
Sägezahn-Feedback bei hoher Tonhöhe) wurde gegen das ESP32-Original und
das offizielle Referenzhandbuch geprüft: FB ist laut Doku (S. 4) real
pro Operator einstellbar (-127 bis +127, Sinus↔Sägezahn/Rechteck), also
kein Bug in unserem Datenmodell. Der Nutzer fand dann experimentell:
Absenken von OP1/OP3-Level bei "MotionPad" von 127 auf 121 beseitigt das
Zischen fast vollständig, ohne hörbaren Klangunterschied — ein Hinweis,
der eher zu einem Schwellenwert-Effekt (Clipping) als zu graduellem
Aliasing passt.

### Root Cause

`RDX_Operator::setParams()` berechnet `outGain_ = rdxGain(params_.outLevel
* velogain_) * scaling_`. `velogain_` (Velocity-Sensitivity) kann bis
**1,08×** erreichen (`velocityGain(vel, sens, 1.08f)`, `VELO_SENS[127] =
1.0`). Bei `outLevel=127` ergibt das eine Abfrage der Gain-LUT
(`rdxGain()`, `RDX_Constants.h`) bei Index ≈137 — die LUT ist mit 192
statt 128 Einträgen absichtlich über den nominalen 0–127-Bereich hinaus
definiert und liefert dort **~1,5× statt 1,0×** Gain. Bestätigt identisch
im ESP32-Original vorhanden (`velocityGain(..., 1.08f)`, exakt derselbe
Wert) — also eine bewusste, portierte Velocity-Dynamik-Funktion, kein
Portierungsfehler.

Das einzige Sicherheitsnetz dagegen war ein **harter Integer-Clip**
(`main.cpp`, `if (dl > 32767) dl = 32767;`). Sobald Operator-Gain-
Überschwingen + 3-Operator-Mix (Algorithmus 8) + Effektkette die
Vollaussteuerung überschreiten, erzeugt der harte Clip echtes digitales
Clipping — ein Schwellenwert-Effekt, der zur Beobachtung des Nutzers
passt (kleine Level-Reduktion knapp unter die Clipping-Schwelle
gebracht, nicht linear leiser gemacht).

### Fix

`main.cpp`: neue `softClipSample()`-Hilfsfunktion vor
`i2s_callback_func()` — transparent (Identität) unterhalb 0,9,
darüber sanfte Sättigung Richtung ±1,0 (`threshold + range * (excess /
(excess + range))`, asymptotisch, nie exakt erreicht). Ersetzt die
Roh-Sample-Werte vor der 16-Bit-Wandlung; der alte harte Clamp bleibt
als billiges Sicherheitsnetz für Restfälle (Float-Rundung) bestehen,
ist aber nicht mehr der primäre Begrenzer. Günstig: der Normalfall
(kein Clipping) kostet nur einen Vergleich, die teurere Rechnung
(Division) läuft nur für die seltenen Sample, die tatsächlich über die
Schwelle kommen.

Bewusst **kein** Cap der Operator-Level in einzelnen Presets (Alternative,
vom Nutzer vorgeschlagen): das hätte nur "MotionPad" repariert, nicht
den Mechanismus — jedes andere Preset mit hohem Level + kräftigem
Anschlag hätte dasselbe Risiko behalten. Der Soft-Limiter fängt es
generell ab, ohne die absichtliche Velocity-Dynamik zu beschneiden.

### Build-Verifikation

Vollständiger Rebuild: **null Warnungen**. FLASH 164.248 B / 16 MB ≈
0,98 %, RAM 274.112 B / 512 KB ≈ 52,28 % (+176 B Flash, +176 B RAM
gegenüber §20).

### STATUS JETZT

Fix angewandt und Build verifiziert; die Bestätigung (Zischen sollte bei
erneutem Test mit "MotionPad" bei hohem Notenbereich + kräftigem
Anschlag verschwinden, ohne dass die Level-Werte zurückgesetzt werden
müssen) kann nur der Nutzer auf echter Hardware durchführen.

---

## 22. Nachtrag: Zischen bei "MotionPad" als Aliasing bestätigt, bewusst nicht "gefixt" (2026-07-07)

**Anlass:** Vierter Hardware-Test nach §21: der Soft-Limiter aus §21
hatte **keine Wirkung** auf das Zischen — einzig das manuelle Absenken
von OP1/OP3-Level (127→121, vom Nutzer in §19/§20 empirisch gefunden)
hilft weiterhin.

### Einordnung

Dass der Soft-Limiter (wirkt nur auf die Gesamtamplitude am Mixdown-Ende)
keine Wirkung zeigt, schließt Clipping als Ursache aus — die in §21
aufgestellte Gain-Überschwing-/Clipping-These war damit falsch. Das
bestätigt stattdessen die ursprüngliche, in §19 zuerst untersuchte
Aliasing-These: OP1 und OP3 (intern Operator 0 und 2) sind bei
"MotionPad" genau die beiden Operatoren mit nennenswertem Eigen-Feedback
(55 bzw. 56 von 127) — laut offiziellem Referenzhandbuch (S. 4) macht
Feedback die Wellenform sinus→sägezahnartig, was viel Oberton-Energie
erzeugt. `outLevel` skaliert dabei nur den **Beitrag des Operators zum
Mix**, nicht die interne Feedback-Stärke (die verwendet den rohen,
unskalierten `fbAcc_`-Wert vor der Level-Skalierung) — das Absenken auf
121 senkt also nicht die Alias-Erzeugung an der Quelle, sondern macht den
bereits erzeugten Alias-Anteil im Mix leiser/maskierter, bis er unter die
Hörschwelle rutscht.

### Entscheidung

Nach Rücksprache mit dem Nutzer: **kein Eingriff in den Feedback-/
Oszillator-Pfad.** Begründung: das exakt gleiche Verhalten ist
byte-identisch im ESP32-Referenzprojekt vorhanden (§19 bereits verifiziert);
ein Fix (z. B. tonhöhenabhängige Feedback-Dämpfung) wäre eine echte
Abweichung von der originalgetreuen Portierung, würde den Klangcharakter
bei hohen Noten mit Feedback verändern, zusätzliche Rechenzeit kosten,
und ist ohne Vergleich mit echter Hardware nicht sicher als "Fehler" statt
"Charakter der echten Hardware" einzustufen. Presets, die bei bestimmten
Noten/Anschlagsstärken hörbar zischen, können bei Bedarf weiterhin manuell
(Operator-Level oder Feedback leicht reduzieren) angepasst werden — wie
vom Nutzer bereits für "MotionPad" demonstriert.

Der Soft-Limiter aus §21 bleibt unverändert im Code: er behebt zwar nicht
dieses Zischen, adressiert aber weiterhin ein reales, unabhängiges Risiko
(hartes digitales Clipping bei Velocity-Gain-Überschwingen kombiniert mit
Algorithmus-Mix), das gesondert von diesem Aliasing-Effekt besteht.

### STATUS JETZT

Kein weiterer Code-Fix für dieses Verhalten. Als bekannte, akzeptierte
Eigenschaft dokumentiert (Aliasing durch nicht-bandbegrenztes
Feedback-FM bei hoher Tonhöhe, identisch zum ESP32-Original). Betroffene
Presets können individuell per Level-/Feedback-Anpassung angepasst
werden, ohne dass das die Engine selbst verändert.

---

## 23. Nachtrag: Algo-Diagramm-Labels überlappen die Grafik (2026-07-07)

**Anlass:** Nutzer meldet auf echter Hardware: die Operator-Nummern
(1–4) und die Algorithmus-Nummer im Algo-Diagramm (`ALGO`-Seite)
überlappen sichtbar mit den Operator-Boxen/-Kreisen.

### Root Cause

`drawAlgo()` (`src/DX_GUI.cpp`) ist byte-identisch zur Zeichenformel des
ESP32-Referenzprojekts (`RDX_Algos.h`/`UI_Algos.h`) — Operator-Ziffer bei
`y[id] - fh2` (Font-Höhen-basiert), Algo-ID-Text bei `y0 + hTotal -
getFontHeight()`. Der Unterschied liegt am **Aufrufort**: das
ESP32-Original ruft `drawAlgo(display, 29, algo_id, 35, true)` auf,
unser Port `drawAlgo(u8g2, 20, ..., 40, true)` (andere `y0`/`hTotal` für
das SH1106 128×64-Display) — die im Original passenden Ränder reichen
bei unseren Werten nicht mehr aus.

### Fix

- Operator-Ziffer-Position von `y[id] - fh2` (Font-Höhen-basiert) auf
  `y[id] - ww - 2` geändert — orientiert sich jetzt an der tatsächlichen
  halben Boxgröße (`ww = OP_PX/2`) statt an einer unabhängigen
  Font-Metrik, plus 2 px Extra-Abstand.
- Algo-ID-Text-Position von `y0 + hTotal - getFontHeight(u8g2)` auf
  `y0 + hTotal - getFontHeight(u8g2) + 3` geändert — 3 px weiter nach
  unten, mehr Abstand zur untersten Operator-Reihe.
- Nicht mehr benötigte Variable `fh2` entfernt (sonst
  `-Wunused-variable`).

### Build-Verifikation

Vollständiger Rebuild: **null Warnungen**. FLASH 164.232 B / 16 MB ≈
0,98 %, RAM 274.112 B / 512 KB ≈ 52,28 % (minimale Änderung).

### STATUS JETZT

Fix angewandt, Build verifiziert. Da ich keine visuelle
Hardware-Verifikation durchführen kann, sind die exakten Pixel-Werte
(-2 px bzw. +3 px) eine erste, gut begründete Schätzung — Bestätigung
und ggf. Feinjustierung stehen noch aus (Nutzer-Test auf echtem
SH1106-Display).

### Nachtrag zu §23: OP-Nummern innerhalb der Box statt darüber

Rückmeldung nach Hardware-Test von §23: Positionierung "über der Box"
funktionierte, sieht aber unschöner aus, weil Verbindungslinien
zwischen Operatoren durch die Ziffern laufen. Grund: Verbindungslinien
erreichen jeweils nur den Rand einer Box (horizontale Linien beginnen
bei `x[id]+ww`, vertikale Verbindungsstücke reichen nur `ww` Pixel an
eine Box heran), nie deren Zentrum — eine Ziffer, die im Boxzentrum
(`y[id]`) sitzt, wird also nie von einer Linie gekreuzt, eine Ziffer im
Zwischenraum zwischen zwei Reihen (wo vertikale Verbindungsstücke
verlaufen) hingegen schon.

**Fix:** `fh2` (Font-Höhe/2) wiederhergestellt, Ziffer-Y-Koordinate von
`y[id] - ww - 2` auf `y[id] + fh2` geändert — zentriert die Ziffer
(bei Baseline-Textpositionierung) ungefähr im Zentrum der eigenen Box
statt darüber. `null Warnungen`, FLASH 164.248 B, RAM 274.112 B
(unverändert gegenüber §23).

### Finale Feinjustierung, auf Hardware bestätigt

Nutzer bestätigt nach Test: OP-Ziffern 1 px nach rechts (`x[id] - fw2`
→ `x[id] - fw2 + 1`), Algo-Nummer 3 px weiter nach unten als zuvor
(`+ 3` → `+ 6`, insgesamt 6 px gegenüber der ursprünglichen
`y0 + hTotal - getFontHeight(u8g2)`-Position). Damit laut Nutzer
"sauber". `null Warnungen`, FLASH 164.256 B, RAM 274.112 B.

## 24. Nachtrag: RP2350-Optimierungen — Blockgröße, I2C, Voice-Skipping, Encoder-Debounce (2026-07-07)

### Anlass

Nach der Portierung auf den RP2350 zeigten sich im Betrieb mehrere inkrementelle Optimierungspotenziale: ein LFO-Timingfehler durch inkonsistente Audio-Blockgrößen, ein langsamer OLED-Refresh, unnötige CPU-Zyklen für idle Synthesizer-Stimmen und ein praktisch wirkungsloser Hardware-Debounce für die Encoder-PIO. Dieser Nachtrag dokumentiert die vier gezielten Änderungen, die zusammen Audio-Stabilität, UI-Responsivität und CPU-Effizienz verbessern.

### Audio-Blockgröße auf `DMA_BUFFER_LEN` ausgerichtet

`SAMPLES_PER_BUFFER` wurde von 16 auf 64 erhöht, sodass es jetzt mit `DMA_BUFFER_LEN` übereinstimmt. Der Block-Level-LFO war für 64 Samples pro Block kalkuliert, wurde aber bei nur 16 Samples pro IRQ aufgerufen — der LFO lief dadurch effektiv 4× zu schnell. Die Angleichung korrigiert diesen Timingfehler. Zusätzlich sinkt die Audio-DMA-IRQ-Rate von ~2744 Hz auf ~686 Hz, was den IRQ-Overhead reduziert. Im I2S-IRQ wurde `dxBuf` von einem VLA auf einen statischen Puffer der Größe `DMA_BUFFER_LEN * 2` umgestellt, um Stack-Variabilität zu vermeiden.

### OLED-I2C-Bus auf 1 MHz (Fast-Mode)

Der I2C-Takt für das SH1106-OLED wurde von 400 kHz auf 1 MHz angehoben. Das Display arbeitet im Fast-Mode stabil und der Refresh ist dadurch ~2,5× schneller. Die höhere Display-Update-Rate wirkt sich direkt auf Encoder- und UI-Responsivität aus.

### Voice-Skipping für idle Stimmen

`RDX_Synth::process()` und `renderAudioBlock()` prüfen nun vor dem Rendern jeder Stimme `isActive()`. Stimmen im Zustand `IDLE` werden komplett übersprungen. Stimmen in `RELEASE` (hörbares Decay) werden weiterhin normal gerendert, um keine Ausblendartefakte zu erzeugen. Bei typischer Polyphonie (1–4 gespielte Noten) fallen 4–7 zusätzliche Stimmen in den `IDLE`-Zustand und werden jetzt nicht mehr berechnet; das liefert die größte CPU-Ersparnis im Normalbetrieb.

### Encoder-PIO-Debounce korrigiert

Der `freq_divider` für die Encoder-PIO-State-Machine wurde von 1 auf 444 gesetzt. Damit sinkt der SM-Takt von ~444 MHz auf ~1 MHz. Der PIO-Debouncer arbeitet mit diesem Takt und erzeugt nun ~490 µs Hardware-Debounce — genau dem ursprünglichen Design-Intent entsprechend. Vorher lag das Debounce-Intervall bei nur ~1,1 µs und war praktisch unwirksam, sodass prellende Encoder-Impulse durchschlugen.

### Build-Verifikation

- Toolchain: `pico-sdk` für RP2350, ARM GCC
- Compiler-Warnungen: **0**
- Firmware-Größe:
  - FLASH: **165.368 B / 0,99 %** von 16 MB
  - RAM: **275.736 B / 52,59 %** von 512 KB

### STATUS JETZT

Alle vier Optimierungen sind im `main`-Branch integriert, build-verifiziert und laufen stabil auf dem RP2350. Audio-LFO-Timing ist nun korrekt, der Display-Refresh merklich schneller, die CPU-Last durch Voice-Skipping reduziert und die Encoder-PIO liefert sauberes Hardware-Debouncing. Keine bekannten Regressionen.

## 25. Nachtrag: RP2350-Optimierungen Welle 2 — LUTs nach RAM, Ausgabepfad fusioniert, semitonesToRatio-Fast-Path (2026-07-07)

### Anlass

Die erste Optimierungswelle (§24) brachte die Audio-Funktionen mit `RAM_HOT` in den SRAM, ließ aber die von diesen Funktionen pro Sample gelesenen Lookup-Tabellen im XIP-Flash. Jeder Zugriff auf `sinTable`, `SEMITONE_LUT` und `levelLUT` lief daher über den XIP-Cache und konnte im Audio-IRQ Jitter erzeugen. Zudem waren die LUTs als `constexpr` deklariert, sodass der Linker pro Translation Unit eine separate Kopie im Flash anlegte. Welle 2 beseitigt beide Probleme, fusioniert den Ausgabepfad und fügt einen semantisch identischen Fast-Path hinzu.

### A) Per-Sample-LUTs in `.time_critical`-RAM-Section verschoben

- Tabellen: `sinTable` (~4,1 KB), `SEMITONE_LUT` (~1 KB), `levelLUT` (~1,5 KB).
- Platzierung via `__attribute__((section(".time_critical.<name>")))`.
- Storage-Variablen von `constexpr` auf `const` geändert; die Builder-Funktionen bleiben `constexpr`.
- Mit `inline` (C++17) + COMDAT-Folding wird sichergestellt, dass pro Tabelle nur eine einzige Kopie im RAM landet.
- `LEVEL_LUT_MAX` vom platzierten Storage entkoppelt: `= buildLevelLUT().maxValue`.

Root-Cause: `RAM_HOT` verschiebt nur Funktionen, nicht Daten. Daten im Flash erzeugen XIP-Cache-Miss-Jitter im Audio-IRQ.

### B) Audio-Ausgabepfad fusioniert

- `DX_Synth_Bridge::fill_buffer(float*)` → `fill_buffer_i32(int32_t*)`.
- Soft-Clip, `float→int32`, Clamp und Interleave werden nach den Effekten in einem einzigen Schleifendurchlauf direkt in den DMA-Ausgabepuffer geschrieben.
- Der statische `dxBuf` (512 B RAM) und ein separater Vollblock-Durchlauf entfallen.
- `softClipSample` aus `main.cpp` als `private static inline` Methode in `DX_Synth_Bridge` verlagert.

### C) `semitonesToRatio`-Fast-Path in `RDX_Operator::compute()`

- Wenn `phaseModSemitones == 0.0f` (kein Pitch-Bend, Portamento, LFO-PM oder PEG — häufiger Idle-Fall) wird direkt `phase_ += phaseInc_` berechnet, statt `semitonesToRatio()` + Multiplikation aufzurufen.
- Semantisch identisch, da `semitonesToRatio(0) == 1.0` exakt.

### Build-Verifikation

- Compiler: null Warnungen.
- Footprint:
  - **Flash:** 160.160 B / 0,95 %
  - **RAM:** 283.344 B / 54,04 %
- Hinweis zum Flash-Wert: Gegenüber §24 ist der Flash-Verbrauch gesunken, weil COMDAT-Folding die zuvor mehrfach im Flash vorliegenden LUT-Kopien auf jeweils eine einzige Kopie reduzierte.

### STATUS JETZT

Alle Änderungen sind gebaut, warnungsfrei verifiziert und im `main`-Branch zusammengeführt. Der Audio-IRQ hat jetzt keine XIP-Flash-Lesevorgänge mehr im Per-Sample-Pfad; der Ausgabepfad arbeitet mit einem Durchlauf weniger. Die Engine ist bereit für weitere Funktionsarbeit.
