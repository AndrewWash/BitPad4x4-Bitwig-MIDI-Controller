/* modes.c - mode state machine, chord detection, and key -> MIDI mapping.
 *
 * Three modes, cycled by the mode-flip chord (extensible: add entries to the
 * mode enum and the dispatch in emit_press/emit_release):
 *
 *   MODE_DRUM    - 16 drum pads -> Note On/Off, MIDI channel 1. Page/row
 *                  chords scroll the Bitwig Drum Machine. Quick-flip chord
 *                  families (1+X, 5+X) let drum mode emit clip-nav CCs
 *                  without leaving the page; gated by a 3+15 toggle.
 *   MODE_DEVICE  - 16 keys send momentary CCs (MIDI channel 2). One chord
 *                  (1+10) deletes the current device. Other keys fire solo.
 *   MODE_CLIPNAV - 16 keys send momentary CCs (MIDI channel 3) for clip
 *                  navigation, transport, the state-aware looper, and clip
 *                  delete. Always-on 5+X scene chord launches scenes 1..8.
 *
 * Chord taxonomy (CHORD_TICKS ~30 ms window):
 *
 *   Corner-only chords (any mode):
 *     0 + 15 -> cycle mode (drum > device > clip-nav)
 *     3 + 12 -> cycle underglow brightness (5 levels)
 *
 *   Drum-mode chords:
 *     0 + 3   -> sustained nav modifier (see below)
 *     12 + 15 -> drum page down (CC 17)
 *     3 + 15  -> toggle drum quick-flip macros (blue <-> amber underglow)
 *     1 + X   -> 14 transport/clip macros via clipnav_cc[X], CH_CLIPNAV
 *                X in {0,2,3,4,6,7,8,9,10,11,12,13,14,15} -- when armed
 *     5 + X   -> launch scenes 1..8 via scene_cc[X-8], CH_CLIPNAV
 *                X in {8..15} -- when armed
 *
 *   Clip-nav-mode chord (always on, no toggle):
 *     5 + X   -> scene launch, same mapping as drum's 5+X
 *
 *   Device-mode chord:
 *     1 + 10  -> delete current device (CC_DEV_DELETE on CH_DEVICE)
 *
 * The 0+3 nav modifier is sustained: while both are held, key 1 scrolls
 * one row up (CC 18), key 5 one row down (CC 19). Releasing the pair
 * with no row sub-key used scrolls a full page up (CC 16).
 *
 * Keys that take part in chord detection enter K_PENDING and are held
 * back for CHORD_TICKS so the chord can form. Quick taps still emit on
 * release, so the only perceptible latency is when a chord-eligible key
 * is held alone past the window. See needs_chord_window() for the
 * per-mode predicate.
 */
#include "modes.h"
#include "matrix.h"
#include "midi.h"
#include "leds.h"
#include "underglow.h"

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

/* ---- Device-mode chord CC (only emitted by the 1+10 chord) ---- */
#define CC_DEV_DELETE   29       /* delete current device (was reserved) */

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
static uint8_t  armed;                   /* drum quick-flip macros on/off */

static uint8_t is_corner(uint8_t k)
{
    return (k == KEY_TL || k == KEY_TR || k == KEY_BL || k == KEY_BR);
}

/* Drum quick-flip "1+X" macro partner keys (every key except 1 and 5).
 * When drum mode is `armed`, pressing key 1 + one of these within
 * CHORD_TICKS fires the partner's clip-nav CC on CH_CLIPNAV in place of
 * the two drum pads. Mask: all 16 bits except 1 and 5 = 0xFFDD. */
static uint8_t is_macro_partner(uint8_t k)
{
    return ((1u << k) & 0xFFDDu) != 0;
}

/* "5+X" scene-launcher partner keys: bottom 8 pads (keys 8..15). Pressing
 * key 5 + one of these fires its scene CC on CH_CLIPNAV. Active in drum
 * mode while armed and ALWAYS in clip-nav mode. Mask: bits 8..15 = 0xFF00. */
static uint8_t is_scene_partner(uint8_t k)
{
    return ((1u << k) & 0xFF00u) != 0;
}

/* Scene CCs sent on CH_CLIPNAV for 5+X chords. Indexed by partner key
 * minus 8 (so key 8 -> scene 1 -> CC 60, ..., key 15 -> scene 8 -> CC 67).
 * Lives in a 15-CC gap above the clip CCs (which run 31..46) so room
 * remains to add more clip-ops before bumping into the scene range. */
static const uint8_t scene_cc[8] = {
    60, 61, 62, 63, 64, 65, 66, 67,
};

