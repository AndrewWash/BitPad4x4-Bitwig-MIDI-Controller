// jj4x4 — Bitwig Studio controller script for the JJ4x4 USB-MIDI controller,
// a KPrepublic JJ4x4 macropad (16 keys) running custom V-USB USB-MIDI firmware.
//
// The firmware sends:
//   - Note On/Off   : drum pads (notes 36..51) — routed to the selected
//                     instrument / Drum Machine via a NoteInput whose
//                     key-translation table tracks the drum bank scroll.
//   - Control Change: navigation commands — handled in onMidi(). The CC
//                     numbers MUST match firmware/modes.c.
//
// Three firmware modes (cycle with the 0+15 chord on the pad):
//   - Drum mode    : pads play; chords scroll the drum bank.
//   - Device mode  : 16 keys = device + transport navigation CCs.
//   - Clip-nav mode: 16 keys = clip-launcher selection, deterministic
//                    play/stop/record/new, plus a state-aware looper key.
//
// CCs are SHARED across modes for shared actions (e.g. Global Play is the
// same CC whether sent from device-mode key 0 or clip-nav-mode key 0). The
// script dispatches purely by CC and doesn't care which mode produced it.
//
// Install: copy this folder into your Bitwig "Controller scripts" directory,
// then add it in Bitwig under Settings > Controllers and select the
// "JJ4x4 MIDI" input port.
//
// Credits
//   Hardware vendor : learninglab
//   Author          : wash

loadAPI(18);

host.defineController(
    "learninglab",
    "jj4x4",
    "2.0",
    "6b1d9c20-4a3e-4f51-9c2a-1d9c204a3e4f",
    "wash"
);
host.defineMidiPorts(1, 0);                   /* 1 MIDI in, 0 out */

/* Best-effort auto-detection by MIDI port name. Windows may name the port
 * differently; if so just pick it manually in the controller settings. */
host.addDeviceNameBasedDiscoveryPair(["JJ4x4 MIDI"], []);

/* ---- CC numbers — keep in sync with firmware/modes.c ---- */
/* Drum-mode nav (CH 1) */
const CC_PAGE_UP        = 16;
const CC_PAGE_DOWN      = 17;
const CC_ROW_UP         = 18;
const CC_ROW_DOWN       = 19;
/* Device + clip-nav shared device/track nav */
const CC_TRACK_UP       = 20;
const CC_TRACK_DOWN     = 21;
const CC_PREV_PARAM     = 22;
const CC_NEXT_PARAM     = 23;
const CC_PREV_DEVICE    = 24;
const CC_NEXT_DEVICE    = 25;
const CC_INSERT_DEV     = 26;
const CC_DEV_EXPAND     = 27;
const CC_DEV_REMOTES    = 28;
/* CC 29 was CC_DEV_NESTED — reserved, unused. */
const CC_DEV_WINDOW     = 30;
/* Shared transport (device + clip-nav modes) */
const CC_PLAY           = 31;
const CC_STOP           = 32;
const CC_REC            = 33;
const CC_UNDO           = 34;
const CC_ARM            = 35;
const CC_OVERDUB        = 36;
/* Clip-nav-only. Keys 1/5 in clip-nav now reuse CC_TRACK_UP/DOWN above
 * (same action as device-mode 1/5), so CCs 37 and 38 are unused. */
const CC_CLIP_LEFT      = 39;   /* clip-nav key 4: focus prev slot */
const CC_CLIP_RIGHT     = 40;   /* clip-nav key 6: focus next slot */
const CC_CLIP_RESERVED  = 41;   /* clip-nav key 10 — unused for now */
const CC_LOOPER         = 42;
const CC_CLIP_PLAY      = 43;
const CC_CLIP_STOP      = 44;
const CC_CLIP_REC       = 45;
const CC_CLIP_NEW       = 46;

/* The 16 drum pads always arrive as notes 36..51 (DRUM_LO..DRUM_HI). They are
 * remapped so firmware note 36 lands on the drum bank's scroll position — the
 * absolute MIDI note of the first visible pad — and the pads play whatever is
 * under the green highlight (one page = 16 pads, one row = 4). */
const DRUM_LO = 36;
const DRUM_HI = 51;

