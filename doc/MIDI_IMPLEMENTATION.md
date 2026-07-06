# PicoFaceDX — MIDI Implementation

Version: 2.0

Implementiert in `include/midi_reface.h` / `src/midi_reface.cpp` (Core 1) sowie `include/midi_input_usb.h` / `src/midi_input_usb.cpp` (USB-MIDI-Transport, TinyUSB, Cable 0, kein DIN-Port). Die alte reface-CP-MIDI-Schicht wurde vollständig entfernt; ausschließlich diese neue Spezifikation ist gültig.

## 1 Coverage

PicoFaceDX ist ein USB-MIDI-Tongenerator (Yamaha reface DX FM-Synth-Klon). Empfangen wird ausschließlich über USB-MIDI (Cable 0); ein DIN-Port existiert nicht. Gesendet werden Identity Reply, Bulk-Dump-Antworten, Parameter-Request-Antworten, Active Sensing sowie Program-Change-Nebenwirkungen (`txProgram`). Es gibt keine lokale Tastatur und keinen generischen Note-Off/Note-On-Sendeweg.

## 2 Compliance

Voice-Messages werden gemäß Channel-Filter (SYSTEM RX-Kanal, Default All) empfangen. Note-Events werden transponiert (+SYSTEM Master Transpose, +UI-Oktave) und per IPC an `DX_Synth_Bridge::noteOn`/`noteOff` weitergereicht. Pitch Bend wird als roher 14-Bit-Wert per IPC übergeben; Core 0 wandelt ihn in einen zentrierten Signed-Wert um, bevor `RDX_Synth::updatePB` aufgerufen wird; der Bereich wird durch den Patch-Parameter `pbRange` gesteuert (algorithmus-/patchseitig, nicht fest ±2 Halbtöne). Aftertouch ist nicht implementiert (RX/TX beides x). Program Change deckt den vollen Adressbereich des echten reface DX ab (0–31, 32 Werkspresets, siehe Abschnitt 9).

## 3 Transmit / Receive

### 3-1 Channel Voice Messages

| Status | Funktion | TX | RX | Bemerkung |
|---|---|---|---|---|
| 8n / 9n v=0 | Note Off | x | o | Kanalgefiltert (SYSTEM RX), transponiert, IPC an `DX_Synth_Bridge::noteOff` |
| 9n v>0 | Note On | x | o | Kanalgefiltert (SYSTEM RX), transponiert, IPC an `DX_Synth_Bridge::noteOn` |
| Bn | Control Change | o* | o | *TX nur als Program-Change-Nebenwirkung (`txProgram`); kein generischer CC-TX (keine Frontpanel-FX-Kette). RX siehe 3-2 und 3-3 |
| En | Pitch Bend | x | o | Roh 14-Bit per IPC; Core 0 wandelt in Signed; Bereich via Patch `pbRange` |
| Cn | Program Change | o | o | 0–31, alle 32 realen Werkspresets (`DX_NPRESETS=32`, siehe `doc/PRESETS.md`). Wählt Preset via `preset_stage()`. Nicht durch MIDI-Control-Gate beschränkt, nur durch Kanal-Filter |
| An / Dn | Aftertouch (Poly/Ch) | x | x | Nicht implementiert |

### 3-2 Control Change — Tier 1 (lokal auf Core 1, nicht an Engine weitergeleitet)

| CC | Funktion | TX | RX | Bemerkung |
|---|---|---|---|---|
| 121 | Reset All Controllers | x | o | Setzt per IPC: Pitch Bend → Center, Modulation (CC1) → 0 (Minimum), Expression (CC11) → 127 (Maximum), Sustain (CC64) → off — exakt wie in der offiziellen Data List spezifiziert |
| 124 | Omni Off | x | o | All Notes Off + RX-Kanal := 0 |
| 125 | Omni On | x | o | All Notes Off + RX-Kanal := All (0x10) |

**CC66 (Sostenuto) und CC67 (Soft Pedal) existieren auf dem echten reface DX nicht** (nur reface CP hat sie) und werden daher bewusst nicht behandelt — sie fallen durch zu Tier 2 und werden ignoriert, da `RDX_Synth::processCC` dafür keinen Fall kennt. Eine frühere Version dieser Firmware übernahm eine CC67-Soft-Pedal-Behandlung aus der (mittlerweile entfernten) reface-CP-Schicht; das war eine Spezifikationsabweichung und wurde entfernt.

### 3-3 Control Change — Tier 2 (per IPC unverändert an Core 0 `DX_Synth_Bridge::processCC` → `RDX_Synth::processCC` weitergeleitet)

