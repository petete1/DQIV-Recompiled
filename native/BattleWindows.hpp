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
// One member of the party panel (Classic, at the right of the window).
struct BattlePartyRow {
    BattleText name,label,level,hp,mp,hpLabel,mpLabel; // label: "Lv." or the member's condition; level empty then
    std::vector<std::uint8_t> portrait;                // 32x32 palette indices (0 transparent), may be empty
    int hpValue=0,hpMax=0,mpValue=0,mpMax=0;
    int colour=0;                                      // the game's HP colour: 0 normal, 1 low, 2 down
    bool active=false;                                 // choosing a command now
};
// A line of the battle message window: the game's own rendered text.
struct BattleMessageLine {BattleText text;int y=0,fullWidth=0;};  // y: DS pixels from the window's top; fullWidth: the whole line, typed or not
struct BattleMessage {
    bool shown=false,waiting=false;   // waiting: for a button (the ▼ blinks)
    std::vector<BattleMessageLine> lines;
    BattleText name;                  // the speaker, on the top border (may be empty)
    int height=0;                     // the game's window height, DS pixels
    int shakeX=0,shakeY=0;            // the game shakes the window when the party is hit (DS pixels)
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
    std::vector<BattlePartyRow> party;               // empty: no party panel
    std::array<std::uint32_t,256> portraitPalette{}; // ARGB of the portraits' palette
    BattleMessage message;                           // the battle message window (Classic)
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
// A group drawn on its own (the party panel, the message window): where its picture goes.
struct BattleGroupLayout {int scale=1;BattleRect picture;};
// The party panel: right edge and bottom, 8 DS pixels in; picture.w 0 without a party.
BattleGroupLayout layoutBattleParty(const BattleMenuState&,int windowWidth,int windowHeight);
std::vector<std::uint32_t> composeBattleParty(const BattleMenuState&,const BattleGroupLayout&);
// The message window: bottom centre of the room left of the party panel, as
// wide as its widest line and 240 DS pixels at least (that room at most), as
// tall as the game's, moved by its shake; picture.w 0 when not shown.
BattleGroupLayout layoutBattleMessage(const BattleMenuState&,int windowWidth,int windowHeight);
std::vector<std::uint32_t> composeBattleMessage(const BattleMenuState&,const BattleGroupLayout&,int blinkFrame);
// A bar's filled width: clamped to 0..width, 0 when max <= 0, at least 1 when value > 0.
int battleBarWidth(int value,int max,int width);
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
