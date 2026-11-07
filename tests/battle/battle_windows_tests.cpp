// Classic battle windows: layout, cursor and picture (build target dqiv_battle_tests).
#include "BattleWindows.hpp"
#include "Settings.hpp"
#include "OverlayMenus.hpp"
#include "Display.hpp"
#include "NitroVideo.hpp"
#include "HdPictures.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int failures=0;
void check(bool ok,const char* what) {
    if(!ok) {++failures;std::cerr<<"FAIL: "<<what<<'\n';}
}
pc::BattleText ink(int width,int height=12) {
    pc::BattleText text;text.width=width;text.height=height;text.ink.assign(std::size_t(width)*height,1);
    return text;
}
pc::BattleMenuEntry entry(int width,short index=0) {
    pc::BattleMenuEntry e;e.label=ink(width);e.index=index;return e;
}
pc::BattleMenuState menu(pc::BattleMenuKind kind,int count,int columns=1,int width=40) {
    pc::BattleMenuState state;state.kind=kind;state.columns=columns;state.cursor=0;
    for(int i=0;i<count;++i)state.entries.push_back(entry(width,short(i)));
    state.palette.fill(0xffffffff);
    return state;
}
bool inside(const pc::BattleRect& r,int width,int height) {return r.x>=0&&r.y>=0&&r.x+r.w<=width&&r.y+r.h<=height;}

