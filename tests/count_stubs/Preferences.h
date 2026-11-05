#pragma once
#include <map>
#include <string>
inline std::map<std::string,std::string> stringPrefs;
struct Preferences {
 unsigned getUInt(const char* k,unsigned fallback){return stringPrefs.count(k)?unsigned(std::stoul(stringPrefs[k])):fallback;}
 size_t putUInt(const char* k,unsigned v){stringPrefs[k]=std::to_string(v);return 4;}
 bool begin(const char*,bool){return true;}
 String getString(const char* k,const char* fallback){return stringPrefs.count(k)?String(stringPrefs[k]):String(fallback);}
 size_t putString(const char* k,const String& v){stringPrefs[k]=v;return v.length();}
};
