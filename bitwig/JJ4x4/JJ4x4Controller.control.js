/* JJ4x4Controller.control.js
 *
 * Bitwig Studio controller script for the JJ4x4 USB-MIDI controller.
 *
 * The firmware sends:
 *   - Note On/Off  : drum pads (notes 36..51) - routed straight to the
 *                    selected instrument / Drum Machine by a NoteInput.
 *   - Control Change: navigation commands - handled below. The CC numbers
 *                    MUST match firmware/modes.c.
 *
 * Install: copy the JJ4x4 folder into
 *   Documents\Bitwig Studio\Controller Scripts\
 * then add it in Bitwig: Settings > Controllers > Add Controller, and
 * select the "JJ4x4 MIDI" input port.
 *
 * If Bitwig refuses to load the script, your Bitwig may be older than API
 * version 17 - lower the number in loadAPI() below to match.
 */
loadAPI(17);

host.defineController(
    "DIY",                                    /* vendor   */
    "JJ4x4 MIDI Controller",                  /* name     */
    "1.0",                                    /* version  */
    "6b1d9c20-4a3e-4f51-9c2a-jj4x4midi0001",  /* unique UUID */
    "JJ4x4");                                 /* author   */

host.defineMidiPorts(1, 0);                   /* 1 MIDI in, 0 out */

/* Best-effort auto-detection by MIDI port name. Windows may name the port
 * differently; if so just pick it manually in the controller settings. */
host.addDeviceNameBasedDiscoveryPair(["JJ4x4 MIDI"], []);

/* ---- CC numbers - keep in sync with firmware/modes.c ---- */
var CC_PAGE_UP      = 16;
var CC_PAGE_DOWN    = 17;
var CC_TRACK_UP     = 20;
var CC_TRACK_DOWN   = 21;
var CC_PREV_PARAM   = 22;
var CC_NEXT_PARAM   = 23;
var CC_PREV_DEVICE  = 24;
var CC_NEXT_DEVICE  = 25;
var CC_INSERT_DEV   = 26;

var cursorTrack, cursorDevice, remotePages, drumBank, endInsertion;

function init() {
    var midiIn = host.getMidiInPort(0);
    midiIn.setMidiCallback(onMidi);

    /* Note On/Off pass straight through to the selected instrument, so the
     * 16 pads play the Drum Machine natively. The masks below match note-off
     * (0x8n) and note-on (0x9n) on any channel; CC messages are NOT matched,
     * so they still reach onMidi(). */
    midiIn.createNoteInput("JJ4x4 Pads", "8?????", "9?????");

    /* A cursor track that follows the selected track in Bitwig. */
    cursorTrack = host.createCursorTrack("JJ4X4_TRACK", "JJ4x4", 0, 0, true);

    /* The selected device on that track, its 8 remote-control (parameter)
     * pages, and a 16-pad drum bank for page navigation. */
    cursorDevice = cursorTrack.createCursorDevice();
    remotePages  = cursorDevice.createCursorRemoteControlsPage(8);
    drumBank     = cursorDevice.createDrumPadBank(16);

    /* Insertion point at the end of the selected track's device chain. */
    endInsertion = cursorTrack.endOfDeviceChainInsertionPoint();

    host.showPopupNotification("JJ4x4 MIDI Controller ready");
}

function onMidi(status, data1, data2) {
    /* Only CC messages are dispatched here; notes are handled by NoteInput. */
    if ((status & 0xF0) != 0xB0) return;

    /* The firmware sends momentary CCs with value 127 on press. Ignore any
     * value-0 messages so each command fires exactly once. */
    if (data2 == 0) return;

    switch (data1) {
    case CC_PAGE_UP:     drumBank.scrollPageForwards();        break;
    case CC_PAGE_DOWN:   drumBank.scrollPageBackwards();       break;
    case CC_TRACK_UP:    cursorTrack.selectPrevious();         break;
    case CC_TRACK_DOWN:  cursorTrack.selectNext();             break;
    case CC_PREV_PARAM:  remotePages.selectPreviousPage(true); break;
    case CC_NEXT_PARAM:  remotePages.selectNextPage(true);     break;
    case CC_PREV_DEVICE: cursorDevice.selectPrevious();        break;
    case CC_NEXT_DEVICE: cursorDevice.selectNext();            break;
    case CC_INSERT_DEV:  endInsertion.browse();                break;
    default: break;
    }
}

function flush() {
    /* Nothing to send back to the device (no LEDs over MIDI). */
}

function exit() {
}