| CC | Funktion | TX | RX | Bemerkung |
|---|---|---|---|---|
| 0 / 32 | Bank Select MSB / LSB | x | o | Akzeptiert, aktuell ungenutzt (alle 32 Presets sind über Program Change 0-31 allein adressierbar, kein Bank-Umschalten nötig) |
| 1 | Mod Wheel | x | o | |
| 5 | Portamento Time | x | o | |
| 7 | Main Volume | x | o | Berechnet Ausgangsverstärkung neu |
| 11 | Expression | x | o | Multiplikativ mit Main Volume; berechnet Ausgangsverstärkung neu (wie bei CC7) |
| 64 | Sustain | x | o | Korrekte Note-Off-if-not-held-Logik beim Loslassen |
| 65 | Portamento On/Off | x | o | |
| 80 *1 | Algorithm Quick-Select | x | o | 0–127 skaliert auf 0–11 |
| 85 *1 | Op1 Output Level | x | o | |
| 86 *1 | Op1 Feedback | x | o | |
| 87 *1 | Op1 Feedback Type | x | o | |
| 88 *1 | Op1 Freq Mode | x | o | |
| 89 *1 | Op1 Freq Coarse | x | o | |
| 90 *1 | Op1 Freq Fine | x | o | |
| 102–107 *1 | Op2: dieselben 6 Parameter | x | o | Entsprechend CC85–90 für Operator 2 |
| 108–113 *1 | Op3: dieselben 6 Parameter | x | o | Entsprechend CC85–90 für Operator 3 |
| 114–119 *1 | Op4: dieselben 6 Parameter | x | o | Entsprechend CC85–90 für Operator 4 |
| 120 | All Sound Off | x | o | |
| 123 | All Notes Off | x | o | |

*1: Nur empfangen (und übertragen), wenn SYSTEM „MIDI Control" (Adresse `0E`) eingeschaltet ist — exakt wie beim echten reface DX spezifiziert. Alle übrigen Tier-2-CCs sind davon unabhängig stets aktiv.


### 3-4 Channel Mode Messages

Siehe Tier 1 (CC124 Omni Off, CC125 Omni On) sowie CC120 All Sound Off, CC123 All Notes Off, CC121 Reset All Controllers. Omni Off setzt RX-Kanal auf 0, Omni On setzt RX-Kanal auf All (0x10); beide lösen zusätzlich All Notes Off aus.

## 4 SYSTEM-Parametertabelle

Basisadresse `00 00 00`, 32 Bytes gesamt (spiegelt `RDX_System` aus `dx_engine` bytefürbyte).

| Adresse | Größe | Feld | Wertebereich / Bemerkung |
|---|---|---|---|
| 00 | 1 | TX MIDI Channel | 0–0F |
| 01 | 1 | RX MIDI Channel | 0–0F; 10 = All |
| 02–05 | 4 | Master Tune | 4 Nibbles (`SYS_TUNE_0..3`), zu 16-Bit-Rohwert rekombiniert (`(t0<<12)\|(t1<<8)\|(t2<<4)\|t3`), dekodiert zu Cent (`(raw-1024)*0.1`, geklemmt auf ±102,4/±102,3 Cent) und per eigenem IPC-Befehl (`IPC_CMD_DX_MASTER_TUNE`) additiv zum Pitch Bend auf alle Operatoren angewandt (Phase D, siehe `doc/CHANGELOG_DX_ENGINE.md` §16) |
| 06 | 1 | Local Control | 0/1; keine lokale Tastatur, nur gespeichert |
| 07 | 1 | Master Transpose | 0x34–0x4C = −12…+12 Halbtöne; auf eingehende Noten angewandt |
| 08–09 | 2 | Tempo | Gespeichert, nicht verbraucht |
| 0A | 1 | LCD Contrast | Gespeichert, nicht verbraucht (kein LCD) |
| 0B | 1 | Pedal Model | Gespeichert, nicht verbraucht |
| 0C | 1 | Auto Power Off | 0/1, gespeichert |
| 0D | 1 | Speaker On | 0/1, gespeichert |
| 0E | 1 | MIDI Control | 0/1, gespeichert; gated CC80/85-90/102-119 (siehe 3-3, *1) |
| 0F–1F | 17 | reserviert | Gespeichert |

## 5 Common-Parametertabelle

Basisadresse `30 00 00`, 38 Bytes gesamt (spiegelt `RDX_Common`).

