/* main.c - JJ4x4 USB-MIDI controller firmware entry point.
 *
 * Cooperative main loop. The single hard rule of V-USB software USB is that
 * usbPoll() must run very frequently and nothing may block, so every stage
 * here returns quickly and timing is derived by polling a Timer0 flag rather
 * than by delaying.
 *
 *   loop:
 *     usbPoll()        - service software USB        (every iteration)
 *     every ~1 ms:
 *       matrix_scan()  - scan the 4x4 matrix + debounce
 *       modes_task()   - chords / modes -> queue MIDI
 *     midi_task()      - forward queued MIDI to the USB endpoint
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usbdrv/usbdrv.h"
#include "board.h"
#include "midi.h"
#include "matrix.h"
#include "modes.h"
#include "leds.h"
#include "underglow.h"

/* Timer0 in CTC mode, prescaler 64: (12 MHz / 64 / 188) ~= 1.0 ms. */
static void timer_init(void)
{
    TCCR0 = (1 << WGM01) | (1 << CS01) | (1 << CS00);
    OCR0  = 187;
    TIFR  = (1 << OCF0);            /* clear any stale flag */
}

/* Returns non-zero once per ~1 ms tick (output-compare flag polled,
 * no interrupt needed). */
static uint8_t timer_tick(void)
{
    if (TIFR & (1 << OCF0)) {
        TIFR = (1 << OCF0);         /* clear the flag by writing 1 */
        return 1;
    }
    return 0;
}

int main(void)
{
    cli();

    matrix_init();
    leds_init();
    underglow_init();
    midi_init();
    timer_init();

    /* Bring up V-USB. The disconnect/delay/connect sequence forces the host
     * to re-enumerate cleanly every time the device powers up. */
    usbInit();
    usbDeviceDisconnect();
    _delay_ms(260);
    usbDeviceConnect();

    sei();

    modes_init();

    for (;;) {
        usbPoll();
        if (timer_tick()) {
            matrix_scan();
            modes_task();
        }
        midi_task();
    }

    return 0;   /* not reached */
}
