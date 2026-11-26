#include "traffic/Firmware.h"
#include "traffic/JournalInspection.h"

// Extracted from the production baseline; behavior fixes are separate changes.









static void sendNightlyReport(AsyncWebServerRequest* request,bool buckets) {
    String csv;
    int status=readNightlyReport(buckets,csv);
    if (status!=200) {
        request->send(status,"text/plain",status==404 ? "No nightly report yet" :
            status==422 ? "Unrecognized nightly CSV header" : "Nightly report unavailable");
        return;
    }
    AsyncWebServerResponse* response=request->beginResponse(200,"text/csv",csv);
    response->addHeader("Cache-Control","no-store");
    response->addHeader("Access-Control-Allow-Origin","*");
    request->send(response);
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

    File &uploadFile = request->_tempFile; // Keep track of the currently uploading file

    // Build full path using helper so currentDirectory is respected
    String fullPath = buildPath(currentDirectory, filename);
    
    if (fullPath.startsWith("//")) {
        fullPath = fullPath.substring(1);  // normalize leading //
    }

    // Handle the start of the upload
    // Check every chunk, including uploads crossing January 1.
    if (!recordingWebWritable(index == 0 ? fullPath : String(uploadFile.path()))) {
        if (uploadFile) uploadFile.close();
        request->send(403, "text/plain", "Protected recording folder or recovery file");
        return;
    }
    if (index == 0) {
        Serial.printf("Upload started: %s\n", fullPath.c_str());

        if (SD.exists(fullPath)) {
            recordingRemove(fullPath); // Remove the file if it already exists
        }

        uploadFile = recordingOpen(fullPath, FILE_WRITE);
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
        File f = recordingOpen(target);
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
    if (!recordingWebWritable(fullPath)) {
        if(request->hasParam("backup") && request->getParam("backup")->value()=="confirmed" &&
           (removeObsoleteClosedCount(fullPath) || removeRetiredJournalArchive(fullPath))) {
            request->send(200,"text/plain","Obsolete backed-up binary archive deleted: "+fullPath);
        } else request->send(403,"text/plain","Protected or still needed for recovery. Binary cleanup requires backup confirmation and an obsolete, verified record. Live flash archive dependencies, undated events and current checkpoints are protected.");
        return;
    }
    if (SD.exists(fullPath)) {
        if (recordingRemove(fullPath)) {
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
    if (!recordingWebWritable(oldPath) || !recordingWebWritable(newPath)) {
        request->send(403, "text/plain", "Protected recording folder or recovery file");
        return;
    }

    if (recordingRename(oldPath, newPath)) {
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
    setupFileManagerRoutes();
    setupOccupancyRoutes(); // Shared flash-backed UI and recovery status.


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
    // Shared embedded report viewer registered by setupFileManagerRoutes().

    // ----------------------------
    // Downloads, HTML tables and printed reports all read these two routes.
    server.on("/ShowSummary.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendNightlyReport(request,false);
    });
    server.on("/ShowSummaryBuckets.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendNightlyReport(request,true);
    });

    // File manager / SD routes
    // ----------------------------
    // /listFiles is registered by the common file manager.
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
                recordingMkdir("/data");
            }
            String fullPath = "/data/" + filename;
            File &uploadFile = request->_tempFile;
            // Check every chunk, including uploads crossing January 1.
            if (!recordingWebWritable(index == 0 ? fullPath : String(uploadFile.path()))) {
                if (uploadFile) uploadFile.close();
                request->send(403, "text/plain", "Protected recording folder or recovery file");
                return;
            }

            if (index == 0) { // First chunk
                if (SD.exists(fullPath)) recordingRemove(fullPath);

                uploadFile = recordingOpen(fullPath, FILE_WRITE);
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

    setupJournalInspection(); // Read-only metadata and verified DATA-region backup.
    server.begin();
    Serial.println("HTTP server started");
}