| Offset | Größe | Feld |
|---|---|---|
| 00–09 | 10 | voiceName |
| 0A–0B | 2 | reserved1 |
| 0C | 1 | transpose |
| 0D | 1 | monoPoly |
| 0E | 1 | portaTime |
| 0F | 1 | pbRange |
| 10 | 1 | algorithm (0–11) |
| 11 | 1 | lfoWave |
| 12 | 1 | lfoSpeed |
| 13 | 1 | lfoDelay |
| 14 | 1 | lfoPMD |
| 15–18 | 4 | pegRate |
| 19–1C | 4 | pegLevel |
| 1D–22 | 6 | effects (2×3): je Slot [Type, Param1, Param2] — Type 0–7 = Thru/Distortion/Touch Wah/Chorus/Flanger/Phaser/Delay/Reverb, verarbeitet post-mix von `DX_FXHost` (`include/dx_engine/DX_FXHost.h`); nur patch-/SysEx-adressierbar, keine dedizierte CC-Steuerung (bestätigt gegen die offizielle Data List) |
| 23–25 | 3 | reserved2 |

## 6 Operator-Parametertabelle

Basisadresse `31 <opNum 0–3> 00`, 28 Bytes pro Operator (spiegelt `RDX_OpParams`).

| Offset | Größe | Feld |
|---|---|---|
| 00 | 1 | enable |
| 01–04 | 4 | egRate |
| 05–08 | 4 | egLevel |
| 09 | 1 | rateScaling |
| 0A–0D | 4 | scaleLD / scaleRD / scaleLC / scaleRC |
| 0E | 1 | lfoAMD |
| 0F | 1 | lfoPMDEnable |
| 10 | 1 | pegEnable |
| 11 | 1 | velSens |
| 12 | 1 | outLevel |
| 13 | 1 | feedback |
| 14 | 1 | fbType |
| 15 | 1 | freqMode |
| 16 | 1 | freqCoarse |
| 17 | 1 | freqFine |
| 18 | 1 | freqDetune |
| 19–1B | 3 | reserved |

## 7 Bulk Dump Blöcke

Yamaha-SysEx-Header: ID `43H`, Group `7F 1C`, Model ID `05H` (reface DX; die alte reface-CP-Schicht verwendete `04H`). Command-Nibble = Bits 4–6 des 3. Bytes (`d[2]&0x70`): `0x10` Parameter Change (RX), `0x00` Bulk Dump (RX), `0x20` Dump Request (RX→TX-Antwort), `0x30` Parameter Request (RX→TX-Antwort).

### Identity Request / Reply

Identity Request (RX): `F0 7E 0n 06 01 F7`
Identity Reply (TX): `F0 7E 7F 06 02 43 00 41 53 06 00 00 00 7F F7`

Hinweis: Model-ID-Bytes `41 53 06` (reface CP verwendete `41 52 06`; Byte 0x53 vs. 0x52 ist der einzige Unterschied, bestätigt gegen das echte reface DX und die ESP32-Referenzimplementierung `tools/refacedx/RDX-Reface-DX-emu/RDX/RDX_Midi.h`).

### Parameter Change (RX)

`F0 43 1n 7F 1C 05 <AddrH> <AddrM> <AddrL> <Data> F7` — ein Byte pro Parameter; adressiert System/Common/Operator-Blöcke (siehe Tabellen in den Abschnitten 4–6).

### Bulk Dump (RX/TX)

`F0 43 0n 7F 1C <ByteCountHi> <ByteCountLo> 05 <AddrH> <AddrM> <AddrL> <Data...> <Checksum> F7`

Byte Count = 1 (Model ID) + 3 (Adresse) + Datenlänge.
Checksumme: Yamaha/Roland-Standard, `(128 - (sum & 0x7F)) & 0x7F` über ModelID+Addr+Data-Bytes; ungültige Checksumme → Block verworfen.

RX schreibt in einen cross-core Staging-Patch (`include/dx_patch_stage.h`) statt direkt in die Live-Engine (Core 1 darf den Live-Patch nie direkt mutieren). Der Bulk-Footer-Block (Adresse `0F 0F xx`) löst `IPC_CMD_DX_PATCH_APPLY` aus, welches den Staging-Patch auf Core 0 in die Live-Engine kopiert. Der Bulk-Header-Block (Adresse `0E 0F xx`) initialisiert den Staging-Bereich zuerst mit dem AKTUELLEN Live-Patch, sodass ein partieller Dump (z. B. nur der Common-Block) keine unverwandten Operator-Daten mit veralteten Staging-Resten überschreibt.

### Dump Request (RX) → TX-Antwort

| Adresse | Antwort |
|---|---|
| `00 00 00` | Bulk: SYSTEM Common (32 Bytes) |
| `0E 0F 00` | Bulk Header (0 Bytes) + Common-Block (`30 00 00`, 38 Bytes) + 4× Operator-Blöcke (`31 00 00`..`31 03 00`, je 28 Bytes) + Bulk Footer (0 Bytes) — vollständiger Patch-Dump |
| `30 00 00` | nur Common-Block (38 Bytes) |
| `31 <op> 00` | nur einzelner Operator-Block (28 Bytes) |

