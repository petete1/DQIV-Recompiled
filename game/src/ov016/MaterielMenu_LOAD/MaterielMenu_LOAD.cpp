#include "ov016/MaterielMenu_LOAD/MaterielMenu_LOAD.hpp"
#include "main/menu/TownMenu_MESSAGE.hpp"
#include "main/global/Global.hpp"
#include "main/status/Status.hpp"
#include "main/status/GameStatus.hpp"
#include "main/status/StoryStatus.hpp"
#include "ov016/UnkMaterielMenuDraw/MaterielMenuDrawExtra.hpp"
#ifdef DQIV_NATIVE
#include "main/menu/MenuPartsDraw.hpp"
#include "main/text/TextAPI.hpp"
#include "Settings.hpp"
#include "Achievements.hpp"
#include "SavePages.hpp"
#include "ov015/btl/BattleMenuNative.hpp"
#include <vector>
#include "main/dss/Pad.hpp"
#include <nitro/pad.h>
#include <stdio.h>
#include <string.h>

namespace {
void copyText(char* destination, int size, const char* text)
{
    strncpy(destination, text, size - 1);
    destination[size - 1] = 0;
}
// The fourth line of Continue: bank 3 of the page shown (quick save or autosave).
profile::CatalogView s_resumeView;
bool s_resumeUsed = false;
const int s_listY = 0x2a; // the logs' first row

pc::SavePages* pages() { return pc::mountedSavePages(); }
int shownPage() { return pages() ? pages()->shownPage() : 0; }
pc::ResumeSlot resumeSlotShown() { return shownPage() == 0 ? pc::ResumeSlot::quick : pc::ResumeSlot::autosave; }

// Classic: the list's cards (autosave, quick save, logs 1-6), read from both pages.
std::vector<pc::BattleSaveCard> s_cards;
int s_cardsRevision = 0;
// Both pages' logs and resume saves (BattleMenuNative's readSaveCards). menuMode 0
// (Continue): all eight pickable; Create and Delete: the six logs.
void readAllCards(int menuMode)
{
    s_cards = readSaveCards(true);
    for (int i = 0; i < 8; i++) {
        s_cards[i].entry = menuMode == 0 ? i : i >= 2 ? i - 2 : -1;
    }
    s_cardsRevision++;
}
}

THUMB int MaterielMenu_LOAD::rootCount() const
{
    return resume_ != 0 ? ROOT_RESUME_COUNT : ROOT_COUNT;
}
#endif

const int MaterielMenu_LOAD::ROOT_RESUME_COUNT = 4;
const int MaterielMenu_LOAD::ROOT_COUNT = 3;
int MaterielMenu_LOAD::activeDiaryNo_ = -1;

THUMB void MaterielMenu_LOAD::menuSetup()
{
    rootItem_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD_CANCEL, menu::MenuItem::CURSORTYPE_ACTIVE);
    dataItem_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD_CANCEL, menu::MenuItem::CURSORTYPE_ACTIVE);
    sexualityItem_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD_CANCEL, menu::MenuItem::CURSORTYPE_ACTIVE);
#ifdef DQIV_NATIVE
    settingsItem_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD_CANCEL, menu::MenuItem::CURSORTYPE_ACTIVE);
    achievementsItem_.setup(menu::MenuItem::MENUITEM_TYPE_TOUCH_PAD_CANCEL, menu::MenuItem::CURSORTYPE_ACTIVE);
#endif
    status_ = LOAD_MODESELECT;
    country_ = 0;
    sexuality_ = 0;
    messageCounter_ = -1;
    menuMode_ = 0;
    country_ = status::g_Game.language;
    killResult_ = 0;
    makeResult_ = 0;
    resume_ = 0;
#ifndef DQIV_NATIVE
    if (profile::SaveLoad::checkbank(3) == 1) {
        resume_ = 1;
    }
#endif
    // The PC version loads the quick save from the Continue list instead.
    frame_ = 0;
}

