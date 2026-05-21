/* matrix.c - 4x4 key matrix scanning + debounce.
 *
 * The JJ4x4 matrix is COL2ROW: we drive one ROW low at a time and read the
 * COLUMN inputs (held low by a pressed key through its diode). Columns use
 * the AVR's internal pull-ups, so no external resistors are needed.
 *
 * Debounce is a per-key counter: a key's raw reading must disagree with the
 * committed state for DEBOUNCE_TICKS consecutive scans before it flips.
 */
#include <avr/io.h>
#include <util/delay.h>

#include "matrix.h"
#include "board.h"

#define DEBOUNCE_TICKS 5         /* ~5 ms at the 1 ms scan rate */

/* Row/column pin bit numbers, indexed by matrix row / column. */
static const uint8_t row_bit[4] = { ROW0_BIT, ROW1_BIT, ROW2_BIT, ROW3_BIT };
static const uint8_t col_bit[4] = { COL0_BIT, COL1_BIT, COL2_BIT, COL3_BIT };

static uint16_t debounced;               /* committed key state            */
static uint8_t  counter[KEY_COUNT];      /* per-key disagreement counters   */

void matrix_init(void)
{
    uint8_t i;

    /* Rows: outputs, idle HIGH (a row is "selected" by driving it low). */
    for (i = 0; i < 4; i++) {
        ROW_DDR  |= (1 << row_bit[i]);
        ROW_PORT |= (1 << row_bit[i]);
    }
    /* Columns: inputs with internal pull-ups enabled. */
    for (i = 0; i < 4; i++) {
        COL_DDR  &= ~(1 << col_bit[i]);
        COL_PORT |=  (1 << col_bit[i]);
    }

    debounced = 0;
    for (i = 0; i < KEY_COUNT; i++)
        counter[i] = 0;
}

void matrix_scan(void)
{
    uint16_t raw = 0;
    uint8_t  r, c, i;

    for (r = 0; r < 4; r++) {
        ROW_PORT &= ~(1 << row_bit[r]);      /* drive this row low      */
        _delay_us(5);                        /* let the lines settle    */

        uint8_t pins = COL_PIN;
        for (c = 0; c < 4; c++) {
            if (!(pins & (1 << col_bit[c]))) /* low column = key down   */
                raw |= (1u << (r * 4 + c));
        }

        ROW_PORT |= (1 << row_bit[r]);       /* release the row         */
    }

    /* Per-key debounce. */
    for (i = 0; i < KEY_COUNT; i++) {
        uint16_t mask = (1u << i);
        if ((raw & mask) != (debounced & mask)) {
            if (++counter[i] >= DEBOUNCE_TICKS) {
                debounced ^= mask;
                counter[i] = 0;
            }
        } else {
            counter[i] = 0;
        }
    }
}

uint16_t matrix_state(void)
{
    return debounced;
}
