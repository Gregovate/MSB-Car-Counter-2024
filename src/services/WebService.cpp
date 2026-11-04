#include "traffic/Firmware.h"

// Extracted from the production baseline; behavior fixes are separate changes.








void listSDFiles(AsyncWebServerRequest *request) {
    if (!sdAvailable) {
        request->send(503, "text/plain", "SD not available");
        return;
    }

    // Header for UI alignment (same as Gate Counter)
    String fileList = "Files in " + currentDirectory + ":\n";
    fileList += "Name\tSize (bytes)\tLast Write\n";

    File root = SD.open(currentDirectory);
    if (!root || !root.isDirectory()) {
        request->send(500, "text/plain",
                      "Failed to open directory: " + currentDirectory);
        return;
    }

    while (true) {
        File file = root.openNextFile();
        if (!file) {
            break;  // no more files
        }

        if (!file.isDirectory()) {
            // Add file name
            fileList += String(file.name());
            fileList += "\t";

            // Add file size
            fileList += String(file.size());
            fileList += "\t";

            // Add last write timestamp
            time_t lw = file.getLastWrite();
            if (lw > 0) {
                struct tm *tmstruct = localtime(&lw);
                char buf[20];
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tmstruct);
                fileList += buf;
            } else {
                fileList += "unknown";
            }

            fileList += "\n";
        }

        file.close();
    }

    root.close();

    request->send(200, "text/plain", fileList);
}

void downloadSDFile(AsyncWebServerRequest *request) {
    if (!request->hasParam("filename")) {
        request->send(400, "text/plain", "Filename is required");
        return;
    }

    String fileName = request->getParam("filename")->value();

    // Build full path using helper
    String fullPath = buildPath(currentDirectory, fileName);

    // Normalize accidental double leading slash
    if (fullPath.startsWith("//")) {
        fullPath = fullPath.substring(1);
    }

    if (!SD.exists(fullPath)) {
        request->send(404, "text/plain", "File not found: " + fullPath);
        return;
    }

    AsyncWebServerResponse *response =
        request->beginResponse(SD, fullPath, "application/octet-stream");

    response->addHeader(
        "Content-Disposition",
        "attachment; filename=\"" + fileName + "\""
    );

    request->send(response);
}

void uploadSDFile(AsyncWebServerRequest *request,
                  String filename,
                  size_t index,
                  uint8_t *data,
                  size_t len,
                  bool final) {

    static File uploadFile; // Keep track of the currently uploading file

    // Build full path using helper so currentDirectory is respected
    String fullPath = buildPath(currentDirectory, filename);
    
    if (fullPath.startsWith("//")) {
        fullPath = fullPath.substring(1);  // normalize leading //
    }

    // Handle the start of the upload
    if (index == 0) {
        Serial.printf("Upload started: %s\n", fullPath.c_str());

        if (SD.exists(fullPath)) {
            SD.remove(fullPath); // Remove the file if it already exists
        }

        uploadFile = SD.open(fullPath, FILE_WRITE);
        if (!uploadFile) {
            Serial.printf("Failed to open file: %s\n", fullPath.c_str());
            request->send(500, "text/plain", "Failed to open file for writing");
            return;
        }
    }

    // Write data to the file
    if (uploadFile) {
        uploadFile.write(data, len);
    }

    // Handle the end of the upload
    if (final) {
        if (uploadFile) {
            uploadFile.close();
            Serial.printf("Upload completed: %s\n", fullPath.c_str());
            request->send(200, "text/plain",
                          "File uploaded successfully to " + currentDirectory);
        } else {
            Serial.printf("Upload failed: %s\n", fullPath.c_str());
            request->send(500, "text/plain", "Failed to write file");
        }
    }
}

void changeDirectory(AsyncWebServerRequest *request) {
    if (!request->hasParam("dir")) {
        request->send(400, "text/plain", "Directory name is required");
        return;
    }

    String dir = request->getParam("dir")->value();
    String target;

    // If request gives absolute path (/cc/2025)
    if (dir.startsWith("/")) {
        target = dir;
    }
    else {
        // Relative path (use helper for consistency)
        target = buildPath(currentDirectory, dir);
    }

    // Normalize accidental leading //
    if (target.startsWith("//")) {
        target = target.substring(1);
    }

    // Must exist AND be a directory
    if (SD.exists(target)) {
        File f = SD.open(target);
        if (f && f.isDirectory()) {
            currentDirectory = target;
            request->send(200, "text/plain", "Changed directory to " + currentDirectory);
            return;
        }
    }

    request->send(404, "text/plain", "Directory not found: " + target);
}

