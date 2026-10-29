#pragma once
#include <stddef.h>
#include <stdint.h>
namespace s3 {
constexpr unsigned ArtworkSide=170;
constexpr size_t ArtworkBytes=ArtworkSide*ArtworkSide*2;
constexpr unsigned ArtworkLargeSide=320;
constexpr size_t ArtworkLargeBytes=ArtworkLargeSide*ArtworkLargeSide*2;
constexpr size_t ArtworkDownloadLimit=256*1024;
// Bounded RGB565 decode directly from the original. Caller owns result (free()).
// Independent decoder instances only; no LVGL objects/cache/renderer access.
uint8_t *decode_artwork(const uint8_t *compressed,size_t size,unsigned side=ArtworkSide);
// Downsample the detailed cache once in the artwork worker.
uint8_t *artwork_thumbnail(const uint8_t *large);
}
