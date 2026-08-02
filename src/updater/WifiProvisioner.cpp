#include "WifiProvisioner.h"
#include "HalTypes.h"
#include <WiFi.h>

bool WifiProvisioner::readCredentialsFromSD(String& ssid, String& password) {
    if (!SD_FS.exists("/wifi.txt")) {
        return false;
    }

    File f = SD_FS.open("/wifi.txt", FILE_READ);
    if (!f) {
        return false;
    }

    ssid = f.readStringUntil('\n');
    ssid.trim();
    password = f.readStringUntil('\n');
    password.trim();

    f.close();

    return ssid.length() > 0;
}

bool WifiProvisioner::connect(const String& ssid, const String& password) {
    WiFi.begin(ssid.c_str(), password.c_str());

    int timeoutSeconds = 15;
    while (WiFi.status() != WL_CONNECTED && timeoutSeconds > 0) {
        delay(1000);
        timeoutSeconds--;
    }

    return WiFi.status() == WL_CONNECTED;
}
