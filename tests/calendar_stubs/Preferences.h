#pragma once
#include <map>
#include <string>
#include <cstdint>
inline std::map<std::string,uint32_t> prefs;
struct Preferences {
 bool begin(const char*,bool){return true;}
 int getInt(const char*k,int fallback){return prefs.count(k)?prefs[k]:fallback;}
 unsigned getUInt(const char*k,unsigned fallback){return prefs.count(k)?prefs[k]:fallback;}
 size_t putInt(const char*k,int v){prefs[k]=v;return 4;}
 size_t putUInt(const char*k,unsigned v){prefs[k]=v;return 4;}
};
