#include <Arduino.h>

// Minimal example to measure ErrorManager size.
// #define INCLUDE_ERROR_NAMES  // Uncomment to measure with string names

// Select the error-code definition file for this sketch.
#define ERROR_CODES_FILE "ErrorCodes.def"

#include "ErrorManager.h"

ErrorManager EM;

void setup() {
    // Minimal usage - no Serial output to avoid bloat
    EM.add(WIFI_CONNECT_FAILED);
    EM.add(SENSOR_TIMEOUT, 1000);
    EM.tick();
    EM.clear(WIFI_CONNECT_FAILED);
}

void loop() {
    EM.tick();
    delay(100);
}
