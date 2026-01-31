#include "variant.h"
#include "wiring_constants.h"
#include "wiring_digital.h"
#include "nrf.h"

const uint32_t g_ADigitalPinMap[] =
{
    // D0 - D9 (left side, top to bottom)
    6,  // D0 is P0.06 (D0, TX)
    8,  // D1 is P0.08 (D1, RX)
    15, // D2 is P0.15 (D2, SCK)
    17, // D3 is P0.17 (D3, MISO)
    20, // D4 is P0.20 (D4, MOSI)
    13, // D5 is P0.13 (D5, SDA)
    24, // D6 is P0.24 (D6, SCL)
    9,  // D7 is P0.09 (D7, NFC1)
    10, // D8 is P0.10 (D8, NFC2)
    38, // D9 is P1.06 (D9, CS)

    // D10 - D17 (right side, bottom to top)
    43, // D10 is P1.11 (D10)
    28, // D11 is P0.28 (D11, A0)
    3,  // D12 is P0.03 (D12, A1)
    45, // D13 is P1.13 (D13)
    2,  // D14 is P0.02 (D14, A2)
    29, // D15 is P0.29 (D15, A3)
    31, // D16 is P0.31 (D16, A4)
    30, // D17 is P0.30 (D17, A5)

    // (extra pins, clockwise from bottom left, viewed from bottom side of board)
    // D18 - D21 (left, bottom to top)
    36, // D18 is P1.04 (D18, SCK1)
    34, // D19 is P1.02 (D19, MISO1)
    32, // D20 is P1.00 (D20, MOSI1)
    22, // D21 is P0.22 (D21)

    // D22 - D24 (top, left to right)
    7,  // D22 is P0.07 (D22, SDA1)
    12, // D23 is P0.12 (D23, SCL1)
    26, // D24 is P0.26 (D24)

    // pins not given 'D' numbers

    4, // PIN_VBAT/'D25' is P0.04 (VBAT)
    41, // EXT_VCC/'D26' is P1.09 (EXT_VCC)
    42, // PIN_LED/'D27' is P1.10 (LED)
};

void initVariant()
{
    // enable EXT_VCC by default
    pinMode(EXT_VCC, OUTPUT);
    digitalWrite(EXT_VCC, LOW);

    // enable LED by default
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
}

