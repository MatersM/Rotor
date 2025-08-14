#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <objectData.h>
#include <stellarium.h>

// WebServer object on port 80
WebServer server(80);

ObjectData currentObjectData;

CalibrationData cData;

/// @brief Helper function to copy data to the webserver code.
/// @param data 
void copyData(ObjectData &data) {
  currentObjectData = data;
}

// Callback function to set tracking
void(*tracking_callback)(bool) = nullptr;

/// @brief Set callback function for when tracking is pushed
void setCallBack(void(*func_ptr)(bool)) {
  tracking_callback = func_ptr;
}

// To store calibration command callback function
void(*calibration_callback)(CalibrationData &) = nullptr;

/// @brief Set callback function when a calibration function is pushed
/// @param func_ptr 
void setCalibrationCallBack(void(*func_ptr)(CalibrationData &)) {
  calibration_callback = func_ptr;
}

// Mutex for thread-safe access to the data structure
portMUX_TYPE dataMutex = portMUX_INITIALIZER_UNLOCKED;

// --- Web Server Request Handlers ---

// Handles the root path ("/")
void handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    Serial.println("Failed to open index.html for reading");
    server.send(500, "text/plain", "500: Internal Server Error");
    return;
  }
  // streamFile is highly efficient, it sends the file in chunks
  // without loading the whole thing into memory.
  // The second argument is the content-type.
  server.streamFile(file, "text/html");
  file.close();
  Serial.println("Served index.html");
}

// API endpoint to get the current data as JSON
void handleData() {
  // Create a JSON document
  JsonDocument doc;

  // Use the mutex to safely read the shared data
  portENTER_CRITICAL(&dataMutex);
  doc["altitude"] = currentObjectData.altitude;
  doc["azimuth"] = currentObjectData.azimuth;
  doc["name"] = currentObjectData.name;
  doc["visible"] = currentObjectData.visible;
  doc["valid"] = currentObjectData.valid;
  doc["tracking"] = currentObjectData.tracking;
  doc["error"] = currentObjectData.error.c_str();
  portEXIT_CRITICAL(&dataMutex);

  // Serialize JSON to a string
  String jsonString;
  serializeJson(doc, jsonString);

  // Send the JSON response
  server.send(200, "application/json", jsonString);
}

// Handler to toggle tracking status
void handleTracking() {
  // Use the mutex to safely write to the shared data
  portENTER_CRITICAL(&dataMutex);
  currentObjectData.tracking = !currentObjectData.tracking;
  portEXIT_CRITICAL(&dataMutex);
  if (tracking_callback)
    tracking_callback(currentObjectData.tracking);
  
  Serial.printf("Tracking is now %s\n", currentObjectData.tracking ? "ON" : "OFF");
  server.send(200, "text/plain", "OK");
}

// Handler for calibration commands
void handleCalibrate() {

  // Check for the speed parameter. Default to 1 if not provided.
  int speed = 1; 
  if (server.hasArg("speed")) {
    speed = server.arg("speed").toInt();
  }

  String direction ="";
  if (server.hasArg("dir")) {
    direction = server.arg("dir");
  }

  int north = 0;
  if (server.hasArg("north")) {
    north = server.arg("north").toInt();
  } 

  cData.command = CC_NONE;
  cData.speed = speed;
  cData.direction = (CalibrationDirection)north;

  // Only directional commands should have a speed.
  if (direction == "up" || direction == "down" || direction == "left" || direction == "right") {
//      Serial.printf("Received CALIBRATE command: %s, Speed: %d\n", direction.c_str(), speed);
      if (direction == "up") cData.command = CC_UP;
      if (direction == "down") cData.command = CC_DOWN;
      if (direction == "left") cData.command = CC_LEFT;
      if (direction == "right") cData.command = CC_RIGHT;
  } else {
      // For 'ok' or other commands
//      Serial.printf("Received CALIBRATE command: %s\n", direction.c_str());
      cData.command = CC_OK;
  }
  
  // Only call the callback when a valid command is received and we do have a callback function
  if (cData.command != CC_NONE and calibration_callback) {
    calibration_callback(cData);
  }

  server.send(200, "text/plain", "OK");
}


// Handles requests to unknown paths
void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}


// --- The Task for Core 0 ---

// This task will handle all web server functions
void WebServerTask(void *pvParameters) {
  Serial.println("Web Server Task started on Core 0");

  // --- Define Server Routes ---
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/tracking", HTTP_POST, handleTracking); // Use POST for state changes
  server.on("/calibrate", HTTP_GET, handleCalibrate);
  server.onNotFound(handleNotFound);

  // Start the server
  server.begin();
  Serial.println("HTTP server started");

  // Task's main loop
  while (1) {
    server.handleClient();
    // A small delay is crucial to allow the idle task to run and prevent
    // watchdog timeouts, especially on an empty loop.
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
