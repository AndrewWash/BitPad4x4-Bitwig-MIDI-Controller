# JJ4x4 → Bitwig USB-MIDI Controller

Custom firmware that turns a **KPrepublic JJ4x4** macropad (ATmega32A, 16 keys)
into a class-compliant **USB-MIDI controller** for **Bitwig Studio** — a Drum
Machine controller with a second device-navigation mode.

The ATmega32A has no USB hardware, so USB is bit-banged in software with the
**V-USB** library. (QMK's MIDI feature does not work on V-USB boards — this is
standalone firmware, not a QMK keymap.)

## Layout

```
firmware/     AVR firmware (V-USB USB-MIDI) — build with avr-gcc, flash with bootloadHID
bitwig/       Bitwig Studio JavaScript controller script
```

## Getting started

1. **Firmware** — see [`firmware/README.md`](firmware/README.md) for the
   toolchain, build (`make`), and flashing (`make flash`, hold key K11 on
   plug-in to enter the bootloader).
2. **Bitwig script** — copy `bitwig/JJ4x4/` into
   `Documents\Bitwig Studio\Controller Scripts\`, then add it in Bitwig under
   *Settings → Controllers* and select the **JJ4x4 MIDI** input port.

## How it works

- **Drum mode** (default): the 16 keys are drum pads (Note On/Off, notes
  36–51) routed straight into the selected Drum Machine.
- **Device mode**: 7 keys send CC commands the controller script turns into
  track / device / parameter-page navigation and "insert device at end of
  chain".
- **Chords** (corner keys): `0+15` flips mode, `0+3` / `12+15` page the Drum
  Machine pad pages.

Full command map and CC numbers are in [`firmware/README.md`](firmware/README.md).

The original design plan is in
`~/.claude/plans/i-want-to-write-melodic-trinket.md`.
