#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WiFiProv.h>

#define WORD_LEDS_PIN 12
#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define NUM_LEDS MATRIX_WIDTH* MATRIX_HEIGHT

#define MAX_FRAMES 40
#define RECV_BUFFER_SIZE (MAX_FRAMES * (NUM_LEDS*3 + 4)) + 1 // 1 byte for num frames, then, PER FRAME: (64 pixels * R,G,B) + 4 bytes for duration

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

void SysProvEvent(arduino_event_t* sys_event) {
    switch (sys_event->event_id) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("\nConnected IP address : ");
            Serial.println(IPAddress(sys_event->event_info.got_ip.ip_info.ip.addr));
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("\nDisconnected. Connecting to the AP again...");
            break;
        case ARDUINO_EVENT_PROV_START:
            Serial.println("\nProvisioning started\nProvide Wifi credentials with the ESP BLE Prov app");
            break;
        case ARDUINO_EVENT_PROV_CRED_RECV: {
            Serial.println("\nReceived Wi-Fi credentials");
            Serial.print("\tSSID: ");
            Serial.println((const char*)sys_event->event_info.prov_cred_recv.ssid);
            Serial.print("\tPassword: <REDACTED>");
            break;
        }
        case ARDUINO_EVENT_PROV_CRED_FAIL: {
            Serial.println("\nProvisioning failed!\nPlease reset to factory and retry provisioning\n");
            if (sys_event->event_info.prov_fail_reason == WIFI_PROV_STA_AUTH_ERROR)
                Serial.println("\nWi-Fi AP password incorrect");
            else
                Serial.println("\nWi-Fi AP not found....Add API \" nvs_flash_erase() \" before beginProvision()");
            break;
        }
        case ARDUINO_EVENT_PROV_CRED_SUCCESS:
            Serial.println("\nProvisioning Successful");
            break;
        case ARDUINO_EVENT_PROV_END:
            Serial.println("\nProvisioning Ends");
            break;
        default:
            break;
    }
}

uint8_t xy(uint8_t x, uint8_t y) {
    return (y % 2 == 0) ? ((y + 1) * 8 - x - 1) : (y * 8 + x);
}

void setAllLEDs(CRGB c, CRGB* strip, uint16_t numLeds) {
    for (uint16_t i = 0; i < numLeds; ++i) {
        strip[i] = c;
    }
}

void setup() {
    Serial.begin(115200);

    FastLED.addLeds<WS2812B, WORD_LEDS_PIN, GRB>(_leds, NUM_LEDS).setCorrection(TypicalLEDStrip);  // don't add FastLED's "typical LED" correction; it makes the whites pink-ish
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 300);
    FastLED.setBrightness(50);
    setAllLEDs(CRGB::Black, _leds, NUM_LEDS);
    FastLED.show();

    WiFi.onEvent(SysProvEvent);
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, "test", "PROV_iconViewer");

    while (WiFi.status() != WL_CONNECTED) {
        delay(50);
    }

    if (!MDNS.begin("iconviewer")) {
        Serial.println("Error setting up MDNS responder!");
        while (1) {
            delay(1000);
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

            // TODO: more error handling! bail if message too large, too small, etc. Without this, this is probably full of dangerous buffer overflows and UMRs etc.
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
        // ICON PROTOCOL DEFINITION:
        // byte 0: number of frames
        // bytes 1-4: 32 bit int duration of frame in ms
        // bytes 5-192: 8x8x3 frame data (row major, 0 in top left, RGB (no alpha channel) color data for one frame)
        // bytes 193-196: duration of next frame
        // ... etc
        uint32_t idx = 0;
        numFrames = receiveBuffer[idx++];

        for (uint8_t frameIdx = 0; frameIdx < numFrames; ++frameIdx) {
            memcpy(&(iconData[frameIdx].duration), &(receiveBuffer[idx]), 4);
            idx += 4;
            memcpy(iconData[frameIdx].frameData, &(receiveBuffer[idx]), 192);
            idx += 192;
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
