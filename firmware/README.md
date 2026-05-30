# JJ4x4 USB-MIDI Controller — Firmware

Custom firmware that turns a **KPrepublic JJ4x4** macropad (ATmega32A, 16 keys)
into a class-compliant **USB-MIDI controller** for Bitwig Studio.

The ATmega32A has no USB hardware, so USB is done in software with the
**V-USB** library. QMK's MIDI feature does not support V-USB boards — this is
standalone firmware, not a QMK keymap.

## What it does

Three modes, cycled by a chord:

| | Drum mode (default) | Device mode | Clip-nav mode |
|---|---|---|---|
| 16 keys | Drum pads — Note On/Off, notes 36–51, MIDI ch 1 | 16 navigation commands, MIDI ch 2 | 16 clip-launcher commands incl. state-aware looper, MIDI ch 3 |
| Backlight | dim (40) | mid (160) | bright (255) |
| Underglow | blue | green | magenta |

The pad layout is aligned to the on-screen Drum Machine grid: the macropad's
top row plays the Drum Machine's top row. Drum notes follow the bank's scroll
position (set by the Bitwig script), so paging/rowing changes which pads play.

**Corner-key chords** — the chord family that uses only the four corner
keys (0 / 3 / 12 / 15). Additional non-corner chord families (`1 + X`,
`5 + X`, `1 + 10` for device delete) are described in the sections that
follow.

| Keys | Action |
|---|---|
| 0 + 15 | Flip to the next mode (cycles drum > device > clip-nav) |
| 3 + 12 | Cycle underglow brightness (5 levels, any mode) |
| 0 + 3  | Drum-nav modifier (drum mode only) — see below |
| 12 + 15 | Drum page down — `CC 17` (drum mode only) |
| 3 + 15 | Toggle drum-mode quick-flip macros (drum mode only) — see below |

**Drum-nav modifier** — hold `0 + 3`, then:

| While 0+3 held | Action |
|---|---|
| (release with no sub-key) | Drum page up — `CC 16` |
| tap key 1 | Scroll one row up — `CC 18` |
| tap key 5 | Scroll one row down — `CC 19` |

**Drum-mode quick-flip macros (`1 + X` and `5 + X`)** — in drum mode, press
key 1 *or* key 5 together with a target key (within the ~30 ms chord
window) to fire a transport / clip / scene command *instead of* the two
drum pads. Lets you record clips, run transport, and launch scenes
without leaving the drum page. CCs go out on **MIDI channel 3 (clip-nav)**
so the existing Bitwig script handlers run unchanged.

Default state is **enabled** (underglow stays blue). The `3 + 15` chord
toggles the entire feature off (both `1+X` macros AND `5+X` scenes);
underglow flips to **amber** as the off indicator. While disabled, the
chord shortcuts go away and every key plays its plain drum note (current
pre-feature behavior), so drumming patterns that include keys 1 or 5 work
without surprises. Power-up always starts enabled. Armed state persists
across mode flips.

| `1 + X` chord (when enabled) | Action | CC on MIDI ch 3 |
|---|---|---|
| 1 + 0  | Global play | 31 |
| 1 + 2  | Global stop | 32 |
| 1 + 3  | Global record toggle | 33 |
| 1 + 4  | Clip selection left | 39 |
| 1 + 6  | Clip selection right | 40 |
| 1 + 7  | Global undo | 34 |
| 1 + 8  | Toggle arm (cursor track) | 35 |
| 1 + 9  | Toggle clip-launcher overdub | 36 |
| 1 + 10 | Delete focused clip | 41 |
| 1 + 11 | State-aware looper | 42 |
| 1 + 12 | Play focused clip | 43 |
| 1 + 13 | Stop focused clip / track | 44 |
| 1 + 14 | Record into focused clip | 45 |
| 1 + 15 | Create new (empty) clip | 46 |

**Scene launcher (`5 + X`)** — works in **both drum mode and clip-nav
mode**. In drum mode it's gated by the `3 + 15` toggle along with the
`1 + X` macros. In clip-nav mode it's **always on** (no toggle there) —
the trade-off is that keys 5 and 8–15 in clip-nav now incur the ~30 ms
chord hold, but quick taps still fire on release so clip launching feels
unchanged.

