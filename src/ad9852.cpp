#include "ad9852.hpp"
#include <Arduino.h>
#include <math.h>

#define AD9852_PIN_MRESET   12
#define AD9852_PIN_SCB      15
#define AD9852_PIN_IORESET  13
#define AD9852_PIN_SCLK     2
#define AD9852_PIN_IOUD     4
#define AD9852_PIN_SDIO     5

// TODO PROPER NAMES
#define REG_PHASE_ADJUST_REGISTER1				0x00
#define REG_PHASE_ADJUST_REGISTER2				0x01
#define REG_FREQUENCY_TUNING_WORD1			0x02
#define REG_FREQUENCY_TUNING_WORD2			0x03
#define DFW				0x04
#define UC				0x05
#define RRC				0x06
#define REG_CONTROL			0x07
#define OSKIM			0x08
#define OSKQM			0x09
#define OSKRR			0x0A
#define QDAC			0x0B


/* System clock fed to the AD9852 core.
 * Set PLL_BYPASS in the control register so SYSCLK == REFCLK == 66.667 MHz.
 * Adjust AD9852_SYSCLK_HZ and the CTRL bytes in ad9852_init() if you use the
 * on-chip PLL instead. */
// #define AD9852_SYSCLK_HZ    66667000UL
constexpr uint32 AD9852_SYSCLK_HZ = 20000000UL;

constexpr uint8 CLOCK_MULTIPLIER = 8;
static_assert(CLOCK_MULTIPLIER >= 4);
static_assert(CLOCK_MULTIPLIER <= 20);

/*
 * FTW = (desired_freq_hz * 2^48) / SYSCLK_HZ
 * Precompute the constant: FQ = 2^48 / SYSCLK_HZ
 */
static const double FQ = pow(2.0, 48.0) / static_cast<double>(AD9852_SYSCLK_HZ * CLOCK_MULTIPLIER);

/* Maximum safe output frequency: ~40 % of SYSCLK (comfortable DAC margin) */
static const uint32_t FREQ_MAX = (uint32_t) (AD9852_SYSCLK_HZ * CLOCK_MULTIPLIER * 0.4);


static void write_byte(const uint8_t data) {
    pinMode(AD9852_PIN_SDIO, OUTPUT);
    digitalWrite(AD9852_PIN_SCLK, LOW);

    for (int i = 7; i >= 0; i--) {
        digitalWrite(AD9852_PIN_SDIO, (data >> i) & 1);
        delayMicroseconds(1);
        digitalWrite(AD9852_PIN_SCLK, HIGH);
        delayMicroseconds(1);
        digitalWrite(AD9852_PIN_SCLK, LOW);
        delayMicroseconds(1);
    }
}


/* SDIO is bidirectional — switch to input for reads, restore to output after */
static uint8_t read_byte() {
    uint8_t data = 0;
    pinMode(AD9852_PIN_SDIO, INPUT);
    digitalWrite(AD9852_PIN_SCLK, LOW);

    for (int i = 7; i >= 0; i--) {
        digitalWrite(AD9852_PIN_SCLK, HIGH);
        delayMicroseconds(1);
        data |= (digitalRead(AD9852_PIN_SDIO) << i);
        digitalWrite(AD9852_PIN_SCLK, LOW);
        delayMicroseconds(1);
    }
    pinMode(AD9852_PIN_SDIO, OUTPUT);
    return data;
}

/* Reset the AD9852 internal serial-address pointer before each transaction */
static void io_reset() {
    digitalWrite(AD9852_PIN_IORESET, HIGH);
    delay(1);
    digitalWrite(AD9852_PIN_IORESET, LOW);
    delayMicroseconds(10);
}

static void chip_select() {
    digitalWrite(AD9852_PIN_SCB, LOW);
    delayMicroseconds(10);
}

static void chip_release() {
    digitalWrite(AD9852_PIN_SCB, HIGH);
}

/* Pulse I/O UD to latch written register values into the active registers */
static void io_update() {
    digitalWrite(AD9852_PIN_IOUD, HIGH);
    delayMicroseconds(10);
    digitalWrite(AD9852_PIN_IOUD, LOW);
    //delayMicroseconds(10);
}

void AD9854_SendData(u8 _register, u8 const *data, u8 ByteNum) {
    io_reset();
    write_byte(_register);
    for (int i = 0; i < ByteNum; i++) {
        write_byte(data[i]);
    }
}

static void master_reset() {
    digitalWrite(AD9852_PIN_MRESET, HIGH);
    delay(100);
    digitalWrite(AD9852_PIN_MRESET, LOW);
    delay(100);
}

void ad9852::init() {
    pinMode(AD9852_PIN_MRESET, OUTPUT);
    pinMode(AD9852_PIN_SCB, OUTPUT);
    pinMode(AD9852_PIN_IORESET, OUTPUT);
    pinMode(AD9852_PIN_SCLK, OUTPUT);
    pinMode(AD9852_PIN_SDIO, OUTPUT);
    pinMode(AD9852_PIN_IOUD, OUTPUT);

    digitalWrite(AD9852_PIN_MRESET, LOW); /* reset not asserted */
    chip_release();
    digitalWrite(AD9852_PIN_IORESET, LOW);
    digitalWrite(AD9852_PIN_SCLK, LOW);
    digitalWrite(AD9852_PIN_SDIO, LOW);
    digitalWrite(AD9852_PIN_IOUD, LOW);

    chip_select();
    master_reset();
    chip_release();

    static const uint8_t ctrl[] = {
        0b00010100, // 0x14
        0b00000000 | CLOCK_MULTIPLIER, // 0x20
        0b00000000, // 0x00
        0b00000000, // 0x00
    };

    chip_select();
    io_reset();
    AD9854_SendData(REG_CONTROL, ctrl, 4);
    delay(50);
    io_update();
    chip_release();

    delay(100);
}

void ad9852::setFrequency(double freqHz) {
    if (freqHz > FREQ_MAX) {
        freqHz = FREQ_MAX;
    }

    /* Frequency tuning word (48-bit) */
    uint64_t ftw = (uint64_t) round(FQ * freqHz);

    chip_select();
    io_reset();
    write_byte(REG_FREQUENCY_TUNING_WORD1); /* FREQ TUNING WORD 1 register */
    delayMicroseconds(10);
    /* Send 6 bytes MSB first (bits 47..0) */
    for (int shift = 40; shift >= 0; shift -= 8) {
        write_byte((uint8_t) (ftw >> shift));
    }
    write_byte(0x00); /* trailing pad byte, matches original */
    io_update();
    chip_release();
}
