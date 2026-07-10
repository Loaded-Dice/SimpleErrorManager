#include <Arduino.h>

// ── Enable string names for errors (optional) ────────────────
// Comment out to save some bytes in Flash/RAM:
#define INCLUDE_ERROR_NAMES

// ── Select the error-code definition file for this project ───
// This file lives next to the sketch (examples/Basic/ErrorCodes.def).
#define ERROR_CODES_FILE "ErrorCodes.def"

#include "ErrorManager.h"

#define CHECK_ERROR_MS 100

// ================================================================
//  Error codes are defined in ErrorCodes.def
//  No COUNT enum required, no template
// ================================================================

// ================================================================
//  Global instance
// ================================================================
ErrorManager ERR;

// ----------------------------------------------------------------
//  Error task (FreeRTOS)
// ----------------------------------------------------------------
void errorTask(void* pvParameters) {
    for (;;) {
        ERR.tick(); // promote pending to active after mercy period

        if (ERR.getActiveCount() != 0) { // do something to handle your error

            if (ERR.isActive(WIFI_CONNECT_FAILED)) {
                // WiFi.reconnect();
                Serial.println("WiFi SSID confusion - But I think I got it now!");
                ERR.clear(WIFI_CONNECT_FAILED); // clear after resolved
            }
            if (ERR.isActive(SENSOR_TIMEOUT)) {
                // deactivateSensor();
                Serial.println("Message from errorTask handler - imagined sensor deactivated. Error cleared");
                ERR.clear(SENSOR_TIMEOUT);
            }
            if (ERR.isActive(LOW_MEMORY)) {
                Serial.printf("[ERR Example] Low Memory - Heap: %d bytes\n", ESP.getFreeHeap());
                ERR.clear(LOW_MEMORY);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(CHECK_ERROR_MS));
    }
}

// ----------------------------------------------------------------
//  Setup
// ----------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(500);

    randomSeed(esp_random());  // Initialize random number generator

    Serial.println("\n=== ErrorManager Demo ===\n");

    xTaskCreate(errorTask, "ErrorTask", 2048, nullptr, 1, nullptr);

    // --- Examples ---

    // Immediately active -> true
    bool added = ERR.add(SENSOR_TIMEOUT);
    Serial.printf("SENSOR_TIMEOUT added:          %s\n", added ? "true" : "false");

    // Add again -> false (already active, no overwrite)
    added = ERR.add(SENSOR_TIMEOUT);
    Serial.printf("SENSOR_TIMEOUT again:          %s (expected: false)\n",
                  added ? "true" : "false");

    // With mercy: active after 3s -> true
    added = ERR.add(WIFI_CONNECT_FAILED, 3000);
    Serial.printf("WIFI_CONNECT_FAILED pending:   %s\n", added ? "true" : "false");

    // Add pending again -> false (mercyTime not restarted)
    added = ERR.add(WIFI_CONNECT_FAILED, 9000);
    Serial.printf("WIFI_CONNECT_FAILED again:     %s (expected: false)\n",
                  added ? "true" : "false");

    // clear() on unset error -> false
    bool cleared = ERR.clear(NTP_SYNC_FAILED);
    Serial.printf("NTP_SYNC_FAILED clear():       %s (expected: false)\n",
                  cleared ? "true" : "false");

    // Set and clear -> true
    ERR.add(LOW_MEMORY);
    cleared = ERR.clear(LOW_MEMORY);
    Serial.printf("LOW_MEMORY clear():            %s (expected: true)\n",
                  cleared ? "true" : "false");
}

void printActiveErrors(ErrorManager& em) {
    static uint8_t last_pending = 0;
    static uint8_t last_active = 0;
    uint8_t n_active = ERR.getActiveCount();
    uint8_t n_pending = ERR.getPendingCount();
    if (last_pending == n_pending && last_active == n_active) { return; }
    last_pending = n_pending;
    last_active = n_active;
    if (n_active == 0 && n_pending == 0) {
        Serial.println("=== No Active Errors ===");
    } else {
        Serial.println("=== Active Errors ===");
        for (ErrCode code : ERR.allErrorCodes()) {
            if (ERR.isActive(code)) {
                uint32_t activeSince = millis() - ERR.getTimestamp(code);
#ifdef INCLUDE_ERROR_NAMES
                Serial.printf("[ERR] %s (since %lu ms) \n", ERR.getErrorName(code), activeSince);
#else
                Serial.printf(" code [%d]  (since %lu ms)\n", GETCODE(code), activeSince);
#endif
            }
        }
        Serial.printf("Active: %d, Pending: %d, Heap: %d\n", n_active, n_pending, ESP.getFreeHeap());
    }
}

// Spawn random errors from the list and clear them randomly to show the capabilities
void randomAction() {
    static uint32_t callCount = 0;
    static uint32_t lastErrorTime = 0;
    const uint8_t MAX_RANDOM_ERRORS = 12;
    uint8_t n_active = ERR.getActiveCount();

    callCount++;
    uint32_t now = millis();

    // Force error after 3 seconds if none active
    if (n_active == 0 && (now - lastErrorTime) > 3000) {
        ErrCode forceError = static_cast<ErrCode>(random(0, _ERR_CODE_COUNT_INTERNAL));
        uint32_t mercy = (random(3) == 0) ? random(5000) : 0;  // 1/3 chance for mercy time
        if (ERR.add(forceError, mercy)) {
            lastErrorTime = now;
        }
        return;
    }

    // Escalating probability: starts low, increases with call count.
    // Damped by the number of active errors (fewer active = higher probability).
    float dampingFactor = 1.0f - (float(n_active) * 1.3 / float(MAX_RANDOM_ERRORS));
    uint16_t baseProbability = (callCount / 3 > 100) ? 100 : callCount / 3;  // increases every 3 calls, max 100
    uint16_t adjustedProb = uint16_t(baseProbability * dampingFactor);

    // Add random error with adjusted probability
    if (random(100) < adjustedProb && n_active < MAX_RANDOM_ERRORS) {
        ErrCode randomError = static_cast<ErrCode>(random(0, _ERR_CODE_COUNT_INTERNAL));
        uint32_t mercy = (random(3) == 0) ? random(5000) : 0;  // 1/3 chance for mercy time

        if (ERR.add(randomError, mercy)) {
            lastErrorTime = now;
        }
    }

    // Random chance to clear active errors
    uint8_t clearChance = 30 + random(-20, 21);  // 10 to 50
    if (n_active > 0 && random(clearChance) == 0) {
        // Clear a random active error
        for (ErrCode code : ERR.allErrorCodes()) {
            if (ERR.isActive(code) && random(2) == 0) {  // 50% chance per active error
                ERR.clear(code);
                break;  // Clear only one per call
            }
        }
    }
}

void loop() {
    delay(250);
    randomAction();      // Demo: Generate/clear random errors
    printActiveErrors(ERR);
}
