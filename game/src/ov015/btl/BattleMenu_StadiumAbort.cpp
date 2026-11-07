#include "ov015/btl/BattleMenu.hpp"
#include "main/status/PartyStatus.hpp"
#include "ov015/btl/BattleMenuNative.hpp"

ARM void BattleMenu_StadiumAbort::menuSetup()
{
    status::g_Party.setBattleMode();
}

ARM void BattleMenu_StadiumAbort::menuExecute()
{
}

ARM void BattleMenu_StadiumAbort::menuDraw()
{
    status::g_Party.setBattleMode();
#ifdef DQIV_NATIVE
    // Classic: the party panel shows the members' HP while the turn plays.
    if (classicBattleMenus()) {
        return;
    }
#endif
    drawPartyHpPercents();
}

ARM void BattleMenu_StadiumAbort::menuUpdate()
{
}