void deleteSDFile(AsyncWebServerRequest *request) {
    if (!request->hasParam("filename")) {
        request->send(400, "text/plain", "Filename is required");
        return;
    }

    String fileName = request->getParam("filename")->value();

    // Build the full path using the helper
    String fullPath = buildPath(currentDirectory, fileName);

    // Normalize accidental leading //
    if (fullPath.startsWith("//")) {
        fullPath = fullPath.substring(1);
    }

    if (SD.exists(fullPath)) {
        if (SD.remove(fullPath)) {
            Serial.printf("File deleted: %s\n", fullPath.c_str());
            request->send(200, "text/plain", "File deleted successfully: " + fullPath);
        } else {
            Serial.printf("Failed to delete file: %s\n", fullPath.c_str());
            request->send(500, "text/plain", "Failed to delete file: " + fullPath);
        }
    } else {
        Serial.printf("File not found: %s\n", fullPath.c_str());
        request->send(404, "text/plain", "File not found: " + fullPath);
    }
}

void renameSDFile(AsyncWebServerRequest *request) {
    if (!sdAvailable) {
        request->send(503, "text/plain", "SD not available");
        return;
    }

    if (!request->hasParam("old") || !request->hasParam("new")) {
        request->send(400, "text/plain", "Parameters 'old' and 'new' are required");
        return;
    }

    String oldName = request->getParam("old")->value();
    String newName = request->getParam("new")->value();

    // Build full paths relative to currentDirectory
    String oldPath = buildPath(currentDirectory, oldName);
    String newPath = buildPath(currentDirectory, newName);

    // Normalize accidental leading //
    if (oldPath.startsWith("//"))  oldPath = oldPath.substring(1);
    if (newPath.startsWith("//")) newPath = newPath.substring(1);

    // Make sure source exists
    if (!SD.exists(oldPath)) {
        request->send(404, "text/plain", "Source file not found: " + oldPath);
        return;
    }

    // Be conservative: refuse to overwrite an existing target
    if (SD.exists(newPath)) {
        request->send(409, "text/plain", "Target already exists: " + newPath);
        return;
    }

    if (SD.rename(oldPath, newPath)) {
        Serial.printf("File renamed: %s -> %s\n", oldPath.c_str(), newPath.c_str());
        request->send(200, "text/plain",
                      "File renamed: " + oldPath + " -> " + newPath);
    } else {
        Serial.printf("Failed to rename: %s -> %s\n", oldPath.c_str(), newPath.c_str());
        request->send(500, "text/plain",
                      "Failed to rename: " + oldPath + " -> " + newPath);
    }
}

