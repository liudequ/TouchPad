#ifndef _NRFMICRO_H_
#define _NRFMICRO_H_

#define TARGET_NRFMICRO

/** Master clock frequency */
#define VARIANT_MCK       (64000000ul)

// ASSUMES CRYSTAL OSCILLATOR IS USED
#define USE_LFXO      // Board uses 32khz crystal for LF
//#define USE_LFRC    // Board uses RC for LF

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/

#include "WVariant.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#define PINS_COUNT           (28)
#define NUM_DIGITAL_PINS     (28)
#define NUM_ANALOG_INPUTS    (7)
#define NUM_ANALOG_OUTPUTS   (0)

// 3V3 Regulator Disable
#define PIN_EXT_VCC          (26) // EXT_VCC is P1.09 (set high to disable 3v3)
#define EXT_VCC              (PIN_EXT_VCC)

// LEDs
#define PIN_LED              (27) // USR LED is P1.10
#define LED_PWR              (PINS_COUNT) // no LED_PWR
#define PIN_NEOPIXEL         (PINS_COUNT) // no neopixel
#define NEOPIXEL_NUM         0
#define LED_BLUE             PIN_LED // required by bluefruit library

#define LED_BUILTIN          PIN_LED

#define LED_STATE_ON         1         // State when LED is on

#define PIN_VBAT             (25) // P0.04/AIN2 (VBAT) - reads battery voltage

/*
 * Buttons
 */
#define PIN_BUTTON1          (PINS_COUNT) // no button as of version 1.2

// Digital PINs
// NOTE: THIS IS A RENUMBERING OF THE PINS
// AND DOES NOT CORRESPOND TO THE NICENANO OR
// PROMICRO DIAGRAM

// left side (top to bottom)
#define D0 (0ul) // P0.06 (TX)
#define D1 (1ul) // P0.08 (RX)
#define D2 (2ul) // P0.15 (SCK)
#define D3 (3ul) // P0.17 (MISO)
#define D4 (4ul) // P0.20 (MOSI)
#define D5 (5ul) // P0.13 (SDA)
#define D6 (6ul) // P0.24 (SCL)
#define D7 (7ul) // P0.09 (NFC1)
#define D8 (8ul) // P0.10 (NFC2)
#define D9 (9ul) // P1.06 (CS)

// right side (bottom to top)
#define D10 (10ul) // P1.11
#define D11 (11ul) // P0.28/AIN4 (A0)
#define D12 (12ul) // P0.03/AIN1 (A1)
#define D13 (13ul) // P1.13
#define D14 (14ul) // P0.02/AIN0 (A2)
#define D15 (15ul) // P0.29/AIN5 (A3)
#define D16 (16ul) // P0.31/AIN7 (A4)
#define D17 (17ul) // P0.30/AIN6 (A5)

// extra center pins (clockwise from bottom left, viewed from bottom side of board)
// only using through-hole pins
// P0.05 (AIN3) is a surface mount that isn't broken out

// four free pins on the left (bottom to top)
#define D18 (18ul) // P1.04 (SCK1)
#define D19 (19ul) // P1.02 (MISO1)
#define D20 (20ul) // P1.00 (MOSI1)
#define D21 (21ul) // P0.22

// three free pins on top (left to right)
#define D22 (22ul) // P0.07 (SDA1)
#define D23 (23ul) // P0.12 (SCL1)
#define D24 (24ul) // P0.26


/*
 * Macros for nRF pin numbers for compatibility
 */
#define P0_06 D0
#define P0_08 D1
#define P0_15 D2
#define P0_17 D3
#define P0_20 D4
#define P0_13 D5
#define P0_24 D6
#define P0_09 D7
#define P0_10 D8
#define P1_06 D9
#define P1_11 D10
#define P0_28 D11
#define P0_03 D12
#define P0_02 D13
#define P0_29 D15
#define P0_31 D16
#define P0_30 D17
#define P1_04 D18
#define P1_02 D19
#define P1_00 D20
#define P0_22 D21
#define P0_07 D22
#define P0_12 D23
#define P0_26 D24
#define P0_04 PIN_VBAT
#define P1_09 PIN_EXT_VCC
#define P1_10 PIN_LED

/*
 * Analog pins
 */
#define PIN_A0               (11) // P0.28/AIN4
#define PIN_A1               (12) // P0.03/AIN1
#define PIN_A2               (14) // P0.02/AIN0
#define PIN_A3               (15) // P0.29/AIN5
#define PIN_A4               (16) // P0.31/AIN7
#define PIN_A5               (17) // P0.30/AIN6

static const uint8_t A0  = PIN_A0 ;
static const uint8_t A1  = PIN_A1 ;
static const uint8_t A2  = PIN_A2 ;
static const uint8_t A3  = PIN_A3 ;
static const uint8_t A4  = PIN_A4 ;
static const uint8_t A5  = PIN_A5 ;
#define ADC_RESOLUTION    12

/*
 * Other pins
 */
#define PIN_NFC1             (7) // P0.09 (D7)
#define PIN_NFC2             (8) // P0.10 (D8)

/*
 * Serial interfaces
 */
#define PIN_SERIAL1_TX       (0) // P0.06 - also D0
#define PIN_SERIAL1_RX       (1) // P0.08 - also D1

/*
 * SPI Interfaces
 */
#define SPI_INTERFACES_COUNT 2

#define PIN_SPI_SCK          (2) // P0.15 - D2
#define PIN_SPI_MISO         (3) // P0.17 - D3
#define PIN_SPI_MOSI         (4) // P0.20 - D4

static const uint8_t SS   = (9); // P1.06 - D9
static const uint8_t MOSI = PIN_SPI_MOSI ;
static const uint8_t MISO = PIN_SPI_MISO ;
static const uint8_t SCK  = PIN_SPI_SCK ;

#define PIN_SPI1_SCK          (18) // P1.04 - D18
#define PIN_SPI1_MISO         (19) // P1.02 - D19
#define PIN_SPI1_MOSI         (20) // P1.00 - D20

/*
 * Wire Interfaces
 */
#define WIRE_INTERFACES_COUNT 2

#define PIN_WIRE_SDA         (5) // P0.13 - D5
#define PIN_WIRE_SCL         (6) // P0.24 - D6

static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;

#define PIN_WIRE1_SDA        (22) // P0.07 - D22
#define PIN_WIRE1_SCL        (23) // P0.12 - D23

#ifdef __cplusplus
}
#endif

/*----------------------------------------------------------------------------
 *        Arduino objects - C++ only
 *----------------------------------------------------------------------------*/

#endif