/* Number of scenes the clip-nav slot bank tracks. Keeps clip-nav focus
 * navigation within a reasonable window of the project. */
const CLIP_SLOTS = 16;

let transport, application;
let cursorTrack, cursorDevice, remotePages, drumBank, endInsertion, noteInput;
let clipSlotBank, focusedSlotIndex;

function init() {
    const midiIn = host.getMidiInPort(0);
    midiIn.setMidiCallback(onMidi);

    /* Note On/Off pass through to the selected instrument so the 16 pads
     * play the Drum Machine natively. The masks below match note-off (0x8n)
     * and note-on (0x9n) on any channel; CC messages are NOT matched, so
     * they still reach onMidi(). A key-translation table (rebuilt whenever
     * the drum bank scrolls) shifts the pad notes so they always play the
     * pads under the green highlight. */
    noteInput = midiIn.createNoteInput("JJ4x4 Pads", "8?????", "9?????");

    transport   = host.createTransport();
    application = host.createApplication();

    /* A cursor track that follows the selected track in Bitwig. */
    cursorTrack = host.createCursorTrack("JJ4X4_TRACK", "jj4x4", 0, CLIP_SLOTS, true);

    /* Arm is a per-track toggle we drive in device/clip-nav modes. */
    cursorTrack.arm().markInterested();

    /* Launcher overdub toggle (used by both the dedicated overdub key and
     * the state-aware looper). */
    transport.isClipLauncherOverdubEnabled().markInterested();

    /* The selected device on that track, its 8 remote-control (parameter)
     * pages, and a 16-pad drum bank for page/row navigation. */
    cursorDevice = cursorTrack.createCursorDevice();
    remotePages  = cursorDevice.createCursorRemoteControlsPage(8);
    drumBank     = cursorDevice.createDrumPadBank(16);

    /* Toggling these needs the API to know their current value. */
    cursorDevice.isExpanded().markInterested();
    cursorDevice.isRemoteControlsSectionVisible().markInterested();
    cursorDevice.isWindowOpen().markInterested();

    /* Re-map the pad notes whenever the drum bank scrolls. Adding the
     * observer also marks the scroll position interested. */
    drumBank.scrollPosition().addValueObserver(updateKeyTranslation);
    updateKeyTranslation(0);

    /* Insertion point at the end of the selected track's device chain. */
    endInsertion = cursorTrack.endOfDeviceChainInsertionPoint();

    /* Clip-nav: a small slot-bank window on the cursor track lets us track
     * which slot is "focused" (the one the play/stop/record/new/looper keys
     * act on). Up/down move the focus within the bank; left/right move the
     * cursor track. */
    clipSlotBank     = cursorTrack.clipLauncherSlotBank();
    focusedSlotIndex = 0;

    /* Observe per-slot state so the looper key can decide what to do
     * (empty -> arm/record, recording -> play, playing -> overdub toggle). */
    for (let i = 0; i < CLIP_SLOTS; i++) {
        const slot = clipSlotBank.getItemAt(i);
        slot.hasContent().markInterested();
        slot.isRecording().markInterested();
        slot.isPlaying().markInterested();
    }

    host.showPopupNotification("jj4x4 MIDI Controller ready");
}

/* Rebuild the 128-entry key-translation table so the pad notes 36..51 are
 * remapped onto the drum bank's pads: firmware note DRUM_LO (36) maps to the
 * bank's current scroll position (the absolute MIDI note of the first visible
 * pad), and the rest follow. Notes are passed through unchanged outside that
 * range, and clamped to 0..127. */
function updateKeyTranslation(offset) {
    const table = [];
    for (let i = 0; i < 128; i++) {
        if (i >= DRUM_LO && i <= DRUM_HI) {
            let n = i + offset - DRUM_LO;   /* map note 36 -> scrollPosition */
            if (n < 0)        n = 0;
            else if (n > 127) n = 127;
            table[i] = n;
        } else {
            table[i] = i;
        }
    }
    noteInput.setKeyTranslationTable(table);
}

/* Move the clip-nav focus up/down within the slot bank, clamped to the
 * tracked window. The script tracks this index itself because the API does
 * not surface a single "focused launcher slot" on the cursor track. */
