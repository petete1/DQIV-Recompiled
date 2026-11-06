#ifdef DQIV_NATIVE
#include "main/menu/MenuLayers.hpp"
#include "main/menu/MenuPartsDraw.hpp"
#include "main/dss/BgBuffer.hpp"
#include "main/dss/Display.hpp"
#include "ov015/btl/BattleMenu.hpp"
#include "OverlayMenus.hpp"
#include "Settings.hpp"

namespace {
enum { LAYER_COUNT = 8 };
MenuLayer s_layers[LAYER_COUNT];
int s_used[LAYER_COUNT];                // in use for s_layers[i].menu_
int s_order[LAYER_COUNT];               // layers in this frame's drawing order
int s_orderCount;
int s_current = -1;
BgCharBuffer* s_savedChars;
BgScreenBuffer* s_savedWindow;
BgScreenBuffer* s_savedBack;
OamBuffer s_oam[2];

pc::OverlayMenu kindOf(menu::MenuBase* menu)
{
    if (menu == &gBattleMenu_ROOT) {
        return pc::OverlayMenu::battleRoot;
    }
    if (menu == &gBattleMenu_ACTIONMENU) {
        return pc::OverlayMenu::battleCommands;
    }
    if (menu == &gBattleMenu_ATTACK || menu == &gBattleMenu_MAGIC2ENEMY || menu == &gBattleMenu_MAGIC2PARTY
        || menu == &gBattleMenu_ITEMUSE2ENEMY || menu == &gBattleMenu_ITEMUSE2PARTY) {
        return pc::OverlayMenu::battleTarget;
    }
    if (menu == &gBattleMenu_MAGIC) {
        return pc::OverlayMenu::battleSpells;
    }
    if (menu == &gBattleMenu_ITEM) {
        return pc::OverlayMenu::battleItems;
    }
    if (menu == &gUnkBattleMenu_0216cf44) {
        return pc::OverlayMenu::battleUseOrEquip;
    }
    return pc::OverlayMenu::other;
}

int findLayer(menu::MenuBase* menu)
{
    for (int i = 0; i < LAYER_COUNT; i++) {
        if (s_used[i] && s_layers[i].menu_ == menu) {
            return i;
        }
    }
    for (int i = 0; i < LAYER_COUNT; i++) {
        if (!s_used[i]) {
            s_used[i] = 1;
            s_layers[i].menu_ = menu;
            s_layers[i].window_.init();
            s_layers[i].back_.init();
            fillCharBuffer(s_layers[i].window_.getCharBuffer(), 0);
            fillScreenBufferRect(s_layers[i].window_.getScreenBuffer(), 0, 0, 32, 48, 0xf000);
            fillScreenBufferRect(s_layers[i].back_.getScreenBuffer(), 0, 0, 32, 48, 0xf000);
            for (int c = 0; c < 0x600; c++) {
                s_layers[i].icon_[c] = 0;
            }
            return i;
        }
    }
    return -1;
}
}

bool overlayMenusOn()
{
    return pc::gameSettings().battleMenus == 2;
}

void beginMenuLayer(menu::MenuBase* menu)
{
    s_current = findLayer(menu);
    if (s_current == -1) {
        return;
    }
    MenuLayer* layer = &s_layers[s_current];
    s_savedChars = g_menuWindowChars;
    s_savedWindow = g_menuWindowScreen;
    s_savedBack = g_menuBackScreen;
    g_menuWindowChars = layer->window_.getCharBuffer();
    g_menuWindowScreen = layer->window_.getScreenBuffer();
    g_menuBackScreen = layer->back_.getScreenBuffer();
    if (g_menuBgDrawEnabled) {
        // As setupMenuBgDraw clears the shared buffers when the menus redraw.
        fillCharBuffer(g_menuWindowChars, 0);
        fillScreenBufferRect(g_menuWindowScreen, 0, 0, 32, 48, 0xf000);
        fillScreenBufferRect(g_menuBackScreen, 0, 0, 32, 48, 0xf000);
        for (int c = 0; c < 0x600; c++) {
            layer->icon_[c] = 0;
        }
    }
    if (!layer->drawn_ && s_orderCount < LAYER_COUNT) {
        s_order[s_orderCount++] = s_current;
    }
    layer->drawn_ = 1;
}

void endMenuLayer()
{
    if (s_current == -1) {
        return;
    }
    g_menuWindowChars = s_savedChars;
    g_menuWindowScreen = s_savedWindow;
    g_menuBackScreen = s_savedBack;
    s_current = -1;
}

int currentMenuLayer()
{
    return s_current;
}

OamBuffer* overlayMenuOam(int screen)
{
    return &s_oam[screen];
}

void clearOverlayMenuOam()
{
    s_oam[0].clear();
    s_oam[1].clear();
}

void keepMenuLayerIcon(int layer, int x, int y, int w, int h, const unsigned short* screen)
{
    if (layer < 0 || layer >= LAYER_COUNT || !s_used[layer]) {
        return;
    }
    // MenuIconBg::drawIcon: cells (x, y - 24) of the bottom screen's icon map; here full height.
    for (int j = 0; j < h / 8; j++) {
        for (int i = 0; i < w / 8; i++) {
            const int row = y + j, column = x + i;
            if (row - 24 < 0 || row >= 48 || column < 0 || column >= 32) {
                continue;
            }
            s_layers[layer].icon_[row * 32 + column] = screen[(row - 24) * 32 + column];
        }
    }
}

void publishMenuLayers()
{
    pc::OverlayMenuState& state = pc::overlayMenuState();
    state = pc::OverlayMenuState();
    for (int i = 0; i < LAYER_COUNT; i++) {
        if (s_used[i] && !s_layers[i].drawn_) {
            s_used[i] = 0;   // its menu closed
        }
    }
    if (overlayMenusOn()) {
        state.active = true;
        for (int k = 0; k < s_orderCount; k++) {
            MenuLayer* layer = &s_layers[s_order[k]];
            pc::MenuLayerMaps& maps = state.layers[state.count++];
            maps.menu = kindOf(layer->menu_);
            maps.window = layer->window_.screenData_;
            maps.back = layer->back_.screenData_;
            maps.icon = layer->icon_;
            maps.chars = (const unsigned char*)layer->window_.charData_;
        }
        state.oam[0] = (const unsigned short*)overlayMenuOam(0)->oam_;
        state.oam[1] = (const unsigned short*)overlayMenuOam(1)->oam_;
        // Each screen's BGs, kept from the frames that tell them (the joined
        // modes name only the current screen's).
        static int s_windowBg[2] = { -1, -1 }, s_backBg[2] = { -1, -1 };
        for (int screen = 0; screen < 2; screen++) {
            const int window = getTextBgAtY(screen * 192), back = unkfunc_02081608(screen * 192);
            if (window != -1) {
                s_windowBg[screen] = window;
            }
            if (back != -1) {
                s_backBg[screen] = back;
            }
            state.windowBg[screen] = s_windowBg[screen];
            state.backBg[screen] = s_backBg[screen];
        }
        state.iconBg[1] = 3;   // MenuIconBg::flush: the bottom screen's BG 3
    }
    for (int i = 0; i < LAYER_COUNT; i++) {
        s_layers[i].drawn_ = 0;
    }
    s_orderCount = 0;
}
#endif
