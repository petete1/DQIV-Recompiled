#ifdef DQIV_NATIVE
#include "ov016/TownMenu_OPERATION/TownMenu_OPERATION_CHEATS.hpp"
#include "ov016/TownMenu_OPERATION/TownMenu_OPERATION_ROOT.hpp"
#include "ov016/UnkTownMenuDraw/TownMenuDraw.hpp"
#include "main/menu/MenuCommonDraw.hpp"
#include "main/status/PartyStatus.hpp"

THUMB void TownMenu_OPERATION_CHEATS::menuSetup()
{
    status::g_Party.setPlayerMode();
    item_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD_CANCEL, menu::MenuItem::CURSORTYPE_ACTIVE);
    item_.active_ = 0;
    cheats_.openCheats();
}

THUMB void TownMenu_OPERATION_CHEATS::menuExecute()
{
    cheats_.execute(item_);
}

THUMB void TownMenu_OPERATION_CHEATS::menuDraw()
{
    cheats_.draw(item_, 0, 0x100, drawMenuFrame);
}

THUMB void TownMenu_OPERATION_CHEATS::menuUpdate()
{
    if (cheats_.update(item_, redraw_)) {
        close();
        gTownMenu_OPERATION_ROOT.open();
    }
}
#endif
