#pragma once

#include <FastLED.h>

uint8_t xy(uint8_t x, uint8_t y) {
    return (y % 2 == 0) ? ((y + 1) * 8 - x - 1) : (y * 8 + x);
}

void setAllLEDs(CRGB c, CRGB* strip, uint16_t numLeds) {
    for (uint16_t i = 0; i < numLeds; ++i) {
        strip[i] = c;
    }
}
