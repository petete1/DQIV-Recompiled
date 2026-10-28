#pragma once
#ifdef DQIV_DEBUG_MENU
#include "globaldefs.h"
#include "main/menu/MenuBase.hpp"
#include "ov016/MenuDebug/DebugPanel.hpp"

// The debug menu (debug builds), a command of the town operation menu.
struct TownMenu_OPERATION_DEBUG : menu::MenuBase
{
    menu::MenuItem item_;
    DebugPanel debug_;

    virtual void menuSetup();
    virtual void menuExecute();
    virtual void menuDraw();
    virtual void menuUpdate();
};

extern TownMenu_OPERATION_DEBUG gTownMenu_OPERATION_DEBUG;
#endif
