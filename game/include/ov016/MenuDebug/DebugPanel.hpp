#pragma once
#ifdef DQIV_DEBUG_MENU
#include "main/menu/MenuBase.hpp"

// The debug tools (DQIV_DEBUG_MENU), pages of the cheats window after the
// cheats themselves (Misc > Cheats), which turns them with L / R. Left /
// right choose a value, A runs a line's action. Its tools are in
// native/DebugTools.hpp.
struct DebugPanel
{
    enum Page { PAGE_WARP, PAGE_BATTLE, PAGE_PARTY, PAGE_STORY, PAGE_TOOLS, PAGE_COUNT };
    enum Row {
        ROW_LOCATION, ROW_MAP_AREA, ROW_MAP, ROW_WALLS, ROW_DAY_NIGHT, ROW_WAGON, ROW_SHIP, ROW_BALLOON,
        ROW_MONSTER, ROW_MONSTER_COUNT, ROW_FIGHT,
        ROW_MEMBER, ROW_LEVEL, ROW_SPELLS, ROW_CHARACTER, ROW_JOINED,
        ROW_CHAPTER, ROW_FLAG_SET, ROW_FLAG, ROW_FLAG_BLOCK, ROW_FLAG_VALUE,
        ROW_OVERLAY, ROW_MUSIC, ROW_SOUND, ROW_BESTIARY, ROW_MINIGAME,
        ROW_BACK,
    };
    int page_ = PAGE_WARP;
    int rows_[16] = {};
    int rowCount_ = 0;
    int location_ = 0;
    int map_ = 0;
    int monster_ = 1;
    int monsterCount_ = 1;
    int member_ = 0;
    int level_ = 1;
    int character_ = 0;
    int chapter_ = 1;
    int flagSet_ = 0;
    int flag_ = 0;
    int music_ = 1;
    int sound_ = 0;
    int minigame_ = 0;
    int doneRow_ = -1;      // the line whose action just ran
    const char* done_ = ""; // what it says
    bool closeMenus_ = false;

    void open();
    // Shows a page; the cheats window draws `tabs` (its first tab is the cheats).
    void showPage(int page, const char* const* tabs, int tabCount);
    const char* const* tabs_ = nullptr;
    int tabCount_ = 0;
    static const char* pageTitle(int page);
    void execute(menu::MenuItem& item);
    // True when the player leaves; closeMenus_ then asks to close every menu
    // (an action that changes the scene, once they are closed).
    bool update(menu::MenuItem& item, int& redraw);
    void draw(menu::MenuItem& item, int x, int w, void (*frame)(int, int, int, int, int));

private:
    void buildRows();
    int step() const;
    void change(int row, int step);
    void run(int row);
};
#endif
