#pragma once
#include "RuntimeState.h"

// Service interface; definitions are compiled once in src/services.
void onOTAStart();
void onOTAProgress(size_t current, size_t final);
void onOTAEnd(bool success);
String buildPath(const String &baseDir, const String &fileName);
void listSDFiles(AsyncWebServerRequest *request);
void downloadSDFile(AsyncWebServerRequest *request);
void uploadSDFile(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void changeDirectory(AsyncWebServerRequest *request);
void deleteSDFile(AsyncWebServerRequest *request);
void renameSDFile(AsyncWebServerRequest *request);
void setupServer();
