#ifdef DQIV_DEBUG_MENU
#include "ov016/TownMenu_OPERATION/TownMenu_OPERATION_DEBUG.hpp"
#include "ov016/TownMenu_OPERATION/TownMenu_OPERATION_ROOT.hpp"
#include "ov016/TownMenu_ROOT/TownMenu_ROOT.hpp"
#include "ov016/UnkTownMenuDraw/TownMenuDraw.hpp"
#include "main/menu/MenuCommonDraw.hpp"
#include "main/status/PartyStatus.hpp"

THUMB void TownMenu_OPERATION_DEBUG::menuSetup()
{
    status::g_Party.setPlayerMode();
    item_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD_CANCEL, menu::MenuItem::CURSORTYPE_ACTIVE);
    item_.active_ = 0;
    debug_.open();
}

THUMB void TownMenu_OPERATION_DEBUG::menuExecute()
{
    debug_.execute(item_);
}

THUMB void TownMenu_OPERATION_DEBUG::menuDraw()
{
    debug_.draw(item_, 0, 0x100, drawMenuFrame);
}

THUMB void TownMenu_OPERATION_DEBUG::menuUpdate()
{
    if (!debug_.update(item_, redraw_)) {
        return;
    }
    close();
    if (debug_.closeMenus_) {
        // A warp, a battle, a new chapter...: it happens once the menus are
        // closed, as after the Zoom spell.
        gTownMenu_ROOT.stat_ = MENUBASE_STAT_OK;
    } else {
        gTownMenu_OPERATION_ROOT.open();
    }
}
#endif
