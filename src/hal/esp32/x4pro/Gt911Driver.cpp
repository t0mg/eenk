#ifdef PLATFORM_ESP32

#include "Gt911Driver.h"
#include <Arduino.h>
#include <Wire.h>

#define GT911_REG_STATUS 0x814E
#define GT911_REG_POINT1 0x814F

Gt911Driver::Gt911Driver(int sda, int scl, int intPin, int rstPin)
    : _sda(sda), _scl(scl), _intPin(intPin), _rstPin(rstPin) {}

void Gt911Driver::reset() {
    pinMode(_intPin, OUTPUT);

    if (_rstPin >= 0) {
        pinMode(_rstPin, OUTPUT);
        // INT low for address 0x5D
        digitalWrite(_rstPin, LOW);
        digitalWrite(_intPin, LOW);
        delay(10);

        digitalWrite(_rstPin, HIGH);
        delay(10);
    } else {
        digitalWrite(_intPin, LOW);
        delay(20);
    }

    pinMode(_intPin, INPUT);
    delay(50);
}

bool Gt911Driver::begin() {
    if (_initialized) return true;

    Wire.begin(_sda, _scl, 400000);
    reset();

    // Probe address 0x5D (or 0x14)
    Wire.beginTransmission(_addr);
    if (Wire.endTransmission() != 0) {
        _addr = 0x14;
        Wire.beginTransmission(_addr);
        if (Wire.endTransmission() != 0) {
            Serial.println("[GT911] Touch controller not responding at 0x5D or 0x14.");
            return false;
        }
    }

    Serial.printf("[GT911] Found touch controller at 0x%02X\n", _addr);
    _initialized = true;
    return true;
}

bool Gt911Driver::writeReg(uint16_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
}

bool Gt911Driver::readRegs(uint16_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(_addr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    if (Wire.endTransmission() != 0) return false;

    uint8_t count = Wire.requestFrom(_addr, len);
    if (count != len) return false;

    for (uint8_t i = 0; i < len; ++i) {
        buf[i] = Wire.read();
    }
    return true;
}

Gt911Driver::TouchState Gt911Driver::readState() {
    TouchState state;
    if (!_initialized && !begin()) return state;

    uint8_t status = 0;
    if (!readRegs(GT911_REG_STATUS, &status, 1)) return state;

    if (status & 0x80) { // Buffer ready
        uint8_t points = status & 0x0F;
        state.homePressed = (status & 0x10) != 0;

        if (points > 0) {
            uint8_t ptBuf[8];
            if (readRegs(GT911_REG_POINT1, ptBuf, 8)) {
                state.touched = true;
                state.x = ptBuf[1] | (ptBuf[2] << 8);
                state.y = ptBuf[3] | (ptBuf[4] << 8);
            }
        }

        // Clear status flag
        writeReg(GT911_REG_STATUS, 0x00);
    }

    return state;
}

#endif
