#pragma once
#include "esp_partition.h"
inline bool nativeRunningData=false;
inline const esp_partition_t* esp_ota_get_running_partition(){return nativeRunningData?&fakeData:&fakeA;}
inline struct NativeESP{void restart(){++nativeRestarts;}}ESP;
inline void yield(){}
