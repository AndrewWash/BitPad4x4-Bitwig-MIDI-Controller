/* matrix.h - 4x4 key matrix scanning + debounce for the JJ4x4. */
#ifndef MATRIX_H
#define MATRIX_H

#include <stdint.h>

#define KEY_COUNT 16

/* Configure the matrix GPIOs. Call once at startup. */
void matrix_init(void);

/* Scan all four rows and run one debounce step.
 * Call once per ~1 ms tick. */
void matrix_scan(void);

/* Debounced key state: bit i is 1 while key i is held. */
uint16_t matrix_state(void);

#endif /* MATRIX_H */
