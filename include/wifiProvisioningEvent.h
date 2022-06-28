#pragma once

#include <WiFiProv.h>

// this probably shouldn't be necessary? See https://github.com/platformio/platform-espressif32/issues/380
#include "SimpleBLE.h"
SimpleBLE ble;

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