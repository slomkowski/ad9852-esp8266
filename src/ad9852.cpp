#include "ad9852.hpp"
#include <Arduino.h>
#include <math.h>

#define AD9852_PIN_MRESET   12
#define AD9852_PIN_SCB      15
#define AD9852_PIN_IORESET  13
#define AD9852_PIN_SCLK     2
#define AD9852_PIN_IOUD     4
#define AD9852_PIN_SDIO     5

#define REG_PHASE_ADJUST_REGISTER1  0x00
#define REG_PHASE_ADJUST_REGISTER2  0x01
#define REG_FREQUENCY_TUNING_WORD1  0x02
#define REG_FREQUENCY_TUNING_WORD2  0x03
#define DFW     0x04
#define UC      0x05
#define RRC     0x06
#define REG_CONTROL 0x07
#define OSKIM   0x08
#define OSKQM   0x09
#define OSKRR   0x0A
#define QDAC    0x0B

/* Reference clock fed to the AD9852. SYSCLK = AD9852_REFCLK_HZ * multiplier. */
static constexpr uint32_t AD9852_REFCLK_HZ = 20000000UL;

/* 2^48 as a compile-time constant — avoids runtime pow() call */
static const double FQ = pow(2.0, 48.0) / static_cast<double>(AD9852_REFCLK_HZ);


/* Runtime state */
static uint8_t s_mult = 8;
static double s_freq = 0.0;


static void writeByte(const uint8_t data) {
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

static void ioReset() {
    digitalWrite(AD9852_PIN_IORESET, HIGH);
    delayMicroseconds(10);
    digitalWrite(AD9852_PIN_IORESET, LOW);
    delayMicroseconds(10);
}

static void chipSelect() {
    digitalWrite(AD9852_PIN_SCB, LOW);
    delayMicroseconds(10);
}

static void chipRelease() {
    digitalWrite(AD9852_PIN_SCB, HIGH);
}

static void ioUpdate() {
    digitalWrite(AD9852_PIN_IOUD, HIGH);
    delayMicroseconds(10);
    digitalWrite(AD9852_PIN_IOUD, LOW);
}

static void masterReset() {
    digitalWrite(AD9852_PIN_MRESET, HIGH);
    delay(10);
    digitalWrite(AD9852_PIN_MRESET, LOW);
    delay(10);
}

void writeData(const u8 reg, u8 const *data, const u8 byteNum) {
    ioReset();
    writeByte(reg);
    for (u8 i = 0; i < byteNum; i++) {
        writeByte(data[i]);
    }
}

static void writeToControlRegister() {
    const uint8_t ctrl[4] = {
        0b00010100,
        s_mult,
        0b00000000,
        0b00000000,
    };

    chipSelect();
    writeData(REG_CONTROL, ctrl, 4);
    delay(1);
    ioUpdate();
    chipRelease();
}

void ad9852::init() {
    pinMode(AD9852_PIN_MRESET, OUTPUT);
    pinMode(AD9852_PIN_SCB, OUTPUT);
    pinMode(AD9852_PIN_IORESET, OUTPUT);
    pinMode(AD9852_PIN_SCLK, OUTPUT);
    pinMode(AD9852_PIN_SDIO, OUTPUT);
    pinMode(AD9852_PIN_IOUD, OUTPUT);

    digitalWrite(AD9852_PIN_MRESET, LOW);
    chipRelease();
    digitalWrite(AD9852_PIN_IORESET, LOW);
    digitalWrite(AD9852_PIN_SCLK, LOW);
    digitalWrite(AD9852_PIN_SDIO, LOW);
    digitalWrite(AD9852_PIN_IOUD, LOW);

    chipSelect();
    masterReset();
    chipRelease();

    writeToControlRegister();
    delay(100);
}

void ad9852::setFrequency(double freqHz) {
    /* Frequency tuning word (48-bit) */
    s_freq = freqHz;

    uint64_t ftw = (uint64_t) round(FQ / s_mult * freqHz);

    chipSelect();
    ioReset();
    writeByte(REG_FREQUENCY_TUNING_WORD1);
    delayMicroseconds(10);
    for (int shift = 40; shift >= 0; shift -= 8) {
        writeByte((uint8_t) (ftw >> shift));
    }
    writeByte(0x00);
    ioUpdate();
    chipRelease();
}

double ad9852::getFrequency() {
    return s_freq;
}

void ad9852::setMultiplier(int mult) {
    if (mult < 1) mult = 1;
    if (mult > 15) mult = 15;
    s_mult = mult;
    writeToControlRegister();
    delay(50); // wait for PLL to relock
    setFrequency(s_freq); // re-tune FTW for new SYSCLK, keeping output freq constant
}

int ad9852::getMultiplier() {
    return s_mult;
}
