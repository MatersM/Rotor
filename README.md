# Introduction

This is my Rotor project. 
It is an ESP32 based Rotor controller that connects with Stellarium and can track objects.
I'm using this with my SDR Antenna to track Satellites.

# How does it work

The ESP32 creates an Access Point and is a webserver at the same time.
With your Laptop running Stellarium make sure the Remote Control Plugin is activated with the default port (8090) without username and password.
Connect your Laptop to the Access Point and open a webbrowser and goto http://192.168.4.1 here you will see the controller.

When you are connected and Stellarium is running and a ojbect is selected you should see the name of the object and it's location in your browser.
You will need to have Stellarium in you active window for the tracking to work, otherwise Stellarium will not update the object position.
So I end up with my browser and Stellarium side-by-side in one window.

# Settings config.ini

All required settings are set in the config.ini file, which needs to be uploaded to your ESP32
When using PlatformIO use "Build File System" and "Upload File System Image", this is required anyway to also upload the index.html file.

Most paramters in the .ini file are self explainatory, hence we'll focus on the servo settings here.
There are 2 servo's an Azimuth and an Altitude servo.

## Servo callibration
[servo]
SERVO_ALT_DEGREES   = 180       // Most servo's have a range of 180 degrees
SERVO_ALT_MIN       = 500       // Min pulse of servo
SERVO_ALT_MAX       = 2500      // Max pulse of servo
SERVO_ALT_OFFSET    = 0.0       // When you have an offset antenna/dish, the offset in degrees
SERVO_ALT_DIRECTION = 1         // Well the direction, some more the other way around, depending on your physical build

SERVO_AZ_DEGREES    = 270       // Most servo's have a range of 180 degrees, I've chosen a 270 degrees servo for my Azimuth
SERVO_AZ_MIN        = 500       // See above
SERVO_AZ_MAX        = 2500      // See above
SERVO_AZ_DIRECTION  = -1        // See above

# Build

All build dependancies are in the platformio.ini file
Building using PlaformIO should be straightforward.

# Observation

I'm using my own implementation of a Servo class, since I couldn't get ESP32Servo.h to work.

# Remarks

Will be completed later after the developemnt is more mature.
Later on I'll make a seperate directory to outline the physical build as well.
