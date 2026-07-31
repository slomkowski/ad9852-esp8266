#pragma once
#include <stdint.h>

namespace ad9852 {
    void init();
    void setFrequency(double freqHz);
    void setMultiplier(uint8_t mult);  // updates ctrl reg, re-tunes to keep output freq constant
    uint8_t getMultiplier();
}