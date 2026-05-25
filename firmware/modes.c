/* modes.c - mode state machine, chord detection, and key -> MIDI mapping.
 *
 * Three modes, cycled by the mode-flip chord (extensible: add entries to the
 * mode enum and the dispatch in emit_press/emit_release):
 *
 *   MODE_DRUM    - all 16 keys are drum pads -> Note On/Off, MIDI channel 1.
 *                  Nav chords scroll the Bitwig Drum Machine's pad pages.
 *   MODE_DEVICE  - 16 keys send momentary CCs (MIDI channel 2) that the
 *                  Bitwig controller script turns into device + transport
 *                  navigation. Every key has a CC so future tweaks are
 *                  script-only (no re-flash).
 *   MODE_CLIPNAV - 16 keys send momentary CCs (MIDI channel 3) for a
 *                  streamlined clip-launcher create/record/play workflow,
 *                  including a state-aware looper key.
 *
 * Chords (only the four corner keys 0/3/12/15 take part):
 *   keys 0 + 15 -> flip to the next mode                 (any mode)
 *   keys 0 + 3  -> drum-nav modifier                     (drum mode only)
 *   keys 12 + 15-> drum page down (CC 17, immediate)     (drum mode only)
 *
 * The 0+3 modifier is sustained: while both keys are held, key 1 scrolls
 * one row up (CC 18) and key 5 one row down (CC 19). Releasing the pair
 * with no row sub-key used scrolls a full page up (CC 16).
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
enum { MODE_DRUM = 0, MODE_DEVICE, MODE_CLIPNAV, MODE_COUNT };

/* ---- MIDI channels (0-based: 0 = MIDI channel 1) ---- */
#define CH_DRUM     0
#define CH_DEVICE   1
#define CH_CLIPNAV  2

/* ---- Drum mode ---- */
#define DRUM_BASE_NOTE  36       /* notes 36..51; see drum_note() for layout */
#define CC_PAGE_UP      16
#define CC_PAGE_DOWN    17
#define CC_ROW_UP       18
#define CC_ROW_DOWN     19

/* ---- Drum-nav modifier sub-keys (held 0+3 + one of these) ---- */
#define KEY_ROW_UP       1
#define KEY_ROW_DOWN     5

/* ---- Corner keys ---- */
#define KEY_TL   0
#define KEY_TR   3
#define KEY_BL  12
#define KEY_BR  15

/* ---- Chord timing ---- */
#define CHORD_TICKS  30          /* ~30 ms decision window */

/* ---- Device mode: key index -> CC number. Every key is assigned so the
 * script can add behaviour to any pad without a firmware change. */
static const uint8_t device_cc[KEY_COUNT] = {
    /*  0 */ 31,   /*  1 */ 20,   /*  2 */ 32,   /*  3 */ 33,
    /*  4 */ 24,   /*  5 */ 21,   /*  6 */ 25,   /*  7 */ 34,
    /*  8 */ 35,   /*  9 */ 36,   /* 10 */ 30,   /* 11 */ 26,
    /* 12 */ 22,   /* 13 */ 28,   /* 14 */ 27,   /* 15 */ 23,
};

/* ---- Clip-nav mode: key index -> CC number. Every key assigned.
 * Keys 1/5 (vertical pad pair) = track up/down, shared with device mode.
 * Keys 4/6 (horizontal pad pair) = clip-focus prev/next within slot bank. */
static const uint8_t clipnav_cc[KEY_COUNT] = {
    /*  0 */ 31,   /*  1 */ 20,   /*  2 */ 32,   /*  3 */ 33,
    /*  4 */ 39,   /*  5 */ 21,   /*  6 */ 40,   /*  7 */ 34,
    /*  8 */ 35,   /*  9 */ 36,   /* 10 */ 41,   /* 11 */ 42,
    /* 12 */ 43,   /* 13 */ 44,   /* 14 */ 45,   /* 15 */ 46,
};

/* ---- Per-key chord/press state ---- */
enum { K_IDLE = 0, K_PENDING, K_ACTIVE, K_CONSUMED };