THUMB void MaterielMenu_LOAD::menuExecute()
{
#ifdef DQIV_NATIVE
    // The original entries, then the settings, the achievements and quitting.
    MenuTemplate_materiel::MATERIEL_DIARY_MENU(&rootItem_, rootCount() + 3);
    settings_.execute(settingsItem_);
    achievements_.execute(achievementsItem_);
    // Classic: one list, the two resume saves (Continue) and the six logs.
    if (classicBattleMenus()) {
        MenuTemplate_materiel::MATERIEL_DIARY_PAGE(&dataItem_, menuMode_ == 0 ? 8 : 6);
    } else {
        MenuTemplate_materiel::MATERIEL_DIARY_PAGE(&dataItem_, menuMode_ == 0 ? 4 : 3);
    }
    MenuTemplate_materiel::MATERIEL_SEXUALITY(&sexualityItem_);
    return;
#endif
    if (resume_ != 0) {
        MenuTemplate_materiel::MATERIEL_DIARY_MENU(&rootItem_, ROOT_RESUME_COUNT);
    } else {
        MenuTemplate_materiel::MATERIEL_DIARY_MENU(&rootItem_, ROOT_COUNT);
    }
    MenuTemplate_materiel::MATERIEL_DIARY_LOAD(&dataItem_);
    MenuTemplate_materiel::MATERIEL_SEXUALITY(&sexualityItem_);
}

inline void MaterielMenu_LOAD::sexualityDraw()
{
    int message;
    int sexualityMessage[2] = { 0xa0000005, 0xa0000006 };
    drawNameEditField(country_, g_nameEditLabels, g_nameEditLength, g_nameEditMaxLength, 2, 1);
    drawFrameStyleB(0x30, 0x10, 0xa0, 0x28, -1);
    message = 0x800001a1;
    drawCenteredMessage(&message, 0x30, 0x38, 0xa0, 0x50);
    drawMessageColumn(sexualityMessage, 2, 0x70, 0x5a);
    drawFrameStyleB(0x30, 0x38, 0xa0, 0x48, 0x50);
    sexualityItem_.drawActive();
}

