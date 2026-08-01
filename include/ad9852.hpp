#pragma once
#include <cstdint>

namespace ad9852 {
    void init();

    void setFrequency(double freqHz);

    double getFrequency();

    void setMultiplier(int mult);

    int getMultiplier();
}
