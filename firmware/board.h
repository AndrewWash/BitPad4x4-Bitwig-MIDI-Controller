/* board.h - JJ4x4 hardware pin map (single source of truth)
 *
 * Target: ATmega32A, ps2avrGB-style board, 12 MHz crystal.
 * Source of pin assignments: QMK keyboards/kprepublic/jj4x4/keyboard.json
 *
 *   Key matrix : 4x4, COL2ROW diodes
 *                cols = A1, A0, A2, A3   (PORTA)
 *                rows = B5, B0, B3, B4   (PORTB)
 *                COL2ROW => drive a ROW low, read COLUMNS.
 *   USB        : D+ = PD2 (INT0), D- = PD3   (ps2avrGB convention;
 *                also configured in usbconfig.h)
 *   Backlight  : single LED on PD4 (= OC1B, Timer1 PWM)
 *   Underglow  : 4 WS2812 LEDs driven by an on-board ATtiny85, addressed
 *                over I2C/TWI (PC0 = SCL, PC1 = SDA). ATtiny slave addr
 *                is 0xB0 (write form).
 *
 * Key index numbering used throughout the firmware (row*4 + col):
 *      0  1  2  3
 *      4  5  6  7
 *      8  9 10 11
 *     12 13 14 15
 */
#ifndef BOARD_H
#define BOARD_H

/* --- Key matrix rows: PORTB (outputs, driven low to scan) --- */
#define ROW_DDR    DDRB
#define ROW_PORT   PORTB
#define ROW0_BIT   5   /* B5 */
#define ROW1_BIT   0   /* B0 */
#define ROW2_BIT   3   /* B3 */
#define ROW3_BIT   4   /* B4 */

/* --- Key matrix columns: PORTA (inputs with internal pull-ups) --- */
#define COL_DDR    DDRA
#define COL_PORT   PORTA
#define COL_PIN    PINA
#define COL0_BIT   1   /* A1 */
#define COL1_BIT   0   /* A0 */
#define COL2_BIT   2   /* A2 */
#define COL3_BIT   3   /* A3 */

/* --- Backlight LED: PD4 / OC1B --- */
#define LED_DDR    DDRD
#define LED_PORT   PORTD
#define LED_BIT    4   /* D4 */

/* --- RGB underglow: 4 WS2812s behind an ATtiny85 I2C slave on PC0/PC1.
 *     ATmega32A TWI hardware peripheral handles the pins. ---            */
#define UNDERGLOW_I2C_ADDR   0xB0   /* QMK convention: 8-bit, write form */
#define UNDERGLOW_LED_COUNT  4

#endif /* BOARD_H */
