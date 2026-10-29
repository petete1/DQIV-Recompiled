#ifdef DQIV_NATIVE
#include "ov016/TownMenu_OPERATION/TownMenu_OPERATION_CHEATS.hpp"
#include "ov016/TownMenu_OPERATION/TownMenu_OPERATION_ROOT.hpp"
#include "ov016/TownMenu_ROOT/TownMenu_ROOT.hpp"
#include "ov016/UnkTownMenuDraw/TownMenuDraw.hpp"
#include "main/menu/MenuCommonDraw.hpp"
#include "main/status/PartyStatus.hpp"
#include "main/dss/Pad.hpp"
#include "Settings.hpp"
#include <nitro/pad.h>

#ifdef DQIV_DEBUG_MENU
namespace {
// The window's tabs: the cheats (in the game's language), then the debug pages.
const char* s_tabs[1 + DebugPanel::PAGE_COUNT];
void buildTabs()
{
    s_tabs[0] = pc::pageTitle(pc::pageCheats, pc::gameSettings().language);
    for (int page = 0; page < DebugPanel::PAGE_COUNT; page++) {
        s_tabs[page + 1] = DebugPanel::pageTitle(page);
    }
}
}
#endif

THUMB void TownMenu_OPERATION_CHEATS::menuSetup()
{
    status::g_Party.setPlayerMode();
    item_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD_CANCEL, menu::MenuItem::CURSORTYPE_ACTIVE);
    item_.active_ = 0;
    cheats_.openCheats();
#ifdef DQIV_DEBUG_MENU
    buildTabs();
    cheats_.tabs_ = s_tabs;
    cheats_.tabCount_ = 1 + DebugPanel::PAGE_COUNT;
    debug_.open();
    tab_ = 0;
#endif
}

THUMB void TownMenu_OPERATION_CHEATS::menuExecute()
{
#ifdef DQIV_DEBUG_MENU
    if (tab_ != 0) {
        debug_.execute(item_);
        return;
    }
#endif
    cheats_.execute(item_);
}

THUMB void TownMenu_OPERATION_CHEATS::menuDraw()
{
#ifdef DQIV_DEBUG_MENU
    if (tab_ != 0) {
        debug_.draw(item_, 0, 0x100, drawMenuFrame);
        return;
    }
#endif
    cheats_.draw(item_, 0, 0x100, drawMenuFrame);
}

THUMB void TownMenu_OPERATION_CHEATS::menuUpdate()
{
#ifdef DQIV_DEBUG_MENU
    // L and R turn the tabs: the cheats, then the debug pages.
    int tab = tab_;
    const int count = 1 + DebugPanel::PAGE_COUNT;
    if (dss::g_Pad.edge() & PAD_BUTTON_R) {
        tab = (tab + 1) % count;
    }
    if (dss::g_Pad.edge() & PAD_BUTTON_L) {
        tab = (tab + count - 1) % count;
    }
    if (tab != tab_) {
        tab_ = tab;
        if (tab_ != 0) {
            debug_.showPage(tab_ - 1, s_tabs, count);
        }
        item_.active_ = 0;
        item_.result_ = 0;
        item_.lastresult_ = 0;
        redraw_ = 1;
        return;
    }
    if (tab_ != 0) {
        if (!debug_.update(item_, redraw_)) {
            return;
        }
        close();
        if (debug_.closeMenus_) {
            // A warp, a battle, a new chapter...: it happens once the menus
            // are closed, as after the Zoom spell.
            gTownMenu_ROOT.stat_ = MENUBASE_STAT_OK;
        } else {
            gTownMenu_OPERATION_ROOT.open();
        }
        return;
    }
#endif
    if (cheats_.update(item_, redraw_)) {
        close();
        gTownMenu_OPERATION_ROOT.open();
    }
}
#endif
