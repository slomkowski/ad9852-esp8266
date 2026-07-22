#include "ad9852.h"
#include <Arduino.h>
#include <math.h>

/*
 * FTW = (desired_freq_hz * 2^48) / SYSCLK_HZ
 * Precompute the constant: FQ = 2^48 / SYSCLK_HZ
 */
static const double FQ = 281474976710656.0 / (double) AD9852_SYSCLK_HZ;

/* Maximum safe output frequency: ~40 % of SYSCLK (comfortable DAC margin) */
static const uint32_t FREQ_MAX = (uint32_t) (AD9852_SYSCLK_HZ * 0.4);

/* ---- low-level bit-bang helpers ---------------------------------------- */

/* SPI mode 0, MSB first — data stable before rising SCLK edge */
static void spi_write_byte(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        digitalWrite(AD9852_PIN_SDIO, (data >> i) & 1);
        digitalWrite(AD9852_PIN_SCLK, HIGH);
        digitalWrite(AD9852_PIN_SCLK, LOW);
    }
}

/* Reset the AD9852 internal serial-address pointer before each transaction */
static void io_reset(void) {
    digitalWrite(AD9852_PIN_IORESET, HIGH);
    delay(1);
    digitalWrite(AD9852_PIN_IORESET, LOW);
    delayMicroseconds(10);
}

static void select(void) {
    digitalWrite(AD9852_PIN_SCB, LOW);
    delayMicroseconds(10);
}

static void release(void) {
    digitalWrite(AD9852_PIN_SCB, HIGH);
}

/* Pulse I/O UD to latch written register values into the active registers */
static void io_update(void) {
    digitalWrite(AD9852_PIN_IOUD, HIGH);
    delayMicroseconds(10);
    digitalWrite(AD9852_PIN_IOUD, LOW);
    delayMicroseconds(10);
}

/* ---- public API --------------------------------------------------------- */

void ad9852_init(void) {
    pinMode(AD9852_PIN_MRESET, OUTPUT);
    pinMode(AD9852_PIN_SCB, OUTPUT);
    pinMode(AD9852_PIN_IORESET, OUTPUT);
    pinMode(AD9852_PIN_SCLK, OUTPUT);
    pinMode(AD9852_PIN_SDIO, OUTPUT);
    pinMode(AD9852_PIN_IOUD, OUTPUT);

    /* Safe idle state */
    digitalWrite(AD9852_PIN_MRESET, LOW); /* reset not asserted */
    digitalWrite(AD9852_PIN_SCB, HIGH); /* chip deselected    */
    digitalWrite(AD9852_PIN_IORESET, LOW);
    digitalWrite(AD9852_PIN_SCLK, LOW);
    digitalWrite(AD9852_PIN_SDIO, LOW);
    digitalWrite(AD9852_PIN_IOUD, LOW);

    /* Hardware master reset */
    digitalWrite(AD9852_PIN_MRESET, HIGH);
    delay(10);
    digitalWrite(AD9852_PIN_MRESET, LOW);
    delay(10);

    /*
     * Control register (address 0x07), 4 data bytes:
     *   Byte 1: 0x10 — comparator power-down
     *   Byte 2: 0x40 — PLL bypass (SYSCLK = REFCLK = 66.667 MHz, no multiplier)
     *           Use 0x14 (MULT=20) if driving from a 10 MHz reference instead.
     *   Byte 3: 0x00 — mode 0 (single-tone), external I/O update clock
     *   Byte 4: 0x00
     * One trailing 0x00 byte matches the original ARM code.
     */
    static const uint8_t ctrl[] = {0x07, 0x10, 0x40, 0x00, 0x00, 0x00};

    io_reset();
    select();
    for (size_t i = 0; i < sizeof(ctrl); i++) {
        spi_write_byte(ctrl[i]);
    }
    release();
    io_update();

    delay(100);
}

void ad9852_set_freq(uint32_t freq_hz) {
    if (freq_hz > FREQ_MAX) {
        freq_hz = FREQ_MAX;
    }

    /* Frequency tuning word (48-bit) */
    uint64_t ftw = (uint64_t) round(FQ * (double) freq_hz);

    io_reset();
    select();
    spi_write_byte(0x02); /* FREQ TUNING WORD 1 register */
    delayMicroseconds(10);
    /* Send 6 bytes MSB first (bits 47..0) */
    for (int shift = 40; shift >= 0; shift -= 8) {
        spi_write_byte((uint8_t) (ftw >> shift));
    }
    spi_write_byte(0x00); /* trailing pad byte, matches original */
    release();
    io_update();
}