/* Does this key need to enter K_PENDING so it can take part in a 1+X or
 * 5+X chord? Independent of the corner/chord-window logic, which already
 * delays the four corner keys for the existing pre-feature chords. */
static uint8_t needs_chord_window(uint8_t k)
{
    if (mode == MODE_DRUM && armed) {
        return (k == 1 || k == 5 || is_macro_partner(k) || is_scene_partner(k));
    }
    if (mode == MODE_CLIPNAV) {
        return (k == 5 || is_scene_partner(k));
    }
    if (mode == MODE_DEVICE) {
        /* Only chord on device mode is 1+10 = delete device. */
        return (k == 1 || k == 10);
    }
    return 0;
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

/* "1+X" macro chord scan: when key 1 is pending, look for a macro partner
 * also pending and fire its clip-nav CC. Returns 1 if a chord fired. */
static uint8_t try_clip_macro_chord(void)
{
    if (kstate[1] != K_PENDING) return 0;
    for (uint8_t p = 0; p < KEY_COUNT; p++) {
        if (p == 1 || !is_macro_partner(p)) continue;
        if (kstate[p] == K_PENDING) {
            consume_pair(1, p);
            midi_send_cc(CH_CLIPNAV, clipnav_cc[p], 127);
            return 1;
        }
    }
    return 0;
}

/* "5+X" scene chord scan: when key 5 is pending, look for a scene partner
 * also pending and launch its scene CC. Returns 1 if a chord fired. */
static uint8_t try_scene_chord(void)
{
    if (kstate[5] != K_PENDING) return 0;
    for (uint8_t p = 0; p < KEY_COUNT; p++) {
        if (p == 5 || !is_scene_partner(p)) continue;
        if (kstate[p] == K_PENDING) {
            consume_pair(5, p);
            midi_send_cc(CH_CLIPNAV, scene_cc[p - 8], 127);
            return 1;
        }
    }
    return 0;
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
        underglow_set_mode(mode);
        return;
    }

    /* 3+12 (opposite diagonal): cycle underglow brightness, every mode. */
    if (kstate[KEY_TR] == K_PENDING && kstate[KEY_BL] == K_PENDING) {
        consume_pair(KEY_TR, KEY_BL);
        flush_notes();
        underglow_cycle_brightness();
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
        /* 3+15: toggle quick-flip macros. Underglow flips to amber when
         * disarmed so the off state is visible at a glance. */
        if (kstate[KEY_TR] == K_PENDING && kstate[KEY_BR] == K_PENDING) {
            consume_pair(KEY_TR, KEY_BR);
            flush_notes();
            armed ^= 1;
            underglow_set_armed(armed);
            return;
        }
        /* 12+15: immediate page down. */
        if (kstate[KEY_BL] == K_PENDING && kstate[KEY_BR] == K_PENDING) {
            consume_pair(KEY_BL, KEY_BR);
            midi_send_cc(CH_DRUM, CC_PAGE_DOWN, 127);
            return;
        }
        /* 1+X clip macros + 5+X scene launches. Both gated by `armed`
         * in drum mode (3+15 toggles them off together). Order: clip
         * macros first so a 1+0/2/etc. pairing wins over a 5+(same
         * key) race; in practice the two modifiers won't be held at
         * once but this keeps the precedence explicit. */
        if (armed) {
            if (try_clip_macro_chord()) return;
            if (try_scene_chord())      return;
        }
    } else if (mode == MODE_CLIPNAV) {
        /* Clip-nav scenes are always on - no toggle, no `armed` gate.
         * Required keys (5 and 8..15) get K_PENDING via the press
         * loop's needs_chord_window() branch. */
        if (try_scene_chord()) return;
    } else if (mode == MODE_DEVICE) {
        /* 1+10: delete the currently selected device. */
        if (kstate[1] == K_PENDING && kstate[10] == K_PENDING) {
            consume_pair(1, 10);
            midi_send_cc(CH_DEVICE, CC_DEV_DELETE, 127);
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
    armed         = 1;            /* quick-flip macros default on */
    for (uint8_t k = 0; k < KEY_COUNT; k++) {
        kstate[k] = K_IDLE;
        ktimer[k] = 0;
    }
    leds_set_mode(mode);
    underglow_set_mode(mode);
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
            } else if (is_corner(k) || needs_chord_window(k)) {
                /* Corner keys take part in the original chords; the
                 * extra keys flagged by needs_chord_window() take part
                 * in 1+X / 5+X chords (drum-armed, or clip-nav scenes).
                 * Pads/CCs still fire on release via the K_PENDING
                 * release path, or on timeout. */
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
