#pragma once
#include <stdint.h>
#include <stddef.h>
namespace orbit_zone {
struct Value {int32_t offset=0;char abbreviation[8]={};};
uint16_t find(const char *name,size_t size); // 0=unknown, no heap or global TZ changes.
const char *name(uint16_t id);
bool at(uint16_t id,uint32_t epoch,Value &value);
uint32_t normalize_key(uint32_t legacy); // Canonicalize persisted alias keys without a format change.
uint32_t key(uint16_t id); // Stable canonical IANA-name hash for the alarm date scope.
}
