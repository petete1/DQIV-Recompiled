#pragma once
#ifdef DQIV_NATIVE
#include "globaldefs.h"
#include "main/menu/MenuBase.hpp"
#include "ov016/MenuSettings/SettingsPanel.hpp"
#ifdef DQIV_DEBUG_MENU
#include "ov016/MenuDebug/DebugPanel.hpp"
#endif

// The PC version's cheats, a command of the town operation menu. With
// DQIV_DEBUG_MENU, the debug tools follow them as more tabs (L / R).
struct TownMenu_OPERATION_CHEATS : menu::MenuBase
{
    menu::MenuItem item_;
    SettingsPanel cheats_;
#ifdef DQIV_DEBUG_MENU
    DebugPanel debug_;
    int tab_ = 0; // 0: the cheats, then the debug pages
#endif

    virtual void menuSetup();
    virtual void menuExecute();
    virtual void menuDraw();
    virtual void menuUpdate();
};

extern TownMenu_OPERATION_CHEATS gTownMenu_OPERATION_CHEATS;
#endif
