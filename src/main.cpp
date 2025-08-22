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
#include <satdump.h>

#define VERSION "0.4.3 (16-AUG 2025)"

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

    if (!config.load(iniPath)) {
      log_e("Could not open %s\n",iniPath);
      return;
    }

    log_i("Open ini file %s\n",iniPath);

    // Read AP settings
    ssid = config.get("network", "WIFI_SSID", DEFAULT_SSID);
    password = config.get("network", "WIFI_PASSWORD", DEFAULT_PASSWORD);
    log_i("SSID: %s", ssid);
    log_i("Password: %s", password);

    data.stellariumMode = config.get("mode","STELLARIUM_MODE","0").toInt();

    // Read the servo stuff and init those as well
    auto eepromAddress = 0;
    auto pin      = config.get("pin","PIN_SERVO_ALT","0").toInt();
    auto degrees  = config.get("servo","SERVO_ALT_DEGREES","0").toInt();
    auto min      = config.get("servo","SERVO_ALT_MIN","500").toInt();
    auto max      = config.get("servo","SERVO_ALT_MAX","2500").toInt();
    auto direction= config.get("servo","SERVO_ALT_DIRECTION","1").toInt();
    auto offset   = config.get("servo","SERVO_ALT_OFFSET","0.0").toFloat();
    bool smooth   = config.get("servo","SERVO_ALT_SMOOTH","1").toInt();

    servoALT.init(pin,eepromAddress,min,max,degrees,direction,offset);
    addError(servoALT.getError());
    servoALT.smooth(smooth);

    eepromAddress = 4;
    pin      = config.get("pin","PIN_SERVO_AZ","0").toInt();
    degrees  = config.get("servo","SERVO_AZ_DEGREES","0").toInt();
    min      = config.get("servo","SERVO_AZ_MIN","500").toInt();
    max      = config.get("servo","SERVO_AZ_MAX","2500").toInt();
    direction= config.get("servo","SERVO_AZ_DIRECTION","1").toInt();
    offset   = config.get("servo","SERVO_AZ_OFFSET","0.0").toFloat();
    smooth   = config.get("servo","SERVO_AZ_SMOOTH","1").toInt();

    servoAZ.init(pin,eepromAddress,min,max,degrees,direction,offset);
    addError(servoAZ.getError());
    servoAZ.smooth(smooth);

}

void setupWiFiAP() {
  WiFi.softAP(ssid.c_str(), password.c_str());
  log_i("Access Point started");
  log_i("IP address: %s", WiFi.softAPIP().toString());
}

// Callback function for the server code
void setTracking(bool t) {
 data.tracking = t; 
}

// Callback function for the server code
// When tracking this is a calibration adjustment
void setCalibrartion(CalibrationData &serverData) {
  switch (serverData.command) {
    case CC_OK: {
      log_i( "Calibrate: OK North=%d", serverData.direction);
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
      log_i("Calibrate: LEFT - %d North=%d", serverData.speed, serverData.direction);
      if (data.tracking)
        servoAZ.recalibrate(-serverData.speed);
      else
        servoAZ.move(-serverData.speed);
      break;
    case CC_RIGHT:
      log_i("Calibrate: RIGHT - %d North=%d", serverData.speed, serverData.direction);
      if (data.tracking)
        servoAZ.recalibrate(serverData.speed);
      else
        servoAZ.move(serverData.speed);
      break;
    case CC_UP:
      log_i("Calibrate: UP - %d North=%d", serverData.speed, serverData.direction);
      if (data.tracking)
        servoALT.recalibrate(serverData.speed);
      else
        servoALT.move(serverData.speed);
      break;
    case CC_DOWN:
      log_i("Calibrate: DOWN - %d North=%d", serverData.speed, serverData.direction);
      if (data.tracking)
        servoALT.recalibrate(-serverData.speed);
      else
        servoALT.move(-serverData.speed);
      break;
  }
}

// Only continue if the setup was successful
bool setupSucces;

void setup() {

  setupSucces = false;

  Serial.begin(115200);
  delay(100);
  if (!SPIFFS.begin(true)) {
      log_e("SPIFFS mount failed => Not usefull to continue");
      ledAction(ledErrorBlink);
      return;
  }

#ifdef DEBUG_BUILD
  constexpr const char* build = "Debug build";
#else
  constexpr const char* build = "Release build";
#endif

  log_i("%s version %s.\n",build,VERSION);

  readInitConfig(); 
  setupWiFiAP();
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

  // Give myserver Access to the data
  linkData(&data);

  setupSucces = true;
  log_i("Setup() is complete. Main loop will run on Core 1. Server runs on Core 0");
}

void loop() {
  static unsigned long lastCheck = 0;
  static unsigned long loopCounter = 0;
  

//satDumpTest() ;

  loopCounter++;
  ledAction();

  // Don't continue if the setup failed (probably because of a SPIFFS error), led should be very fast "bleeping"
  if (!setupSucces) return;

  // Clear errorString after a while
  clearError();

  // Move servo's to their target location very smoothly only does something if smooth=1 in config.ini ....
  if (!servoAZ.run()) {
    addError("Failed to track azimuth");
  }

  if (!servoALT.run()){
    addError("Failed to track altitude");
  }

  if (millis() - lastCheck > 1000) {  // every second

    log_i("****** INFO ******");
    log_i("AZ  target=%0.2f (%4d)",servoAZ.getDegrees(),  servoAZ.getTarget());
    log_i("ALT target=%0.2f (%4d)", servoALT.getDegrees(),  servoALT.getTarget());

    // Save if tracking and restore after getting new data
    if (data.stellariumMode) {

      bool tracking = data.tracking;
      data = getStellariumData();
      data.tracking = tracking;

    } else {

      // Satdump mode
      auto satDump = handleSatDump(servoALT.getDegrees(),servoAZ.getDegrees());
      float alt = std::get<0>(satDump); 
      float az = std::get<1>(satDump); 

      if (alt!=0.0 and az!=0.0) {
        log_i("****** SATDUMP ******");
        log_i("ALT = %0.2f",alt);
        log_i("AZ  = %0.2f",az);
        // Ah we have Satdump tracking
        data.altitude = alt;
        data.azimuth = az;
        data.name = "<see Satdump>";
        data.valid = true;
        data.visible = alt>=0.0;
        data.valid = true;
        data.tracking = true; // Force tracking on
      } else {
        data.tracking = false;
      }
  }   // End SatDump stuff

    data.currAlt = servoALT.getDegrees();
    data.currAz = servoAZ.getDegrees();
    if (data.error!="") { // We couldn't retrieve data from Stellarium
      errorTime = millis();
    } else
      data.error = errorString;

    if (data.valid) {
      ledAction(ledOff);
      if (loopCounter%5==0) { // Every 5 seconds
        log_i("****** OBJECT ******");
        log_i("Object\t: %s",data.name);
        log_i("Altitude\t: %0.4f",data.altitude);
        log_i("Azimuth\t: %0.4f",data.azimuth);
        log_i("Visible\t: %d",data.visible);
      }
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
      ledAction(ledBlink);
      data.tracking = false;
      addError("Invalid data. Stop Tracking.");
      log_i("Invalid data. Stop tracking");
    }

    if (!data.visible) data.tracking = false;
    lastCheck = millis();
  }
}