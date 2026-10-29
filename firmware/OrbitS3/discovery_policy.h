#pragma once
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
namespace s3 {
inline bool s3_model(const char *value) {
    if(!value)return false;
    char normalized[65]={};size_t n=0;
    for(const unsigned char *p=reinterpret_cast<const unsigned char*>(value);*p;++p) {
        if(isalnum(*p)) {if(n==sizeof(normalized)-1)return false;normalized[n++]=char(tolower(*p));}
    }
    return !strcmp(normalized,"s3") || !strcmp(normalized,"fosis3") ||
        !strcmp(normalized,"fosiaudios3") || !strncmp(normalized,"fosis3stream",12);
}
inline bool cast_id(const char *value) {
    if(!value)return false;
    size_t n=strlen(value);if(n<16 || n>64)return false;
    for(size_t i=0;i<n;++i) if(!isxdigit(static_cast<unsigned char>(value[i])) && value[i]!='-')return false;
    return true;
}
inline bool same_id(const char *a,const char *b) {
    while(*a && *b) {
        if(tolower(static_cast<unsigned char>(*a++))!=tolower(static_cast<unsigned char>(*b++)))return false;
    }
    return *a==*b;
}
inline bool local_candidate(uint32_t ip,uint32_t local,uint32_t mask) {
    unsigned first=ip>>24;
    return mask && first && first!=127 && first<224 && ip!=local &&
        ip!=((local&mask)|~mask) && (ip&mask)==(local&mask);
}
struct DiscoveryChoice {
    char ip[16]={},id[65]={};bool multiple=false;
    void consider(const char *address,const char *identity,const char *pinned) {
        if(!cast_id(identity) || (pinned[0] && !same_id(identity,pinned)))return;
        if(ip[0] && (!same_id(identity,id) || strcmp(address,ip))) {multiple=true;return;}
        snprintf(ip,sizeof(ip),"%s",address);snprintf(id,sizeof(id),"%s",identity);
    }
    bool unique() const {return ip[0] && !multiple;}
};
}
