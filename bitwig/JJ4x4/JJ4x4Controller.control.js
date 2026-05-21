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
// Install: copy this folder into your Bitwig "Controller scripts" directory,
// then add it in Bitwig under Settings > Controllers and select the
// "JJ4x4 MIDI" input port.
//
// Credits
//   Hardware vendor : wash
//   Author          : wash

loadAPI(18);

host.defineController(
    "wash",
    "jj4x4",
    "1.0",
    "6b1d9c20-4a3e-4f51-9c2a-1d9c204a3e4f",
    "wash"
);
host.defineMidiPorts(1, 0);                   /* 1 MIDI in, 0 out */

/* Best-effort auto-detection by MIDI port name. Windows may name the port
 * differently; if so just pick it manually in the controller settings. */
host.addDeviceNameBasedDiscoveryPair(["JJ4x4 MIDI"], []);

/* ---- CC numbers — keep in sync with firmware/modes.c ---- */
const CC_PAGE_UP      = 16;
const CC_PAGE_DOWN    = 17;
const CC_ROW_UP       = 18;
const CC_ROW_DOWN     = 19;
const CC_TRACK_UP     = 20;
const CC_TRACK_DOWN   = 21;
const CC_PREV_PARAM   = 22;
const CC_NEXT_PARAM   = 23;
const CC_PREV_DEVICE  = 24;
const CC_NEXT_DEVICE  = 25;
const CC_INSERT_DEV   = 26;
const CC_DEV_EXPAND   = 27;
const CC_DEV_REMOTES  = 28;
const CC_DEV_NESTED   = 29;
const CC_DEV_WINDOW   = 30;

/* The 16 drum pads always arrive as notes 36..51 (DRUM_LO..DRUM_HI). They are
 * remapped so firmware note 36 lands on the drum bank's scroll position — the
 * absolute MIDI note of the first visible pad — and the pads play whatever is
 * under the green highlight (one page = 16 pads, one row = 4). */
const DRUM_LO = 36;
const DRUM_HI = 51;

let cursorTrack, cursorDevice, remotePages, drumBank, endInsertion, noteInput;

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

    /* A cursor track that follows the selected track in Bitwig. */
    cursorTrack = host.createCursorTrack("JJ4X4_TRACK", "jj4x4", 0, 0, true);

    /* The selected device on that track, its 8 remote-control (parameter)
     * pages, and a 16-pad drum bank for page/row navigation. */
    cursorDevice = cursorTrack.createCursorDevice();
    remotePages  = cursorDevice.createCursorRemoteControlsPage(8);
    drumBank     = cursorDevice.createDrumPadBank(16);

    /* Toggling these needs the API to know their current value. */
    cursorDevice.isExpanded().markInterested();
    cursorDevice.isRemoteControlsSectionVisible().markInterested();
    cursorDevice.isNestedDeviceChainExpanded().markInterested();
    cursorDevice.isWindowOpen().markInterested();

    /* Re-map the pad notes whenever the drum bank scrolls. Adding the
     * observer also marks the scroll position interested. */
    drumBank.scrollPosition().addValueObserver(updateKeyTranslation);
    updateKeyTranslation(0);

    /* Insertion point at the end of the selected track's device chain. */
    endInsertion = cursorTrack.endOfDeviceChainInsertionPoint();

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

function onMidi(status, data1, data2) {
    /* Only CC messages are dispatched here; notes are handled by NoteInput. */
    if ((status & 0xF0) != 0xB0) return;

    /* The firmware sends momentary CCs with value 127 on press. Ignore any
     * value-0 messages so each command fires exactly once. */
    if (data2 == 0) return;

    switch (data1) {
    case CC_PAGE_UP:     drumBank.scrollPosition().inc(16);    break;
    case CC_PAGE_DOWN:   drumBank.scrollPosition().inc(-16);   break;
    case CC_ROW_UP:      drumBank.scrollPosition().inc(4);     break;
    case CC_ROW_DOWN:    drumBank.scrollPosition().inc(-4);    break;
    case CC_TRACK_UP:    cursorTrack.selectPrevious();         break;
    case CC_TRACK_DOWN:  cursorTrack.selectNext();             break;
    case CC_PREV_PARAM:  remotePages.selectPreviousPage(true); break;
    case CC_NEXT_PARAM:  remotePages.selectNextPage(true);     break;
    case CC_PREV_DEVICE: cursorDevice.selectPrevious();        break;
    case CC_NEXT_DEVICE: cursorDevice.selectNext();            break;
    case CC_INSERT_DEV:  endInsertion.browse();                break;
    case CC_DEV_EXPAND:  cursorDevice.isExpanded().toggle();   break;
    case CC_DEV_REMOTES: cursorDevice.isRemoteControlsSectionVisible().toggle(); break;
    case CC_DEV_NESTED:  cursorDevice.isNestedDeviceChainExpanded().toggle();    break;
    case CC_DEV_WINDOW:  cursorDevice.isWindowOpen().toggle();                   break;
    default: break;
    }
}

function flush() {
    /* Nothing to send back to the device (no LEDs over MIDI). */
}

function exit() {
}
