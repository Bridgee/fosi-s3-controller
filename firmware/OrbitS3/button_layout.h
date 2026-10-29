#pragma once
// Project pixels at480x480, not Android dp or Apple pt. See BUTTON_LAYOUT.md.
namespace orbit_layout {
constexpr int HomeSideTop=208, HomeSideSize=64;
constexpr int Radius=12, ChoicePad=8, IconSlot=24, Gap=8, DetailGap=4;
constexpr int ActionIcon=18, SideIcon=26, TileIcon=28, TileGap=16;
inline int centered(int outer,int inner) {return (outer-inner)/2;}
inline int text_height(int title,int detail) {return title+(detail?DetailGap+detail:0);}
inline int text_left() {return ChoicePad+IconSlot+Gap;}
inline int text_width(int width) {return width-text_left()-ChoicePad;}
}
