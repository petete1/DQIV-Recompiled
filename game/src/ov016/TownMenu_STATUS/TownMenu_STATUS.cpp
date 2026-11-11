#include "ov016/TownMenu_STATUS/TownMenu_STATUS.hpp"
#include "ov016/TownMenu_ROOT/TownMenu_ROOT.hpp"
#include "main/status/PartyStatus.hpp"
#include "main/status/PlayerStatus.hpp"
#include "main/status/BattleHistory.hpp"
#include "ov016/MenuTemplate/MenuTemplate_town.hpp"
#include "ov016/UnkTownMenuDraw/TownMenuScreens.hpp"
#ifdef DQIV_NATIVE
#include "main/menu/MenuCommonDraw.hpp"
#include "main/menu/MenuDataCommon.hpp"
#include "main/status/GameStatus.hpp"
#include "ov015/btl/BattleMenuNative.hpp"
#include "ov016/MenuTemplate/MenuTemplate_materiel.hpp"
#endif

THUMB void TownMenu_STATUS::menuSetup()
{
    status::g_Party.setBattleMode();
    status::g_BattleHistory.historyType_ = status::BattleHistory::RightNow;
    menuItem_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD, menu::MenuItem::CURSORTYPE_ACTIVE);
    cancelItem_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_CANCEL, menu::MenuItem::CURSORTYPE_NONE);
    menuItem_.active_ = 0;
    partyCount_ = status::g_Party.getCount() + 1;
    page_ = 0;
    activeChara_ = 0;
    pageSwap_ = 0;
    summaryPage_ = 2;
    navigator_.setPageNo(0);
    updateSelectSound();
}

THUMB void TownMenu_STATUS::menuExecute()
{
#ifdef DQIV_NATIVE
    // Classic: the whole party in one column (not pages of five), then the party's summary.
    if (classicBattleMenus()) {
        MenuTemplate_materiel::MATERIEL_DIARY_PAGE(&menuItem_, partyCount_ < 12 ? partyCount_ : 12);
        MenuTemplate_town::TOWN_CANCEL(&cancelItem_);
        return;
    }
#endif
    int count = partyCount_ - page_ * 5;
    if (count > 5) {
        count = 5;
    }
    MenuTemplate_town::townMenuSelectCharaIcon(&menuItem_, count, activeChara_);
    MenuTemplate_town::TOWN_CANCEL(&cancelItem_);
}

THUMB void TownMenu_STATUS::menuDraw()
{
    status::g_Party.setBattleMode();
    int index = activeChara_ + page_ * 5;
#ifdef DQIV_NATIVE
    // Classic: the Items window: the members (and the party), the member's equipment (or
    // spells, after A) and their attributes; the party's money and play time.
    if (classicBattleMenus()) {
        const int count = status::g_Party.getCount();
        beginItemsWindow();
        setItemsWindowTitle(menu::MenuDataCommon::getRootMenuName(TownMenu_ROOT::ROOT_STATUS));
        status::HaveStatusInfo* info = index < count ? &status::g_Party.getPlayerStatus(index)->haveStatusInfo_ : NULL;
        if (info != NULL) {
            if (pageSwap_ == 1) {
                int names[32];
                int n = 0;
                for (int i = 0; i < info->haveAction_.getCount() && n < 32; i++) {
                    names[n++] = getActionNameMessage(info->haveAction_.getAction(i));
                }
                storeItemsTexts(names, n, 0xa0000022);
            } else {
                unsigned char worn[5];
                int n = 0;
                for (int i = 0; i < 5; i++) {
                    if (info->haveEquipment_.getEquipment((ItemType)i) != 0) {
                        worn[n++] = info->haveEquipment_.getEquipment((ItemType)i);
                    }
                }
                storeEquipList(worn, n, 0, info, info->haveStatus_.playerIndex_ + 0x50000000);
            }
            setItemsKind(info->haveStatus_.playerIndex_ + 0x50000000);
            addItemsRow(0xa0000003, info->haveStatus_.level_);
            addItemsRowPair(0xa0000001, info->getHp(), info->getHpMax());
            addItemsRowPair(0xa0000002, info->getMp(), info->getMpMax());
            for (int i = 0; i < 7; i++) {
                const int ability = i + 2 > 6 ? i + 2 - 7 : i + 2;  // as drawStatusAbilities lists them
                addItemsRow(menu::MenuDataCommon::getAbilityKind(ability), menu::MenuDataCommon::getStatus(ability, *info));
            }
            if (info->haveStatus_.isPlayer()) {
                addItemsRow(0xa0000010, info->getExp());
            } else {
                addItemsRowText(0xa0000010, 0xa0000073);
            }
        } else {
            setItemsKind(0x8000006e);
            addItemsRow(0xa000002b, status::g_Party.gold_);
            addItemsRow(0xa000002c, status::g_Party.bankMoney_);
            addItemsRow(0xa0000029, status::g_Party.casinoCoin_);
            addItemsRow(0xa000002a, status::g_Party.medalCoin_ + status::g_Party.playerMedalCoin_);
            addItemsRowTime(0xa000002d, status::g_Game.getPlayTime());
        }
        status::g_Party.setBattleMode();
        beginFieldMenu(pc::BattleMenuKind::list, 1);
        for (int i = 0; i < count; i++) {
            addMemberEntry(&status::g_Party.getPlayerStatus(i)->haveStatusInfo_, (short)i);
        }
        addBattleEntry(0x8000006e, (short)count, true);
        setBattleCursor((short)index);
        endItemsWindow(pc::BattleItemsFocus::whose, info);
        status::g_Party.setBattleMode();
        return;
    }
#endif
    if (index == status::g_Party.getCount()) {
        switch (summaryPage_) {
        case 2:
            drawStatusMoneyScreen(page_);
            break;
        case 3:
            drawStatusPartySummaryScreen(-2, 1, page_);
            break;
        case 4:
            drawStatusPartySummaryScreen(-2, 0, page_);
            break;
        }
    } else {
        if (pageSwap_ == 0) {
            drawStatusMainPage(index, page_);
        }
        if (pageSwap_ == 1) {
            drawStatusSpellPage(index, page_);
        }
    }
    menuItem_.drawActive();
}

