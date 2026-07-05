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
- **Verbleibend offen:** Patch-Persistenz, DX-eigene Effektkette, echte Werte-Anzeige auf den OP/LFO-Seiten (noch Platzhaltertext statt Zahlen — `dxDrawScreen()` zeigt für Nicht-ALGO-Seiten weiterhin nur '(values shown via encoders)'), CPU-Lasttest bei `MAX_VOICES=8`.