static uint8_t  mode;
static uint8_t  kstate[KEY_COUNT];
static uint8_t  ktimer[KEY_COUNT];       /* counts down while K_PENDING   */
static uint16_t last_state;              /* matrix state from prev tick   */
static uint16_t notes_on;                /* drum notes currently sounding */
static uint8_t  navmod_active;           /* 1 while 0+3 are both held     */
static uint8_t  navmod_used;             /* 1 if a row sub-key fired      */

static uint8_t is_corner(uint8_t k)
{
    return (k == KEY_TL || k == KEY_TR || k == KEY_BL || k == KEY_BR);
}

/* Physical key -> Drum Machine note. The macropad's top row is the Drum
 * Machine's BOTTOM row, so the row index is flipped: this aligns the
 * physical pad layout with the on-screen 4x4 grid. Note 36 = bottom-left. */
static uint8_t drum_note(uint8_t k)
{
    uint8_t r = k >> 2;          /* 0 = top physical row  */
    uint8_t c = k & 3;           /* 0 = left column       */
    return (uint8_t)(DRUM_BASE_NOTE + (3 - r) * 4 + c);
}

/* Emit a key's normal "press" action for the active mode. */
static void emit_press(uint8_t k)
{
    if (mode == MODE_DRUM) {
        midi_send_note_on(CH_DRUM, drum_note(k), 127);
        notes_on |= (1u << k);
    } else if (mode == MODE_DEVICE) {
        midi_send_cc(CH_DEVICE, device_cc[k], 127);
    } else { /* MODE_CLIPNAV */
        midi_send_cc(CH_CLIPNAV, clipnav_cc[k], 127);
    }
}

/* Emit a key's "release" action. A drum note is released whenever it is
 * still sounding, regardless of the current mode, so a mode flip while a
 * pad is held can never leave a stuck note. */
static void emit_release(uint8_t k)
{
    if (notes_on & (1u << k)) {
        midi_send_note_off(CH_DRUM, drum_note(k), 0);
        notes_on &= ~(1u << k);
    }
    /* device/clip-nav CCs are momentary: nothing to do on release */
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
        navmod_active = 0;          /* a flip leaves no stale modifier */
        navmod_used   = 0;
        mode = (mode + 1) % MODE_COUNT;
        leds_set_mode(mode);
        return;
    }

    if (mode == MODE_DRUM) {
        /* 0+3 is a sustained modifier: while both are held, keys 1/5
         * scroll by one row; if no row sub-key is used, releasing the
         * pair scrolls a full page up (see modes_task()). */
        if (kstate[KEY_TL] == K_PENDING && kstate[KEY_TR] == K_PENDING) {
            consume_pair(KEY_TL, KEY_TR);
            navmod_active = 1;
            navmod_used   = 0;
            return;
        }
        /* 12+15: immediate page down. */
        if (kstate[KEY_BL] == K_PENDING && kstate[KEY_BR] == K_PENDING) {
            consume_pair(KEY_BL, KEY_BR);
            midi_send_cc(CH_DRUM, CC_PAGE_DOWN, 127);
            return;
        }
    }
}

void modes_init(void)
{
    mode          = MODE_DRUM;
    last_state    = 0;
    notes_on      = 0;
    navmod_active = 0;
    navmod_used   = 0;
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

    /* 1. New presses: corner keys wait for a possible chord, others fire.
     * While the 0+3 drum-nav modifier is held, keys 1/5 are stolen to
     * scroll one row instead of playing a pad. */
    for (k = 0; k < KEY_COUNT; k++) {
        if (pressed & (1u << k)) {
            if (navmod_active && mode == MODE_DRUM &&
                (k == KEY_ROW_UP || k == KEY_ROW_DOWN)) {
                midi_send_cc(CH_DRUM,
                             (k == KEY_ROW_UP) ? CC_ROW_UP : CC_ROW_DOWN,
                             127);
                navmod_used = 1;
                kstate[k]   = K_CONSUMED;
            } else if (is_corner(k)) {
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
            /* Releasing either half of the 0+3 modifier ends the hold;
             * if no row sub-key was used, it scrolls a full page up. */
            if (navmod_active && (k == KEY_TL || k == KEY_TR)) {
                if (!navmod_used)
                    midi_send_cc(CH_DRUM, CC_PAGE_UP, 127);
                navmod_active = 0;
                navmod_used   = 0;
            }
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
