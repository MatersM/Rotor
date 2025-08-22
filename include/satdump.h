#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <tuple>

std::tuple<float,float> handleSatDump(float currentAlt, float currentAz) {
    static float targetAz=0.0, targetAlt=0.0;
    static WiFiServer rotctldServer(4533);
    static bool started = false;
    static WiFiClient client;

    if (!started) {
        rotctldServer.begin();
        started = true;
    }

    if (!client) client = rotctldServer.available();

    if (!client) return {0.0,0.0} ;

    if (!client.connected()) return {0.0,0.0} ;
  

    if (client.available()) {
        String cmd = client.readStringUntil('\n');  // rotctld uses \n line endings
        cmd.trim();
        log_i("CMD: %s", cmd);

        if (cmd.startsWith("P ")) {
            // Format: "P az alt"
            float az, alt;
            if (sscanf(cmd.c_str(), "P %f %f", &az, &alt) == 2) {
                targetAz = az;
                targetAlt = alt;
                client.print("RPRT 0\n");
                return {targetAlt,targetAz};
            } else {
                client.print("RPRT -1\n");
            }
        } 
        else if (cmd == "p") {
            // Report current position
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f %.2f\n", currentAz, currentAlt);
            client.print(buf);
            return {targetAlt,targetAz};
        } 
        else if (cmd == "q") {
            client.print("RPRT 0\n");
            client.stop();

        } 
        else {
            client.print("RPRT -1\n");
        }
    }

    return {0.0,0.0};
}


void satDumpTest() {
    float alt=0.0, az=0.0;
    while (true) {
        log_i("Running");
        auto res = handleSatDump(alt,az);
        alt = std::get<0>(res); 
        az = std::get<1>(res); 
        if (az!=0.0 and alt!=0.0) log_i("Stadumptest Alt: %0.2f, Az: %0.2f", alt,az);
        delay(1000);
    }
}

std::tuple<float,float> SAVEhandleSatDump(float currentAlt, float currentAz) {
    static float targetAz=0.0, targetAlt=0.0;
    static WiFiServer rotctldServer(4533);
    static bool started = false;
    static WiFiClient client;

  if (!started) {
    rotctldServer.begin();
    started = true;
  }

  if (!client) client = rotctldServer.available();

  if (client) {

    if (client.connected()) {
      if (client.available()) {
        String cmd = client.readStringUntil('\n');  // rotctld uses \n line endings
        cmd.trim();
        Serial.println("CMD: " + cmd);

        if (cmd.startsWith("P ")) {
          // Format: "P az alt"
          float az, alt;
          if (sscanf(cmd.c_str(), "P %f %f", &az, &alt) == 2) {
            targetAz = az;
            targetAlt = alt;
            client.print("RPRT 0\n");
          } else {
            client.print("RPRT -1\n");
          }
        } 
        else if (cmd == "p") {
          // Report current position
          char buf[32];
          snprintf(buf, sizeof(buf), "%.2f %.2f\n", currentAz, currentAlt);
          client.print(buf);
        } 
        else if (cmd == "q") {
          client.print("RPRT 0\n");
          client.stop();

        } 
        else {
          client.print("RPRT -1\n");
        }
      }
    }

  }

  
  return {targetAlt,targetAz};
}