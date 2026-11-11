#include "main/part/CardCheckPart.hpp"
#include "main/part/GameEndPart.hpp"
#include "main/menu/CatalogView.hpp"
#ifdef DQIV_NATIVE
#include "Settings.hpp"
#endif

CardCheckPart g_CardCheckPart;

ARM void CardCheckPart::initialize()
{
    // The logos (then the opening and the title screen), or the launch menu at once.
#ifdef DQIV_NATIVE
    const int next = pc::gameSettings().skipIntro ? GAME_START_PART : MENU_PART;
#else
    const int next = MENU_PART;
#endif
    switch (profile::SaveLoad::checkBackup()) {
    case 1:
        g_GlobalDQ4.unkfunc_020580fc(next);
        break;
    case -1:
        g_GlobalDQ4.unkfunc_020580fc(next);
        break;
    case -2:
        g_GameEndPart.startWithNextPart(5, next);
        break;
    case -3:
        g_GameEndPart.start(1);
        break;
    case -4:
        g_GameEndPart.start(4);
        break;
    case -5:
        g_GameEndPart.start(3);
        break;
    }
}

ARM void CardCheckPart::terminate()
{
}

ARM void CardCheckPart::onExecutePart()
{
}

ARM void CardCheckPart::onDrawPart()
{
}

ARM void CardCheckPart::onWindowPart()
{
}

ARM void CardCheckPart::onDebugPart()
{
}
