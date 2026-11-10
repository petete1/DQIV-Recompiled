#pragma once
#ifdef DQIV_DEBUG_MENU
#include "main/menu/MenuBase.hpp"

// The debug tools (DQIV_DEBUG_MENU), pages of the cheats window after the
// cheats themselves (Misc > Cheats), which turns them with L / R. Left /
// right choose a value, A runs a line's action. Its tools are in
// native/DebugTools.hpp.
struct DebugPanel
{
    enum Page {
        PAGE_WARP, PAGE_BATTLE, PAGE_PARTY, PAGE_STORY, PAGE_TOOLS, PAGE_COUNT,
        // Opened from a line of a page: a group's stats (Battle), the member editor (Party).
        PAGE_GROUP = PAGE_COUNT, PAGE_MEMBER,
    };
    enum Row {
        ROW_LOCATION, ROW_MAP_AREA, ROW_MAP, ROW_WALLS, ROW_DAY_NIGHT, ROW_WAGON, ROW_SHIP, ROW_BALLOON,
        ROW_GROUP, ROW_GROUP_LAST = ROW_GROUP + 3, ROW_MONSTER_COUNT, ROW_BACKGROUND, ROW_ARENA, ROW_BATTLE_INFO,
        ROW_TILE, ROW_RESET_BATTLE, ROW_FIGHT,
        ROW_GROUP_HEAD, ROW_STAT, ROW_STAT_LAST = ROW_STAT + 7, ROW_RESET_STATS,
        ROW_MEMBER, ROW_LEVEL, ROW_SPELLS, ROW_EDIT_MEMBER, ROW_MAX_STATS, ROW_CHARACTER, ROW_JOINED,
        ROW_MEMBER_STAT, ROW_MEMBER_STAT_LAST = ROW_MEMBER_STAT + 8, ROW_CONDITION, ROW_POISON, ROW_HEAL,
        ROW_CHAPTER, ROW_FLAG_SET, ROW_FLAG, ROW_FLAG_BLOCK, ROW_FLAG_VALUE,
        ROW_OVERLAY, ROW_MUSIC, ROW_SOUND, ROW_BESTIARY, ROW_MINIGAME,
        ROW_BACK,
    };
    int page_ = PAGE_WARP;
    int rows_[16] = {};
    int rowCount_ = 0;
    int location_ = 0;
    int map_ = 0;
    int group_ = 0; // the Select Battle group being changed
    int tile_ = -1; // Tile battle's tile, -1 the party's
    int held_ = 0;  // left / right repeats, for the bigger steps
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
    void openPage(int page, int row, menu::MenuItem& item); // the cursor on that row
    void back(menu::MenuItem& item);
    int step() const;
    void change(int row, int step);
    void run(int row);
};
#endif