void setupServer() {

    // ----------------------------
    // Root UI with SAFE fallback
    // ----------------------------
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {

        if (sdAvailable && SD.exists("/data/index.html")) {
            request->send(SD, "/data/index.html", "text/html");
            return;
        }

        // Fallback (never 500)
        request->send(200, "text/plain",
                      "Hi! This is The Car Counter. UI not found in /data.");
    });

    // ----------------------------
    // Enable Reboot via HTTP
    // ----------------------------

    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Car Counter rebooting...");
        Serial.println("HTTP /reboot requested – restarting ESP32");
        delay(200);
        ESP.restart();
    });    

    // ----------------------------
    // Serve CSS (fallback = 404)
    // ----------------------------
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {

        if (sdAvailable && SD.exists("/data/style.css")) {
            request->send(SD, "/data/style.css", "text/css");
            return;
        }

        request->send(404, "text/plain", "style.css not found in /data");
    });

    // ----------------------------
    // Serve Show Summary webpage
    // Put showSummary.html in /data/
    // ----------------------------
    server.on("/showSummary.html", HTTP_GET, [](AsyncWebServerRequest *request) {

        if (sdAvailable && SD.exists("/data/showSummary.html")) {
            request->send(SD, "/data/showSummary.html", "text/html");
            return;
        }

        request->send(404, "text/plain", "showSummary.html not found in /data");
    });

    // ----------------------------
    // Seasonal ShowSummary.csv
    // /CC/YYYY/ShowSummary.csv
    // ----------------------------
    server.on("/ShowSummary.csv", HTTP_GET, [](AsyncWebServerRequest *request) {

        if (!sdAvailable) {
            request->send(503, "text/plain", "SD not available");
            return;
        }

        String fullPath = String(seasonFolder) + "/ShowSummary.csv";

        if (!SD.exists(fullPath)) {
            request->send(404, "text/plain",
                          "ShowSummary.csv not found in season folder");
            return;
        }

        AsyncWebServerResponse *response =
            request->beginResponse(SD, fullPath, "text/csv");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    // ----------------------------
    // Serve Show Summary Buckets webpage
    // Put showSummaryBuckets.html in /data/
    // ----------------------------
    server.on("/showSummaryBuckets.html", HTTP_GET, [](AsyncWebServerRequest *request) {

        if (sdAvailable && SD.exists("/data/showSummaryBuckets.html")) {
            request->send(SD, "/data/showSummaryBuckets.html", "text/html");
            return;
        }

        request->send(404, "text/plain",
                      "showSummaryBuckets.html not found in /data");
    });

    // ----------------------------
    // Seasonal ShowSummaryBuckets.csv
    // /CC/YYYY/ShowSummaryBuckets.csv
    // ----------------------------
    server.on("/ShowSummaryBuckets.csv", HTTP_GET, [](AsyncWebServerRequest *request) {

        if (!sdAvailable) {
            request->send(503, "text/plain", "SD not available");
            return;
        }

        String fullPath = String(seasonFolder) + "/ShowSummaryBuckets.csv";

        if (!SD.exists(fullPath)) {
            request->send(404, "text/plain",
                          "ShowSummaryBuckets.csv not found in season folder");
            return;
        }

        AsyncWebServerResponse *response =
            request->beginResponse(SD, fullPath, "text/csv");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    // ----------------------------
    // File manager / SD routes
    // ----------------------------
    server.on("/listFiles", HTTP_GET, listSDFiles);
    server.on("/download", HTTP_GET, downloadSDFile);
    // Simple upload page for /uploadToData (so browser GET doesn't 500)
    server.on("/uploadToData", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html",
            "<!doctype html><html><body>"
            "<h3>Upload UI files to /data</h3>"
            "<form method='POST' action='/uploadToData' enctype='multipart/form-data'>"
            "<input type='file' name='file' multiple>"
            "<input type='submit' value='Upload to /data'>"
            "</form>"
            "<p>After upload, go back to <a href='/'>home</a>.</p>"
            "</body></html>"
        );
    });
    server.on("/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        uploadSDFile);
        
    // ----------------------------
    // Handle file uploads to /data directory
    // ---------------------------- 
    server.on("/uploadToData", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        [](AsyncWebServerRequest *request, String filename, size_t index,
           uint8_t *data, size_t len, bool final) {

            if (!sdAvailable) {
                request->send(503, "text/plain", "SD not available");
                return;
            }
            // GAL 25-11-23.x: guarantee /data exists
            if (!SD.exists("/data")) {
                SD.mkdir("/data");
            }
            String fullPath = "/data/" + filename;
            static File uploadFile;

            if (index == 0) { // First chunk
                if (SD.exists(fullPath)) SD.remove(fullPath);

                uploadFile = SD.open(fullPath, FILE_WRITE);
                if (!uploadFile) {
                    request->send(500, "text/plain",
                                  "Failed to open file for writing");
                    return;
                }
            }

            if (uploadFile) uploadFile.write(data, len);

            if (final) {
                if (uploadFile) uploadFile.close();
                request->send(200, "text/plain",
                              "File uploaded successfully to /data");
            }
        });

    // ----------------------------
    // Identity endpoint (Changing Directory)
    // ----------------------------
    server.on("/changeDirectory", HTTP_GET, changeDirectory);

    // ----------------------------
    // NEW: delete and rename 25-12-01 GAL
    // ----------------------------
    server.on("/delete", HTTP_ANY, deleteSDFile);
    server.on("/rename", HTTP_ANY, renameSDFile);

    // ----------------------------
    // Identity endpoint (for UI theming) 25-12-01 GAL
    // ----------------------------
    server.on("/identity", HTTP_GET, [](AsyncWebServerRequest *request) {
        // THIS_MQTT_CLIENT is already "CarCounter" or "GateCounter"
        request->send(200, "text/plain", THIS_MQTT_CLIENT);
    });


    // ----------------------------
    // Elegant OTA
    // ----------------------------
    ElegantOTA.setID(THIS_MQTT_CLIENT);
    ElegantOTA.setFWVersion(FWVersion);
    ElegantOTA.setTitle(OTA_Title);

    ElegantOTA.begin(&server);
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);

    server.begin();
    Serial.println("HTTP server started");
}
