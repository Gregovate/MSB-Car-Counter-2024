#pragma once
#include "PartitionGuard.h"
#include <esp_partition.h>
namespace traffic {
// Explicit ESP-IDF partition handle, never an absolute flash address. This
// adapter does NOT format, resize, mount or alter any partition table.
// Bank erase is bounds-checked; JournalStore requires a verified recovery receipt.
class Esp32JournalPartition {
    const esp_partition_t* partition=nullptr;
    bool approved=false;
    static JournalPartition describe(const esp_partition_t* p) {
        return {uint32_t(p->type),uint32_t(p->subtype),p->address,p->size,p->encrypted};
    }
public:
    bool begin() {
        approved=false;partition=nullptr;
        auto* data=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
        auto* a=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,nullptr);
        auto* b=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,nullptr);
        if(!data || !a || !b || !approveJournalPartition(describe(data),describe(a),describe(b)))return false;
        partition=data;approved=true;return true;
    }
    bool read(uint32_t offset,void* out,size_t bytes) {
        return approved && offset<=1024U*1024U && bytes<=1024U*1024U-offset &&
            esp_partition_read(partition,offset,out,bytes)==ESP_OK;
    }
    bool erase(uint32_t offset,size_t bytes) {
        return approved && offset%4096==0 && bytes%4096==0 && offset<=1024U*1024U &&
            bytes<=1024U*1024U-offset && esp_partition_erase_range(partition,offset,bytes)==ESP_OK;
    }
    bool write(uint32_t offset,const void* data,size_t bytes) {
        return approved && offset<=1024U*1024U && bytes<=1024U*1024U-offset &&
            esp_partition_write(partition,offset,data,bytes)==ESP_OK;
    }
};
}
