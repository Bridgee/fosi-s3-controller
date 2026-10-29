#pragma once
#include "config.h"
namespace orbit_iphone {
enum class LaunchState {Unconfigured,Offline,Syncing,Ready,Sending,Sent,Uncertain,Failed,Cooldown};
LaunchState state();
bool start(); // UI queues only; HTTP stays on the existing network worker.
void serial_tick(); // Compiled out unless explicitly enabled for bench tests.
void worker_tick();
bool busy(); // One-shot launch has priority over background weather.
}
