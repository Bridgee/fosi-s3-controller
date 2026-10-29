#pragma once
#include <stdint.h>
namespace orbit_touch {
enum class SwipeDirection : uint8_t {None,Left,Right,Up,Down};
// One contact, one release decision. Distances are native480px coordinates.
class SwipeGesture {
public:
    static constexpr int TapSlop=20,MinDistance=48;
    static constexpr uint32_t MinDuration=30,MaxDuration=1200;
    void begin(int x,int y,uint32_t now) {
        x_=x;y_=y;started_=now;active_=true;cancelled_=!valid(x,y);moved_=cancelled_;
    }
    void move(int x,int y) {
        if(!active_ || cancelled_)return;
        if(!valid(x,y)){cancelled_=moved_=true;return;}
        int dx=x-x_,dy=y-y_;
        if(dx*dx+dy*dy>TapSlop*TapSlop)moved_=true;
    }
    SwipeDirection end(int x,int y,uint32_t now) {
        if(!active_)return SwipeDirection::None;
        move(x,y);active_=false;
        uint32_t elapsed=now-started_;
        if(cancelled_ || elapsed<MinDuration || elapsed>MaxDuration)return SwipeDirection::None;
        int dx=x-x_,dy=y-y_,ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
        if(ax>=MinDistance && ax*2>=ay*3)return dx<0?SwipeDirection::Left:SwipeDirection::Right;
        if(ay>=MinDistance && ay*2>=ax*3)return dy<0?SwipeDirection::Up:SwipeDirection::Down;
        return SwipeDirection::None;
    }
    void cancel(){cancelled_=true;}
    void reset(){active_=cancelled_=moved_=false;}
    bool active() const{return active_;}
    bool moved() const{return moved_;}
    bool cancelled() const{return cancelled_;}
private:
    static bool valid(int x,int y){return x>=0 && x<480 && y>=0 && y<480;}
    int16_t x_=0,y_=0;uint32_t started_=0;
    bool active_=false,cancelled_=false,moved_=false;
};
}
