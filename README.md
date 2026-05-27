# JJ4x4 → Bitwig USB-MIDI Controller

Custom firmware that turns a **KPrepublic JJ4x4** macropad (ATmega32A, 16 keys)
into a class-compliant **USB-MIDI controller** for **Bitwig Studio** — a Drum
Machine controller with two extra navigation/performance modes.

This firmware is for the board featured here: https://kprepublic.com/collections/jj4x4-macropad  

items needed:
1. JJ4x4 circuit board - $22
2. one of the cases on that site or build your own - $18-20
3. 16 MX/ALPS switches - $10-20 depending on the amount you find
4. 16 keycaps - $10 for cheapo ones
5. Mini USB cord $$

This is standalone firmware, not a QMK keymap.

## Layout

```
firmware/     AVR firmware (V-USB USB-MIDI) — build with avr-gcc, flash with bootloadHID
bitwig/       Bitwig Studio JavaScript controller script
```

## Getting started

1. **Firmware** — see [`firmware/README.md`](firmware/README.md) for the
   toolchain, build (`make`), and flashing (`make flash`).

   In plain English: download and install **QMK Toolbox**, enter the
   bootloader (hold key K11, position 5, while plugging in), click *Open*,
   select the `.hex` file, and hit *Flash*. Done.

   If you want to *edit* the firmware, you'll also need **QMK MSYS** to
   compile it before flashing.

2. **Bitwig script** — copy `bitwig/JJ4x4/` into
   `Documents\Bitwig Studio\Controller Scripts\` (or wherever your controller
   scripts folder points within Bitwig), then add it in Bitwig under
   *Settings → Controllers* and select the **JJ4x4 MIDI** input port.

## How it works

The 16 keys are numbered 0–15:

```
 0   1   2   3
 4   5   6   7
 8   9  10  11