THUMB void MaterielMenu_LOAD::menuDraw()
{
    int rootMessage[3] = { 0x80000196, 0x80000197, 0x80000198 };
    int rootResumeMessage[4] = { 0x80000196, 0x8000019f, 0x80000197, 0x80000198 };
    if (status_ == LOAD_MODESELECT) {
#ifdef DQIV_NATIVE
        // Three more lines: the settings, the achievements and quitting.
        static char s_settingsEntry[48];
        static char s_achievementsEntry[48];
        static char s_quitEntry[48];
        copyText(s_quitEntry, sizeof(s_quitEntry), pc::quitLabel(pc::gameSettings().language));
        copyText(s_settingsEntry, sizeof(s_settingsEntry), pc::settingsTitle(pc::gameSettings().language));
        copyText(s_achievementsEntry, sizeof(s_achievementsEntry), pc::achievementsTitle(pc::gameSettings().language));
        if (classicBattleMenus()) {
            int texts[ROOT_RESUME_COUNT + 3];
            const int* root = resume_ == 1 ? rootResumeMessage : rootMessage;
            for (int i = 0; i < rootCount(); i++) {
                texts[i] = root[i];
            }
            texts[rootCount()] = (int)s_settingsEntry;
            texts[rootCount() + 1] = (int)s_achievementsEntry;
            texts[rootCount() + 2] = (int)s_quitEntry;
            publishLaunchCommands(rootCount() + 3, texts, rootItem_.active_);
            return;
        }
        if (resume_ == 1) {
            drawMessageColumnSmall(rootResumeMessage, ROOT_RESUME_COUNT, 0x18, 0x10);
        } else {
            drawMessageColumnSmall(rootMessage, ROOT_COUNT, 0x18, 0x10);
        }
        int param = (int)s_settingsEntry;
        MenuPartsEntry entry[] = {
            { 0x0d, 0x08, (short)0xf000, 0, 0x18, 0x10, 0x90, 0xe },
            { 0xff, 0x00, 0, 0, 0, 0, 0, 0 },
        };
        drawMenuParts(entry, &param, 0, rootCount() * 14, 1);
        param = (int)s_achievementsEntry;
        drawMenuParts(entry, &param, 0, (rootCount() + 1) * 14, 1);
        param = (int)s_quitEntry;
        drawMenuParts(entry, &param, 0, (rootCount() + 2) * 14, 1);
        drawFrameStyleA(8, 8, 0xa0, resume_ == 1 ? 0x48 + 44 : 0x38 + 44, -1);
        rootItem_.drawActive();
        return;
#endif
        if (resume_ == 1) {
            drawMessageColumnSmall(rootResumeMessage, ROOT_RESUME_COUNT, 0x18, 0x10);
            drawFrameStyleA(8, 8, 0xa0, 0x48, -1);
            rootItem_.drawActive();
        } else {
            drawMessageColumnSmall(rootMessage, ROOT_COUNT, 0x18, 0x10);
            drawFrameStyleA(8, 8, 0xa0, 0x38, -1);
            rootItem_.drawActive();
        }
    }
#ifdef DQIV_NATIVE
    if (status_ == LOAD_DATASELECT && classicBattleMenus()) {
        // The game's prompts (a DS message and Yes/No) show alone: the list would cover them.
        if (g_TownMenuMessage.isOpen()) {
            return;
        }
        publishLaunchList(rootMessage[menuMode_], s_cards, menuMode_ == 0 ? 8 : 6, dataItem_.active_, s_cardsRevision);
        return;
    }
    if (status_ == LOAD_DATASELECT) {
        // Higher than the original, to make room for the fourth line and the page label.
        int message = rootMessage[menuMode_];
        int flag = menuMode_ != 1;
        const int page = shownPage();
        const int language = pc::gameSettings().language;
        drawCenteredMessage(&message, 8, 0x08, 0xf0, 0x20);
        for (int i = 0; i < 3; i++) {
            if (diary_[i].savetype_ == 4) {
                drawJournalSlotNumbered(i, page * 3 + i + 1, diary_[i].name_, -1, -1, -1, diary_[i].time_, s_listY, flag);
            } else {
                drawJournalSlotNumbered(i, page * 3 + i + 1, diary_[i].name_, diary_[i].chapter_, diary_[i].level_, diary_[i].town_, diary_[i].time_, s_listY, flag);
            }
        }
        MenuPartsEntry entry[] = {
            { 0x0d, 0x08, (short)0xf000, 0, 0x18, 0x10, 0xd0, 0xe },
            { 0xff, 0x00, 0, 0, 0, 0, 0, 0 },
        };
        static char s_resumeLabel[48];
        static char s_pageLabel[48];
        int labelY = s_listY + 3 * 0x1a;
        if (menuMode_ == 0) {
            copyText(s_resumeLabel, sizeof(s_resumeLabel), page == 0 ? pc::quickSaveLabel(language) : pc::autosaveLabel(language));
            int param = (int)s_resumeLabel;
            drawMenuParts(entry, &param, 0, labelY - 0x10, 1);
            if (s_resumeUsed) {
                drawJournalSlotNumbered(3, 0, s_resumeView.name_, s_resumeView.chapter_, s_resumeView.level_, s_resumeView.town_, s_resumeView.time_, s_listY + 14, 1);
            } else {
                drawJournalSlotNumbered(3, 0, 0, 0, 0, 0, 0, s_listY + 14, 1);
            }
            labelY += 14 + 0x1a;
        }
        copyText(s_pageLabel, sizeof(s_pageLabel), pc::savePageLabel(language, page));
        int param = (int)s_pageLabel;
        drawMenuParts(entry, &param, 0, labelY - 0x10, 1);
        drawFrameStyleB(8, 0x08, 0xf0, labelY + 0x10, 0x20);
        dataItem_.drawActive();
    }
#else
    if (status_ == LOAD_DATASELECT) {
        int message = rootMessage[0];
        int flag = 1;
        if (menuMode_ == 1) {
            message = rootMessage[1];
            flag = 0;
        } else if (menuMode_ == 2) {
            message = rootMessage[2];
        }
        drawCenteredMessage(&message, 8, 0x38, 0xf0, 0x50);
        for (int i = 0; i < 3; i++) {
            if (diary_[i].savetype_ == 4) {
                drawJournalSlot(i, diary_[i].name_, -1, -1, -1, diary_[i].time_, 0x5a, flag);
            } else {
                drawJournalSlot(i, diary_[i].name_, diary_[i].chapter_, diary_[i].level_, diary_[i].town_, diary_[i].time_, 0x5a, flag);
            }
        }
        drawFrameStyleB(8, 0x38, 0xf0, 0x78, 0x50);
        dataItem_.drawActive();
    }
#endif
    if (status_ == LOAD_SEXUALITY) {
        sexualityDraw();
    }
#ifdef DQIV_NATIVE
    if (status_ == LOAD_SETTINGS) {
        settings_.draw(settingsItem_, 0, 0x100, drawFrameStyleA);
    }
    if (status_ == LOAD_ACHIEVEMENTS) {
        achievements_.draw(achievementsItem_, 0, 0x100, drawFrameStyleA);
    }
#endif
}

