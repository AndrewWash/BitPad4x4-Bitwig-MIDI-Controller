/* midi.h - USB-MIDI send queue for the JJ4x4 controller.
 *
 * The device only ever SENDS MIDI to the host (one interrupt-IN endpoint).
 * MIDI channels are 0-based here: channel 0 == MIDI channel 1.
 */
#ifndef MIDI_H
#define MIDI_H

#include <stdint.h>

/* Set up the (empty) send queue. Call once at startup. */
void midi_init(void);

/* Drain one or two queued events to USB if the endpoint is ready.
 * Call this every iteration of the main loop. */
void midi_task(void);

/* Queue MIDI events. These never block; they just append to a ring buffer
 * that midi_task() forwards to the host. */
void midi_send_note_on(uint8_t ch, uint8_t note, uint8_t vel);
void midi_send_note_off(uint8_t ch, uint8_t note, uint8_t vel);
void midi_send_cc(uint8_t ch, uint8_t cc, uint8_t val);

#endif /* MIDI_H */
