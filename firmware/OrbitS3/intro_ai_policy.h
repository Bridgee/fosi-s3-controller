#pragma once
#include <stddef.h>
#include <stdint.h>
namespace orbit_intro_ai {
constexpr size_t MaxResponseBytes=16384,MaxIntroTextBytes=1536;
constexpr unsigned MaxReferences=4;
enum class Topic:uint8_t {Artist,Release,Song};
inline bool present(const char *s){if(!s)return false;for(;*s;++s)if(static_cast<unsigned char>(*s)>32)return true;return false;}
}
