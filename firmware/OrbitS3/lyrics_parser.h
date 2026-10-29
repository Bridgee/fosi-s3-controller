#pragma once
#include "lyrics.h"
#include <string.h>
#include <math.h>
namespace orbit_lyrics {
bool same_track(const Track &a,const Track &b);
bool make_track(const s3::Snapshot &source,Track &track);
bool same_name(const char *a,const char *b);
bool same_title(const char *a,const char *b); // Annotation cleanup, with actual live/remix/language versions retained.
bool relaxed_track(const Track &source,Track &result);
bool valid_text(const char *text,size_t length,bool multiline=false);
bool parse_lrc(const char *source,size_t length,const Track &track,Document &out,bool requireSignature=false);
}
