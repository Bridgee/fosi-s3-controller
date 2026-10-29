#pragma once
#include <stddef.h>
#include <stdint.h>
namespace orbit_intro_platform {
using Task=void*;
uint64_t clock();void *allocate(size_t);void dispose(void*);bool admitted(bool);
bool start(void(*)(void*),void*,unsigned,Task&);void notify(Task);void wait(bool);
namespace fake {extern unsigned allocations,frees,tasks,notifications,failAllocation;extern bool failTask,allow;extern size_t liveBytes;}
}
