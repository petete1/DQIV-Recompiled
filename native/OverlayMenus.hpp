#pragma once
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
// The bounding box of the non-empty cells of the three maps; w 0 when empty.
DsRect drawnArea(const MenuLayerMaps&);
// Anchor -1 left/top, 0 centre, 1 right/bottom of the game window; offsets in DS pixels.
struct OverlayRule {int anchorX=0,anchorY=0;int offsetX=0,offsetY=0;};
// nullptr: the menu stays where the DS screens show it.
const OverlayRule* overlayRule(OverlayMenu);
struct LayerPlacement {DsRect area;int x=0,y=0;int scale=1;}; // window pixels of the area's top-left
// All layers at once (a rule can depend on another layer's area), in the
// layers' order. dsOrigin: window pixels of DS pixel (0,0) as presented;
// dsScale: window pixels per DS pixel.
std::vector<LayerPlacement> placeLayers(std::span<const MenuLayerMaps>,int windowWidth,int windowHeight,
    int dsOriginX,int dsOriginY,int dsScale);
// The topmost placement under a window point (later ones are drawn above), and that point's DS coordinates.
bool pointerOnLayer(std::span<const LayerPlacement>,int x,int y,int& dsX,int& dsY);
// The layer a sprite (its DS centre) belongs to: the topmost whose area contains it, or -1.
int spriteLayer(std::span<const LayerPlacement>,int dsX,int dsY);
// The menu system's frame, filled by the game, read by the window.
struct OverlayMenuState {
    bool active=false;                    // Overlay and the menu system drew this frame
    int count=0;
    MenuLayerMaps layers[8];
    const std::uint16_t* oam[2]={};       // top, bottom: 128 entries of 4 halfwords
    int windowBg=-1,backBg=-1,iconBg=-1;  // main-engine BG numbers of the three menu BGs
};
OverlayMenuState& overlayMenuState();
}
