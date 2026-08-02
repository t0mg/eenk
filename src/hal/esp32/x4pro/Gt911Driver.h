#pragma once
#ifdef PLATFORM_ESP32

#include <cstdint>

class Gt911Driver {
public:
    Gt911Driver(int sda = 39, int scl = 38, int intPin = 10, int rstPin = 4);

    bool begin();

    struct TouchState {
        bool touched = false;
        uint16_t x = 0;
        uint16_t y = 0;
        bool homePressed = false;
    };

    TouchState readState();

private:
    int _sda, _scl, _intPin, _rstPin;
    uint8_t _addr = 0x5D;
    bool _initialized = false;

    void reset();
    bool writeReg(uint16_t reg, uint8_t val);
    bool readRegs(uint16_t reg, uint8_t* buf, uint8_t len);
};

#endif
