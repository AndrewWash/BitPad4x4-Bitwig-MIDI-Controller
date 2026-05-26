/* underglow.h - 4 WS2812 RGB underglow LEDs via the on-board ATtiny85
 * (I2C slave). The ATmega32A talks TWI on PC0=SCL, PC1=SDA. */
#ifndef UNDERGLOW_H
#define UNDERGLOW_H

#include <stdint.h>

/* Bring up TWI and render the default state. Call once at startup, before
 * the first underglow_set_mode() / underglow_cycle_brightness() call. */
void underglow_init(void);

/* Set the underglow color to match the active mode. Re-renders at the
 * current brightness level. Out-of-range mode -> all off. */
void underglow_set_mode(uint8_t mode);

/* Advance to the next of 5 brightness levels and re-render. Wraps from
 * max back to very-dim. */
void underglow_cycle_brightness(void);

#endif /* UNDERGLOW_H */
