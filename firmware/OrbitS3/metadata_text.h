#pragma once
#include <stddef.h>
namespace orbit_metadata {
constexpr size_t MaxMetadataBytes=160;
// Display-only Latin canonical composition. Source/model bytes stay untouched.
// Bounded to159 input bytes, no heap, no partial output UTF-8 code point.
// Source and destination must not overlap; unsupported valid scripts are retained.
// Lyrics use the same composition, bounded to240 input bytes.
size_t compose_lyric(char *dest,size_t capacity,const char *source,size_t length);
size_t compose_latin(char *dest,size_t capacity,const char *source,size_t length);
}
