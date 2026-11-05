#pragma once
#include <map>
#include <string>
inline std::map<std::string,std::string> stringPrefs;
struct Preferences {
 bool begin(const char*,bool){return true;}
 String getString(const char* k,const char* fallback){return stringPrefs.count(k)?String(stringPrefs[k]):String(fallback);}
 size_t putString(const char* k,const String& v){stringPrefs[k]=v;return v.length();}
};
