/* underglow.c - 4-LED RGB underglow over I2C.
 *
 * The JJ4x4 PCB has an ATtiny85 that drives 4 WS2812 LEDs from its own
 * pin. The ATmega32A reaches them by sending RGB bytes to the ATtiny over
 * the TWI bus (PC0=SCL, PC1=SDA). One I2C write per render: 4 LEDs * 3
 * bytes = 12 bytes, in RGB order; the ATtiny does the GRB-on-wire
 * conversion.
 *
 * The TWI spin loops are *bounded* (CAP_ITERS). If the ATtiny is missing
 * or unresponsive the write times out in well under a millisecond, forces
 * a STOP, and returns - so V-USB keeps polling and the rest of the
 * firmware works normally even with no underglow LEDs populated.
 */
#include <avr/io.h>
#include <stdint.h>

#include "underglow.h"
#include "board.h"

/* ---- Per-mode base color (R, G, B). Hues ~120 deg apart so the three
 *      modes are unambiguous at a glance. Kept moderate (~80/255 max) so
 *      total current stays under USB's budget. ---- */
static const uint8_t mode_colors[][3] = {
    /*  R    G    B  */
    {   0,   0,  80 },   /* mode 0: drum     - blue    */
    {   0,  80,   0 },   /* mode 1: device   - green   */
    {  80,   0,  60 },   /* mode 2: clip-nav - magenta */
};
#define MODE_COLOR_COUNT (sizeof(mode_colors) / sizeof(mode_colors[0]))

/* ---- Brightness scale (applied as (channel * scale) >> 8). 5 levels
 *      from "really dim" to "max". ---- */
static const uint8_t brightness_scale[] = {
    0x10,   /* 0: very dim (~6%)  */
    0x30,   /* 1: dim       (~19%) */
    0x60,   /* 2: medium    (~38%) - default */
    0xA0,   /* 3: bright    (~63%) */
    0xFF,   /* 4: max       (100%) */
};
#define BRIGHTNESS_LEVELS (sizeof(brightness_scale) / sizeof(brightness_scale[0]))
#define BRIGHTNESS_DEFAULT 2

/* ---- Persistent state across calls. ---- */
static uint8_t cur_mode             = 0;
static uint8_t cur_brightness_level = BRIGHTNESS_DEFAULT;
static uint8_t cur_armed            = 1;   /* drum-mode quick-flip on (blue) */

/* ---- TWI primitives (master mode, bounded busy-wait). ---- */

/* Cap each TWINT wait to ~700 us @ 12 MHz so a missing/stuck ATtiny can
 * never hang the main loop. */
#define CAP_ITERS 2000

static uint8_t twi_wait(void)
{
    for (uint16_t t = 0; t < CAP_ITERS; t++) {
        if (TWCR & (1 << TWINT))
            return 1;
    }
    return 0;       /* timed out */
}

static void twi_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

/* Write `len` bytes to 7-bit-shifted slave address `addr` (i.e. the QMK
 * 8-bit form: 0xB0 for the ATtiny). Returns 0 on success, non-zero on
 * any TWI fault. */
static uint8_t i2c_write(uint8_t addr, const uint8_t *data, uint8_t len)
{
    /* START */
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    if (!twi_wait())                                       { twi_stop(); return 1; }
    if ((TWSR & 0xF8) != 0x08)                             { twi_stop(); return 2; }

    /* SLA+W */
    TWDR = addr;                                /* 0xB0 already has R/W=0 */
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!twi_wait())                                       { twi_stop(); return 3; }
    if ((TWSR & 0xF8) != 0x18)                             { twi_stop(); return 4; }

    /* DATA */
    for (uint8_t i = 0; i < len; i++) {
        TWDR = data[i];
        TWCR = (1 << TWINT) | (1 << TWEN);
        if (!twi_wait())                                   { twi_stop(); return 5; }
        if ((TWSR & 0xF8) != 0x28)                         { twi_stop(); return 6; }
    }

    twi_stop();
    return 0;
}

/* ---- Render the current (mode, brightness) to the LEDs. ---- */
static void render(void)
{
    /* Amber shown in drum mode when quick-flip macros are disabled. Same
     * ~80 peak as the regular mode colors so the brightness budget is
     * unchanged. Visible only in drum mode; other modes use their own
     * color regardless of cur_armed. */
    static const uint8_t disarmed_color[3] = { 80, 30, 0 };

    uint8_t r, g, b;

    if (cur_mode < MODE_COLOR_COUNT) {
        uint8_t s = brightness_scale[cur_brightness_level];
        const uint8_t *src = (!cur_armed && cur_mode == 0)
                             ? disarmed_color
                             : mode_colors[cur_mode];
        r = ((uint16_t)src[0] * s) >> 8;
        g = ((uint16_t)src[1] * s) >> 8;
        b = ((uint16_t)src[2] * s) >> 8;
    } else {
        r = g = b = 0;                              /* unknown mode: off */
    }

    uint8_t buf[UNDERGLOW_LED_COUNT * 3];
    for (uint8_t i = 0; i < UNDERGLOW_LED_COUNT; i++) {
        buf[i * 3 + 0] = r;
        buf[i * 3 + 1] = g;
        buf[i * 3 + 2] = b;
    }

    (void)i2c_write(UNDERGLOW_I2C_ADDR, buf, sizeof(buf));
}

/* ---- Public API ---- */

void underglow_init(void)
{
    /* Insurance pull-ups on PC0/PC1. Real ATtiny board likely already
     * has externals; the internal pulls are weak (~30 kOhm) and won't
     * hurt if both are present. DDRC bits 0/1 stay 0 (inputs) - the TWI
     * hardware drives them as open-drain. */
    PORTC |= (1 << 1) | (1 << 0);

    TWSR = 0;          /* prescaler = 1                                  */
    TWBR = 12;         /* SCL = 12 MHz / (16 + 2*12) = 300 kHz           */
    TWCR = (1 << TWEN);

    cur_mode             = 0;
    cur_brightness_level = BRIGHTNESS_DEFAULT;
    cur_armed            = 1;
    render();
}

void underglow_set_mode(uint8_t mode)
{
    cur_mode = mode;
    render();
}

void underglow_cycle_brightness(void)
{
    cur_brightness_level = (cur_brightness_level + 1) % BRIGHTNESS_LEVELS;
    render();
}

void underglow_set_armed(uint8_t armed)
{
    cur_armed = armed ? 1 : 0;
    render();
}
