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
struct BattleLayout {
    int scale=1;
    std::vector<BattleRect> windows;      // frames, in drawing order
    std::vector<BattleRect> entries;      // one per state entry (hit areas)
    BattleRect picture;                   // where the composed picture goes
};
int battleScale(int windowHeight);
BattleLayout layoutBattleMenu(const BattleMenuState&,int windowWidth,int windowHeight);
// The entry index after a direction (0 up, 1 down, 2 left, 3 right), or -1 to pass it to the game (page turn).
int moveBattleCursor(const BattleMenuState&,int cursor,int direction);
// Composed at DS scale (picture.w / scale × picture.h / scale), premultiplied ARGB; empty when kind is none.
std::vector<std::uint32_t> composeBattleMenu(const BattleMenuState&,const BattleLayout&,int blinkFrame);
// The entry under a window-pixel point, or -1.
int battleEntryAt(const BattleLayout&,int x,int y);
}
