#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

extern "C" {
  #include "esp_wifi.h"
  #include "esp_event.h"
  #include "esp_netif.h"
}

struct ObjectData {
  float altitude = 0.0; // Was NAN
  float azimuth = 0.0; // Was NAN
  String name = "";
  bool visible = false;
  bool valid = false;
  bool tracking = false;
  String error = "";
  // Current Servo direction
  float currAlt = 0.0, currAz = 0.0;
  // What mode are we in
  bool stellariumMode = false;
};

/// @brief Helper function, assumption is that only one device is connected to the AP
/// @return IP-Address of a connected client if any
IPAddress checkConnectedClients() {
  wifi_sta_list_t staList;
  tcpip_adapter_sta_list_t adapterList;

  esp_wifi_ap_get_sta_list(&staList);
  tcpip_adapter_get_sta_list(&staList, &adapterList);

    IPAddress clientIP;

  for (int i = 0; i < adapterList.num; ++i) {
    esp_ip4_addr_t ip = adapterList.sta[i].ip;
    esp_ip4_addr_t ipAddr = adapterList.sta[i].ip;
    uint32_t ip_raw = ipAddr.addr; // this is in little-endian format
    IPAddress myIP(
      ip_raw & 0xFF,
      (ip_raw >> 8) & 0xFF,
      (ip_raw >> 16) & 0xFF,
      (ip_raw >> 24) & 0xFF
    );
    
    clientIP = myIP;
    
    Serial.print("Client connected: ");
    Serial.println(clientIP);
  }
  return clientIP;
}

/// @brief Helper function, sends a request for the currently tracking object to stellarium
/// @param clientIP 
/// @return Json response from stellarium
String sendHTTPRequestToClient(IPAddress clientIP) {
  if (!clientIP) return "";

  String url = "http://" + clientIP.toString() + ":8090/api/objects/info?format=json";

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    http.end();
    return payload;
  } else {
    Serial.printf("HTTP request failed: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return "";
  }
}


/// @brief Helper function - parse data returned from stellarium
/// @param jsonString input data
/// @return ObjectData object, if valid this property is true
ObjectData parseStellariumJson(const String& jsonString) {

  ObjectData data;
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  data.valid = false;

  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.f_str());
    data.error = "No object selected";
    return data;
  }

  try {
    if (doc["altitude"] && doc["azimuth"] && doc["localized-name"] /*&& doc["above-horizon"]*/) {
      data.altitude = doc["altitude"].as<float>();
      data.azimuth = doc["azimuth"].as<float>();
      data.name = doc["localized-name"].as<String>();
      data.visible = doc["above-horizon"].as<bool>();
      data.valid = true;
    } else {
      data.error = "Missing expected keys in JSON.";
      Serial.println("Missing expected keys in JSON.");
    }
  } catch (...) {
    Serial.println("Failed misserably....");
  }
  return data;
}

ObjectData getStellariumData() {

    ObjectData data;
    data.valid = false;
    // Get the IP address of the client connected to this AP
    auto clientIP = checkConnectedClients();
    if (!clientIP) {// No connected client
      data.error = "No connected client";
        return data;
    }

    // Send request to stellarium to get the current object if any.
    auto response = sendHTTPRequestToClient(clientIP);

    if (response=="") { // No valid server response.
      data.error = "No response from Stellarium";
        return data;
    }

    // Try to parse the stellarium data
    data = parseStellariumJson(response);
    return data;
}