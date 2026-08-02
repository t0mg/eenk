#include <Arduino.h>
#include "HalTypes.h"
#include "HalInit.h"
// SD.h included via HalTypes.h
#include <SPI.h>
#include <Update.h>
#include "WifiProvisioner.h"
#include <esp_ota_ops.h>
#include <GfxRenderer.h>
#include <EpdFont.h>
#include <builtinFonts/ui_12.h>
#include <cstring>

EspEinkDisplay* display = nullptr;

static EpdFont s_font12(&ui_12);
static EpdFontFamily s_fam12(&s_font12);

static constexpr int kUpdaterFontId   = 0;
static constexpr int kUpdaterMarginX  = 16;
static constexpr int kUpdaterMarginY  = 20;
static constexpr int kUpdaterDispW    = 480;   // portrait logical width
static constexpr int kUpdaterLineH    = 20;    // approximate line height for ui_12

// Render a message string on the display, honouring '\n' and wrapping long
// lines at the right margin. Keeps the updater partition lean — no heap
// allocation, no GfxRenderer wrap helpers.
void drawMessage(const char* msg) {
    if (!display) return;
    auto* r = display->getRenderer();
    r->insertFont(kUpdaterFontId, s_fam12);
    display->clear();

    int y = kUpdaterMarginY;
    const int maxWidth = kUpdaterDispW - 2 * kUpdaterMarginX;
    const int spaceW = r->getSpaceWidth(kUpdaterFontId);

    const char* p = msg;
    while (*p) {
        // Find end of this paragraph (next '\n' or end of string)
        const char* nl = p;
        while (*nl && *nl != '\n') ++nl;

        // Wrap the paragraph text
        const char* paraStart = p;
        while (paraStart < nl) {
            // Find how many words fit in this line
            const char* lineEnd = paraStart;
            const char* wordStart = paraStart;
            int currentW = 0;

            while (wordStart < nl) {
                // Find end of current word
                const char* wordEnd = wordStart;
                while (wordEnd < nl && *wordEnd != ' ') ++wordEnd;

                // Measure word
                char wordBuf[64];
                size_t wordLen = wordEnd - wordStart;
                if (wordLen >= sizeof(wordBuf)) wordLen = sizeof(wordBuf) - 1;
                memcpy(wordBuf, wordStart, wordLen);
                wordBuf[wordLen] = '\0';
                
                int wordW = r->getTextWidth(kUpdaterFontId, wordBuf);
                
                if (currentW + wordW > maxWidth && currentW > 0) {
                    // Word doesn't fit and it's not the first word, break line here
                    break;
                }
                
                // Add word to current line
                currentW += wordW + spaceW;
                lineEnd = wordEnd;
                
                // Skip spaces
                wordStart = wordEnd;
                while (wordStart < nl && *wordStart == ' ') ++wordStart;
            }

            if (lineEnd == paraStart) {
                // Word is longer than maxWidth, force break it or skip
                lineEnd = paraStart;
                while (lineEnd < nl && *lineEnd != ' ') ++lineEnd;
            }

            // Draw the line
            char lineBuf[128];
            size_t lineLen = lineEnd - paraStart;
            if (lineLen >= sizeof(lineBuf)) lineLen = sizeof(lineBuf) - 1;
            memcpy(lineBuf, paraStart, lineLen);
            lineBuf[lineLen] = '\0';

            if (lineLen > 0) {
                r->drawText(kUpdaterFontId, kUpdaterMarginX, y, lineBuf, true);
            }
            y += kUpdaterLineH;

            // Advance to next line in paragraph
            paraStart = lineEnd;
            while (paraStart < nl && *paraStart == ' ') ++paraStart;
        }

        if (p == nl) {
            // Empty line (just '\n')
            y += kUpdaterLineH;
        }

        // Advance past the newline (if any)
        p = *nl ? nl + 1 : nl;
    }

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
    
            drawMessage("Update complete!\nVerifying...");
            if (Update.end()) {
                drawMessage("Success!\nRebooting to new firmware...");
                updateFile.close();
                if (SD_FS.exists("/firmware.bin.bak")) {
                    SD_FS.remove("/firmware.bin.bak");
                }
                SD_FS.rename("/firmware.bin", "/firmware.bin.bak");
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
    
    drawMessage("OTA Updater Started...\nMounting SD Card...");

    // Mount SD card using correct pins (SCLK=8, MISO=7, MOSI=10, SD_CS=12)
    // EspEinkDisplay already called SPI.begin(8, 7, 10, 21), we can just use SD.begin
    if (!HalInit::mountSdForUpdater()) {
        drawMessage("Error: SD Card Mount Failed.\nPlease insert SD card and restart.\nRebooting in 5s...");
        delay(5000);
        bootToApp0();
        return;
    }
    Serial.println("SD card mounted successfully.");

       // EspSdmmcStorage already called SD_MMC.begin() if we are on X4 Pro.
    // On X4, HalInit::earlyBootCheck already initialized SD.
    // So we don't need to call SD_FS.begin() here!

    if (SD_FS.exists("/firmware.bin")) {
        File updateFile = SD_FS.open("/firmware.bin", FILE_READ);
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
