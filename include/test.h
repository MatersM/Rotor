#pragma once
#include <Arduino.h>
#include <ESP32ServoLite.h>


/*
This is only used for testing purposes.
*/

void test(int pin1, int pin2) {
    log_d("****** TEST STARTED ******");
    log_d("Pin 1 (Servo)=%d Pin 2 (Digital)=%d",pin1,pin2);

    int servoPin = 17;
    const int freq = 50;       // 50 Hz
    const int ledcChannel = 0;
    const int resolution = 16; // 16-bit

    ledcSetup(ledcChannel, freq, resolution);
    ledcAttachPin(servoPin, ledcChannel);

    // 1.5 ms pulse in 20 ms period → duty = 1.5/20 * 2^16
    int duty = (1500 * 65536) / 20000;
    ledcWrite(ledcChannel, duty);

    while(true) delay(20);

/*
    Servo servo1;
    ESP32PWM::allocateTimer(0);   // explicitly allocate timer
    servo1.attach(pin1, 500, 2500);
    servo1.writeMicroseconds(1500);
*/
    
    pinMode(pin2,OUTPUT);

    log_d("Servo attached and pulse sent");

    int counter = 0;
    while (true) {
        counter++;
        if (digitalRead(pin2)==LOW)
            digitalWrite(pin2, HIGH);
        else
            digitalWrite(pin2, LOW);
        delay(1);
    }

}