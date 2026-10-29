#pragma once
#include "s3_model.h"
#include <stddef.h>
#include <stdint.h>
namespace orbit_lyrics {
constexpr unsigned MaxLines=256,TextBytes=24576,MaxLineBytes=240;
struct Track {char title[160]={},artist[160]={},album[160]={};int duration=0;};
enum class Provider : uint8_t {Lrclib,LrcApi};
struct Line {uint32_t at=0;uint16_t offset=0;};
struct Document {Track track;Provider provider=Provider::Lrclib;uint16_t count=0,used=0;Line lines[MaxLines];char text[TextBytes]={};};
struct Result {uint32_t generation=0;Document *document=nullptr;};
void begin();
uint32_t observe(const s3::Snapshot &snapshot); // UI queues metadata only; no network/heap.
bool receive(Result &result); // Document ownership passes to UI; free with release().
void release(Document *document);
bool worker_tick(bool defer=false); // Existing background worker; true if work consumed this turn.
int current_line(const Document &document,uint64_t positionMs);
const char *provider_name(Provider provider);
}
