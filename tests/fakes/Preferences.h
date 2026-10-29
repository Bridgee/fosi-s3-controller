#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <string>
#include <map>
#include <vector>
namespace fake_preferences {
struct State {std::map<std::string,std::vector<uint8_t>> values;unsigned writes=0;bool failOpen=false,failWrite=false;};
inline State &state(){static State value;return value;}
}
class Preferences {
    std::string ns;bool opened=false,readOnly=false;
public:
    bool begin(const char *name,bool ro=false,const char * =nullptr) {
        opened=!fake_preferences::state().failOpen;readOnly=ro;ns=std::string(name)+"/";return opened;
    }
    void end(){opened=false;}
    size_t getBytesLength(const char *key) {
        auto &values=fake_preferences::state().values;auto i=values.find(ns+key);
        return opened && i!=values.end()?i->second.size():0;
    }
    size_t getBytes(const char *key,void *dest,size_t length) {
        size_t n=getBytesLength(key);if(!n || length<n)return 0;
        memcpy(dest,fake_preferences::state().values[ns+key].data(),n);return n;
    }
    size_t putBytes(const char *key,const void *data,size_t length) {
        auto &s=fake_preferences::state();if(!opened || readOnly || s.failWrite)return 0;
        auto *p=static_cast<const uint8_t*>(data);s.values[ns+key]={p,p+length};++s.writes;return length;
    }
};
