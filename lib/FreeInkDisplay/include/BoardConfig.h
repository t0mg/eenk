#pragma once

#include <Arduino.h>

// Shim BoardConfig to satisfy FreeInk SDK display driver dependencies
// without pulling in the entire FreeInk framework.

#define FREEINK_DEVICE_X4PRO 1
#define FREEINK_DRIVER_SSD1677 1
#define FREEINK_DRIVER_UC8179 1
#define FREEINK_DRIVER_UC8253_X3 0
#define FREEINK_DRIVER_UC8279 0
#define FREEINK_DRIVER_ED2208 0
#define FREEINK_DRIVER_M5_OFFICIAL 0
#define FREEINK_DRIVER_UC8253_MURPHY 0
#define FREEINK_DRIVER_LGFX_EPD 0
#define FREEINK_DRIVER_IT8951 0

#define FREEINK_MCU_S3 1
#define FREEINK_FB_PSRAM 1

namespace BoardConfig {

    struct DisplayPins {
        int8_t sclk;
        int8_t mosi;
        int8_t miso;
        int8_t cs;
        int8_t dc;
        int8_t rst;
        int8_t busy;
        int8_t powerEnable = -1;
    };

    struct TouchController {
        enum Type { None, Gt911 };
        Type type = Type::None;
    };

    struct TouchPins {
        TouchController::Type controller = TouchController::None;
        int8_t sda = -1;
        int8_t scl = -1;
        int8_t intPin = -1;
        int8_t rst = -1;
        bool hasHomeKey = false;
    };
    
    struct InputConfig {
        int8_t power = -1;
        bool powerActiveHigh = false;
    };
    
    struct FrontlightConfig {
        int8_t gpio = -1;
    };

    struct AudioConfig {
        enum Output { None };
        Output output = Output::None;
        int8_t buzzer = -1;
    };

    struct MicConfig {
        enum Input { None };
        Input input = Input::None;
    };

    struct SensorConfig {
        uint8_t rtcAddr = 0;
        uint8_t tempHumidityAddr = 0;
        uint8_t imuAddr = 0;
        enum ImuType { None };
        ImuType imuType = ImuType::None;
    };
    
    struct LedConfig {
        int8_t data = -1;
        int count = 0;
    };

    struct SdConfig {
        int8_t cs = -1;
        int8_t sclk = -1;
        int8_t mosi = -1;
        int8_t miso = -1;
        bool separateSpi = false;
        int8_t powerEnable = -1;
        bool powerActiveHigh = false;
        uint32_t spiHz = 0;
    };

    enum class DisplayController { 
        SSD1677, 
        UC8179, 
        UC8253, 
        UC8279 
    };

    enum class Board { 
        XteinkX4Pro,
        XteinkX3Uc8279,
        XteinkX3,
        Sticky,
        DeLink,
        XteinkX4
    };

    struct PowerLatch {
        int8_t latch0 = -1;
        int8_t latch1 = -1;
    };

    struct BoardProfile {
        const char* name;
        Board board;
        
        uint16_t displayWidth;
        uint16_t displayHeight;
        uint32_t displaySpiHz;
        DisplayController displayController;
        
        struct {
            bool mirrorX;
            bool mirrorY;
        } orientation;

        DisplayPins display;
        SdConfig sd;
        InputConfig input;
        FrontlightConfig frontlight;
        AudioConfig audio;
        MicConfig mic;
        SensorConfig sensors;
        LedConfig leds;
        TouchPins touch;
        PowerLatch power;
    };

    extern BoardProfile ACTIVE;
    constexpr uint32_t MAX_FRAMEBUFFER_BYTES = 800 * 600 / 8; // Safely fits largest panel

    inline void selectDevice(Board b) {
        if (b == Board::XteinkX3Uc8279 || b == Board::XteinkX3) {
            ACTIVE.displayWidth = 792;
            ACTIVE.displayHeight = 528;
            ACTIVE.displayController = DisplayController::UC8279;
        }
    }
}
