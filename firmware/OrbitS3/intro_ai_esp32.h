#pragma once
#include "intro_ai_https.h"
namespace orbit_intro_ai {
// Returns null when disabled / compile-only. No task is started. Allocate/use
// only on a background owner. API settings and CA bytes must outlive the job.
SecureChannel *createEsp32Channel();
void destroyEsp32Channel(SecureChannel *);
}
