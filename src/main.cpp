#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <tuple>

#include <ledaction.h>
#include <iniparser.h>
#include <myserver.h>
#include <objectData.h>
#include <stellarium.h>
#include <rotorservo.h>
#include <test.h>

//#define TEST
#define VERSION "0.3.0 (13-AUG 2025)"

extern "C" {
  #include "esp_wifi.h"
  #include "esp_event.h"
  #include "esp_netif.h"
}

String ssid;
String password;

ObjectData data;
RotorServo servoAZ, servoALT;

const char *iniPath = "/config.ini";
#define DEFAULT_SSID "ESP32-Hotspot"
#define DEFAULT_PASSWORD "12345678"

#ifdef TEST
std::tuple<int,int> getPins() {
    INIConfig config;
    if (config.load(iniPath)) {
        Serial.printf("Open ini file %s\n",iniPath);
        auto pin1      = config.get("pin","PIN_SERVO_ALT","0").toInt();        
        auto pin2      = config.get("pin","PIN_SERVO_AZ","0").toInt();
        return {pin1, pin2};
      }
      
      Serial.printf("Could not open %s\n",iniPath);
      return {0,0};
}
#endif

String errorString = "";
unsigned long errorTime = 0;

void addError(const String error) {
    if (error=="") return; // Nothing to add
    if (errorString=="")
      errorString = error;
    else
      errorString += "\n" + error;
    errorTime = millis();
}

void clearError() {
if (millis()-errorTime>5000) errorString = "";
}

/// @brief Read config parameters from ini file and init servo's
void readInitConfig() {
    INIConfig config;
    if (config.load(iniPath)) {
        Serial.printf("Open ini file %s\n",iniPath);

        // Read AP settings
        ssid = config.get("network", "WIFI_SSID", DEFAULT_SSID);
        password = config.get("network", "WIFI_PASSWORD", DEFAULT_PASSWORD);
        Serial.println("SSID: " + ssid);
        Serial.println("Password: " + password);

        // Read the servo stuff and init those as well

	      //ESP32PWM::allocateTimer(0);
	      //ESP32PWM::allocateTimer(1); 

        auto eepromAddress = 0;
        auto pin      = config.get("pin","PIN_SERVO_ALT","0").toInt();
        auto degrees  = config.get("servo","SERVO_ALT_DEGREES","0").toInt();
        auto min      = config.get("servo","SERVO_ALT_MIN","0").toInt();
        auto max      = config.get("servo","SERVO_ALT_MAX","0").toInt();
        auto direction= config.get("servo","SERVO_ALT_DIRECTION","1").toInt();
        auto offset   = config.get("servo","SERVO_ALT_OFFSET","0.0").toFloat();

        servoALT.init(pin,eepromAddress,min,max,degrees,direction,offset);
        addError(servoALT.getError());

        eepromAddress = 4;
        pin      = config.get("pin","PIN_SERVO_AZ","0").toInt();
        degrees  = config.get("servo","SERVO_AZ_DEGREES","0").toInt();
        min      = config.get("servo","SERVO_AZ_MIN","0").toInt();
        max      = config.get("servo","SERVO_AZ_MAX","0").toInt();
        direction= config.get("servo","SERVO_AZ_DIRECTION","1").toInt();
        offset   = config.get("servo","SERVO_AZ_OFFSET","0.0").toFloat();

        servoAZ.init(pin,eepromAddress,min,max,degrees,direction,offset);
        addError(servoAZ.getError());
        
    } else {
      Serial.printf("Could not open %s\n",iniPath);
    }
}

void setupWiFiAP() {
  WiFi.softAP(ssid.c_str(), password.c_str());
  Serial.println("Access Point started");
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
}

// Callback function for the server code
void setTracking(bool t) {
 data.tracking = t; 
}

