#pragma once
#include <ESP32ServoLite.h>
#include <EEPROM.h>
#include <esp_log.h>

#define EEPROM_SIZE         32  // Need to have a value here
#define UPDATE_INTERVAL     20  // ms, ~50Hz update rate
#define MAX_US_PER_SECOND   300 // limit speed in microseconds/sec

class RotorServo {

public:
    RotorServo() {
        /* Moved to init
        if (!EEPROM.begin(EEPROM_SIZE)) {
            log_e("Failed to initialise EEPROM, the servo might jump");
        } else {
            log_d("Succesful to initialise EEPROM");
        }
        */
    }

    ~RotorServo() {
        if (_init) _servo.detach();
    }

    bool init(int8_t pin, int8_t eepromAddress, int16_t min, int16_t max, int16_t degrees, int8_t direction, float offset) {


        if (!EEPROM.begin(EEPROM_SIZE)) {
            log_e("Failed to initialise EEPROM, the servo might jump");
        } else {
            log_d("Succesful to initialise EEPROM");
        }

        _errorString = "";
        if (_init) {
            _errorString = "Servo has already been initialized";
            log_e("%s", _errorString.c_str());
            return false;
        }

        _pin = pin;
        _eepromAddress = eepromAddress;

        if (_eepromAddress < 0 or _eepromAddress > EEPROM_SIZE-4) {
           _errorString = "EEPROM address out of range";
            log_e("%s", _errorString.c_str());
            return false;             
        }

        _targetPulse = _currentPulse = EEPROM.readInt(_eepromAddress);
        log_i("Target read from EEPROM: %d",_targetPulse);
        _min = min;
        _max = max;
        _degrees = degrees;
        _direction = direction;
        _offset = offset;

        // Bunch of other error checking here
        if (_pin<0 or _pin>39) { // This can't be right, can it?
            _errorString = "Pin number must be between 0 and 39";
            log_e("%s", _errorString.c_str());
            return false;   
        }

        if (_targetPulse < _min) { // Don't return error, but set to _min
            _targetPulse = _currentPulse = _min;
            _errorString = "Target read from EEPROM smaller than minimum value";
            log_w("%s", _errorString.c_str());
        }

        if (_targetPulse > _max) { // Don't return error, but set to _max
            _errorString = "Target read from EEPROM greater than minimum value";
            _targetPulse = _currentPulse = _max;
            log_w("%s", _errorString.c_str());
        }

        if (_min<300 or _min>1000 or _max>3500 or _max<1500) {
            _errorString = "Min/max out of range";
            log_e("%s", _errorString.c_str());
            return false; 
        }

        if (_direction != 1 and _direction != -1) {
            _errorString = "Direction can only be 1 or -1";
            log_e("%s", _errorString.c_str());
            return false;             
        }

        if (_degrees < 100 or _degrees > 360) {
           _errorString = "Degree setting out of range";
            log_e("%s", _errorString.c_str());
            return false;   
        }

        if (_offset<-360 or _offset>360) {
           _errorString = "Offset out of range";
            log_e("%s", _errorString.c_str());
            return false;   
        }

        _currentPulse--;    // Force a small movement on the first run(), it will move to the target again and store in EEPROM

        int s = _servo.attach((int)pin, (int)RotorServo::_min, (int)RotorServo::_max);
        _servo.writeMicroseconds(_currentPulse);
        _init = true;
        log_i("Pin=%d, Min=%d, Max=%d, Current Pulse=%d Return value=%d",(int)pin,(int)RotorServo::_min,(int)RotorServo::_max, _currentPulse, s);
        log_i("Servo on pin %d succesfully initialized.",(int)pin);
        return true;
    }

    bool move(int16_t steps) {
        _errorString = "";

        if (!_init) {
            _errorString = "Call init first!";
            log_e("%s", _errorString.c_str());
            return false;
        }

        _targetPulse += _direction * steps;

        if (_targetPulse < _min) {
            _targetPulse = _min;
            _errorString = "Target smaller than minimum";
            log_e("%s", _errorString.c_str());
            return false;
        }

        if (_targetPulse > _max) {
            _targetPulse = _max;
            _errorString = "Target greater than maximum";
            log_e("%s", _errorString.c_str());
            return false;
        }
        return true;
    }

    bool moveTo(int16_t position) {
        _errorString = "";

        if (!_init) {
            _errorString = "Call init first!";
            log_e("%s", _errorString.c_str());
            return false;
        }

        _targetPulse = position;

        if (_targetPulse < _min) {
            _targetPulse = _min;
            _errorString = "Target smaller than minimum";
            log_e("%s", _errorString.c_str());
            return false;
        }

        if (_targetPulse > _max) {
            _targetPulse = _max;
            _errorString = "Target greater than maximum";
            log_e("%s", _errorString.c_str());
            return false;
        }


        return true;    
    }

