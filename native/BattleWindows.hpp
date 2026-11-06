#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace pc {
// Classic battle menus: the game's battle menus (ov015/btl) describe what they
// show; these windows lay it out along the bottom of the game window, like the
// console games, and compose one picture for the interface layer.
enum class BattleMenuKind {none,root,commands,targetEnemy,targetParty,spells,items,
    tacticsMember,tacticsSelect,formation,message,stadiumAbort};
// A text drawn by the game's font: palette indices (0 transparent), one byte per pixel.
struct BattleText {std::vector<std::uint8_t> ink;int width=0,height=0;};
struct BattleMenuEntry {
    BattleText label,right;  // right: MP cost, count or mark (may be empty)
    short index=0;           // the game's MenuItem::active_ for this entry
    bool enabled=true;
};
struct BattleMenuState {
    BattleMenuKind kind=BattleMenuKind::none;
    BattleText title,footer;          // member name (or a message); "MP left" line (may be empty)
    std::vector<BattleMenuEntry> entries;  // the current page, row-major
    int columns=1;                    // 1 list, 2 two columns
    int cursor=-1;                    // entries index of the game's active_
    int page=0,pages=1;
    bool waiting=true;
    std::array<std::uint32_t,16> palette{};  // ARGB of BG palette 15
    std::uint32_t revision=0;         // changes whenever anything above changes
};
// Filled by the game each frame (kind none when no battle menu draws).
BattleMenuState& battleMenuState();
struct BattleRect {int x=0,y=0,w=0,h=0;}; // game window pixels
struct BattleMenuLayout {
    int scale=1;
    std::vector<BattleRect> windows;      // frames, in drawing order
    std::vector<BattleRect> entries;      // one per state entry (hit areas)
    BattleRect picture;                   // where the composed picture goes
};
int battleScale(int windowHeight);
BattleMenuLayout layoutBattleMenu(const BattleMenuState&,int windowWidth,int windowHeight);
// The entry index after a direction (0 up, 1 down, 2 left, 3 right), or -1 to pass it to the game (page turn).
int moveBattleCursor(const BattleMenuState&,int cursor,int direction);
// Composed at DS scale (picture.w / scale × picture.h / scale), premultiplied ARGB; empty when kind is none.
std::vector<std::uint32_t> composeBattleMenu(const BattleMenuState&,const BattleMenuLayout&,int blinkFrame);
// The entry under a window-pixel point, or -1.
int battleEntryAt(const BattleMenuLayout&,int x,int y);
// The layout the pointer is resolved against (the window sets it each frame it draws).
void rememberBattleLayout(const BattleMenuLayout&);
// True while classic battle windows are on screen (the window sends no DS touch then).
bool battleWindowsShown();
struct BattlePointer {int entry=-1;};
// The window: the mouse this frame (window pixels, buttons pressed this frame).
void setBattlePointer(int x,int y,bool leftPressed,bool rightPressed);
// The game: the entry the mouse moved onto or clicked since the last call, or -1.
BattlePointer takeBattlePointer();
// The window: the entry a click at this point confirms (pressed as A), or -1.
int battleClickAt(int x,int y);
// The mouse buttons as the game's: a click on an entry moves the cursor in
// its poll and is A from the next (so A confirms the clicked entry), a right
// click is B; each held 3 polls so the game's frame sees it.
class BattleMouse {
public:
    // The pad bits to add this poll (1 A, 2 B).
    unsigned poll(bool shown,bool leftPressed,bool rightPressed,bool leftDown,bool onEntry);
    // No DS touch while the windows show, nor while a click begun on them is held.
    bool touchBlocked(bool shown) const {return shown||held_;}
private:
    unsigned press_=0;int polls_=0;bool pendingA_=false,held_=false;
};
}