12  13  14  15
```

There are **three modes** (drum / device / clip-nav). You cycle them with
the `0 + 15` chord. The underglow color tells you which one you're in:

| Mode     | Underglow | Backlight | What it sends                  |
|----------|-----------|-----------|--------------------------------|
| Drum     | 🟦 blue   | dim       | Notes 36–51 (MIDI ch 1)         |
| Device   | 🟩 green  | mid       | 16 nav/transport CCs (ch 2)     |
| Clip-nav | 🟪 magenta| bright    | 16 clip-launcher CCs (ch 3)     |

Drum-mode quick-flip macros (the `1+X` and `5+X` chords described below)
add another color to the table: 🟧 **amber** = drum mode with quick-flip
disabled.

---

## Cheat sheet — every chord at a glance

**Universal** (work in every mode):

| Chord     | Action                                  |
|-----------|-----------------------------------------|
| `0 + 15`  | Cycle mode (drum → device → clip-nav)   |
| `3 + 12`  | Cycle underglow brightness (5 levels)   |

**Drum mode only:**

| Chord                        | Action                                                |
|------------------------------|-------------------------------------------------------|
| `0 + 3` (release, no sub-key)| Page up (one screen)                                  |
| `0 + 3` + `1`                | Row up (4 pads)                                       |
| `0 + 3` + `5`                | Row down                                              |
| `12 + 15`                    | Page down                                             |
| `3 + 15`                     | Toggle quick-flip macros (blue ↔ amber)               |
| `1 + X` chords               | 14 transport/clip macros (table below) — when blue    |
| `5 + X` chords (X = 8..15)   | Launch scenes 1–8 — when blue                         |

**Clip-nav mode only:**

| Chord                       | Action                                                |
|-----------------------------|-------------------------------------------------------|
| `5 + X` chords (X = 8..15)  | Launch scenes 1–8 — **always on, no toggle here**     |

**Device mode only:**

| Chord     | Action                                                |
|-----------|-------------------------------------------------------|
| `1 + 10`  | Delete currently selected device                      |

**Drum-mode `1 + X` quick-flip macros** (blue underglow = on; 3+15 toggles):

| `1 + X` | Action                            | `1 + X` | Action                  |
|---------|-----------------------------------|---------|-------------------------|
| `1 + 0` | Global play                       | `1 + 9` | Toggle overdub          |
| `1 + 2` | Global stop                       | `1 + 10`| Delete focused clip     |
| `1 + 3` | Global record                     | `1 + 11`| State-aware looper      |
| `1 + 4` | Clip selection left               | `1 + 12`| Play focused clip       |
| `1 + 6` | Clip selection right              | `1 + 13`| Stop focused clip/track |
| `1 + 7` | Global undo                       | `1 + 14`| Record into focused clip|
| `1 + 8` | Toggle arm                        | `1 + 15`| Create new clip         |

**Scene launcher `5 + X`** (drum-armed and clip-nav both):

| Chord    | Scene | Chord    | Scene |
|----------|-------|----------|-------|
| `5 + 8`  | 1     | `5 + 12` | 5     |
| `5 + 9`  | 2     | `5 + 13` | 6     |
| `5 + 10` | 3     | `5 + 14` | 7     |
| `5 + 11` | 4     | `5 + 15` | 8     |

How chords work: press both keys within ~30 ms and the chord fires
instead of the individual key presses. Hold one key longer and it falls
through to its solo action (drum note / CC). Quick taps still feel
instant because the press fires on release, before the 30 ms timeout.

---

### Mode 1 — Drum Machine

One-to-one pad → note assignments. The Bitwig script's key-translation
table follows the drum bank's scroll position so the macropad's top row
always plays the pads under the green highlight on screen. Also works as
a chromatic mode for synths / note devices.

Page/row nav lives on `0+3` and `12+15` (see cheat sheet above). The
14 `1+X` macros plus 8 scene chords (`5+X`) let you control transport,
clips, looper, and scenes without leaving drum mode. Press `3+15` to
disable them when you want pure pads.

### Mode 2 — Device

Every pad is assigned (16/16). One chord: press `1 + 10` together to
delete the currently selected device. Adding behaviour to any pad later
is a controller-script change, not a re-flash.

| Key  | Action                                              |
|------|-----------------------------------------------------|
| `0`  | Global play                                         |
| `1`  | Track up                                            |
| `2`  | Global stop                                         |
| `3`  | Global record toggle                                |
| `4`  | Previous device                                     |
| `5`  | Track down                                          |
| `6`  | Next device                                         |
| `7`  | Global undo                                         |
| `8`  | Toggle arm (cursor track)                           |
| `9`  | Toggle clip-launcher overdub                        |
| `10` | Show / hide expanded device view                    |
| `11` | Add device after (browse at end of chain)           |
| `12` | Previous remote (parameter) page                    |
| `13` | Show / hide remote controls section                 |
| `14` | Collapse / expand device                            |
| `15` | Next remote (parameter) page                        |

### Mode 3 — Clip Nav

A streamlined create / record / play / delete / undo workflow on the
clip launcher. The vertical pad pair (`1`/`5`) moves the cursor track;
the horizontal pair (`4`/`6`) moves the focused slot within that track.
Keys 12–15 act on the focused slot. The looper key (11) is state-aware
and cycles arm → record → play → overdub on one button.

Hold `5` + tap one of 8–15 to launch scenes 1–8 (always on here — no
toggle needed since this is the clip-launcher screen).

| Key  | Action                                              |
|------|-----------------------------------------------------|
| `0`  | Global play                                         |
| `1`  | Track up                                            |
| `2`  | Global stop                                         |
| `3`  | Global record toggle                                |
| `4`  | Clip selection left (prev focused slot)             |
| `5`  | Track down (also: scene-launcher modifier)          |
| `6`  | Clip selection right (next focused slot)            |
| `7`  | Global undo                                         |
| `8`  | Toggle arm (cursor track)                           |
| `9`  | Toggle clip-launcher overdub                        |
| `10` | Delete focused clip                                 |
| `11` | **Looper** — state-aware arm > rec > play > overdub |
| `12` | Play focused clip                                   |
| `13` | Stop focused clip / track                           |
| `14` | Record into focused clip                            |
| `15` | Create new (empty) clip in focused slot             |

Full command map and CC numbers are in [`firmware/README.md`](firmware/README.md).