THUMB void MaterielMenu_LOAD::menuUpdate()
{
    if (frame_ < 30) {
        frame_++;
        return;
    }
    if (messageUpdate()) {
        return;
    }
    switch (status_) {
    case LOAD_MODESELECT:
        rootUpdate();
        break;
    case LOAD_DATACHECK:
#ifdef DQIV_NATIVE
        if (pages()) {
            pages()->showPage(pages()->sessionPage());
        }
#endif
        if (updateActiveDiary()) {
#ifdef DQIV_NATIVE
            if (classicBattleMenus()) {
                readAllCards(menuMode_);
                // The cursor on the session page's recent log.
                const int log = (pages() ? pages()->sessionPage() * 3 : 0) + dataItem_.active_;
                dataItem_.active_ = menuMode_ == 0 ? log + 2 : log;
            }
#endif
            changeStatus(LOAD_DATASELECT);
            activeDiaryNo_ = 0;
            redraw_ = 1;
        } else {
            g_TownMenuMessage.openMessageForMENU();
            g_TownMenuMessage.addMessageNOWAIT(0xcb60b);
            g_TownMenuMessage.addMessageWAITKEY();
            changeStatus(LOAD_BLANK);
        }
        break;
    case LOAD_DATASELECT:
        dataSelectUpdate();
        break;
    case LOAD_SEXUALITY:
        sexualityUpdate();
        break;
#ifdef DQIV_NATIVE
    case LOAD_SETTINGS:
        if (settings_.update(settingsItem_, redraw_)) {
            changeStatus(LOAD_MODESELECT);
            redraw_ = 1;
        }
        // The language may have changed.
        country_ = status::g_Game.language;
        break;
    case LOAD_ACHIEVEMENTS:
        if (achievements_.update(achievementsItem_, redraw_)) {
            changeStatus(LOAD_MODESELECT);
            redraw_ = 1;
        }
        break;
#endif
    case LOAD_WRITECHECK: {
        int result = profile::SaveLoad::isCardOK();
        if (result) {
            result = makeDiary();
        }
        if (result) {
#ifdef DQIV_NATIVE
            if (pages()) {
                pages()->setSessionPage(pages()->shownPage());
            }
            pc::autosaveArrived();
#endif
            changeStatus(LOAD_END);
        } else {
            g_TownMenuMessage.close();
            g_TownMenuMessage.openMessageForMENU();
            g_TownMenuMessage.addMessageNOWAIT(0xcb5fa);
            g_TownMenuMessage.addMessageWAITKEY();
            changeStatus(LOAD_BLANK);
        }
        break;
    }
    case LOAD_DELETECHECK:
        if (g_TownMenuMessage.isOpen() && g_TownMenuMessage.isMessageWAITPROG()) {
            if (deleteDiary()) {
                g_TownMenuMessage.openMessageForMENU();
                g_TownMenuMessage.addMessage(0xcb605);
                messageCounter_++;
            } else {
                g_TownMenuMessage.openMessageForMENU();
                g_TownMenuMessage.addMessageNOWAIT(0xcb608);
                g_TownMenuMessage.addMessageWAITKEY();
                changeStatus(LOAD_BLANK);
            }
        }
        break;
    case LOAD_LOADCHECK:
        if (profile::SaveLoad::loadbank(activeDiaryNo_)) {
#ifdef DQIV_NATIVE
            if (pages()) {
                pages()->setSessionPage(pages()->shownPage());
            }
            pc::autosaveArrived();
#endif
            changeStatus(LOAD_END);
        } else {
            g_TownMenuMessage.openMessageForMENU();
            g_TownMenuMessage.addMessageNOWAIT(0xcb60b);
            g_TownMenuMessage.addMessageWAITKEY();
            changeStatus(LOAD_BLANK);
        }
        break;
    case LOAD_BLANK:
        if (g_TownMenuMessage.isOpen()) {
            g_TownMenuMessage.isMessageWAITPROG();
        }
        break;
    case LOAD_END:
        close();
        break;
    case LOAD_RESUME: {
#ifdef DQIV_NATIVE
        // The shown page's bank 3, then that save's own page for the adventure.
        const pc::ResumeSlot slot = resumeSlotShown();
        if (pages()) {
            pages()->useResumeSlot(slot);
        }
        const int loaded = profile::SaveLoad::loadbank(3);
        if (pages()) {
            if (loaded) {
                pages()->setSessionPage(pages()->resumePage(slot));
                pc::autosaveArrived();
            }
            pages()->useResumeSlot(pc::ResumeSlot::quick);
        }
        if (loaded) {
#else
        if (profile::SaveLoad::loadbank(3)) {
#endif
            changeStatus(LOAD_END);
        } else {
            g_TownMenuMessage.openMessageForMENU();
            g_TownMenuMessage.addMessageNOWAIT(0xcb60b);
            g_TownMenuMessage.addMessageWAITKEY();
            changeStatus(LOAD_BLANK);
        }
        break;
    }
    }
}

THUMB bool MaterielMenu_LOAD::messageUpdate()
{
    if (!g_TownMenuMessage.isOpen()) {
        return false;
    }
    int stat = g_TownMenuMessage.stat_;
    switch (status_) {
    case LOAD_DATASELECT:
        switch (menuMode_) {
        case 0:
            if (stat == menu::MenuBase::MENUBASE_STAT_OK) {
                g_TownMenuMessage.close();
                changeStatus(LOAD_NAMEEDIT);
                redraw_ = 1;
            } else if (stat == menu::MenuBase::MENUBASE_STAT_CANCEL) {
                g_TownMenuMessage.close();
            }
            break;
        case 1:
            if (stat == menu::MenuBase::MENUBASE_STAT_OK) {
                g_TownMenuMessage.close();
                changeStatus(LOAD_NAMEEDIT);
                redraw_ = 1;
            } else if (stat == menu::MenuBase::MENUBASE_STAT_CANCEL) {
                g_TownMenuMessage.close();
            }
            break;
        case 2:
            if (stat == menu::MenuBase::MENUBASE_STAT_OK) {
                g_TownMenuMessage.close();
                g_TownMenuMessage.openMessageForMENU();
                g_TownMenuMessage.addMessageNOWAIT(0xcb604);
                g_TownMenuMessage.addMessageWAITKEY();
                changeStatus(LOAD_DELETECHECK);
            } else if (stat == menu::MenuBase::MENUBASE_STAT_CANCEL) {
                g_TownMenuMessage.close();
            }
            break;
        }
        break;
    case LOAD_DELETECHECK:
        if (messageCounter_ > 0) {
            if ((unsigned int)(stat - 1) <= 1) {
                redraw_ = 1;
                g_TownMenuMessage.close();
                changeStatus(LOAD_MODESELECT);
            }
        } else {
            return false;
        }
        break;
    case LOAD_WRITECHECK:
    case LOAD_LOADCHECK:
        if (messageCounter_ > 0) {
            if ((unsigned int)(stat - 1) <= 1) {
                redraw_ = 1;
                g_TownMenuMessage.close();
                changeStatus(LOAD_MODESELECT);
            }
        } else if (g_TownMenuMessage.isMessageWAITPROG()) {
            g_TownMenuMessage.close();
            return false;
        }
        break;
    case LOAD_BLANK:
        if ((unsigned int)(stat - 1) <= 1) {
            g_TownMenuMessage.close();
        }
        return false;
    case LOAD_END:
        closeMessage();
        break;
    }
    return true;
}

THUMB void MaterielMenu_LOAD::rootUpdate()
{
    rootItem_.execInput();
#ifdef DQIV_NATIVE
    battleListInput(rootItem_);
#endif
    switch (rootItem_.result_) {
    case 2: {
        rootItem_.result_ = 0;
        rootItem_.lastresult_ = 0;
        dataItem_.active_ = 0;
#ifdef DQIV_NATIVE
        if (rootItem_.active_ == rootCount()) {
            settingsItem_.active_ = 0;
            settings_.open(false);
            changeStatus(LOAD_SETTINGS);
            redraw_ = 1;
            break;
        }
        if (rootItem_.active_ == rootCount() + 2) {
            pc::requestQuit();
            break;
        }
        if (rootItem_.active_ == rootCount() + 1) {
            achievementsItem_.active_ = 0;
            achievements_.open();
            changeStatus(LOAD_ACHIEVEMENTS);
            redraw_ = 1;
            break;
        }
#endif
        if (!profile::SaveLoad::isCardOK()) {
            stat_ = menu::MenuBase::MENUBASE_STAT_OK;
            changeStatus(LOAD_END);
            break;
        }
        if (resume_ != 0) {
            int active = rootItem_.active_;
            if (active == 0) {
                menuMode_ = 0;
            } else if (active == 1) {
                changeStatus(LOAD_RESUME);
                break;
            } else if (active == 2) {
                menuMode_ = 1;
            } else if (active == 3) {
                menuMode_ = 2;
            }
            changeStatus(LOAD_DATACHECK);
        } else {
            int active = rootItem_.active_;
            if (active == 0) {
                menuMode_ = 0;
            } else if (active == 1) {
                menuMode_ = 1;
            } else if (active == 2) {
                menuMode_ = 2;
            }
            changeStatus(LOAD_DATACHECK);
        }
        break;
    }
    case 3:
        rootItem_.result_ = 0;
        rootItem_.lastresult_ = 0;
        break;
    case 5:
#ifdef DQIV_NATIVE
        rootItem_.active_ = rootCount() + 2;
        break;
#endif
        if (resume_ != 0) {
            rootItem_.active_ = 3;
        } else {
            rootItem_.active_ = 2;
        }
        break;
    case 6:
        rootItem_.active_ = 0;
        break;
    }
}

THUMB void MaterielMenu_LOAD::dataSelectUpdate()
{
#ifdef DQIV_NATIVE
    // L / R: the other page of logs.
    const bool classic = classicBattleMenus();
    const int rows = menuMode_ == 0 ? 8 : 6;
    const int edge = dss::g_Pad.edge();
    if (!classic && pages() && (edge & (PAD_BUTTON_L | PAD_BUTTON_R))) {
        const int step = edge & PAD_BUTTON_R ? 1 : pc::SavePages::pageCount - 1;
        pages()->showPage((pages()->shownPage() + step) % pc::SavePages::pageCount);
        updateActiveDiary();
        redraw_ = 1;
        return;
    }
#endif
    dataItem_.execInput();
#ifdef DQIV_NATIVE
    battleListInput(dataItem_);
#endif
    switch (dataItem_.result_) {
    case 2: {
        dataItem_.result_ = 0;
        dataItem_.lastresult_ = 0;
        int active = dataItem_.active_;
#ifdef DQIV_NATIVE
        if (classic) {
            // Rows: the autosave and the quick save (Continue), then logs 1-6. A resume
            // save: its page shown, LOAD_RESUME loads that page's slot; a log: its page
            // shown and its slot there, then the game's own handling.
            const int row = active;
            if (menuMode_ == 0 && row < 2) {
                if (pages()) {
                    pages()->showPage(row == 0 ? 1 : 0);
                }
                if (s_cards.size() > (size_t)row && s_cards[row].time.width > 0) {
                    changeStatus(LOAD_RESUME);
                } else if (pages()) {
                    pages()->showPage(pages()->sessionPage());
                }
                redraw_ = 1;
                break;
            }
            const int log = menuMode_ == 0 ? row - 2 : row;
            if (pages()) {
                pages()->showPage(log / 3);
            }
            updateActiveDiary();
            dataItem_.active_ = row;
            active = log % 3;
        } else if (active == 3) {
            if (s_resumeUsed) {
                changeStatus(LOAD_RESUME);
            }
            redraw_ = 1;
            break;
        }
#endif
        activeDiaryNo_ = active;
        switch (menuMode_) {
        case 0:
            if (diary_[active].name_ == 0) {
                g_TownMenuMessage.openMessageForMENU();
                g_TownMenuMessage.addMessage(0xcb5eb);
                g_TownMenuMessage.setYesNo();
            } else {
                changeStatus(LOAD_LOADCHECK);
            }
            break;
        case 1:
            if (diary_[active].name_ != 0) {
                g_TownMenuMessage.openMessageForMENU();
                g_TownMenuMessage.addMessage(0xcb5ef);
                g_TownMenuMessage.setYesNo();
            } else {
                changeStatus(LOAD_NAMEEDIT);
            }
            break;
        case 2:
            if (diary_[active].name_ != 0) {
                g_TownMenuMessage.openMessageForMENU();
#ifdef DQIV_NATIVE
                // The log's number counts the page's logs before it (id_ is its bank in its file).
                profile::CatalogView named = catalogview_[activeDiaryNo_];
                named.id_ += shownPage() * 3;
                profile::SaveLoad::setCatalogMacro(&named);
#else
                profile::SaveLoad::setCatalogMacro(&catalogview_[activeDiaryNo_]);
#endif
                if (diary_[activeDiaryNo_].savetype_ == 4) {
                    g_TownMenuMessage.addMessage(0xcb610);
                } else if (diary_[activeDiaryNo_].chapter_ >= 5) {
                    g_TownMenuMessage.addMessage(0xcb601);
                } else {
                    g_TownMenuMessage.addMessage(0xcb5fe);
                }
                g_TownMenuMessage.setYesNo(1);
            }
            break;
        }
        redraw_ = 1;
        break;
    }
    case 3:
        dataItem_.result_ = 0;
        dataItem_.lastresult_ = 0;
#ifdef DQIV_NATIVE
        if (classic && pages()) {
            pages()->showPage(pages()->sessionPage());
        }
#endif
        changeStatus(LOAD_MODESELECT);
        redraw_ = 1;
        break;
    case 1:
        activeDiaryNo_ = dataItem_.active_;
        break;
    case 5:
#ifdef DQIV_NATIVE
        dataItem_.active_ = classic ? rows - 1 : menuMode_ == 0 ? 3 : 2;
#else
        dataItem_.active_ = 2;
#endif
        activeDiaryNo_ = dataItem_.active_;
        break;
    case 6:
        dataItem_.active_ = 0;
        activeDiaryNo_ = dataItem_.active_;
        break;
    }
}

THUMB void MaterielMenu_LOAD::sexualityUpdate()
{
    sexualityItem_.execInput();
    switch (sexualityItem_.result_) {
    case 2:
        sexualityItem_.result_ = 0;
        sexualityItem_.lastresult_ = 0;
        exitCode_ = activeDiaryNo_;
        g_TownMenuMessage.openMessageForMENU();
        g_TownMenuMessage.addMessageNOWAIT(0xcb5f7);
        g_TownMenuMessage.addMessageWAITKEY();
        changeStatus(LOAD_WRITECHECK);
        redraw_ = 1;
        break;
    case 3:
        sexualityItem_.result_ = 0;
        sexualityItem_.lastresult_ = 0;
        changeStatus(LOAD_NAMEEDIT);
        redraw_ = 1;
        break;
    case 5:
        sexualityItem_.active_ = 1;
        break;
    case 6:
        sexualityItem_.active_ = 0;
        break;
    }
    sexuality_ = sexualityItem_.active_;
}

THUMB bool MaterielMenu_LOAD::updateActiveDiary()
{
    catalogview_ = profile::SaveLoad::getCatalogView();
    int active = profile::SaveLoad::getCatalogRecent();
    profile::CatalogView* view = catalogview_;
    DiaryInfo* diary = diary_;
    int useFlag[3] = { 0, 0, 0 };
    int i;
    int* use = useFlag;
    for (i = 0; i < 3; i++, view++, diary++, use++) {
        *use = view->useFlag_;
        if (*use != 0) {
            diary->name_ = view->name_;
            diary->chapter_ = view->chapter_;
            diary->level_ = view->level_;
            diary->town_ = view->town_;
            diary->time_ = view->time_;
            diary->savetype_ = view->savetype_;
        } else {
            diary->name_ = 0;
            diary->chapter_ = 0;
            diary->level_ = 0;
            diary->town_ = 0;
            diary->time_ = 0;
            diary->savetype_ = 0;
        }
    }
    if (useFlag[active] == 0) {
        active = 0;
        for (int k = 0; k < 3; k++) {
            if (useFlag[k] != 0) {
                active = k;
                break;
            }
        }
    }
    dataItem_.active_ = active;
#ifdef DQIV_NATIVE
    s_resumeUsed = false;
    if (pages()) {
        pages()->useResumeSlot(resumeSlotShown());
        s_resumeUsed = profile::SaveLoad::getResumeView(&s_resumeView);
        pages()->useResumeSlot(pc::ResumeSlot::quick);
    }
#endif
    return true;
}

THUMB bool MaterielMenu_LOAD::deleteDiary()
{
    return profile::SaveLoad::killbank(activeDiaryNo_);
}

THUMB bool MaterielMenu_LOAD::makeDiary()
{
    status::g_Story.sex_ = (Sex)sexuality_;
    status::Status::setEventParty(0);
    status::g_Game.setPlayTime(0);
    status::g_Game.resetUniqueID();
    g_Global.startFirstTown();
    status::g_Story.setHeroName(gMaterielMenu_NameEdit.getNameUTF8());
    return profile::SaveLoad::savebank(activeDiaryNo_, profile::SAVETYPE_NEW);
}

THUMB void MaterielMenu_LOAD::changeStatus(LOAD_STATUS status)
{
    if (status == LOAD_NAMEEDIT) {
        close();
        gMaterielMenu_NameEdit.open();
        gMaterielMenu_NameEdit.returnMenu_ = MaterielMenu_NameEdit::RETURN_MENU_LOAD;
        if (status_ != LOAD_SEXUALITY) {
            gMaterielMenu_NameEdit.clearName();
        }
    }
    status_ = status;
    messageCounter_ = 0;
}

THUMB void MaterielMenu_LOAD::closeMessage()
{
    int stat = g_TownMenuMessage.stat_;
    redraw_ = 1;
    if ((unsigned int)(stat - 1) <= 1) {
        g_TownMenuMessage.close();
    }
}