    bool moveToDegrees(int16_t degrees) {
        _errorString = "";

        if (!_init) {
            _errorString = "Call init first!";
            log_e("%s", _errorString.c_str());
            return false;
        }

        /*
            y = a.x+b                           whereby x = degrees (input) and y = target pulse
            a = (max-min)/degrees*direction
            b = calibration - a.offset 
        */

        float a = (_max-_min)/_degrees*_direction;
        float b = _calibration - a*_offset;
        int16_t y = a*degrees + b;

        if (y<_min) {
            y = _min;
            _targetPulse = y;
            _errorString = "Out of range";
            return false;
        }

        if (y>_max) {
            y = _max;
            _targetPulse = y;
            _errorString = "Out of range";
            return false;
        }


        _targetPulse = y;
        return true;
    }

    bool calibrate(int16_t angle=-1) {
        _errorString = "";

        if (!_init) {
            _errorString = "Call init first!";
            log_e("%s", _errorString.c_str());
            return false;
        }
        
        if (angle!=-1) {
            if (angle<0 or angle>360) {
                _errorString = "Invalid calibration angle";
                log_e("%s", _errorString.c_str());
                return false;                
            }
            _offset = angle;
        }

        _calibration = _targetPulse ;
        _calibrated = true;
        return true;
    }


    bool recalibrate(int16_t adjust) {
        _errorString = "";

        if (!_init) {
            _errorString = "Call init first!";
            log_e("%s", _errorString.c_str());
            return false;
        }

        _calibration += _direction * adjust;

        // Once we have recalibrated we should also adjus the target, error checking, but no error generation when out-of-range
        _targetPulse += _direction * adjust;
        if (_targetPulse<_min) _targetPulse = _min;
        if (_targetPulse>_max) _targetPulse = _max;

        return true;
    }

    bool run() {
        static unsigned long lastUpdate = 0;

        _errorString = "";

        if (!_init) {
            _errorString = "Call init first!";
            log_e("%s", _errorString.c_str());
            return false;
        }

        // Should never happen, but check anyway
        if (_targetPulse < _min or _targetPulse > _max) {
            _errorString = "Target pulse out of range";
            log_e("%s", _errorString.c_str());
            return false;            
        }

        unsigned long now = millis();
        if (now - lastUpdate >= UPDATE_INTERVAL) {
            lastUpdate = now;
            int stepSize = (MAX_US_PER_SECOND * UPDATE_INTERVAL) / 1000; // µs per update

            if (_currentPulse < _targetPulse) {
                _currentPulse += stepSize;
                if (_currentPulse > _targetPulse) {
                _currentPulse = _targetPulse;
                }
                log_d("Servo on pin %d: %d",(int)_pin,_currentPulse);
                _servo.writeMicroseconds(_currentPulse);
                size_t s = EEPROM.writeInt(_eepromAddress, (int)_currentPulse);
                if (EEPROM.commit()) {
                    log_d("Writing %d to EEPROM address %d succes", _currentPulse, _eepromAddress);
                } else {
                    log_d("Writing %d to EEPROM address %d failed", _currentPulse, _eepromAddress);
                }
            }
            else if (_currentPulse > _targetPulse) {
                _currentPulse -= stepSize;
                if (_currentPulse < _targetPulse) {
                _currentPulse = _targetPulse;
                }
                log_d("Servo on pin %d: %d",(int)_pin,_currentPulse);
                _servo.writeMicroseconds(_currentPulse);
                size_t s = EEPROM.writeInt(_eepromAddress, (int)_currentPulse);
                if (EEPROM.commit()) {
                    log_d("Writing %d to EEPROM address %d succes", _currentPulse, _eepromAddress);
                } else {
                    log_d("Writing %d to EEPROM address %d failed", _currentPulse, _eepromAddress);
                }
            }

        }

        return true;
    }

    float getDegrees() {
        float a = (_max - _min) / _degrees * _direction;
        float b = _calibration - a * _offset;
        float degrees = ((float)_currentPulse - b) / a;
        return degrees;
    }

    int16_t getCurrent() { return _currentPulse; }
    int16_t getTarget() { return _targetPulse; }
    String getError() { return _errorString; }

private:
    ESP32ServoLite _servo;
    bool    _init = false, _calibrated = false;
    int16_t _min, _max, _degrees;
    float   _offset;
    int8_t  _pin, _direction = 1, _eepromAddress;
    int16_t _currentPulse, _targetPulse, _calibration=0;
    String  _errorString = "" ;

};