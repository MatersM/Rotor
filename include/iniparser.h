// Minimal INI parser for SPIFFS (Arduino/ESP32)
// Supports [sections] and key=value pairs

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <map>

class INIConfig {
public:
    // Data structure: section -> (key -> value)
    std::map<String, std::map<String, String>> data;

    bool load(const char* path) {
        File file = SPIFFS.open(path, "r");
        if (!file || file.isDirectory()) {
            Serial.println("Failed to open INI file.");
            return false;
        }

        String section = "";
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() == 0 || line.startsWith(";") || line.startsWith("#")) continue;

            if (line.startsWith("[") && line.endsWith("]")) {
                section = line.substring(1, line.length() - 1);
                section.trim();
            } else {
                int eqPos = line.indexOf('=');
                if (eqPos != -1) {
                    String key = line.substring(0, eqPos);
                    String value = line.substring(eqPos + 1);
                    key.trim();
                    value.trim();
                    data[section][key] = value;
                }
            }
        }

        file.close();
        return true;
    }

    String get(const String& section, const String& key, const String& defaultValue = "") const {
        auto secIt = data.find(section);
        if (secIt != data.end()) {
            auto keyIt = secIt->second.find(key);
            if (keyIt != secIt->second.end()) {
                return keyIt->second;
            } 
        }
        return defaultValue;
    }
};

// Example usage:
/*
void setup() {
    Serial.begin(115200);
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed");
        return;
    }

    INIConfig config;
    if (config.load("/config.ini")) {
        String ssid = config.get("WIFI", "SSID", "defaultSSID");
        String password = config.get("WIFI", "PASSWORD", "defaultPass");
        Serial.println("SSID: " + ssid);
        Serial.println("Password: " + password);
    }
}
*/
