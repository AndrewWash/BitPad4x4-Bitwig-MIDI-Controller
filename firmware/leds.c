/* leds.c - backlight feedback on PD4.
 *
 * PD4 is OC1B, so Timer1 drives it as an 8-bit fast-PWM output. The
 * backlight brightness indicates which mode is active.
 *
 * NOTE: if the backlight is wired active-low on your board, the brightness
 * will look inverted. In that case switch the COM1B bits in leds_init()
 * from non-inverting (COM1B1) to inverting (COM1B1 | COM1B0).
 */
#include <avr/io.h>

#include "leds.h"
#include "board.h"

/* Brightness per mode index (0..255). mode 0 = drum, mode 1 = device. */
static const uint8_t mode_brightness[] = { 40, 255 };

void leds_init(void)
{
    LED_DDR |= (1 << LED_BIT);                  /* PD4 as output            */

    /* Timer1: 8-bit fast PWM (WGM13:0 = 0101), non-inverting on OC1B,
     * prescaler 8 -> ~5.9 kHz PWM at 12 MHz. */
    TCCR1A = (1 << COM1B1) | (1 << WGM10);
    TCCR1B = (1 << WGM12)  | (1 << CS11);
    OCR1B  = 0;
}

void leds_set_mode(uint8_t mode)
{
    if (mode < sizeof(mode_brightness))
        OCR1B = mode_brightness[mode];
    else
        OCR1B = 255;
}
