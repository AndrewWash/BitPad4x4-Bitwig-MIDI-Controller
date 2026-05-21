/* leds.h - backlight LED feedback on PD4 (Timer1 PWM). */
#ifndef LEDS_H
#define LEDS_H

#include <stdint.h>

/* Configure PD4 / Timer1 for PWM. Call once at startup. */
void leds_init(void);

/* Set the backlight brightness to indicate the active mode. */
void leds_set_mode(uint8_t mode);

#endif /* LEDS_H */
