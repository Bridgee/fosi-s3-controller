#include "intro_async_platform.h"
#include <lvgl.h>
#include <stdlib.h>
#include <assert.h>
namespace orbit_intro_platform {
namespace fake {unsigned allocations=0,frees=0,tasks=0,notifications=0,failAllocation=0;bool failTask=false,allow=true;size_t liveBytes=0;}
uint64_t clock(){static uint64_t elapsed=0;static uint32_t previous=0;const auto n=lv_tick_get();elapsed+=uint32_t(n-previous);previous=n;return elapsed;}
void *allocate(size_t n){++fake::allocations;if(fake::failAllocation==fake::allocations)return nullptr;auto *p=static_cast<size_t*>(malloc(n+sizeof(size_t)));if(!p)return nullptr;*p=n;fake::liveBytes+=n;return p+1;}
void dispose(void *p){if(p){auto *n=static_cast<size_t*>(p)-1;fake::liveBytes-=*n;++fake::frees;free(n);}}
bool admitted(bool){return fake::allow;}
bool start(void(*)(void*),void*,unsigned n,Task &task){assert(n==8192);if(fake::failTask)return false;++fake::tasks;task=reinterpret_cast<void*>(1);return true;}
void notify(Task){++fake::notifications;}
void wait(bool){assert(false&&"host UI test must schedule pump explicitly, not run infinite task");}
}
