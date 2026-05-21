# JJ4x4 → Bitwig USB-MIDI Controller

Custom firmware that turns a **KPrepublic JJ4x4** macropad (ATmega32A, 16 keys)
into a class-compliant **USB-MIDI controller** for **Bitwig Studio** — a Drum
Machine controller with a second device-navigation mode.  

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

Press **`0 + 15`** to cycle modes / change page.

### Page 1 — Drum Machine mode

One-to-one pad assignments — use the macros below to navigate the Drum
Machine. Also works as a chromatic mode for synths / note devices.

| Chord            | Action                                  |
|------------------|-----------------------------------------|
| `0 + 3`          | Move active pads up one full screen     |
| `12 + 15`        | Move active pads down one full screen   |
| `12 + 15 + 9`    | Move active pads up one row             |
| `12 + 15 + 13`   | Move active pads down one row           |

### Page 2 — Device + Ottopot companion

| Key  | Action                                                        |
|------|---------------------------------------------------------------|
| `0`  | Previous parameter page                                       |
| `1`  | Collapse / expand device                                      |
| `2`  | Show / hide remote controls                                   |
| `3`  | Next parameter page                                           |
| `4`  | Previous device                                               |
| `6`  | Show / hide expanded device view                              |
| `7`  | Next device                                                   |
| `8`  | Track up (on the arranger — device buttons usable immediately) |
| `12` | Track down (on the arranger — device buttons usable immediately) |
| `15` | Insert device at end of chain                                 |

Full command map and CC numbers are in [`firmware/README.md`](firmware/README.md).

The original design plan is in
`~/.claude/plans/i-want-to-write-melodic-trinket.md`.
