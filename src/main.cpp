#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WiFiProv.h>
#include <tuple>

#include "settings.h"
#include "wifiProvisioningEvent.h"

#define MAX_FRAMES 50
#define RECV_BUFFER_SIZE (MAX_FRAMES * (NUM_LEDS * 3 + 4)) + 1  // 1 byte for num frames, then, PER FRAME: (64 pixels * R,G,B) + 4 bytes for duration

struct AnimationFrame {
    uint8_t frameData[NUM_LEDS * 3] = {0};
    uint32_t duration = 0;
};

uint8_t receiveBuffer[RECV_BUFFER_SIZE] = {0};

AnimationFrame iconData[MAX_FRAMES];
uint8_t numFrames = 0;
bool newData = false;
bool isDataPresent = false;

CRGB _leds[NUM_LEDS];

AsyncWebServer server(80);

uint8_t xy(uint8_t x, uint8_t y) {
    return (y % 2 == 0) ? ((y + 1) * 8 - x - 1) : (y * 8 + x);
}

void setAllLEDs(CRGB c, CRGB* strip, uint16_t numLeds) {
    for (uint16_t i = 0; i < numLeds; ++i) {
        strip[i] = c;
    }
}

void handleOptions(AsyncWebServerRequest* request) {
    if (request->method() == HTTP_OPTIONS) {
        request->send(200);
        return;
    } else {
        request->send(405);
        return;
    }
};

std::tuple<bool, uint8_t> handlePut(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (request->method() != HTTP_PUT) {
        request->send(405);
        return {false, 0};
    }

    if (total == 0) {
        request->send(400);
        return {false, 0};
    }

    request->send(200);

    char input[4] = {0};
    strncpy(input, (char*)data, 3);
    uint8_t val = atoi(input);
    return {true, val};
}

void setup() {
    Serial.begin(115200);

    FastLED.addLeds<WS2812B, WORD_LEDS_PIN, GRB>(_leds, NUM_LEDS);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
    FastLED.setBrightness(255);
    setAllLEDs(CRGB::Black, _leds, NUM_LEDS);
    FastLED.show();

    WiFi.onEvent(SysProvEvent);
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, "test", "PROV_iconViewer");

    while (WiFi.status() != WL_CONNECTED) {
        delay(50);
    }

    if (!MDNS.begin(iconViewerDNSName)) {
        Serial.println("Error setting up MDNS responder!");
        while (1) {
            delay(50);
        }
    }
    Serial.println("mDNS responder started");

    for (uint8_t idx = 0; idx < 64; ++idx) {
        setAllLEDs(CRGB::Black, _leds, NUM_LEDS);
        _leds[idx] = CRGB::Red;
        FastLED.show();
        delay(10);
    }

    setAllLEDs(CRGB::Black, _leds, NUM_LEDS);
    FastLED.show();

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "*");

    server.on(
        "/brightness",
        HTTP_ANY,
        handleOptions,
        NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            const auto [ res, val ] = handlePut(request, data, len, index, total);
            if (!res)
                return;

            Serial.print("Setting brightness: ");
            Serial.println(val);

            FastLED.setBrightness(val);
            FastLED.show();
        });

    server.on(
        "/correction",
        HTTP_ANY,
        handleOptions,
        NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            const auto [ res, val ] = handlePut(request, data, len, index, total);
            if (!res)
                return;

            Serial.print("Setting correction: ");
            Serial.println(val);

            FastLED.setCorrection(correctionMap[val]);
            FastLED.show();
        });

    server.on(
        "/temperature",
        HTTP_ANY,
        handleOptions,
        NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            const auto [ res, val ] = handlePut(request, data, len, index, total);
            if (!res)
                return;

            Serial.print("Setting temperature: ");
            Serial.println(val);

            FastLED.setTemperature(temperatureMap[val]);
            FastLED.show();
        });

    server.on(
        "/icon",
        HTTP_ANY,
        [](AsyncWebServerRequest* request) {
            if (request->method() == HTTP_OPTIONS) {
                request->send(200);
                return;
            } else if (request->method() == HTTP_DELETE) {
                isDataPresent = false;
                request->send(200);
                return;
            } else {
                request->send(405);
                return;
            }
        },
        NULL,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            Serial.print("Body handler; length: ");
            Serial.print(len);
            Serial.print("; index: ");
            Serial.print(index);
            Serial.print("; total: ");
            Serial.println(total);
            if (request->method() != HTTP_POST) {
                request->send(405);
                return;
            }

            // bail if message is too large
            // TODO: also handle other error cases
            if (total > RECV_BUFFER_SIZE) {
                request->send(413);
                return;
            }

            request->send(200);

            // the body might have gotten chunked, we'll reconstruct it and flag our work as done once we have our total number of bytes
            memcpy(&(receiveBuffer[index]), data, len);
            if (len + index == total) {
                newData = true;
            }
        });

    server.begin();
}

void displayFrame(uint8_t frameIdx) {
    setAllLEDs(CRGB::Black, _leds, NUM_LEDS);
    for (uint8_t y = 0; y < 8; ++y) {
        for (uint8_t x = 0; x < 8; ++x) {
            auto idx = y * 8 + x;
            _leds[xy(x, y)] = CRGB(iconData[frameIdx].frameData[(idx * 3) + 0], iconData[frameIdx].frameData[(idx * 3) + 1], iconData[frameIdx].frameData[(idx * 3) + 2]);
        }
    }
    FastLED.show();
}

void loop() {
    static uint8_t currentFrame = 0;
    static bool alreadyCleared = false;

    if (newData) {
        uint32_t idx = 0;
        numFrames = receiveBuffer[idx++];

        for (uint8_t frameIdx = 0; frameIdx < numFrames; ++frameIdx) {
            memcpy(&(iconData[frameIdx].duration), &(receiveBuffer[idx]), 4);
            idx += 4;
            memcpy(iconData[frameIdx].frameData, &(receiveBuffer[idx]), 3 * NUM_LEDS);
            idx += 3 * NUM_LEDS;
        }

        newData = false;
        currentFrame = 0;
        isDataPresent = true;
        alreadyCleared = false;
        Serial.print("Displaying new icon; ");
        Serial.print(numFrames);
        Serial.println(" frames available.");

        displayFrame(currentFrame);
    }

    if (isDataPresent) {
        uint16_t now = millis();
        static uint16_t prevFrameUpdateCheck = millis();

        if (now - prevFrameUpdateCheck > iconData[currentFrame].duration) {
            prevFrameUpdateCheck = now;

            currentFrame++;
            if (currentFrame == numFrames)
                currentFrame = 0;
            displayFrame(currentFrame);
        }
    } else {
        if (!alreadyCleared) {
            setAllLEDs(CRGB::Black, _leds, NUM_LEDS);
            FastLED.show();
            alreadyCleared = true;
        }
    }
}
