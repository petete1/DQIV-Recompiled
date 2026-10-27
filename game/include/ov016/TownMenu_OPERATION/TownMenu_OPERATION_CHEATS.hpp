#pragma once
#ifdef DQIV_NATIVE
#include "globaldefs.h"
#include "main/menu/MenuBase.hpp"
#include "ov016/MenuSettings/SettingsPanel.hpp"

// The PC version's cheats, a command of the town operation menu.
struct TownMenu_OPERATION_CHEATS : menu::MenuBase
{
    menu::MenuItem item_;
    SettingsPanel cheats_;

    virtual void menuSetup();
    virtual void menuExecute();
    virtual void menuDraw();
    virtual void menuUpdate();
};

extern TownMenu_OPERATION_CHEATS gTownMenu_OPERATION_CHEATS;
#endif
