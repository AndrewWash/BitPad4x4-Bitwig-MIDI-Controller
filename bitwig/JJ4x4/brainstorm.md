# JJ4x4 — Pages & Macros Brainstorm

Working picture: 16 keys, corner keys (0/3/12/15) reserved for chords, `0+15`
cycles modes, firmware sends Notes (drum/chromatic) + momentary CCs, and the
Bitwig API exposes far more than the current two pages use.

Layout reference:
```
 0   1   2   3
 4   5   6   7
 8   9  10  11
12  13  14  15
```

## New page ideas

### 1. Clip Launcher page (the big one)
The natural twin of the Drum page. The drum page scrolls a 16-pad `drumBank`
window over the kit; this scrolls a **4x4 window over the session grid** —
4 tracks across, 4 scenes down, mirroring what's on screen.
```
 T1S1 T2S1 T3S1 T4S1
 T1S2 T2S2 T3S2 T4S2
 T1S3 T2S3 T3S3 T4S3
 T1S4 T2S4 T3S4 T4S4
```
Press = launch that slot. Reuse the existing page/row chords to scroll the
window. API: `host.createTrackBank(4, 0, 4)` →
`track.clipLauncherSlotBank().getItemAt(n).launch()`. LED-less for now, but
`isPlaying()` observers exist if per-key LEDs are added later.

### 2. Transport / Record page
```
play  stop  rec   loop
ovdub metro tap   precount
undo  redo  punchIn punchOut
autoWr  <<   >>   PANIC
```
`host.createTransport()` gives `play()`, `stop()`, `record()`,
`isArrangerLoopEnabled().toggle()`, `isMetronomeEnabled().toggle()`,
`tapTempo()`, `isArrangerOverdubEnabled()`, `isClipLauncherOverdubEnabled()`,
`isArrangerAutomationWriteEnabled()`, `application.undo()/redo()`.
PANIC = stop + all-notes-off.

### 3. Mixer page (4 tracks)
```
sel1  sel2  sel3  sel4
mute1 mute2 mute3 mute4
solo1 solo2 solo3 solo4
arm1  arm2  arm3  arm4
```
One row per function, one column per track. Chords scroll the track bank by 4.
`trackBank.getItemAt(i).mute()/solo()/arm()/selectInEditor()`.

### 4. Scene launcher page
Dead simple: 16 keys = 16 scenes, press launches. Corner chords scroll the
scene bank +/-16. `host.createSceneBank(16)`.

### 5. Performance / FX-shot page (momentary)
Keys become **momentary FX hits** — press jams a remote-control parameter to a
value, release restores it. Filter sweep, beat-repeat stutter, reverb throw,
gate. Needs the firmware to send CC 127 on press *and* CC 0 on release (the
script currently drops value-0 in `onMidi` — keep them for this page).
`remotePages.getParameter(i).value().set(...)`.

### 6. Step-sequencer page (TR-style)
16 keys = 16 steps of the selected drum pad's launcher clip. Press toggles a
step on/off. Chords: pick the active pad lane, clear lane, page if clip > 16
steps. `cursorTrack.createLauncherCursorClip(16,1)` →
`clip.toggleStep(x, y, velocity)`. Turns the JJ4x4 into a drum sequencer, not
just a play surface.

### 7. Browser / preset page
Next/prev device preset, open browser, confirm/cancel, filter columns.
`host.createPopupBrowser()` + `cursorDevice` preset navigation. Fast
sound-hunting without the mouse.

### 8. Project nav / arrange page
Track up/down, scene up/down, zoom in/out, toggle arranger/clip focus, follow
playhead, jump to next/prev cue marker.

## Macro / interaction ideas (multiply capacity without more pages)

- **Shift layer.** Hold one key (say key 4) and every *other* key fires its alt
  CC. Instantly doubles every page's vocabulary with zero new modes. Cheap in
  firmware — one held-key check in `modes.c`.
- **Direct page jump chords.** Cycling with `0+15` gets tedious past 3 modes.
  Add `0+3` → page 1, `3+15` → page 2, `12+15` → page 3, etc.
- **Long-press vs tap.** Same key, two actions (tap = mute, hold = solo). The
  30 ms chord timer already exists; a press-duration timer is the same pattern.
- **Double-tap.** Tap-tap a key for a destructive/confirm action (clear clip,
  delete).
- **Global chords that work on every page.** `0+12` = tap tempo anywhere;
  `3+15` = stop/panic anywhere. Reserve a couple of corner combos as always-on.
- **Velocity layers** for the drum page via a held modifier — soft/normal/accent.

## Firmware enablers worth doing first

- **Mode indicator.** With >2 pages, dim/bright backlight isn't enough — blink
  the LED **N times** on mode switch to announce the page number. Small change
  in `leds.c`/`modes.c`.
- **Send CC-0 on release** (page-dependent) so momentary FX/gate pages work.
- Consider **Program Change** messages for page state instead of cycling CCs —
  cleaner for the script to track which page is active.

## "Skies the limit" wild ones

- **Looper page** — drive Bitwig clip recording as a live looper:
  arm-record-overdub on one key with state-aware behavior.
- **Live-set page** — scene launch + transport + a "next song" key to perform a
  whole set off the pad.
- **Note FX page** — toggle arpeggiator/note-repeat rates for chromatic mode
  (1/4, 1/8, 1/16, triplets on 4 keys).
- **Snapshot page** — capture/recall remote-control states as scene-like presets.

## Highest payoff-to-effort picks

- **Clip Launcher page** — reuses the existing scroll pattern almost verbatim.
- **Shift layer** — doubles everything that already exists.
