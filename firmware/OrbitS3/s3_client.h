#pragma once
#include "s3_model.h"
namespace s3 {
struct Artwork { uint8_t *data=nullptr,*large=nullptr; uint32_t size=0,largeSize=0,revision=0; char url[512]={}; };
// HTTP/state and artwork decoding have separate workers. Call these from UI.
void begin();
// Local polling hint only; does not send any pairing/control action.
void requestBluetoothInfo(bool aboutVisible);
bool receive(Snapshot &snapshot);
bool send(Action action,int value=0);
// Ownership of data passes to the UI, which must free it after dropping its
// LVGL image-cache entry. Workers never touch LVGL objects/cache/rendering;
// bundled PNG allocation wrappers use the pinned thread-safe PSRAM allocator.
bool receiveArtwork(Artwork &artwork);
}
