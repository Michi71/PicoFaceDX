# Werkspresets

## Überblick

Die Firmware enthält die vollständige Werkspreset-Bank des Yamaha reface DX: alle 32 offiziellen Factory-Voices, byte-exakt geparst aus den offiziellen `.syx`-Dumps. Die Presets sind in `src/presets.cpp` als konstante Tabelle abgelegt und werden über Index bzw. MIDI Program Change angesprochen.

Definitionen in `include/presets.h`:

```c
#define DX_NPRESETS 32
struct DxPreset { const char* name; RDX_Patch patch; };
extern const DxPreset dxPresets[DX_NPRESETS];
```

Die Array-Reihenfolge entspricht exakt der realen reface DX MIDI Program Change-Belegung (laut offiziellem Yamaha Data List):

- PC 0–7 = Bank 1, Slots 1–8
- PC 8–15 = Bank 2, Slots 1–8
- PC 16–23 = Bank 3, Slots 1–8
- PC 24–31 = Bank 4, Slots 1–8

## Preset-Auswahl

### a) Menü-gesteuert

Langer Druck auf den Selector öffnet das Menü: **MENU → Presets**. Es wird eine Auswahlliste aller 32 Werkspreset-Namen angezeigt. Der Encoder scrollt durch die Liste; bei der Auswahl wird das Preset über `preset_stage`/`preset_set_current` angewendet und der entsprechende MIDI Program Change über `refaceMidi.txProgram` gesendet — wie bisher.

### b) MIDI Program Change

MIDI Program Change funktioniert nun über den **gesamten realen Adressbereich**:

- PC 0–31 sind gültig und wählen das entsprechende Preset über `onProgramChange -> preset_stage` aus.
- Werte ≥ 32 werden ignoriert (entspricht der realen Spec: genau 32 adressbare Slots, keine offene Lücke mehr).

## Werkspresettabelle

| PC# | Name |
|----:|------|
| 0 | DigiChord |
| 1 | WobbleBass |
| 2 | MotionPad |
| 3 | LegendEP |
| 4 | DynaLead |
| 5 | DarkBass |
| 6 | TublarBell |
| 7 | D_n_Beats |
| 8 | BeginSweep |
| 9 | MoDemLead |
| 10 | BeepBass |
| 11 | BitTune |
| 12 | TinPerc |
| 13 | BleepClv |
| 14 | FeelIt |
| 15 | BuzzSiren |
| 16 | WoodEP |
| 17 | UniLead |
| 18 | AttackBass |
| 19 | CloudPad |
| 20 | AmbiPluck |
| 21 | Marimba |
| 22 | CheezOrgan |
| 23 | FM_Brass |
| 24 | SolPhase |
| 25 | FlyingKode |
| 26 | AlTiPad |
| 27 | StarPad |
| 28 | WarmPad |
| 29 | FutureBell |
| 30 | GlassHarp |
| 31 | Chopper |

## Technische Details

### Provenienz der `.syx`-Konvertierung

Die 32 Presets wurden aus den offiziellen `.syx`-Factory-Voice-Bulk-Dumps extrahiert, die mit dem ESP32-Referenzprojekt geliefert wurden, aus dessen FM-Engine diese Codebase portiert wurde (`tools/refacedx/RDX-Reface-DX-emu/RDX/data/patches/*.syx`).

Ein einmaliges Host-Only-Tool, `tools/refacedx/syx_to_patches.cpp`, parst diese Dumps mit der bereits verifizierten `syxToPatch()`-Funktion (`include/dx_engine/RDX_Types.h`). Das Tool ist **nicht Teil des Firmware-Builds** und wird **nicht von CMake kompiliert**. Sein stdout erzeugt 32 byte-exakte `patchFromBytes({...})`-Initialisierer, die direkt in die `dxPresets[]`-Tabelle in `src/presets.cpp` eingefügt wurden.

### Verifikation

Byte-exakte Verifikation: Das `voiceName`-Feld jedes Patches, als ASCII dekodiert, stimmt exakt mit dem realen Factory-Voice-Namen überein (stichprobenartig geprüft: „DigiChord", „WobbleBass", „GlassHarp", „Chopper").

### preset_apply / preset_stage

- `preset_stage(uint8_t idx)` (Core 1): Stagt `dxPresets[idx].patch` in den Cross-Core-Staging-Slot (`include/dx_patch_stage.h`) und sendet `IPC_CMD_DX_PATCH_APPLY`.
- `preset_apply(DX_Synth_Bridge* dx)` (**nur Core 0**): Kopiert das gestagte Patch in `dx->patch()`.

## Persistenz

Unverändert: Nach dem Laden eines Presets werden die resultierenden **Patch-Bytes** (nicht der Preset-Index) durch das normale SettingsV2-Autosave gespeichert.

## Status

Die zuvor als separat geplante Aufgabe („Factory-Voice-Bibliothek": nur 1 hartcodiertes Preset vs. 32-Voice-Werksbank des realen reface DX) ist **abgeschlossen**. Die Preset-Tabelle erreicht nun volle Parität mit der realen reface DX-Werksbank (32 Stimmen, PC 0–31).

Hinweis: Das Patch-Verzeichnis des ESP32-Referenzprojekts enthält zusätzlich eine 33. Datei `00-Init_Voice.syx`. Diese ist kein realer Factory-Bank-Slot und fällt aus dem Program-Change-Adressbereich 0–31 heraus; sie wurde bewusst nicht in die Preset-Tabelle aufgenommen, um die Adressierung exakt an der realen 32-Slot-Spec zu halten.
