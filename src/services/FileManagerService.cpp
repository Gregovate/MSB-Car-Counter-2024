#include "traffic/Firmware.h"
#include "traffic/FileManagerPage.h"
#include "traffic/DailyReportPage.h"

// A page visit opens the active recording season, not the machine's date/year.
// Browsing afterward remains explicit; no count/settings file is moved or reset.
void setupFileManagerRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        if(seasonFolder[0]=='/') currentDirectory=String(seasonFolder);
        auto* response=request->beginResponse(200,"text/html",fileManagerPage);
        response->addHeader("Cache-Control","no-store");
        request->send(response);
    });
    server.on("/storageStatus", HTTP_GET, [](AsyncWebServerRequest* request) {
        // Read RAM only: status is available with SD offline and cannot initiate
        // flash writes, journal initialization, bank reclamation or SD recovery.
        // Car uses a char array; Gate uses Arduino String. Normalize before varargs.
        const String activeSeason(seasonFolder);
        char payload[640];
        snprintf(payload,sizeof(payload),
            "{\"device\":\"%s\",\"firmware\":\"%s\",\"seasonFolder\":\"%s\","
            "\"sdAvailable\":%s,\"countsReady\":%s,\"journalHealth\":\"%s\","
            "\"journalArchive\":\"%s\",\"journalFreeRecords\":%lu,"
            "\"beamCaptureHealth\":\"%s\",\"beamCaptureDropped\":%lu,\"daily\":%d,\"show\":%d}",
            THIS_MQTT_CLIENT,FWVersion,activeSeason.c_str(),sdAvailable?"true":"false",
            countsReady()?"true":"false",trafficJournalHealth(),trafficJournalArchiveHealth(),
            static_cast<unsigned long>(trafficJournalFreeRecords()),beamCaptureHealth(),
            static_cast<unsigned long>(beamCaptureDropped()),totalDailyCars,totalShowCars);
        auto* response=request->beginResponse(200,"application/json",payload);
        response->addHeader("Cache-Control","no-store");request->send(response);
    });
    // Register before legacy SD viewers: reports remain reachable without UI assets.
    server.on("/showSummary.html",HTTP_GET,[](AsyncWebServerRequest* request) {
        request->send(request->beginResponse(200,"text/html",dailyReportPage));
    });
    server.on("/showSummaryBuckets.html",HTTP_GET,[](AsyncWebServerRequest* request) {
        request->send(request->beginResponse(200,"text/html",dailyReportPage));
    });
    server.on("/listFiles",HTTP_GET,listSDFiles);
}

void listSDFiles(AsyncWebServerRequest* request) {
    if(!sdAvailable){request->send(503,"text/plain","SD OFFLINE: flash journal status remains available above.");return;}
    String listing="Files in "+currentDirectory+":\nName\tSize (bytes)\tLast Write\n";
    File root=recordingOpen(currentDirectory);
    if(!root || !root.isDirectory()) {
        request->send(404,"text/plain","Directory not found: "+currentDirectory);return;
    }
    // Include folders: TrafficJournal must be discoverable alongside exports.
    while(true) {
        File file=root.openNextFile();if(!file)break;
        listing+=String(file.name());
        if(file.isDirectory())listing+="/\t(folder)\t-\n";
        else {
            listing+="\t";listing+=String(file.size());listing+="\t";
            time_t written=file.getLastWrite();
            if(written>0){struct tm* local=localtime(&written);char date[24];
                strftime(date,sizeof(date),"%Y-%m-%d %H:%M",local);listing+=date;}
            else listing+="unknown";
            listing+="\n";
        }
        file.close();
    }
    root.close();request->send(200,"text/plain",listing);
}
