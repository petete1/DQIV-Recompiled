#include "ov016/TownMenu_OPERATION/TownMenu_OPERATION_CAREER.hpp"
#include "ov016/TownMenu_OPERATION/TownMenu_OPERATION_ROOT.hpp"
#include "main/menu/MenuManager.hpp"
#include "main/status/PartyStatus.hpp"
#include "main/status/BattleHistory.hpp"
#include "ov003/btl/BattleActorManager2.hpp"
#include "ov037/PlayerTitle.hpp"
#include "ov016/MenuTemplate/MenuTemplate_town.hpp"
#include "ov016/UnkTownMenuDraw/TownMenuOperationDraw.hpp"
#include "main/menu/MenuPartsDraw.hpp"
#include "main/menu/MenuCommonDraw.hpp"
#include "main/menu/MaterielMenuOverlay.hpp"
#ifdef DQIV_NATIVE
#include "ov015/btl/BattleMenuNative.hpp"
#include "ov016/TownMenu_ROOT/TownMenu_ROOT.hpp"
#include "main/dss/Pad.hpp"
#include <nitro/pad.h>
#endif

THUMB void TownMenu_OPERATION_CAREER::menuSetup()
{
    status::g_Party.setPlayerMode();
    menuItem_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD_CANCEL, menu::MenuItem::CURSORTYPE_NONE);
    menuItem_.enableSE_ = 0;
    navigator_.setupBase();
    bookOfBeasts_ = 0;
    for (int i = 0; i < status::g_Party.getCount(); i++) {
        if (status::g_Party.getPlayerStatus(i)->haveStatusInfo_.haveItem_.isItem(0x9d)) {
            bookOfBeasts_ = 1;
            break;
        }
    }
    if (status::g_Party.haveItemSack_.isItem(0x9d)) {
        bookOfBeasts_ = 1;
    }
    loadMaterielMenuOverlay(14);
    cmn::PlayerTitle::setPlayerTitle(0);
    for (int i = 0; i < 3; i++) {
        status::g_BattleHistory.historyType_ = (status::BattleHistory::HistoryType)i;
        if (status::g_BattleHistory.getAdventureTime() != 0) {
            pageMax_ = i;
        }
        unk_28[i] = i;
    }
    page_ = 0;
    status::g_BattleHistory.historyType_ = status::BattleHistory::RightNow;
    status::g_BattleHistory.regenesisAdventureTime();
}

THUMB void TownMenu_OPERATION_CAREER::menuExecute()
{
    MenuTemplate_town::TOWN_CAREER_SWITCH(&menuItem_, menuItem_.active_);
}

THUMB void TownMenu_OPERATION_CAREER::menuDraw()
{
    int type;
    if (page_ != 0) {
        type = 2;
    } else {
        type = 1;
    }
#ifdef DQIV_NATIVE
    // Classic: one window of records, now (and beside it the earlier record A switches).
    if (classicBattleMenus()) {
        static const int names[7] = { 0xa0000191, 0xa0000192, 0xa0000193, 0xa0000194, 0xa0000195, 0xa0000196, 0xa0000197 };
        static const int units[7] = { 0xa000019d, 0xa000019e, 0xa000003c, 0xa000019d, 0xa000019d, 0xa000019d, 0xa00001a0 };
        const bool earlier = pageMax_ >= 1;
        const int columns = earlier ? 2 : 1;
        status::BattleHistory& history = status::g_BattleHistory;
        beginFieldMenu(pc::BattleMenuKind::list, columns);
        setBattleTitle(0x80000082);
        short n = 0;
        // Row by row: now, then the earlier record.
        for (int row = 0; row < 10; row++) {
            for (int column = 0; column < columns; column++) {
                history.historyType_ = column == 0 ? status::BattleHistory::RightNow : (status::BattleHistory::HistoryType)type;
                if (row == 0) {
                    addRecordTime(column == 0 ? 0xa000002d : type == 1 ? 0xa000019a : 0xa000019b, history.getAdventureTime(), n++);
                } else if (row == 1) {
                    addRecordText(0xa0000199, history.getTitle() - 0x20000000, n++);
                } else if (row < 9) {
                    int value = 0;
                    switch (row) {
                    case 2: value = history.getBattleCount(); break;
                    case 3: value = history.getMonsterCount(); break;
                    case 4: value = history.getTotalGold(); break;
                    case 5: value = history.getVictoryCount(); break;
                    case 6: value = history.getWipeoutCount(); break;
                    case 7: value = history.getEscapeCount(); break;
                    default: value = history.getMaxDamage(); break;
                    }
                    addRecordNumber(names[row - 2], value, units[row - 2], n++);
                } else if (column == 1) {
                    addRecordNumber(0xa000019c, history.getHeroLevel(), 0xa000003f, n++);
                } else if (bookOfBeasts_ != 0) {
                    addRecordNumber(0xa0000198, history.getRestMonsterCount(), 0xa000019e, n++);
                } else if (earlier) {
                    addRecordText(0, 0, n++);  // keeps the earlier record's hero level in its column
                }
            }
        }
        history.historyType_ = status::BattleHistory::RightNow;
        setBattleCursor(-1);
        endFieldMenu(TownMenu_ROOT::ROOT_OPERATION, NULL);
        return;
    }
#endif
    SetTextPage(0, type);
    drawMenuFrame(0, 0, 0x100, 0xc0, -1);
    if (pageMax_ >= 1) {
        SetTextPage(1, type);
        drawMenuFrame(0, 0, 0x100, 0xc0, -1);
    }
}

