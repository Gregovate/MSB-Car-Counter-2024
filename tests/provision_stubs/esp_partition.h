#pragma once
#include <cstdint>
#define ESP_OK 0
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_SUBTYPE_DATA_SPIFFS 0x82
#define ESP_PARTITION_SUBTYPE_APP_OTA_0 0x10
#define ESP_PARTITION_SUBTYPE_APP_OTA_1 0x11
struct esp_partition_t{uint32_t type,subtype,address,size;bool encrypted;};
inline esp_partition_t fakeData{1,0x82,0x290000,0x160000,false},fakeA{0,0x10,0x10000,0x140000,false},fakeB{0,0x11,0x150000,0x140000,false};
inline const esp_partition_t* esp_partition_find_first(int type,int subtype,const char*){if(type==1 && subtype==0x82)return &fakeData;if(type==0 && subtype==0x10)return &fakeA;if(type==0 && subtype==0x11)return &fakeB;return nullptr;}
inline unsigned nativeEraseCalls=0,nativeRestarts=0;
inline int nativeSeedWriteBudget=-1;
inline int nativeEraseBudget=-1,nativeInvalidateBaselineAfter=-1;
inline int esp_partition_read(const esp_partition_t*,uint32_t offset,void*out,size_t n){return nativeFlash.read(offset,out,n)?0:-1;}
inline int esp_partition_write(const esp_partition_t*,uint32_t offset,const void*data,size_t n){size_t take=n;if(nativeSeedWriteBudget>=0){take=std::min(n,size_t(nativeSeedWriteBudget));nativeSeedWriteBudget-=take;}
 return nativeFlash.write(offset,data,take) && take==n?0:-1;}
inline int esp_partition_erase_range(const esp_partition_t*p,uint32_t offset,size_t n){
 if(p!=&fakeData || offset%4096 || n!=4096 || offset+n>1048576)return -1;
 if(nativeEraseBudget==0)return -1;
 if(nativeEraseBudget>0)--nativeEraseBudget;
 ++nativeEraseCalls;
 if(nativeInvalidateBaselineAfter==int(nativeEraseCalls))++totalDailyCars;
 return nativeFlash.erase(offset,n)?0:-1;
}
