#include <Arduino.h>
#include "hal/esp32/EspEinkDisplay.h"
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include "WifiProvisioner.h"
#include <esp_ota_ops.h>
#include <GfxRenderer.h>
#include <EpdFont.h>
#include <builtinFonts/ui_12.h>

EspEinkDisplay* display = nullptr;

static EpdFont s_font12(&ui_12);
static EpdFontFamily s_fam12(&s_font12);

void drawMessage(const char* msg) {
    if (!display) return;
    auto* r = display->getRenderer();
    display->clear();
    r->drawText(0, 10, 100, msg, true); // true = black text
    display->present();
    Serial.println(msg);
}

void bootToApp0() {
    const esp_partition_t* app0_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (app0_part) {
        esp_ota_set_boot_partition(app0_part);
    }
    esp_restart();
}

void performOfflineUpdate(File& updateFile) {
    size_t updateSize = updateFile.size();
    drawMessage("Offline Update Found.\nFlashing firmware...");
    
    if (Update.begin(updateSize, U_FLASH)) {
        size_t written = Update.writeStream(updateFile);
        if (written == updateSize) {
            drawMessage("Update complete!\nVerifying...");
            if (Update.end()) {
                drawMessage("Success!\nRebooting to new firmware...");
                updateFile.close();
                SD.rename("/firmware.bin", "/firmware.bin.bak");
                delay(2000);
                esp_restart(); // Update library already sets boot partition to app0
                return;
            } else {
                drawMessage((String("Update Error: ") + Update.getError()).c_str());
            }
        } else {
            drawMessage("Error writing to flash.");
        }
    } else {
        drawMessage("Not enough space to begin OTA.");
    }
    
    delay(5000);
    bootToApp0();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== eenk OTA Updater (app1) ===");

    display = new EspEinkDisplay();
    auto* r = display->getRenderer();
    r->insertFont(0, s_fam12);
    
    drawMessage("OTA Updater Started...\nMounting SD Card...");

    // Mount SD card using correct pins (SCLK=8, MISO=7, MOSI=10, SD_CS=12)
    // EspEinkDisplay already called SPI.begin(8, 7, 10, 21), we can just use SD.begin
    if (!SD.begin(12, SPI, 40000000)) {
        drawMessage("Error: SD Card Mount Failed.\nPlease insert SD card and restart.\nRebooting in 5s...");
        delay(5000);
        bootToApp0();
        return;
    }
    Serial.println("SD card mounted successfully.");

    // Check for offline update
    if (SD.exists("/firmware.bin")) {
        File updateFile = SD.open("/firmware.bin", FILE_READ);
        if (updateFile) {
            performOfflineUpdate(updateFile);
            return;
        }
    }
    Serial.println("No offline update found.");

    String ssid, password;
    if (!WifiProvisioner::readCredentialsFromSD(ssid, password)) {
        drawMessage("Error: wifi.txt not found on SD card.\nPlease create /wifi.txt with:\nSSID\nPASSWORD\nRebooting in 5s...");
        delay(5000);
        bootToApp0();
        return;
    }

    String connectingMsg = String("Connecting to WiFi: ") + ssid + "...";
    drawMessage(connectingMsg.c_str());

    if (!WifiProvisioner::connect(ssid, password)) {
        drawMessage("Error: WiFi Connection Failed.\nCheck credentials in /wifi.txt.\nRebooting in 5s...");
        delay(5000);
        bootToApp0();
        return;
    }

    drawMessage("Connected to WiFi!\nLooking for updates...");
    
    // TODO: Implement actual HTTP OTA download
    // For now, just simulate a check and boot back to app0
    delay(2000);
    drawMessage("Update check complete.\nRebooting to main system...");
    delay(3000);
    
    bootToApp0();
}

void loop() {
    delay(1000);
}