THUMB void TownMenu_OPERATION_CAREER::menuUpdate()
{
    navigator_.setup(1, 1, 1);
#ifdef DQIV_NATIVE
    // Classic: nothing to point at; A switches the earlier record, B leaves.
    int result = classicBattleMenus() ? (dss::g_Pad.edge() & PAD_BUTTON_A ? 2 : dss::g_Pad.edge() & PAD_BUTTON_B ? 3 : 0)
                                      : MenuUpdate_Assist::menuSelect(menuItem_, navigator_);
#else
    int result = MenuUpdate_Assist::menuSelect(menuItem_, navigator_);
#endif
    if (result == 0) {
        return;
    }
    if ((result == 2 || result == 7) && pageMax_ == 2) {
        page_ = !page_;
    }
    if (result == 3) {
        close();
        gTownMenu_OPERATION_ROOT.open();
    }
    redraw_ = 1;
}

THUMB void TownMenu_OPERATION_CAREER::SetTextPage(int page, int type)
{
    if (page != 0) {
        status::g_BattleHistory.historyType_ = (status::BattleHistory::HistoryType)type;
        setMenuDrawOffset(0, 0);
        drawCareerRecordLine(status::g_BattleHistory.getHeroLevel(), 7, bookOfBeasts_, 1);
        if (type == 1) {
            drawCareerPlayTime(status::g_BattleHistory.getAdventureTime(), 1);
        } else {
            drawCareerPlayTime(status::g_BattleHistory.getAdventureTime(), 2);
        }
        drawCareerTitle(status::g_BattleHistory.getTitle());
    } else {
        status::g_BattleHistory.historyType_ = status::BattleHistory::RightNow;
        if (bookOfBeasts_ != 0) {
            drawCareerRecordLine(status::g_BattleHistory.getRestMonsterCount(), 7, bookOfBeasts_, 0);
        }
        if (pageMax_ == 2) {
            if (type == 1) {
                drawCareerHistoryType(0);
            } else {
                drawCareerHistoryType(1);
            }
        }
        drawCareerTitle(status::g_BattleHistory.getTitle());
        drawCareerPlayTime(status::g_BattleHistory.getAdventureTime(), 0);
    }
    drawCareerRecordLine(status::g_BattleHistory.getBattleCount(), 0, 0, 0);
    drawCareerRecordLine(status::g_BattleHistory.getMonsterCount(), 1, 0, 0);
    drawCareerRecordLine(status::g_BattleHistory.getTotalGold(), 2, 0, 0);
    drawCareerRecordLine(status::g_BattleHistory.getVictoryCount(), 3, 0, 0);
    drawCareerRecordLine(status::g_BattleHistory.getWipeoutCount(), 4, 0, 0);
    drawCareerRecordLine(status::g_BattleHistory.getEscapeCount(), 5, 0, 0);
    drawCareerRecordLine(status::g_BattleHistory.getMaxDamage(), 6, 0, 0);
}