// Callback function for the server code
// When tracking this is a calibration adjustment
void setCalibrartion(CalibrationData &serverData) {
  Serial.printf("Calibrate: ");
  switch (serverData.command) {
    case CC_OK: {
      Serial.printf( "OK North=%d\n", serverData.direction);
      if (data.tracking) break; // Don't do anything if tracking
      // Oke, now set the calibration data
      servoALT.calibrate();
      int16_t degrees = 0;
      if (serverData.direction) // if 1 then 180 degrees
        degrees = 180;
      servoAZ.calibrate(degrees);
      break;
    }
    case CC_LEFT:
      Serial.printf("LEFT - %d North=%d\n", serverData.speed, serverData.direction);
      if (data.tracking)
        servoAZ.recalibrate(-serverData.speed);
      else
        servoAZ.move(-serverData.speed);
      break;
    case CC_RIGHT:
      Serial.printf("RIGHT - %d North=%d\n", serverData.speed, serverData.direction);
      if (data.tracking)
        servoAZ.recalibrate(serverData.speed);
      else
        servoAZ.move(serverData.speed);
      break;
    case CC_UP:
      Serial.printf("UP - %d North=%d\n", serverData.speed, serverData.direction);
      if (data.tracking)
        servoALT.recalibrate(serverData.speed);
      else
        servoALT.move(serverData.speed);
      break;
    case CC_DOWN:
      Serial.printf("DOWN - %d North=%d\n", serverData.speed, serverData.direction);
      if (data.tracking)
        servoALT.recalibrate(-serverData.speed);
      else
        servoALT.move(-serverData.speed);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  if (!SPIFFS.begin(true)) {
      Serial.println("SPIFFS mount failed");
      return;
  }

#ifdef TEST
//    auto [pin1, pin2] = getPins(); // gnu++17, so not for here
    auto pins = getPins();
    int pin1 = std::get<0>(pins);
    int pin2 = std::get<1>(pins);
    test(pin1, pin2);
#endif

#ifdef DEBUG_BUILD
  constexpr const char* build = "Debug build";
#else
  constexpr const char* build = "Release build";
#endif

Serial.printf("%s version %s.\n",build,VERSION);


  ledAction(ledFastBlink);
  readInitConfig(); 
  setupWiFiAP();

  unsigned int timer=millis();

  while (millis()-timer<5000) ledAction();
  ledAction(ledOff);

  setCallBack(setTracking);
  setCalibrationCallBack(setCalibrartion);

  // Setup webserver
  xTaskCreatePinnedToCore(
      WebServerTask,   // Task function
      "WebServer",     // Task name
      10000,           // Stack size (bytes)
      NULL,            // Task parameters
      1,               // Priority
      NULL,            // Task handle
      0);              // Pin to Core 0

  Serial.println("Setup() is complete. Main loop will run on Core 1. Server runs on Core 0");
}

void loop() {
  static unsigned long lastCheck = 0;
  
  ledAction();

  // Clear errorString after a while
  clearError();

  // Move servo's to their target location very smoothly....
  if (!servoAZ.run()) {
    addError("Failed to track azimuth");
  }

  if (!servoALT.run()){
    addError("Failed to track altitude");
  }

  if (millis() - lastCheck > 2000) {  // every 2s

    log_i("*** INFO ***");
    log_i("AZ  target=%0.2f (%4d)",servoAZ.getDegrees(),  servoAZ.getTarget());
    log_i("ALT target=%0.2f (%4d)", servoALT.getDegrees(),  servoALT.getTarget());

    // Save if tracking and restore after getting new data
    bool tracking = data.tracking;
    data = getStellariumData();
    data.tracking = tracking;
    if (data.error!="") { // We couldn't retrieve data from Stellarium /* MJM TODO CHECK NOT SURE HOW THIS WORKS */
      errorTime = millis();
    } else
      data.error = errorString;

    if (data.valid) {
      ledAction(ledOff);
      Serial.printf("Object\t: %s\n",data.name);
//      Serial.printf("Alt\t: %0.4f\n",data.altitude);
//      Serial.printf("Az\t: %0.4f\n",data.azimuth);
//      Serial.printf("Visible\t: %d\n",data.visible);

      // When tracking move it!
      if (data.tracking) {
        if (!servoALT.moveToDegrees(data.altitude)) {
          addError(servoALT.getError());
          data.tracking = false;
        }
        if (!servoAZ.moveToDegrees(data.azimuth)) {
          addError(servoAZ.getError());
          data.tracking = false;         
        }
      }

    } else {
      ledAction(ledFastBlink);
      data.tracking = false;
      addError("Invalid data. Stop Tracking.");
      Serial.println("Invalid data. Stop tracking");
    }

    if (!data.visible) data.tracking = false;
    // Copy data to "server side"
    copyData(data);
    lastCheck = millis();
  }
}