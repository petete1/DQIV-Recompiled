// Classic battle windows: layout, cursor and picture (build target dqiv_battle_tests).
#include "BattleWindows.hpp"
#include "Settings.hpp"
#include "OverlayMenus.hpp"
#include "Display.hpp"
#include "NitroVideo.hpp"
#include <array>
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
    const auto p=pc::placeLayers(std::span(&maps,1),1920,1080,640,0,4);
    check(p.size()==1&&p[0].x==32&&p[0].y==1080-32-48*4&&p[0].scale==4,"Root at the bottom left, 8 DS pixels in");
}
void noRuleStaysOnDs() {
    TestLayer other;other.box(5,25,14,28);  // {40,200,80,32}
    const auto maps=other.maps(pc::OverlayMenu::other);
    const auto p=pc::placeLayers(std::span(&maps,1),1920,1080,640,0,2);
    check(p.size()==1&&p[0].x==640+80&&p[0].y==400,"a menu without a rule stays where the DS shows it");
}
void pointerMapsBackToLayer() {
    TestLayer root;root.box(2,34,13,39);
    const auto maps=root.maps(pc::OverlayMenu::battleRoot);
    const auto p=pc::placeLayers(std::span(&maps,1),1920,1080,640,0,4);
    int dsX=0,dsY=0;
    check(pc::pointerOnLayer(p,p[0].x+40,p[0].y+20,dsX,dsY)&&dsX==26&&dsY==277,"a point on a moved layer maps to its DS place");
    check(!pc::pointerOnLayer(p,5,5,dsX,dsY),"a point off every layer is on none");
}
void layersKeepTheirOwnCells() {
    TestLayer commands,spells;commands.box(2,34,13,45);spells.box(10,30,29,40);  // overlapping on the DS
    const std::array<pc::MenuLayerMaps,2> maps{commands.maps(pc::OverlayMenu::battleCommands),spells.maps(pc::OverlayMenu::battleSpells)};
    const auto p=pc::placeLayers(maps,1920,1080,640,0,4);
    check(p.size()==2,"two placements");
    if(p.size()<2)return;
    const bool apart=p[1].x>=p[0].x+p[0].area.w*4;
    check(apart,"the spell list goes beside the commands, not over them");
    check(p[0].area.w==96&&p[1].area.x==80&&p[1].area.w==160,"each area is its own map's box");
}
void spriteGoesToContainingLayer() {
    TestLayer a,b;a.box(0,24,15,35);b.box(8,30,23,40);
    const std::array<pc::MenuLayerMaps,2> maps{a.maps(pc::OverlayMenu::other),b.maps(pc::OverlayMenu::other)};
    const auto p=pc::placeLayers(maps,1920,1080,640,0,2);
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
    const auto picture=display.menuLayerPicture(video,maps,0,-1,-1,{},{},{});
    check(picture.size()==256*384,"a 256x384 picture");
    if(picture.size()!=256*384)return;
    check(picture[0]!=0&&((picture[0]>>16)&255)>200&&(picture[0]&0xffff)==0,"a VRAM character in the BG palette's colour");
    check(picture[8]==picture[0],"a character from the layer's own buffer");
    check(picture[16]==0,"no character: transparent");
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
    clickConfirmsNextPoll();
    heldButtonBlocksTouch();
    areaOfCells();
    ruleAnchorsBottomLeft();
    noRuleStaysOnDs();
    pointerMapsBackToLayer();
    layersKeepTheirOwnCells();
    spriteGoesToContainingLayer();
    menuSpritesHaveCentres();
    rendersRamAndVramTiles();
    std::cout<<(failures?"battle windows: FAILED ":"battle windows: all passed ")<<failures<<'\n';
    return failures?1:0;
}
