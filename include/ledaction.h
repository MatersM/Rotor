#pragma once
#include <Arduino.h>
#include <tuple>

enum LEDACTION {
  ledOff,
  ledBlink,
  ledFastBlink,
  ledErrorBlink,
  ledDummy=0xFF
};

std::tuple<int, int> ledArray[] = {
    // on-time, off-time in ms
    {0, 1000000},   // Off
    {1000, 1000},   // Normal blink
    {250, 250},     // Fast Blink
    {100, 250}       // Error blink
};

void ledAction(LEDACTION led_action=ledDummy) {
  static bool init = false;
  static unsigned long timeStamp = millis(); 
  static bool on = false;
  static LEDACTION action = ledOff;

  if (!init) {
    pinMode(LED_BUILTIN,OUTPUT);
    digitalWrite(LED_BUILTIN,LOW);
    init = true;
    Serial.printf("Led pin: %d\n",LED_BUILTIN);
  }

  if (led_action!=ledDummy) action = led_action;

  int compTime;

  if (on)
    compTime = std::get<0>(ledArray[action]);
  else
    compTime = std::get<1>(ledArray[action]);

  if (millis()-timeStamp>=compTime) {
    if (on) 
      digitalWrite(LED_BUILTIN,LOW);
    else
      digitalWrite(LED_BUILTIN,HIGH);
    on = !on;
    timeStamp = millis();
  }

}