THUMB void TownMenu_STATUS::menuUpdate()
{
    status::g_Party.setBattleMode();
    if (MenuUpdate_Assist::isCancel(cancelItem_)) {
        if (navigator_.getIndex(activeChara_) == status::g_Party.getCount()) {
            summaryPage_--;
            if (summaryPage_ >= 2) {
                redraw_ = 1;
                return;
            }
        } else {
            pageSwap_--;
            if (pageSwap_ == 0) {
                redraw_ = 1;
                return;
            }
        }
        close();
        gTownMenu_ROOT.open();
        gTownMenu_ROOT.menuItem_.active_ = TownMenu_ROOT::ROOT_STATUS;
        redraw_ = 1;
        return;
    }
    navigator_.setup(5, 1, partyCount_);
#ifdef DQIV_NATIVE
    // Classic: one column; a member's index is still activeChara_ on page_ (five a page).
    if (classicBattleMenus()) {
        menuItem_.active_ = activeChara_ + page_ * 5;
        const int result = battleMenuSelect(menuItem_, navigator_);
        if (result == 0) {
            return;
        }
        const int index = menuItem_.active_;
        if (result == 2) {
            if (index == status::g_Party.getCount()) {
                summaryPage_++;
                if (summaryPage_ > 4) {
                    summaryPage_ = 2;
                }
            } else if (status::g_Party.getPlayerStatus(index)->haveStatusInfo_.haveAction_.getCount() > 0) {
                pageSwap_ = !pageSwap_;
            }
        } else if (index != activeChara_ + page_ * 5) {
            summaryPage_ = 2;
            status::g_BattleHistory.regenesisAdventureTime();
            pageSwap_ = 0;
        }
        page_ = index / 5;
        activeChara_ = index % 5;
        navigator_.setPageNo(page_);
        updateSelectSound();
        redraw_ = 1;
        return;
    }
#endif
    int result = MenuUpdate_Assist::menuSelect(menuItem_, navigator_);
    if (result == 0) {
        return;
    }
    if (result == 2) {
        if (navigator_.getIndex(activeChara_) == status::g_Party.getCount()) {
            summaryPage_++;
            if (summaryPage_ > 4) {
                summaryPage_ = 2;
            }
        } else if (status::g_Party.getPlayerStatus(activeChara_ + page_ * 5)->haveStatusInfo_.haveAction_.getCount() > 0) {
            pageSwap_++;
            if (pageSwap_ > 1) {
                pageSwap_ = 0;
            }
        }
        activeChara_ = menuItem_.active_;
        redraw_ = 1;
        return;
    }
    summaryPage_ = 2;
    status::g_BattleHistory.regenesisAdventureTime();
    pageSwap_ = 0;
    page_ = navigator_.getPageNo();
    activeChara_ = menuItem_.active_;
    updateSelectSound();
    redraw_ = 1;
}

THUMB void TownMenu_STATUS::updateSelectSound()
{
    int index = navigator_.getIndex(activeChara_);
    if (index == status::g_Party.getCount()) {
        menuItem_.enableSE_ = 1;
        return;
    }
    if (status::g_Party.getPlayerStatus(index)->haveStatusInfo_.haveAction_.getCount() == 0) {
        menuItem_.enableSE_ = 0;
    } else {
        menuItem_.enableSE_ = 1;
    }
}
