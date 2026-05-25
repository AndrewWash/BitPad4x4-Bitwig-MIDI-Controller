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

Press **`0 + 15`** to cycle modes (drum → device → clip-nav → drum). The
backlight steps through three brightness levels so the active mode is
readable at a glance.

### Page 1 — Drum Machine mode

One-to-one pad assignments — use the macros below to navigate the Drum
Machine. Also works as a chromatic mode for synths / note devices.

| Chord            | Action                                  |
|------------------|-----------------------------------------|
| `0 + 3`          | Move active pads up one full screen (release with no sub-key) |
| `0 + 3` + `1`    | Move active pads up one row             |
| `0 + 3` + `5`    | Move active pads down one row           |
| `12 + 15`        | Move active pads down one full screen   |

### Page 2 — Device mode

Every pad is assigned (16/16). Adding behaviour to any pad later is a
controller-script change, not a re-flash.

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

### Page 3 — Clip Nav mode

A streamlined create / record / play / undo workflow on the clip launcher.
The vertical pad pair (`1`/`5`) moves the cursor track; the horizontal
pair (`4`/`6`) moves the focused slot within that track. Keys 12-15 act
on the focused slot. The looper key (11) is state-aware and cycles
arm → record → play → overdub on one button.

| Key  | Action                                              |
|------|-----------------------------------------------------|
| `0`  | Global play                                         |
| `1`  | Track up                                            |
| `2`  | Global stop                                         |
| `3`  | Global record toggle                                |
| `4`  | Clip selection left (prev focused slot)             |
| `5`  | Track down                                          |
| `6`  | Clip selection right (next focused slot)            |
| `7`  | Global undo                                         |
| `8`  | Toggle arm (cursor track)                           |
| `9`  | Toggle clip-launcher overdub                        |
| `10` | *(reserved — CC assigned for future use)*           |
| `11` | **Looper** — state-aware arm > rec > play > overdub |
| `12` | Play focused clip                                   |
| `13` | Stop focused clip / track                           |
| `14` | Record into focused clip                            |
| `15` | Create new (empty) clip in focused slot             |

Full command map and CC numbers are in [`firmware/README.md`](firmware/README.md).

The original design plan is in
`~/.claude/plans/i-want-to-write-melodic-trinket.md`.
