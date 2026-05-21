/* modes.c - mode state machine, chord detection, and key -> MIDI mapping.
 *
 * Two modes, cycled by the mode-flip chord (extensible: add entries to the
 * mode enum and the dispatch in emit_press/emit_release):
 *
 *   MODE_DRUM   - all 16 keys are drum pads -> Note On/Off, MIDI channel 1.
 *                 Page chords scroll the Bitwig Drum Machine's pad pages.
 *   MODE_DEVICE - 7 keys send momentary CCs (MIDI channel 2) that the
 *                 Bitwig controller script turns into device navigation.
 *
 * Chords (only the four corner keys 0/3/12/15 take part):
 *   keys 0 + 15 -> flip to the next mode          (any mode)
 *   keys 0 + 3  -> drum page up   (CC 16)         (drum mode only)
 *   keys 12 + 15-> drum page down (CC 17)         (drum mode only)
 *
 * A corner key's note/CC is held back for CHORD_TICKS ms: if a second key
 * completes a chord in that window the chord fires and both keys are
 * suppressed; otherwise the key's normal action is emitted (slightly late).
 * The 12 non-corner keys fire immediately.
 */
#include "modes.h"
#include "matrix.h"
#include "midi.h"
#include "leds.h"

/* ---- Modes ---- */
enum { MODE_DRUM = 0, MODE_DEVICE, MODE_COUNT };

/* ---- MIDI channels (0-based: 0 = MIDI channel 1) ---- */
#define CH_DRUM     0
#define CH_DEVICE   1

/* ---- Drum mode ---- */
#define DRUM_BASE_NOTE  36       /* key 0 -> note 36 ... key 15 -> note 51 */
#define CC_PAGE_UP      16
#define CC_PAGE_DOWN    17

/* ---- Corner keys ---- */
#define KEY_TL   0
#define KEY_TR   3
#define KEY_BL  12
#define KEY_BR  15

/* ---- Chord timing ---- */
#define CHORD_TICKS  30          /* ~30 ms decision window */

/* ---- Device mode: key index -> CC number (0xFF = key unused) ---- */
static const uint8_t device_cc[KEY_COUNT] = {
    /*  0 */ 22,   /*  1 */ 0xFF, /*  2 */ 0xFF, /*  3 */ 23,
    /*  4 */ 24,   /*  5 */ 0xFF, /*  6 */ 0xFF, /*  7 */ 25,
    /*  8 */ 20,   /*  9 */ 0xFF, /* 10 */ 0xFF, /* 11 */ 0xFF,
    /* 12 */ 21,   /* 13 */ 0xFF, /* 14 */ 0xFF, /* 15 */ 26,
};

/* ---- Per-key chord/press state ---- */
enum { K_IDLE = 0, K_PENDING, K_ACTIVE, K_CONSUMED };

static uint8_t  mode;
static uint8_t  kstate[KEY_COUNT];
static uint8_t  ktimer[KEY_COUNT];       /* counts down while K_PENDING   */
static uint16_t last_state;              /* matrix state from prev tick   */
static uint16_t notes_on;                /* drum notes currently sounding */

static uint8_t is_corner(uint8_t k)
{
    return (k == KEY_TL || k == KEY_TR || k == KEY_BL || k == KEY_BR);
}

/* Emit a key's normal "press" action for the active mode. */
static void emit_press(uint8_t k)
{
    if (mode == MODE_DRUM) {
        midi_send_note_on(CH_DRUM, DRUM_BASE_NOTE + k, 127);
        notes_on |= (1u << k);
    } else { /* MODE_DEVICE */
        uint8_t cc = device_cc[k];
        if (cc != 0xFF)
            midi_send_cc(CH_DEVICE, cc, 127);
    }
}

/* Emit a key's "release" action. A drum note is released whenever it is
 * still sounding, regardless of the current mode, so a mode flip while a
 * pad is held can never leave a stuck note. */
static void emit_release(uint8_t k)
{
    if (notes_on & (1u << k)) {
        midi_send_note_off(CH_DRUM, DRUM_BASE_NOTE + k, 0);
        notes_on &= ~(1u << k);
    }
    /* device-mode CCs are momentary: nothing to do on release */
}

/* Release every drum note that is still sounding (used on a mode flip). */
static void flush_notes(void)
{
    for (uint8_t k = 0; k < KEY_COUNT; k++)
        emit_release(k);
}

static void consume_pair(uint8_t a, uint8_t b)
{
    kstate[a] = K_CONSUMED;
    kstate[b] = K_CONSUMED;
}

/* Look for a completed chord among the keys currently in K_PENDING. */
static void chord_check(void)
{
    /* Mode flip works in every mode. */
    if (kstate[KEY_TL] == K_PENDING && kstate[KEY_BR] == K_PENDING) {
        consume_pair(KEY_TL, KEY_BR);
        flush_notes();
        mode = (mode + 1) % MODE_COUNT;
        leds_set_mode(mode);
        return;
    }

    if (mode == MODE_DRUM) {
        if (kstate[KEY_TL] == K_PENDING && kstate[KEY_TR] == K_PENDING) {
            consume_pair(KEY_TL, KEY_TR);
            midi_send_cc(CH_DRUM, CC_PAGE_UP, 127);
            return;
        }
        if (kstate[KEY_BL] == K_PENDING && kstate[KEY_BR] == K_PENDING) {
            consume_pair(KEY_BL, KEY_BR);
            midi_send_cc(CH_DRUM, CC_PAGE_DOWN, 127);
            return;
        }
    }
}

void modes_init(void)
{
    mode       = MODE_DRUM;
    last_state = 0;
    notes_on   = 0;
    for (uint8_t k = 0; k < KEY_COUNT; k++) {
        kstate[k] = K_IDLE;
        ktimer[k] = 0;
    }
    leds_set_mode(mode);
}

void modes_task(void)
{
    uint16_t state    = matrix_state();
    uint16_t changed  = state ^ last_state;
    uint16_t pressed  = changed & state;     /* press edges   */
    uint16_t released = changed & ~state;    /* release edges */
    uint8_t  k;
    last_state = state;

    /* 1. New presses: corner keys wait for a possible chord, others fire. */
    for (k = 0; k < KEY_COUNT; k++) {
        if (pressed & (1u << k)) {
            if (is_corner(k)) {
                kstate[k] = K_PENDING;
                ktimer[k] = CHORD_TICKS;
            } else {
                emit_press(k);
                kstate[k] = K_ACTIVE;
            }
        }
    }

    /* 2. Resolve any chord formed by keys still pending. */
    chord_check();

    /* 3. Pending corner keys that did not form a chord time out and fire. */
    for (k = 0; k < KEY_COUNT; k++) {
        if (kstate[k] == K_PENDING) {
            if (ktimer[k] > 0)
                ktimer[k]--;
            if (ktimer[k] == 0) {
                emit_press(k);
                kstate[k] = K_ACTIVE;
            }
        }
    }

    /* 4. Releases. */
    for (k = 0; k < KEY_COUNT; k++) {
        if (released & (1u << k)) {
            if (kstate[k] == K_PENDING) {
                /* Tapped and released inside the chord window: emit the
                 * quick press now so the tap is not lost. */
                emit_press(k);
                emit_release(k);
            } else if (kstate[k] == K_ACTIVE) {
                emit_release(k);
            }
            /* K_CONSUMED: silent - it was part of a chord. */
            kstate[k] = K_IDLE;
        }
    }
}

uint8_t modes_current(void)
{
    return mode;
}
