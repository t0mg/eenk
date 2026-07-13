#pragma once
#include <Arduino.h>

class WifiProvisioner {
public:
    // Reads the first two lines of /wifi.txt on the SD card into ssid and password
    static bool readCredentialsFromSD(String& ssid, String& password);

    // Connects to WiFi, blocks until connected or timeout
    static bool connect(const String& ssid, const String& password);
};
