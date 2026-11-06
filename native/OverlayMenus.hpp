#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace pc {
// Overlay menus: the game's menus drawn as on the DS, each menu on a layer of
// its own (MenuLayers.hpp), then placed anywhere on the game window by rules.
enum class OverlayMenu {other,battleRoot,battleCommands,battleTarget,battleSpells,battleItems,battleUseOrEquip};
// One menu's buffers, as the game drew them this frame (32x48 cells each).
struct MenuLayerMaps {
    OverlayMenu menu=OverlayMenu::other;
    const std::uint16_t* window=nullptr;
    const std::uint16_t* back=nullptr;
    const std::uint16_t* icon=nullptr;   // this layer's icon cells only
    const std::uint8_t* chars=nullptr;   // 32x48 tiles of 0x20 bytes
};
struct DsRect {int x=0,y=0,w=0,h=0;};    // DS pixels, y 0..383 (top screen first)
// The bounding box of the cells with a character (blank cells are character 0,
// 0xf000 in the window and back maps) of the three maps; w 0 when empty.
DsRect drawnArea(const MenuLayerMaps&);
// Anchor -1 left/top, 0 centre, 1 right/bottom of the game window; offsets in DS pixels.
struct OverlayRule {int anchorX=0,anchorY=0;int offsetX=0,offsetY=0;};
// nullptr: the menu stays where the DS screens show it.
const OverlayRule* overlayRule(OverlayMenu);
// Where the presented picture shows a DS screen: the window pixel of its
// corner and window pixels per DS pixel (the top screen can be smaller).
struct ScreenPlace {float x=0,y=0,scale=1;};
struct LayerPlacement {DsRect area;int x=0,y=0,w=0,h=0;}; // window pixels: the area's top-left and drawn size
// All layers at once (a rule can depend on another layer's area), in the
// layers' order. A menu with a rule is drawn at `ruleScale` window pixels per
// DS pixel; one without stays on its screen (the area's top row's), as presented.
std::vector<LayerPlacement> placeLayers(std::span<const MenuLayerMaps>,int windowWidth,int windowHeight,
    const std::array<ScreenPlace,2>& screens,int ruleScale);
// The topmost placement under a window point (later ones are drawn above), and that point's DS coordinates.
bool pointerOnLayer(std::span<const LayerPlacement>,int x,int y,int& dsX,int& dsY);
// The layer a sprite (its DS centre) belongs to: the topmost whose area contains it, or -1.
int spriteLayer(std::span<const LayerPlacement>,int dsX,int dsY);
// A shown sprite of the two full-screen buffers (128 entries of 4 halfwords each).
struct MenuSprite {int screen=0,entry=0,centreX=0,centreY=0;}; // centre in DS pixels, y 0..383
std::vector<MenuSprite> menuSprites(const std::uint16_t* oamTop,const std::uint16_t* oamBottom);
// The menu system's frame, filled by the game, read by the window.
struct OverlayMenuState {
    bool active=false;                    // Overlay and the menu system drew this frame
    int count=0;
    MenuLayerMaps layers[8];
    const std::uint16_t* oam[2]={};       // top, bottom: 128 entries of 4 halfwords
    // BG numbers of the three menu BGs (0-3 main, 4-7 sub; -1 none), per screen: the two screens can use different BGs.
    int windowBg[2]={-1,-1},backBg[2]={-1,-1},iconBg[2]={-1,-1};
};
OverlayMenuState& overlayMenuState();
}
