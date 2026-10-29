#include "intro_ai_settings.h"
#include "config.h"
#include <string.h>
// A credential file cannot enable network by its presence. Never include it in
// normal disabled builds or dummy/compile-only verification environments.
#if ORBIT_AI_ENABLED && !defined(ORBIT_BUILD_CHECK) && !ORBIT_AI_TRANSPORT_CHECK
#if __has_include("ai_local.h")
#include "ai_local.h"
#endif
#endif
#ifndef ORBIT_AI_API_KEY
#define ORBIT_AI_API_KEY ""
#endif
#ifndef ORBIT_AI_CA_CERT
#define ORBIT_AI_CA_CERT ""
#endif
namespace orbit_intro_ai {
namespace {size_t bounded(const char *s,size_t cap){if(!s)return 0;size_t n=0;while(n<cap&&s[n])++n;return n;}}
SettingsStatus validateSettings(const ApiSettings &s){
    if(!s.enabled)return SettingsStatus::Disabled;
    const size_t n=bounded(s.key,MaxApiKeyBytes+1);
    if(!n)return SettingsStatus::MissingKey;
    if(n<16||n>MaxApiKeyBytes)return SettingsStatus::InvalidKey;
    for(size_t i=0;i<n;++i){char c=s.key[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return SettingsStatus::InvalidKey;}
    const size_t ca=bounded(s.caPem,MaxCaBytes+1);
    if(!ca)return SettingsStatus::MissingCa;
    // Structural check only. Actual trust/expiry/hostname validation is TLS's
    // responsibility; test fixtures cannot establish a real certificate chain.
    if(ca>MaxCaBytes||strncmp(s.caPem,"-----BEGIN CERTIFICATE-----",27)||!strstr(s.caPem,"-----END CERTIFICATE-----"))return SettingsStatus::InvalidCa;
    for(size_t i=0;i<ca;++i)if(static_cast<unsigned char>(s.caPem[i])<32&&s.caPem[i]!='\r'&&s.caPem[i]!='\n')return SettingsStatus::InvalidCa;
    return SettingsStatus::Ready;
}
ApiSettings configuredApiSettings(){
    ApiSettings s;
#if ORBIT_AI_ENABLED && !defined(ORBIT_BUILD_CHECK) && !ORBIT_AI_TRANSPORT_CHECK
    s.enabled=true;s.key=ORBIT_AI_API_KEY;s.caPem=ORBIT_AI_CA_CERT;
#endif
    return s;
}
}
