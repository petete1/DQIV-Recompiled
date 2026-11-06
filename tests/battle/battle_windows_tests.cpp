// Classic battle windows: layout, cursor and picture (build target dqiv_battle_tests).
#include "BattleWindows.hpp"
#include "Settings.hpp"
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
    std::cout<<(failures?"battle windows: FAILED ":"battle windows: all passed ")<<failures<<'\n';
    return failures?1:0;
}
