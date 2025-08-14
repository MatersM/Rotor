#pragma once
#include <Arduino.h>

class ESP32ServoLite {
public:
    ESP32ServoLite() : _attached(false), _channel(-1), _pin(-1), _minPulse(500), _maxPulse(2500) {}

    // Attach to a pin with optional min/max pulse width
    int attach(int pin, int minPulse=500, int maxPulse=2500) {
        if (_attached) return _channel;  // already attached

        _pin = pin;
        _minPulse = minPulse;
        _maxPulse = maxPulse;

        // Find a free LEDC channel (0..15)
        for (int ch=0; ch<16; ++ch) {
            if (!_usedChannels[ch]) {
                _channel = ch;
                _usedChannels[ch] = true;
                break;
            }
        }
        if (_channel < 0) return -1; // no free channel

        // 50 Hz, 16-bit resolution
        ledcSetup(_channel, 50, 16);
        ledcAttachPin(_pin, _channel);

        _attached = true;
        return _channel;
    }

    void detach() {
        if (_attached) {
            ledcDetachPin(_pin);
            _usedChannels[_channel] = false;
            _attached = false;
        }
    }

    void write(int angle) {
        // Convert 0..180° to microseconds
        int us = map(angle, 0, 180, _minPulse, _maxPulse);
        writeMicroseconds(us);
    }

    void writeMicroseconds(int us) {
        if (!_attached) return;
        us = constrain(us, _minPulse, _maxPulse);

        // 50Hz period = 20ms = 20000 µs
        uint32_t duty = (uint32_t)us * 65536 / 20000;
        ledcWrite(_channel, duty);
    }

    int attached() { return _attached; }

private:
    bool _attached;
    int _channel;
    int _pin;
    int _minPulse, _maxPulse;

    // Track used LEDC channels globally
    static bool _usedChannels[16];
};

// Initialize static member
bool ESP32ServoLite::_usedChannels[16] = {false};