function moveFocus(delta) {
    let next = focusedSlotIndex + delta;
    if (next < 0)             next = 0;
    if (next >= CLIP_SLOTS)   next = CLIP_SLOTS - 1;
    focusedSlotIndex = next;
    clipSlotBank.getItemAt(focusedSlotIndex).select();
}

function focusedSlot() {
    return clipSlotBank.getItemAt(focusedSlotIndex);
}

/* State-aware looper: one key cycles arm -> rec -> play -> overdub on the
 * focused clip slot. State machine lives entirely in the script so the
 * firmware just sends a momentary CC. */
function looperPress() {
    const slot = focusedSlot();
    if (slot.isRecording().get()) {
        /* Recording: stop recording, start playback. */
        slot.launch();
    } else if (!slot.hasContent().get()) {
        /* Empty: arm + record into this slot. */
        slot.launch();
    } else if (slot.isPlaying().get()) {
        /* Playing: toggle overdub (turn on if off, off if on). */
        transport.isClipLauncherOverdubEnabled().toggle();
    } else {
        /* Stopped clip with content: launch it (play). */
        slot.launch();
    }
}

function onMidi(status, data1, data2) {
    /* Only CC messages are dispatched here; notes are handled by NoteInput. */
    if ((status & 0xF0) != 0xB0) return;

    /* The firmware sends momentary CCs with value 127 on press. Ignore any
     * value-0 messages so each command fires exactly once. */
    if (data2 == 0) return;

    switch (data1) {
    /* Drum-mode pad-bank nav */
    case CC_PAGE_UP:        drumBank.scrollPosition().inc(16);    break;
    case CC_PAGE_DOWN:      drumBank.scrollPosition().inc(-16);   break;
    case CC_ROW_UP:         drumBank.scrollPosition().inc(4);     break;
    case CC_ROW_DOWN:       drumBank.scrollPosition().inc(-4);    break;

    /* Track + device + remote page nav */
    case CC_TRACK_UP:       cursorTrack.selectPrevious();         break;
    case CC_TRACK_DOWN:     cursorTrack.selectNext();             break;
    case CC_PREV_PARAM:     remotePages.selectPreviousPage(true); break;
    case CC_NEXT_PARAM:     remotePages.selectNextPage(true);     break;
    case CC_PREV_DEVICE:    cursorDevice.selectPrevious();        break;
    case CC_NEXT_DEVICE:    cursorDevice.selectNext();            break;
    case CC_INSERT_DEV:     endInsertion.browse();                break;
    case CC_DEV_EXPAND:     cursorDevice.isExpanded().toggle();   break;
    case CC_DEV_REMOTES:    cursorDevice.isRemoteControlsSectionVisible().toggle(); break;
    case CC_DEV_WINDOW:     cursorDevice.isWindowOpen().toggle(); break;

    /* Shared transport */
    case CC_PLAY:           transport.play();                     break;
    case CC_STOP:           transport.stop();                     break;
    case CC_REC:            transport.record();                   break;
    case CC_UNDO:           application.undo();                   break;
    case CC_ARM:            cursorTrack.arm().toggle();           break;
    case CC_OVERDUB:        transport.isClipLauncherOverdubEnabled().toggle(); break;

    /* Clip-nav: clip focus movement within the cursor track's slot bank.
     * (Track up/down on clip-nav keys 1/5 is handled by CC_TRACK_UP/DOWN
     * above — same CCs as device mode.) */
    case CC_CLIP_LEFT:      moveFocus(-1);                        break;
    case CC_CLIP_RIGHT:     moveFocus(1);                         break;

    /* Clip-nav: direct slot actions on the focused slot */
    case CC_CLIP_PLAY:      focusedSlot().launch();               break;
    case CC_CLIP_STOP:      cursorTrack.stop();                   break;
    case CC_CLIP_REC:       focusedSlot().record();               break;
    case CC_CLIP_NEW:       focusedSlot().createEmptyClip(4);     break;

    /* Clip-nav: state-aware looper */
    case CC_LOOPER:         looperPress();                        break;

    /* Reserved — assigned in firmware so future use is script-only. */
    case CC_CLIP_RESERVED:  /* intentional no-op */               break;

    default: break;
    }
}

function flush() {
    /* Nothing to send back to the device (no LEDs over MIDI). */
}

function exit() {
}
