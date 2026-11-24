#pragma once
#include <map>
#include <string>
inline std::map<std::string,std::string> stringPrefs;
struct Preferences {
 unsigned getUInt(const char* k,unsigned fallback){return stringPrefs.count(k)?unsigned(std::stoul(stringPrefs[k])):fallback;}
 size_t putUInt(const char* k,unsigned v){stringPrefs[k]=std::to_string(v);return 4;}
 size_t getBytesLength(const char* k){return stringPrefs.count(std::string("env:")+k)?stringPrefs[std::string("env:")+k].size():0;}
 size_t getBytes(const char* k,void* out,size_t n){auto key=std::string("env:")+k;if(!stringPrefs.count(key))return 0;n=std::min(n,stringPrefs[key].size());memcpy(out,stringPrefs[key].data(),n);return n;}
 size_t putBytes(const char* k,const void* data,size_t n){stringPrefs[std::string("env:")+k]=std::string(static_cast<const char*>(data),n);return n;}
 bool begin(const char*,bool){return true;}
 String getString(const char* k,const char* fallback){return stringPrefs.count(k)?String(stringPrefs[k]):String(fallback);}
 size_t putString(const char* k,const String& v){stringPrefs[k]=v;return v.length();}
};
