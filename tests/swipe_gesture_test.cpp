#include "swipe_gesture.h"
#include "swipe_cases.h"
#include <assert.h>
#include <stdio.h>
using orbit_touch::SwipeGesture;using D=orbit_touch::SwipeDirection;
int main(){
 for(const auto &c:cases){SwipeGesture g;g.begin(c.x1,c.y1,c.start);auto d=g.end(c.x2,c.y2,c.start+c.elapsed);assert(int(d)==c.expected);assert(!g.active());assert(g.end(c.x2,c.y2,c.start+c.elapsed)==D::None);}
 SwipeGesture g;g.begin(240,240,0);g.move(140,240);g.move(240,240);assert(g.moved()&&g.end(240,240,300)==D::None);
 g.begin(240,240,0);g.move(260,240);assert(!g.moved());g.move(261,240);assert(g.moved());g.cancel();assert(g.end(140,240,300)==D::None);
 g.begin(-32768,-32768,0);g.move(479,479);assert(g.end(479,479,300)==D::None);
 g.begin(240,240,0);g.move(-1,240);g.move(100,240);assert(g.end(100,240,300)==D::None);
 g.begin(240,240,0);g.reset();assert(g.end(100,240,300)==D::None);
 printf("PASS: native swipe direction/threshold/diagonal/duration/wrap/invalid/cancel/reset tests; %zu shared cases\n",sizeof(cases)/sizeof(cases[0]));
}
