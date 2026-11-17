#include "traffic/Firmware.h"

// Shared implementation: keep identical in both projects (verify_modular.py).
void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  //saveHourlyCounts();
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

String buildPath(const String &baseDir, const String &fileName) {
    if (baseDir.endsWith("/")) {
        return baseDir + fileName;
    }
    return baseDir + "/" + fileName;
}