void scaleSteps() {
    check(pc::battleScale(720)==2,"scale at 720p is 2");
    check(pc::battleScale(1080)==4,"scale at 1080p is 4");
    check(pc::battleScale(2160)==8,"scale at 4K is 8");
    check(pc::battleScale(200)==1,"scale is at least 1");
}
void rootAtBottomLeft() {
    const auto layout=pc::layoutBattleMenu(menu(pc::BattleMenuKind::root,2),1920,1080);
    check(layout.windows.size()==1,"root has one window");
    if(layout.windows.empty())return;
    check(layout.windows[0].x==8*4,"root window 8 DS pixels from the left");
    check(layout.windows[0].y+layout.windows[0].h==1080-8*4,"root window 8 DS pixels from the bottom");
}
void listBesideCommands() {
    auto state=menu(pc::BattleMenuKind::spells,8,2);state.title=ink(30);
    const auto layout=pc::layoutBattleMenu(state,1920,1080);
    check(layout.windows.size()==2,"spells show the commands window and the list");
    if(layout.windows.size()<2)return;
    check(layout.windows[1].x>=layout.windows[0].x+layout.windows[0].w+4*4,"the list opens beside the commands");
    check(layout.entries.size()==8,"one hit area per entry");
}
void windowsStayInside() {
    auto state=menu(pc::BattleMenuKind::spells,4,2);state.title=ink(30);state.entries[1].label=ink(400);
    const auto layout=pc::layoutBattleMenu(state,640,480);
    bool all=!layout.windows.empty();
    for(const auto& w:layout.windows)all=all&&inside(w,640,480);
    check(all,"windows stay inside a narrow game window");
    check(inside(layout.picture,640,480),"the picture stays inside a narrow game window");
}
void cursorSkipsEmptySlot() {
    const auto state=menu(pc::BattleMenuKind::spells,5,2);
    check(pc::moveBattleCursor(state,3,1)==3,"down from the last right entry stays");
    check(pc::moveBattleCursor(state,2,1)==4,"down from the left goes to the last entry");
}
void listWraps() {
    const auto state=menu(pc::BattleMenuKind::commands,4);
    check(pc::moveBattleCursor(state,0,0)==3,"up from the first wraps to the last");
    check(pc::moveBattleCursor(state,3,1)==0,"down from the last wraps to the first");
}
void pageTurnPasses() {
    auto state=menu(pc::BattleMenuKind::spells,6,2);state.pages=3;
    check(pc::moveBattleCursor(state,0,2)==-1,"left in the left column turns the page");
    check(pc::moveBattleCursor(state,1,3)==-1,"right in the right column turns the page");
    check(pc::moveBattleCursor(state,1,2)==0,"left from the right column");
    check(pc::moveBattleCursor(state,0,3)==1,"right from the left column");
    state.pages=1;
    check(pc::moveBattleCursor(state,0,2)==0,"left with one page stays");
    check(pc::moveBattleCursor(state,1,3)==1,"right with one page stays");
}
void noneHidesPicture() {
    const pc::BattleMenuState state;
    const auto layout=pc::layoutBattleMenu(state,1920,1080);
    check(layout.windows.empty(),"no windows without a menu");
    check(pc::composeBattleMenu(state,layout,0).empty(),"no picture without a menu");
}
void composesAtDsScale() {
    const auto state=menu(pc::BattleMenuKind::root,2);
    const auto layout=pc::layoutBattleMenu(state,1920,1080);
    const auto pixels=pc::composeBattleMenu(state,layout,0);
    check(pixels.size()==std::size_t(layout.picture.w/layout.scale)*(layout.picture.h/layout.scale),"picture at DS scale");
}
void hitTest() {
    const auto layout=pc::layoutBattleMenu(menu(pc::BattleMenuKind::commands,4),1920,1080);
    if(layout.entries.size()<3) {check(false,"commands have hit areas");return;}
    const auto& r=layout.entries[2];
    check(pc::battleEntryAt(layout,r.x+r.w/2,r.y+r.h/2)==2,"a point on entry 2");
    check(pc::battleEntryAt(layout,1900,10)==-1,"a point outside every window");
}
void pointerOutsideWindows() {
    const auto layout=pc::layoutBattleMenu(menu(pc::BattleMenuKind::commands,4),1920,1080);
    pc::rememberBattleLayout(layout);
    pc::setBattlePointer(5,5,true,false);
    check(pc::takeBattlePointer().entry==-1,"a click outside a window points at nothing");
    check(pc::battleClickAt(5,5)==-1,"a click outside a window is no confirm");
    if(layout.entries.size()<2)return;
    const auto& r=layout.entries[1];
    pc::setBattlePointer(r.x+2,r.y+2,false,false);
    check(pc::takeBattlePointer().entry==1,"moving onto entry 1 points at it");
    check(pc::takeBattlePointer().entry==-1,"resting there points at it only once");
    pc::setBattlePointer(r.x+2,r.y+2,true,false);
    check(pc::takeBattlePointer().entry==1,"clicking entry 1 points at it again");
    check(pc::battleClickAt(r.x+2,r.y+2)==1,"a click on entry 1 confirms it");
}
void settingCyclesThree() {
    check(pc::gameSettings().battleMenus==0,"Original by default");
    const char* shown[3]={"Classic","Overlay","Original"};
    for(int i=0;i<3;++i) {
        pc::changeSetting(pc::settingBattleMenus,1);
        check(pc::gameSettings().battleMenus==(i+1)%3,"the setting steps through three values");
        check(pc::settingValue(pc::settingBattleMenus,1)==shown[i],"each value shown by name");
    }
}
void clickConfirmsNextPoll() {
    pc::BattleMouse mouse;
    // The click moves the cursor first (this poll), A follows on the next polls.
    check(mouse.poll(true,true,false,true,true)==0u,"no A in the poll of the click");
    check(mouse.poll(true,false,false,true,false)==1u,"A on the next poll");
    check(mouse.poll(true,false,false,true,false)==1u,"A held a second poll");
    check(mouse.poll(true,false,false,true,false)==1u,"A held a third poll");
    check(mouse.poll(true,false,false,false,false)==0u,"then released");
    check(mouse.poll(true,true,false,true,false)==0u,"a click outside the entries is no A");
    check(mouse.poll(true,false,false,false,false)==0u,"and stays none");
    check(mouse.poll(true,false,true,false,false)==2u,"a right click is B at once");
    pc::BattleMouse hidden;
    check(hidden.poll(false,true,false,true,true)==0u&&hidden.poll(false,false,false,true,false)==0u,"nothing while the windows don't show");
}
void heldButtonBlocksTouch() {
    pc::BattleMouse mouse;
    mouse.poll(true,true,false,true,true);
    check(mouse.touchBlocked(true),"no touch while the windows show");
    mouse.poll(false,false,false,true,false);
    check(mouse.touchBlocked(false),"no touch while a click begun on them is held");
    mouse.poll(false,false,false,false,false);
    check(!mouse.touchBlocked(false),"touch again once released");
}
// A layer whose window map has the given cells (column, row) set.
struct TestLayer {
    std::array<std::uint16_t,0x600> window{},back{},icon{};
    pc::MenuLayerMaps maps(pc::OverlayMenu menu) {
        pc::MenuLayerMaps m;m.menu=menu;m.window=window.data();m.back=back.data();m.icon=icon.data();return m;
    }
    void cell(int x,int y) {window[y*32+x]=0x0081;}
    void box(int x0,int y0,int x1,int y1) {for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x)cell(x,y);}
};
// Both DS screens stacked at window x, the top one's corner at y 0.
std::array<pc::ScreenPlace,2> screensAt(float x,float scale) {return {pc::ScreenPlace{x,0,scale},pc::ScreenPlace{x,192*scale,scale}};}
void noRuleFollowsSmallerTopScreen() {
    TestLayer status;status.box(0,1,31,22);  // a top-screen menu {0,8,256,176}
    const auto maps=status.maps(pc::OverlayMenu::other);
    const std::array<pc::ScreenPlace,2> screens{pc::ScreenPlace{700,0,1.5f},pc::ScreenPlace{640,384,2}};
    const auto p=pc::placeLayers(std::span(&maps,1),1920,1080,screens,2);
    check(p.size()==1&&p[0].x==700&&p[0].y==12&&p[0].w==384&&p[0].h==264,"a top-screen menu follows the top screen's own place and scale");
}
void areaOfCells() {
    TestLayer layer;layer.cell(3,26);layer.cell(10,30);
    const auto a=pc::drawnArea(layer.maps(pc::OverlayMenu::other));
    check(a.x==24&&a.y==208&&a.w==64&&a.h==40,"the area is the cells' box in DS pixels");
    TestLayer empty;
    check(pc::drawnArea(empty.maps(pc::OverlayMenu::other)).w==0,"an empty layer has no area");
}
void ruleAnchorsBottomLeft() {
    TestLayer root;root.box(2,34,13,39);  // DS pixels {16,272,96,48}
    const auto maps=root.maps(pc::OverlayMenu::battleRoot);
    const auto p=pc::placeLayers(std::span(&maps,1),1920,1080,screensAt(640,4),4);
    check(p.size()==1&&p[0].x==32&&p[0].y==1080-32-48*4&&p[0].w==96*4&&p[0].h==48*4,"Root at the bottom left, 8 DS pixels in");
}
void noRuleStaysOnDs() {
    TestLayer other;other.box(5,25,14,28);  // {40,200,80,32}
    const auto maps=other.maps(pc::OverlayMenu::other);
    const auto p=pc::placeLayers(std::span(&maps,1),1920,1080,screensAt(640,2),2);
    check(p.size()==1&&p[0].x==640+80&&p[0].y==400,"a menu without a rule stays where the DS shows it");
}
void pointerMapsBackToLayer() {
    TestLayer root;root.box(2,34,13,39);
    const auto maps=root.maps(pc::OverlayMenu::battleRoot);
    const auto p=pc::placeLayers(std::span(&maps,1),1920,1080,screensAt(640,4),4);
    int dsX=0,dsY=0;
    check(pc::pointerOnLayer(p,p[0].x+40,p[0].y+20,dsX,dsY)&&dsX==26&&dsY==277,"a point on a moved layer maps to its DS place");
    check(!pc::pointerOnLayer(p,5,5,dsX,dsY),"a point off every layer is on none");
}
void layersKeepTheirOwnCells() {
    TestLayer commands,spells;commands.box(2,34,13,45);spells.box(10,30,29,40);  // overlapping on the DS
    const std::array<pc::MenuLayerMaps,2> maps{commands.maps(pc::OverlayMenu::battleCommands),spells.maps(pc::OverlayMenu::battleSpells)};
    const auto p=pc::placeLayers(maps,1920,1080,screensAt(640,4),4);
    check(p.size()==2,"two placements");
    if(p.size()<2)return;
    const bool apart=p[1].x>=p[0].x+p[0].w;
    check(apart,"the spell list goes beside the commands, not over them");
    check(p[0].area.w==96&&p[1].area.x==80&&p[1].area.w==160,"each area is its own map's box");
}
void spriteGoesToContainingLayer() {
    TestLayer a,b;a.box(0,24,15,35);b.box(8,30,23,40);
    const std::array<pc::MenuLayerMaps,2> maps{a.maps(pc::OverlayMenu::other),b.maps(pc::OverlayMenu::other)};
    const auto p=pc::placeLayers(maps,1920,1080,screensAt(640,2),2);
    check(pc::spriteLayer(p,100,260)==1,"a sprite where two layers overlap goes to the upper one");
    check(pc::spriteLayer(p,20,200)==0,"a sprite on the first layer only goes to it");
    check(pc::spriteLayer(p,250,10)==-1,"a sprite on no layer goes to none");
}
void menuSpritesHaveCentres() {
    std::array<std::uint16_t,512> top{},bottom{};
    top.fill(0xc0c0);bottom.fill(0xc0c0);              // OamBuffer::clear: hidden below the screen
    top[0]=10;top[1]=0x4000|20;top[2]=5;                // 16x16 at (20,10)
    bottom[8]=5;bottom[9]=0;bottom[10]=7;               // entry 2: 8x8 at (0,5)
    const auto sprites=pc::menuSprites(top.data(),bottom.data());
    check(sprites.size()==2,"two sprites shown, hidden ones left out");
    if(sprites.size()<2)return;
    check(sprites[0].screen==0&&sprites[0].entry==0&&sprites[0].centreX==28&&sprites[0].centreY==18,"a top sprite's centre");
    check(sprites[1].screen==1&&sprites[1].entry==2&&sprites[1].centreX==4&&sprites[1].centreY==192+9,"a bottom sprite's centre, below the top screen");
}
void rendersRamAndVramTiles() {
    pc::NitroVideo video;
    *static_cast<std::uint8_t*>(video.reg(0x04000240,1))=0x81;           // VRAM A: main BG
    const std::uint8_t red[]={0x1f,0x00};video.write(0x05000002,red);   // BG palette colour 1
    std::array<std::uint8_t,32> tile;tile.fill(0x11);
    video.write(0x06000020,tile);                                        // VRAM character 1
    TestLayer layer;
    std::array<std::uint8_t,0xc000> chars{};std::fill_n(chars.begin(),32,std::uint8_t(0x11)); // RAM character 0x80
    layer.window[0]=0x0001;layer.window[1]=0x0080;
    auto maps=layer.maps(pc::OverlayMenu::other);maps.chars=chars.data();
    pc::Display display;
    const int windowBg[2]={0,0},none[2]={-1,-1};
    const auto picture=display.menuLayerPicture(video,maps,windowBg,none,none,{},{},{});
    check(picture.size()==256*384,"a 256x384 picture");
    if(picture.size()!=256*384)return;
    check(picture[0]!=0&&((picture[0]>>16)&255)>200&&(picture[0]&0xffff)==0,"a VRAM character in the BG palette's colour");
    check(picture[8]==picture[0],"a character from the layer's own buffer");
    check(picture[16]==0,"no character: transparent");
}
void placedPicturesInColour() {
    std::array<std::uint8_t,32*32> source;source.fill(1);
    pc::placeHdPicture(0,10,20,20,20,source);
    std::array<std::uint8_t,0x200> palette{};palette[2]=0x1f;   // colour 1: red
    const auto pictures=pc::placedHdPictures(0,palette);
    check(pictures.size()==1,"one placed picture");
    if(pictures.size()!=1)return;
    const auto& p=pictures[0];
    check(p.x==10&&p.y==20&&p.width==20&&p.height==20&&p.size==32,"its place and size");
    check(p.pixels.size()==32*32&&p.pixels[0]==0xffff0000u,"its pixels in the palette's colours");
    pc::clearHdPictures(0);
    check(pc::placedHdPictures(0,palette).empty(),"none once cleared");
}
void touchesOnlyWhatShows() {
    TestLayer commands,spells;commands.box(2,34,13,45);spells.box(10,30,29,40);  // overlapping on the DS
    const std::array<pc::MenuLayerMaps,2> maps{commands.maps(pc::OverlayMenu::battleCommands),spells.maps(pc::OverlayMenu::battleSpells)};
    const auto screens=screensAt(640,2);   // the DS screens at x2, the moved menus at x4
    const auto p=pc::placeLayers(maps,1920,1080,screens,4);
    int dsX=0,dsY=0;
    // Commands' DS cell (12,35) lies under the spell list there: not a touch of either.
    check(!pc::overlayTouch(p,screens,p[0].x+(12*8-16)*4+2,p[0].y+(35*8-272)*4+2,dsX,dsY),"a moved window's spot another menu covers on the DS takes no touch");
    // Commands' cell (3,44), clear of the list.
    check(pc::overlayTouch(p,screens,p[0].x+(3*8-16)*4+2,p[0].y+(44*8-272)*4+2,dsX,dsY)&&dsX==24&&dsY==352,"a moved window takes touches where it shows");
    // Off every window, on the bottom screen clear of the menus' DS areas: passed through.
    check(pc::overlayTouch(p,screens,640+250*2,192*2+2*2,dsX,dsY)&&dsX==250&&dsY==194,"the scene takes touches as on the DS");
    // Off every window, where a moved menu sits on the DS: none.
    check(!pc::overlayTouch(p,screens,640+100*2,192*2+108*2,dsX,dsY),"the DS spot a menu left takes no touch");
}
void panelLook() {
    const auto state=menu(pc::BattleMenuKind::root,2);
    const auto layout=pc::layoutBattleMenu(state,1920,1080);
    const auto pixels=pc::composeBattleMenu(state,layout,0);
    const int w=layout.picture.w/layout.scale,h=layout.picture.h/layout.scale;
    if(pixels.size()!=std::size_t(w)*h) {check(false,"a picture");return;}
    auto at=[&](int x,int y){return pixels[std::size_t(y)*w+x];};
    check(at(0,0)==0,"rounded outer corner");
    check(at(0,h/2)==0xe0281810u,"the brown fill out to the edge");
    check(at(2,h/2)==0xffb89870u,"the tan border inset 2 pixels");
    check(at(6,h/2)==0xe0281810u,"the brown fill inside");
    bool hand=false;
    for(std::uint32_t p:pixels)hand=hand||p==0xfff0c040u;
    check(hand,"the golden hand cursor");
}
pc::BattleMenuState partyOf(int members) {
    pc::BattleMenuState state;
    for(int i=0;i<members;++i) {
        pc::BattlePartyRow row;row.name=ink(40);row.hpValue=50;row.hpMax=100;row.mpValue=10;row.mpMax=20;
        row.portrait.assign(48*48,1);state.party.push_back(row);
    }
    return state;
}
void partyInsideWindow() {
    for(const auto [w,h]:{std::pair{640,480},std::pair{1280,720}}) {
        const auto layout=pc::layoutBattleParty(partyOf(4),w,h);
        const auto& r=layout.picture;
        check(r.x>=0&&r.y>=0&&r.x+r.w<=w&&r.y+r.h<=h,"the party panel stays inside the window");
        check(r.x+r.w==w-8*layout.scale,"the party panel 8 DS pixels from the right edge");
        check(r.y+r.h==h-8*layout.scale,"the party panel 8 DS pixels from the bottom");
        check(pc::composeBattleParty(partyOf(4),layout).size()==std::size_t(r.w/layout.scale)*(r.h/layout.scale),"a party picture at DS scale");
    }
    check(pc::layoutBattleParty(pc::BattleMenuState{},1280,720).picture.w==0,"no party, no panel");
}
void barWidths() {
    check(pc::battleBarWidth(50,100,64)==32,"half HP, half the bar");
    check(pc::battleBarWidth(0,0,64)==0,"no maximum: an empty bar");
    check(pc::battleBarWidth(150,100,64)==64,"over the maximum: a full bar");
    check(pc::battleBarWidth(-5,100,64)==0,"below zero: an empty bar");
    check(pc::battleBarWidth(1,1000,64)==1,"a little HP still shows");
}
void messageInsideWindow() {
    pc::BattleMenuState state;state.message.shown=true;state.message.height=40;
    state.message.lines.push_back({ink(400),4,400});
    state.message.lines.push_back({ink(100),20,100});
    for(const auto [w,h]:{std::pair{640,480},std::pair{1920,1080}}) {
        const auto layout=pc::layoutBattleMessage(state,w,h);
        const auto& r=layout.picture;const int s=layout.scale;
        check(r.x>=0&&r.y>=0&&r.x+r.w<=w&&r.y+r.h<=h,"the message window stays inside the window");
        check(std::abs(r.x-(w-r.x-r.w))<=s,"the message window is centred");
        check(r.w==std::min((400+16)*s,w-16*s),"as wide as its widest line (240 at least), or the window less its margins");
        check(r.y+r.h==h-8*s,"8 DS pixels from the bottom");
        check(pc::composeBattleMessage(state,layout,0).size()==std::size_t(r.w/s)*(r.h/s),"a message picture at DS scale");
    }
    check(pc::layoutBattleMessage(pc::BattleMenuState{},1280,720).picture.w==0,"no message, no window");
    pc::BattleMenuState small;small.message.shown=true;small.message.lines.push_back({ink(50),4,50});
    check(pc::layoutBattleMessage(small,1920,1080).picture.w==240*4,"240 DS pixels for short lines");
}
bool overlap(const pc::BattleRect& a,const pc::BattleRect& b) {return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
void messageClearOfParty() {
    auto state=partyOf(4);state.message.shown=true;state.message.height=40;state.message.lines.push_back({ink(250),4,250});
    for(const auto [w,h]:{std::pair{1920,1080},std::pair{3840,2160},std::pair{1280,720}}) {
        const auto party=pc::layoutBattleParty(state,w,h),message=pc::layoutBattleMessage(state,w,h);
        check(message.picture.w>0&&!overlap(party.picture,message.picture),"the message window keeps clear of the party panel");
    }
}
void messageWidthFromFullLine() {
    pc::BattleMenuState state;state.message.shown=true;state.message.height=40;
    state.message.lines.push_back({ink(10),4,300});  // typing: 10 shown of 300
    check(pc::layoutBattleMessage(state,1920,1080).picture.w==(300+16)*4,"the window is as wide as the whole line from its first letter");
}
void messageShakes() {
    pc::BattleMenuState state;state.message.shown=true;state.message.height=40;state.message.lines.push_back({ink(50),4,50});
    const auto still=pc::layoutBattleMessage(state,1920,1080);
    state.message.shakeX=2;state.message.shakeY=-1;
    const auto shaken=pc::layoutBattleMessage(state,1920,1080);
    check(shaken.picture.x==still.picture.x+8&&shaken.picture.y==still.picture.y-4,"the window shakes with the game's offset, scaled");
}
void messageInTinyWindow() {
    pc::BattleMenuState state;state.message.shown=true;state.message.height=40;state.message.lines.push_back({ink(50),4,50});
    const auto layout=pc::layoutBattleMessage(state,25,480);
    check(pc::composeBattleMessage(state,layout,0).size()==std::size_t(layout.picture.w/layout.scale)*(layout.picture.h/layout.scale),"a tiny window does not break the message window");
}
void classicOnlyWide() {
    pc::GameSettings s;s.battleMenus=1;s.wide=true;
    check(pc::classicBattleMenusOn(s),"Classic with the wide picture");
    s.wide=false;
    check(!pc::classicBattleMenusOn(s),"the original menus with the DS screens");
    s.battleMenus=2;s.wide=true;
    check(!pc::classicBattleMenusOn(s),"not Classic when Overlay is chosen");
}
void activeCardSlidesOut() {
    auto state=partyOf(2);
    const auto layout=pc::layoutBattleParty(state,1920,1080);
    const std::array<float,2> slides{1.f,0.f};
    const auto pixels=pc::composeBattleParty(state,layout,slides);
    const int w=layout.picture.w/layout.scale;
    auto leftmost=[&](int y){for(int x=0;x<w;++x)if(pixels[std::size_t(y)*w+x]>>24)return x;return w;};
    check(leftmost(20)+pc::battleCardSlide<=leftmost(50+20)+1,"the active card slides out to the left");
    check(pc::composeBattleParty(state,layout,{}).size()==pixels.size(),"the same picture without slides");
}
}

int main() {
    scaleSteps();
    rootAtBottomLeft();
    listBesideCommands();
    windowsStayInside();
    cursorSkipsEmptySlot();
    listWraps();
    pageTurnPasses();
    noneHidesPicture();
    composesAtDsScale();
    hitTest();
    pointerOutsideWindows();
    settingCyclesThree();
    classicOnlyWide();
    clickConfirmsNextPoll();
    heldButtonBlocksTouch();
    areaOfCells();
    ruleAnchorsBottomLeft();
    noRuleStaysOnDs();
    pointerMapsBackToLayer();
    layersKeepTheirOwnCells();
    spriteGoesToContainingLayer();
    noRuleFollowsSmallerTopScreen();
    touchesOnlyWhatShows();
    panelLook();
    partyInsideWindow();
    barWidths();
    activeCardSlidesOut();
    messageInsideWindow();
    messageClearOfParty();
    messageWidthFromFullLine();
    messageShakes();
    messageInTinyWindow();
    menuSpritesHaveCentres();
    rendersRamAndVramTiles();
    placedPicturesInColour();
    std::cout<<(failures?"battle windows: FAILED ":"battle windows: all passed ")<<failures<<'\n';
    return failures?1:0;
}
