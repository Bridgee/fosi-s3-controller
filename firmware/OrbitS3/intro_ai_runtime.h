#pragma once
#include "config.h"
#if ORBIT_INTRO_MOCK || (ORBIT_AI_ENABLED && ORBIT_LIVE && !defined(ORBIT_BUILD_CHECK)) || ORBIT_AI_TRANSPORT_CHECK
#define ORBIT_INTRO_ASYNC 1
#else
#define ORBIT_INTRO_ASYNC 0
#endif
#if ORBIT_INTRO_ASYNC
#include "intro_ai_async.h"
namespace orbit_intro_ai {struct ApiSettings;}
namespace orbit_intro_runtime {
uint64_t nowMs();
orbit_intro_async::UiClient *client(); // lazy checked PSRAM + one background task
orbit_intro_ai::Outcome startupOutcome();
#if defined(ORBIT_AI_RUNTIME_HOST_TEST)
void testPump(); // fake scheduler entry; never compiled for an ESP32
void testReset();
#if !ORBIT_INTRO_MOCK
orbit_intro_async::UiClient *testRealClient(orbit_intro_ai::ApiSettings);
#endif
#endif
}
#endif
