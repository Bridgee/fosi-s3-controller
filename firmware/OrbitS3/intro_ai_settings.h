#pragma once
#include <stddef.h>
namespace orbit_intro_ai {
struct ApiSettings {
    bool enabled=false;
    const char *key="",*caPem="";
};
enum class SettingsStatus {Disabled,MissingKey,InvalidKey,MissingCa,InvalidCa,Ready};
SettingsStatus validateSettings(const ApiSettings &);
ApiSettings configuredApiSettings();
constexpr size_t MaxApiKeyBytes=512,MaxCaBytes=16384;
}