| `5 + X` chord | Action | CC on MIDI ch 3 |
|---|---|---|
| 5 + 8  | Launch scene 1 | 60 |
| 5 + 9  | Launch scene 2 | 61 |
| 5 + 10 | Launch scene 3 | 62 |
| 5 + 11 | Launch scene 4 | 63 |
| 5 + 12 | Launch scene 5 | 64 |
| 5 + 13 | Launch scene 6 | 65 |
| 5 + 14 | Launch scene 7 | 66 |
| 5 + 15 | Launch scene 8 | 67 |

**Device-mode chord** — one chord lives on the device page: press key 1
together with key 10 within the chord window to delete the currently
selected device. Solo behavior is unchanged (key 1 = Track up, key 10 =
Show / hide device window).

| Chord    | Action                  | CC on MIDI ch 2 |
|----------|-------------------------|-----------------|
| 1 + 10   | Delete current device   | 29              |

**Device-mode key map** (momentary `CC = 127` on press, MIDI ch 2):

```
 0 play          1 trackUp       2 stop          3 record
 4 prevDevice    5 trackDown     6 nextDevice    7 undo
 8 arm           9 overdub      10 devWindow    11 insertDev
12 prevParam    13 devRemotes   14 devExpand    15 nextParam
```

| Key | Command | CC |
|---|---|---|
| 0 | Global play | 31 |
| 1 | Track up | 20 |
| 2 | Global stop | 32 |
| 3 | Global record toggle | 33 |
| 4 | Previous device | 24 |
| 5 | Track down | 21 |
| 6 | Next device | 25 |
| 7 | Global undo | 34 |
| 8 | Toggle arm (cursor track) | 35 |
| 9 | Toggle clip-launcher overdub | 36 |
| 10 | Show / hide device window (expanded device view) | 30 |
| 11 | Add device after (open browser at end of chain) | 26 |
| 12 | Previous remote (parameter) page | 22 |
| 13 | Show / hide remote controls section | 28 |
| 14 | Collapse / expand device | 27 |
| 15 | Next remote (parameter) page | 23 |

**Clip-nav-mode key map** (momentary `CC = 127` on press, MIDI ch 3):

```
 0 play          1 trackUp       2 stop          3 record
 4 clipLeft      5 trackDown     6 clipRight     7 undo
 8 arm           9 overdub      10 clipDelete   11 looper
12 clipPlay     13 clipStop     14 clipRec      15 clipNew
```

Plus the **5+X scene launcher** (always on in clip-nav, see Drum-mode
quick-flip section above): hold key 5 and tap one of keys 8..15 to launch
scenes 1..8 (CCs 60..67).

| Key | Command | CC |
|---|---|---|
| 0 | Global play | 31 |
| 1 | Track up (shared with device mode) | 20 |
| 2 | Global stop | 32 |
| 3 | Global record toggle | 33 |
| 4 | Clip selection left (prev focused slot) | 39 |
| 5 | Track down (shared with device mode) | 21 |
| 6 | Clip selection right (next focused slot) | 40 |
| 7 | Global undo | 34 |
| 8 | Toggle arm (cursor track) | 35 |
| 9 | Toggle clip-launcher overdub | 36 |
| 10 | Delete focused clip | 41 |
| 11 | **Looper** (state-aware: arm > rec > play > overdub) | 42 |
| 12 | Play focused clip | 43 |
| 13 | Stop focused clip / track | 44 |
| 14 | Record into focused clip | 45 |
| 15 | Create new (empty) clip in focused slot | 46 |

Shared CCs (Global play / stop / record / undo / arm / overdub) are
identical between device mode and clip-nav mode. The script dispatches
purely by CC, so the same action is triggered regardless of which mode
produced it. All 16 keys in device and clip-nav modes are CC-assigned.

**Chord-window latency.** Corner keys (0/3/12/15) are always delayed
~30 ms so they can take part in chord detection; the other keys fire
instantly *except* in two cases where they also need to participate in
chord-press macros:

