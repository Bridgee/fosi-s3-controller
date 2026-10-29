#pragma once
namespace orbit_touch {
// Screen coordinates: clockwise135..405 degrees, matching the270-degree track.
inline bool track_angle(int x,int y) {
    if(x<0 || x>=480 || y<0 || y>=480)return false;
    const int dx=x-240,dy=y-240;
    return dy<=0 || (dx<0?-dx:dx)>=dy;
}
inline bool volume_track(int x,int y) {
    if(x<0 || x>=480 || y<0 || y>=480)return false;
    const int dx=x-240,dy=y-240,d=dx*dx+dy*dy;
    return d>=185*185 && d<=246*246 && track_angle(x,y);
}
// The bottom aperture leaves a shallow strip. Widen the status target without
// changing its24px artwork or covering the Back glyph above it.
inline bool footer(int x,int y) {
    if(x<200 || x>=280 || y<440 || y>=480)return false;
    const int dx=x-240,dy=y-240;
    return dx*dx+dy*dy<=240*240;
}
}