### Parameter Request (RX) → TX-Antwort

Gleiche Adressierung wie Dump Request; Antwort ist jeweils das einzelne aktuelle Byte an dieser Adresse (Live-Lesezugriff aus der Engine; Core-1-Lesezugriff auf `DX_Synth_Bridge::patch()` ist eine akzeptierte Konvention in dieser Codebasis).

### Bulk-Dump-Blockübersicht

| Block | Adresse | Bytes |
|---|---|---|
| SYSTEM Common | `00 00 00` | 32 |
| Bulk Header | `0E 0F 00` | 0 |
| Common | `30 00 00` | 38 |
| Operator 0 | `31 00 00` | 28 |
| Operator 1 | `31 01 00` | 28 |
| Operator 2 | `31 02 00` | 28 |
| Operator 3 | `31 03 00` | 28 |
| Bulk Footer | `0F 0F xx` | 0 |

## 8 MIDI Implementation Chart

```
PicoFaceDX — MIDI Implementation Chart
Model: PicoFaceDX (reface DX clone)        Version: 2.0

+-----------------------------+----+----+-------------------------------------------+
| Function                    | TX | RX | Remarks                                   |
+-----------------------------+----+----+-------------------------------------------+
| Basic Channel               |  o |  o | 1–16 (SYSTEM TX/RX channel)              |
| Mode                        |  3 |  3 | Mode 3: OMNI OFF / POLY                   |
| Note Number                 |  x |  o | RX only; transposed (master+UI); IPC      |
| Velocity                    |  x |  o | RX note on v>0                            |
| After Touch                 |  x |  x | Not implemented                           |
| Pitch Bend                  |  x |  o | 14-bit raw via IPC; range = patch pbRange |
| Control Change              |  o*|  o | *TX only PC side-effect; Tier1: CC121/    |
|                             |    |    | 124/125 local; Tier2: CC0/32/1/5/7/11/64/ |
|                             |    |    | 65/120/123 always on, CC80/85-90/102-119  |
|                             |    |    | gated by MIDI Control (real spec)         |
| Program Change               |  o |  o | 0-31, all 32 real factory voices          |
| System Exclusive             |  o |  o | Yamaha ID 43H, Group 7F 1C, Model 05H;    |
|                             |    |    | Param Change/Bulk/Dump Req/Param Req;     |
|                             |    |    | Identity Reply 41 53 06                    |
| System Common                |  x |  x | Not implemented                           |
| System Real Time             |  o |  o | Active Sensing FE: TX 200ms; RX 350ms      |
|                             |    |    | timeout -> All Sound Off + All Notes Off   |
| Aux: All Sound Off           |  x |  o | CC120                                     |
| Aux: Reset All Controllers   |  x |  o | CC121 (local)                             |
| Aux: Local On/Off             |  x |  o | SYSTEM byte stored; no local keyboard     |
| Aux: All Notes Off            |  x |  o | CC123; CC124/125 also trigger             |
| Aux: Active Sense              |  o |  o | See System Real Time                      |
| Aux: Reset                    |  x |  x | Not implemented                           |
| Notes                        |    |    | Mode 1: OMNI ON/POLY; Mode 3: OMNI OFF/    |
|                             |    |    | POLY. 'o'=Yes, 'x'=No. No DIN port; USB-   |
|                             |    |    | MIDI Cable 0 only. Front-panel FX1/FX2    |
|                             |    |    | pages. MIDI Control gate active.          |
+-----------------------------+----+----+-------------------------------------------+
```

## 9 Deviations

1. **Keine lokale Tastatur / kein generischer Note-TX** — PicoFaceDX ist ein Tongenerator; Local Control wird akzeptiert/gespeichert, hat aber keine Wirkung.
2. **Master Tune vollständig verdrahtet** (seit Phase D) — `tuningSemitones` wirkt additiv zum Pitch Bend auf alle Operatoren; siehe SYSTEM-Tabelle oben (Adresse 02–05) und `doc/CHANGELOG_DX_ENGINE.md` §16.
3. **Program Change vollständig implementiert** — 0–31, alle 32 echten Werkspresets (Bank 1-4 à 8), byte-exakt aus den offiziellen `.syx`-Factory-Dumps geparst. Siehe `doc/PRESETS.md` für die vollständige Preset-Tabelle und die Provenienz der Konvertierung.
4. **CC66 (Sostenuto) / CC67 (Soft Pedal) bewusst nicht implementiert** — beide existieren auf dem echten reface DX nicht (nur reface CP hat sie). Eine frühere Version dieser Firmware hatte eine CC67-Soft-Pedal-Behandlung aus der (mittlerweile entfernten) reface-CP-Schicht übernommen; das war eine Spezifikationsabweichung und wurde entfernt.
