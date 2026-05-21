# JJ4x4 USB-MIDI Controller — Firmware

Custom firmware that turns a **KPrepublic JJ4x4** macropad (ATmega32A, 16 keys)
into a class-compliant **USB-MIDI controller** for Bitwig Studio.

The ATmega32A has no USB hardware, so USB is done in software with the
**V-USB** library. QMK's MIDI feature does not support V-USB boards — this is
standalone firmware, not a QMK keymap.

> **Build status:** the source has not been compiled in the environment where
> it was written (no AVR toolchain was installed there). Run `make` and report
> any errors — see *Troubleshooting* below.

## What it does

Two modes, switched by a chord:

| | Drum mode (default) | Device mode |
|---|---|---|
| 16 keys | Drum pads — Note On/Off, notes 36–51, MIDI ch 1 | 7 navigation commands (see below), MIDI ch 2 |
| Backlight | dim | bright |

**Chords** (only the four corner keys 0 / 3 / 12 / 15 take part):

| Keys | Action |
|---|---|
| 0 + 15 | Flip to the next mode (cycles) |
| 0 + 3  | Drum page up — `CC 16` (drum mode only) |
| 12 + 15 | Drum page down — `CC 17` (drum mode only) |

**Device-mode key map** (momentary `CC = 127` on press, MIDI ch 2):

```
 0 prevParamPage   1 --            2 --     3 nextParamPage
 4 prevDevice      5 --            6 --     7 nextDevice
 8 trackUp         9 --           10 --    11 --
12 trackDown      13 --           14 --    15 insertDevice
```

| Key | Command | CC |
|---|---|---|
| 0 | Previous parameter page | 22 |
| 3 | Next parameter page | 23 |
| 4 | Previous device | 24 |
| 7 | Next device | 25 |
| 8 | Track up | 20 |
| 12 | Track down | 21 |
| 15 | Insert device at end of chain | 26 |

A corner key's note/CC is delayed ~30 ms (the chord window); the other 12
keys fire instantly.

## Files

```
firmware/
  main.c        main loop (usbPoll + 1 ms tick)
  usbconfig.h   V-USB configuration (ATmega32A, 12 MHz, port D, INT0)
  board.h       pin map (from QMK keyboards/kprepublic/jj4x4/keyboard.json)
  midi.c/.h     USB-MIDI descriptors + 4-byte-packet send queue
  matrix.c/.h   4x4 COL2ROW scan + debounce
  modes.c/.h    mode state machine + chord detection + key->MIDI map
  leds.c/.h     PD4 backlight feedback (mode brightness)
  Makefile      avr-gcc build + bootloadHID flash
  usbdrv/       V-USB library (obdev), used unmodified
```

## Toolchain (Windows)

You need the AVR compiler and the bootloadHID flasher on your `PATH`.

1. **AVR toolchain.** Easiest: install **QMK MSYS**
   (<https://msys.qmk.fm/>). It ships `avr-gcc`, `avr-objcopy`, `avr-size`
   and `make`, even though we don't use QMK itself. Do all builds from the
   QMK MSYS terminal. (Alternatives: Microchip "AVR 8-bit Toolchain", or
   MSYS2 with the `avr-gcc`/`make` packages.)
2. **bootloadHID flasher.** Get `bootloadHID.exe` from the obdev bootloadHID
   package (<https://www.obdev.at/products/vusb/bootloadhid.html>) and put it
   on your `PATH`. **QMK Toolbox** (<https://qmk.fm/toolbox>) also speaks the
   bootloadHID protocol and gives a one-click GUI flash — recommended for a
   first flash.

## Build

From the `firmware/` directory:

```
make            # compiles -> main.hex, prints flash/RAM size
make size       # just print the size report
make clean      # remove build products
```

The ATmega32A has 32 KB flash / 2 KB RAM; this firmware uses only a small
fraction. Watch the `make` size report stay well under those limits.

## Flash

1. **Enter the bootloader:** unplug the JJ4x4, hold **key K11** (2nd row,
   2nd column from the top-left), and plug the USB cable back in while
   holding it. The board is now in the bootloadHID bootloader.
2. **Flash:**
   ```
   make flash          # runs: bootloadHID -r main.hex
   ```
   or use QMK Toolbox: select `main.hex`, click *Flash*.
3. The `-r` flag reboots the board into the new firmware.

**Recovery:** the bootloader is separate from the app and is not touched by
this firmware, so a bad build is always recoverable — just re-enter the
bootloader (step 1) and flash a known-good `main.hex`. Keep your last working
hex. **Never** burn fuses; doing so without an ISP programmer can brick the
board.

## Bring-up / testing order

Build the confidence up in stages — don't debug everything at once:

1. **Enumeration.** Flash, then check Windows *Device Manager* → *Sound,
   video and game controllers*: a device named **JJ4x4 MIDI** appears with
   no yellow `!`. A `!` means a USB descriptor or V-USB timing problem.
2. **Raw MIDI.** Install **MIDI-OX** (free). Open the *JJ4x4 MIDI* input and
   its monitor window. Press each key — you should see Note On/Off (drum
   mode) with no chatter or stuck notes. Test the chords (0+15 flip,
   0+3 / 12+15 paging) and device mode CCs.
3. **Bitwig.** Install the controller script (see `../bitwig/`), load a
   project with a Drum Machine, and test pads + navigation end-to-end.

## Troubleshooting

- **`make` fails: command not found** — the AVR toolchain isn't on `PATH`.
  Build from the QMK MSYS terminal.
- **Compile error in `usbdrv/`** — confirm `F_CPU` is `12000000` and the
  board really has a 12 MHz crystal (V-USB needs an exact, supported clock).
- **Device doesn't enumerate / yellow `!`** — most likely the USB D+/D-
  pins. This firmware assumes the ps2avrGB convention **D+ = PD2 (INT0),
  D- = PD3** (`usbconfig.h`). If your board differs, fix `USB_CFG_DPLUS_BIT`
  / `USB_CFG_DMINUS_BIT`.
- **`bootloadHID` can't find the device** — the board isn't in the
  bootloader. Repeat the K11-held plug-in.
- **Backlight looks inverted** — the LED is wired active-low; see the note
  in `leds.c`.

## Licensing / credits

The V-USB library (`usbdrv/`) is © OBJECTIVE DEVELOPMENT Software GmbH,
GNU GPL v2 (see `usbdrv/License.txt`). The USB-MIDI descriptor in `midi.c`
follows the USB MIDI 1.0 spec (Appendix B) and the V-USB MIDI example by
Martin Homuth-Rosemann (GPL v2). This firmware is therefore GPL v2.