- **Drum mode while armed** — every key except 5 picks up a chord
  window so `1 + X` and `5 + X` chords are detectable. The 3+15 toggle
  removes the delay entirely.
- **Clip-nav mode** — keys 5 and 8..15 always carry the chord window
  for `5 + X` scene chords. No toggle here; scenes are always on.
- **Device mode** — keys 1 and 10 carry the chord window for the
  `1 + 10` delete-device chord. Other keys fire instantly.

In every case, a quick tap still fires *on release* (well inside 30 ms),
so the delay is only perceptible when a key is held alone past the
window.

## Files

```
firmware/
  main.c        main loop (usbPoll + 1 ms tick)
  usbconfig.h   V-USB configuration (ATmega32A, 12 MHz, port D, INT0)
  board.h       pin map (from QMK keyboards/kprepublic/jj4x4/keyboard.json)
  midi.c/.h     USB-MIDI descriptors + 4-byte-packet send queue
  matrix.c/.h   4x4 COL2ROW scan + debounce
  modes.c/.h   mode state machine + chord detection + key->MIDI map
  leds.c/.h     PD4 backlight feedback (mode brightness)
  underglow.c/.h  4 WS2812 RGB underglow over I2C->ATtiny85 (PC0/PC1)
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
   video and game controllers*: a device named **BitPad4x4 MIDI** appears with
   no yellow `!`. A `!` means a USB descriptor or V-USB timing problem.
2. **Raw MIDI.** Install **MIDI-OX** (free). Open the *BitPad4x4 MIDI* input
   and its monitor window. Press each key — you should see Note On/Off
   (drum mode) with no chatter or stuck notes. Test the chords:
   - `0+15` flip (cycles backlight brightness *and* underglow color),
   - `12+15` page down (`CC 17`),
   - `0+3` held then released with no sub-key → page up (`CC 16`),
   - `0+3` held + tap `1` → row up (`CC 18`),
   - `0+3` held + tap `5` → row down (`CC 19`),
   - `3+15` → underglow toggles blue ↔ amber (drum-mode quick-flip
     enable/disable),
   - Armed (blue) `1 + X` chords → each fires its CC on channel 3
     (see the `1 + X` table above),
   - Armed `5 + X` chords → CCs 60..67 on channel 3,
   - Disarmed (amber): `1 + X` and `5 + X` play both keys as plain drum
     pads — no CCs on channel 3.
   In device + clip-nav modes, confirm every key sends its assigned CC.
   In clip-nav, `5 + X` chords fire scenes regardless of any toggle.
   In device mode: tap key 1 → `CC 20` (track up); tap key 10 → `CC 30`
   (device window); press 1+10 within the chord window → `CC 29` on
   channel 2 (delete current device).
3. **Underglow.** On boot the 4 underglow LEDs light **blue** at medium
   brightness. `0+15` should cycle the color blue → green → magenta →
   blue in lock-step with the backlight. `3+12` (opposite diagonal)
   should step the underglow brightness through 5 levels and wrap,
   without changing the color and without emitting MIDI from key 3 or
   key 12.
4. **Bitwig.** Install the controller script (see `../bitwig/`), load a
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
- **Underglow stays dark, backlight works** — the ATtiny85 isn't ACKing
  the I²C write. Check: (a) the board variant actually has the 4
  underglow LEDs populated (some JJ4x4 batches ship without them);
  (b) try raising `TWBR` in `underglow.c` (e.g. `32` ≈ 170 kHz) in case
  the ATtiny firmware is slow; (c) confirm nothing else is driving
  PC0/PC1. Failure is silent by design — USB and backlight keep working.

## Licensing / credits

The V-USB library (`usbdrv/`) is © OBJECTIVE DEVELOPMENT Software GmbH,
GNU GPL v2 (see `usbdrv/License.txt`). The USB-MIDI descriptor in `midi.c`
follows the USB MIDI 1.0 spec (Appendix B) and the V-USB MIDI example by
Martin Homuth-Rosemann (GPL v2). This firmware is therefore GPL v2.
