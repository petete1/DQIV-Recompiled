#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace pc {
// Classic battle menus: the game's battle menus (ov015/btl) describe what they
// show; these windows lay it out along the bottom of the game window, like the
// console games, and compose one picture for the interface layer.
enum class BattleMenuKind {none,root,commands,targetEnemy,targetParty,spells,items,
    tacticsMember,tacticsSelect,formation,message,stadiumAbort,
    list};  // list: a field one-column list (whose items, item commands, towns, casters)
// A text drawn by the game's font: palette indices (0 transparent), one byte per pixel.
struct BattleText {std::vector<std::uint8_t> ink;int width=0,height=0;};
struct BattleMenuEntry {
    BattleText label,right;  // right: MP cost, count or mark (may be empty)
    std::vector<std::uint32_t> icon;  // a square icon (the commands' 24x24, the items' 32x32), ARGB; may be empty
    short index=0;           // the game's MenuItem::active_ for this entry
    bool enabled=true;
};
// One member of the party panel (Classic, at the right of the window).
struct BattlePartyRow {
    BattleText name,label,level,hp,mp,hpLabel,mpLabel; // label: "Lv." or the member's condition; level empty then
    std::vector<std::uint8_t> portrait;                // the face: 48x48 palette indices (0 transparent), may be empty
    int hpValue=0,hpMax=0,mpValue=0,mpMax=0;
    int colour=0;                                      // the game's HP colour: 0 normal, 1 low, 2 down
    bool active=false;                                 // choosing a command now
    bool ready=false;                                  // can still act this round: the card shows its background
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
// A monster group's name plate, where the battle places it (bottom screen DS
// pixels, which can run outside the screen in the band around it).
struct BattlePlate {BattleText text;int x=0,y=0,width=0;int tone=0;bool dim=false;std::array<std::uint32_t,16> palette{};}; // width: the DS plate's (0: fit the text); tone: 0 normal, 1 low health, 2 very low
struct BattleMenuState {
    BattleMenuKind kind=BattleMenuKind::none;
    BattleText title,footer;          // member name (or a message); "MP left" line (may be empty)
    std::vector<BattleMenuEntry> entries;  // the current page, row-major
    int columns=1;                    // 1 list, 2 two columns
    int cursor=-1;                    // entries index of the game's active_
    int page=0,pages=1;
    BattleText pageText;              // "1/3" of the pager in a list's header (pages > 1)
    std::vector<BattleText> help;     // the description's words (10-dot; an empty one breaks the line)
    BattleText costLabel,cost;        // at the description's bottom: "MP" and "cost/MP" (may be empty)
    bool waiting=true;
    std::array<std::uint32_t,16> palette{};  // ARGB of BG palette 15
    std::uint32_t revision=0;         // changes whenever anything above changes
    std::vector<BattlePartyRow> party;               // empty: no party panel
    std::array<std::uint32_t,256> portraitPalette{}; // ARGB of the faces' palette
    BattleMessage message;                           // the battle message window (Classic)
    std::vector<BattlePlate> plates;                 // the monster name plates (Classic)
    bool field=false;                                // a field menu: placed from the top left
    std::vector<BattleMenuEntry> parent;             // the root commands shown above a field list (2 columns)
    int parentCursor=-1;                             // the chosen command (a still hand)
    BattleText gold,goldUnit;                        // "1259" and "G"; empty gold: no gold panel
    std::array<std::vector<std::uint32_t>,5> cursorFrames;  // the game's animated cursor, 16x16 ARGB each (empty: the hand)
    int cursorFrame=0;                               // the frame showing now
};
// The world map inset at the bottom left while a field menu shows, canvas units (canvas
// canvasHeight high): {x,y,w,h}.
std::array<float,4> fieldMapInset(float canvasHeight,float insetWidth,float insetHeight);
// A Classic field menu shows (this frame) with no message over it: the map shows at the bottom left.
bool classicFieldMapShown(const BattleMenuState&);
// The words in lines at most width wide (a word of width 0 breaks the line):
// the index of each line's first word.
std::vector<int> wrapBattleWords(std::span<const BattleText> words,int width,int space);
// The spells' kinds, as DQ11 marks them: an icon each, drawn here (DQ4 has none).
enum class BattleSpellKind {attack,healing,buff,debuff,utility};
std::vector<std::uint32_t> battleSpellIcon(BattleSpellKind kind);  // 16x16 ARGB
// Filled by the game each frame (kind none when no battle menu draws).
BattleMenuState& battleMenuState();
struct BattleRect {int x=0,y=0,w=0,h=0;}; // game window pixels
struct BattleMenuLayout {
    int scale=1;
    std::vector<BattleRect> windows;      // frames, in drawing order
    std::vector<BattleRect> entries;      // one per state entry (hit areas)
    BattleRect picture;                   // where the composed picture goes
    int planWidth=0;                      // the game window's width in DS pixels, as laid out
};
int battleScale(int windowHeight);
// A group drawn on its own (the party panel, the message window): where its picture goes.
struct BattleGroupLayout {int scale=1;BattleRect picture;};
// The field menu's gold panel: top right, 8 DS pixels in; picture.w 0 without gold.
BattleGroupLayout layoutBattleGold(const BattleMenuState&,int windowWidth,int windowHeight);
std::vector<std::uint32_t> composeBattleGold(const BattleMenuState&,const BattleGroupLayout&);
// The party panel: right edge and bottom, 8 DS pixels in; picture.w 0 without a party.
BattleGroupLayout layoutBattleParty(const BattleMenuState&,int windowWidth,int windowHeight);
// How far (DS pixels) the active member's card slides out to the left.
inline constexpr int battleCardSlide=12;
// slides: per row, 0 in place .. 1 slid out (missing rows: 0); backs: per row, the
// background's opacity 0..1 (missing rows: 1 when ready, else 0).
std::vector<std::uint32_t> composeBattleParty(const BattleMenuState&,const BattleGroupLayout&,std::span<const float> slides={},
    std::span<const float> backs={});
// The message window: bottom centre of the room left of the party panel, as
// wide as its widest line and 240 DS pixels at least (that room at most), as
// tall as the game's, moved by its shake; picture.w 0 when not shown.
BattleGroupLayout layoutBattleMessage(const BattleMenuState&,int windowWidth,int windowHeight);
std::vector<std::uint32_t> composeBattleMessage(const BattleMenuState&,const BattleGroupLayout&,int blinkFrame);
// A name plate's picture at DS scale (width x height), in the panels' look.
std::vector<std::uint32_t> composeBattlePlate(const BattlePlate&,const BattleMenuState&,int& width,int& height);
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
