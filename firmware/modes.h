/* modes.h - mode state machine + chord detection for the JJ4x4. */
#ifndef MODES_H
#define MODES_H

#include <stdint.h>

/* Reset to drum mode with all keys idle. Call once at startup. */
void modes_init(void);

/* Consume the latest debounced matrix state: resolve chords, advance
 * timers, and queue MIDI. Call once per ~1 ms tick, after matrix_scan(). */
void modes_task(void);

/* Index of the currently active mode (0 = drum, 1 = device). */
uint8_t modes_current(void);

#endif /* MODES_H */
