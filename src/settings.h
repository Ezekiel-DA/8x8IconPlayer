#pragma once

#include <FastLED.h>

#define WORD_LEDS_PIN 12
#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define NUM_LEDS MATRIX_WIDTH* MATRIX_HEIGHT

const char* iconViewerDNSName = "iconviewer";

const CRGB correctionMap[] = { UncorrectedColor, TypicalSMD5050, TypicalLEDStrip, Typical8mmPixel, TypicalPixelString };
const CRGB temperatureMap[] = { UncorrectedTemperature, Candle, Tungsten40W, Tungsten100W, Halogen, CarbonArc, HighNoonSun, DirectSunlight, OvercastSky, ClearBlueSky,
    WarmFluorescent, StandardFluorescent, CoolWhiteFluorescent, FullSpectrumFluorescent, GrowLightFluorescent, BlackLightFluorescent, MercuryVapor, SodiumVapor, MetalHalide, HighPressureSodium
};
