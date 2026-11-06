#include "ov015/btl/BattleMenu.hpp"
#include "ov015/btl/BattleMenuNative.hpp"
#include "ov015/btl/BattleMenuPlayerControl.hpp"
#include "main/status/PartyStatus.hpp"
#include "main/status/PlayerStatus.hpp"

THUMB void BattleMenu_MAGIC2PARTY::menuSetup()
{
    status::g_Party.setBattleMode();
    menuItem_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD, menu::MenuItem::CURSORTYPE_ACTIVE);
    cancelItem_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_CANCEL, menu::MenuItem::CURSORTYPE_NONE);
    partyMax_ = status::g_Party.getCarriageOutCount();
    magic_ = btl::BattleMenuPlayerControl::getSingleton()->activeMagic_;
}

THUMB void BattleMenu_MAGIC2PARTY::menuExecute()
{
    navigator_.setup(2, 2, partyMax_);
    MenuTemplate_battle::BATTLE_PARTY_2x2(&menuItem_, partyMax_);
    MenuTemplate_battle::BATTLE_CANCEL(&cancelItem_);
}

THUMB void BattleMenu_MAGIC2PARTY::menuDraw()
{
#ifdef DQIV_NATIVE
    if (classicBattleMenus()) {
        drawBattlePartyTargets(partyMax_, menuItem_.active_);
        return;
    }
#endif
    drawMagicOnPartyMenu();
    menuItem_.drawActive();
}

THUMB void BattleMenu_MAGIC2PARTY::menuUpdate()
{
    int chara = btl::BattleMenuPlayerControl::getSingleton()->activeChara_;
    if (MenuUpdate_Assist::isCancel(cancelItem_)) {
        status::g_Party.getPlayerStatus(chara)->haveBattleStatus_.setSelectCommand(status::HaveBattleStatus::UseAction, -1);
        close();
        gBattleMenu_MAGIC.open();
        gBattleMenu_MAGIC.setActiveMagicPos(activeMagicPos_);
        return;
    }
#ifdef DQIV_NATIVE
    int result = classicBattleMenus() ? battleMenuSelect(menuItem_, navigator_) : MenuUpdate_Assist::menuSelect(menuItem_, navigator_);
#else
    int result = MenuUpdate_Assist::menuSelect(menuItem_, navigator_);
#endif
    if (result != 0) {
        redraw_ = 1;
        if (result == 2) {
            close();
            BattleMenuJudge::getSingleton()->setMagicParty(magic_, menuItem_.active_);
            BattleMenuJudge::getSingleton()->setNextPlayer();
        }
        int target = menuItem_.active_;
        btl::BattleMenuPlayerControl::getSingleton()->targetChara_ = target;
        return;
    }
    int target = menuItem_.active_;
    btl::BattleMenuPlayerControl::getSingleton()->targetChara_ = target;
}
