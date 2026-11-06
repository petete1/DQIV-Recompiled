#pragma once
#ifdef DQIV_NATIVE
#include "main/menu/MenuBase.hpp"
#include "main/menu/MenuBg.hpp"
#include "main/dss/VramRequest.hpp"

// Overlay menus (battleMenus 2): each open menu draws into buffers of its own
// instead of the shared ones (g_MenuBg), so overlapping windows of different
// menus keep all their pixels; nothing reaches the DS screens, and the native
// side draws the layers on the overlay (OverlayMenus.hpp).
struct MenuLayer {
    menu::MenuBase* menu_;
    int drawn_;                         // drawn this frame
    MenuWindowBg window_;
    MenuBackBg back_;
    unsigned short icon_[0x600];        // 32x48 cells: this layer's icons
};

bool overlayMenusOn();
// Around one menu's MenuBase::menuBaseDraw: its buffers become the menu
// buffers (cleared first when the menus redraw); a full pool (8) keeps the
// shared ones.
void beginMenuLayer(menu::MenuBase* menu);
void endMenuLayer();
// The layer drawing now, -1 outside a menu's draw.
int currentMenuLayer();
// drawMenuDisplays: the two full-screen sprite buffers (top, bottom), cleared each frame.
OamBuffer* overlayMenuOam(int screen);
// Cleared when the DS ones are (clearOamBuffers).
void clearOverlayMenuOam();
// MenuIconBg::flush, after its requests are drawn: the request's cells go to its layer.
void keepMenuLayerIcon(int layer, int x, int y, int w, int h, const unsigned short* screen);
// End of drawMenuSystem: frees the layers of menus that did not draw and
// publishes the frame for the native side.
void publishMenuLayers();
#endif
