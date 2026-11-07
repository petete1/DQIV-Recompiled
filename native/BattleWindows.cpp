#include "BattleWindows.hpp"
#include <algorithm>
#include <cmath>

namespace pc {
namespace {
// DS pixels.
constexpr int margin=8,gap=4,pad=8,row=16,cursorWidth=14,columnGap=16,border=2,rightGap=8;
// The DQ11-like look: see-through brown panels (premultiplied), a tan border inset 2 pixels, cream text.
constexpr std::uint32_t fill=0xe0281810,tan=0xffb89870,cream=0xfff4ead8,gold=0xfff0c040,goldEdge=0xff806010;
// What a window shows, in DS pixels relative to the picture.
struct Box {int x=0,y=0,w=0,h=0;};
struct Plan {
    Box picture;
    std::vector<Box> windows,entries;
    Box titleBox;bool titleOnBorder=false;   // where the title text goes
    int labelWidth=0,rightWidth=0;          // a column's label and right parts
    int footerY=-1;                          // the footer row (picture coordinates)
};
bool listed(BattleMenuKind kind) {return kind!=BattleMenuKind::none&&kind!=BattleMenuKind::message;}
// The commands and root windows carry their entries; the others show the
// member's name in a window of its own at the left.
bool ownWindow(BattleMenuKind kind) {return kind==BattleMenuKind::root||kind==BattleMenuKind::commands;}
Plan plan(const BattleMenuState& state,int width) {
    Plan p;
    if(state.kind==BattleMenuKind::none)return p;
    if(state.kind==BattleMenuKind::message) {
        const int w=std::min(state.title.width+2*pad,width-2*margin),h=state.title.height+2*pad;
        p.windows.push_back({0,0,std::max(w,2*pad),h});p.titleBox={pad,pad,w-2*pad,state.title.height};
        p.picture={margin,0,p.windows[0].w,h};
        return p;
    }
    int left=0;
    const bool nameWindow=!ownWindow(state.kind)&&state.title.width>0;
    if(nameWindow) {
        const int w=state.title.width+2*pad;
        p.windows.push_back({0,0,w,state.title.height+2*pad});p.titleBox={pad,pad,state.title.width,state.title.height};
        left=w+gap;
    }
    const int columns=std::max(1,state.columns),count=int(state.entries.size());
    for(const auto& e:state.entries) {
        p.labelWidth=std::max(p.labelWidth,e.label.width);p.rightWidth=std::max(p.rightWidth,e.right.width);
    }
    auto columnWidth=[&] {return cursorWidth+p.labelWidth+(p.rightWidth?rightGap+p.rightWidth:0);};
    // Cut the labels so the window fits the game window.
    const int room=width-2*margin-left-2*pad-(columns-1)*columnGap;
    if(columnWidth()*columns>room)p.labelWidth=std::max(0,room/columns-cursorWidth-(p.rightWidth?rightGap+p.rightWidth:0));
    const int rows=(count+columns-1)/columns;
    const bool footer=state.footer.width>0||state.pages>1;
    int titleHeight=0;
    if(ownWindow(state.kind)&&state.title.width>0) {p.titleOnBorder=true;titleHeight=state.title.height/2;}
    const int innerWidth=std::max(columns*columnWidth()+(columns-1)*columnGap,ownWindow(state.kind)?state.title.width+4:0);
    const int w=innerWidth+2*pad,h=titleHeight+2*pad+std::max(rows,1)*row+(footer?row:0);
    p.windows.push_back({left,0,w,h});
    if(p.titleOnBorder)p.titleBox={left+pad,0,state.title.width,state.title.height};
    for(int i=0;i<count;++i)
        p.entries.push_back({left+pad+(i%columns)*(columnWidth()+columnGap),titleHeight+pad+(i/columns)*row,columnWidth(),row});
    if(footer)p.footerY=titleHeight+pad+std::max(rows,1)*row;
    // Every window ends on the same bottom line.
    int bottom=0;
    for(const auto& box:p.windows)bottom=std::max(bottom,box.h);
    for(auto& box:p.windows)box.y=bottom-box.h;
    if(nameWindow)p.titleBox.y=p.windows.front().y+pad;
    const int shift=p.windows.back().y;
    for(auto& e:p.entries)e.y+=shift;
    if(p.titleOnBorder)p.titleBox.y+=shift;
    if(p.footerY>=0)p.footerY+=shift;
    p.picture={margin,0,p.windows.back().x+p.windows.back().w,bottom};
    return p;
}
void blit(std::vector<std::uint32_t>& out,int width,int height,const BattleText& text,int x,int y,int maxWidth,
    const std::array<std::uint32_t,16>& palette,bool dim,std::uint32_t tone=cream) {
    const bool cut=text.width>maxWidth;
    const int shown=cut?std::max(0,maxWidth-7):text.width;
    for(int ty=0;ty<text.height;++ty)for(int tx=0;tx<shown;++tx) {
        const auto index=text.ink[std::size_t(ty)*text.width+tx];
        const int px=x+tx,py=y+ty;
        if(!index||px<0||py<0||px>=width||py>=height)continue;
        auto colour=palette[index&15];
        // The game's white letters in cream (its dark edges stay dark).
        auto tint=[&](unsigned shift){return (((colour>>shift)&255)*((tone>>shift)&255)/255)<<shift;};
        colour=(colour&0xff000000)|tint(16)|tint(8)|tint(0);
        if(dim)colour=(colour&0xff000000)|((colour>>1)&0x007f7f7f);
        out[std::size_t(py)*width+px]=colour;
    }
    // "…": three dots on the text's baseline.
    if(cut)for(int d=0;d<3;++d)for(int dy=0;dy<2;++dy)for(int dx=0;dx<1;++dx) {
        const int px=x+shown+1+d*2+dx,py=y+text.height-3+dy;
        if(px>=0&&py>=0&&px<width&&py<height)out[std::size_t(py)*width+px]=dim?0xff7a756c:cream;
    }
}
// A small curl in each corner, inside the border (top-left; mirrored for the others).
constexpr const char* curl[5]={".XXX.","X....","X.XX.","X.X..","....."};
void frame(std::vector<std::uint32_t>& out,int width,const Box& box,bool curls=true) {
    for(int y=0;y<box.h;++y)for(int x=0;x<box.w;++x) {
        const int dx=std::min(x,box.w-1-x),dy=std::min(y,box.h-1-y);
        if(dx+dy<1)continue;  // the rounded outer corners
        std::uint32_t colour=fill;
        if((dx==border&&dy>=border)||(dy==border&&dx>=border))colour=tan;
        else if(curls&&dx>border&&dy>border&&dx-border-1<5&&dy-border-1<5&&curl[dy-border-1][dx-border-1]=='X')colour=tan;
        out[std::size_t(box.y+y)*width+box.x+x]=colour;
    }
}
// ▶ (or ◀) of the page marks: 5 wide, 9 tall.
void triangle(std::vector<std::uint32_t>& out,int width,int x,int y,bool right) {
    for(int ty=0;ty<9;++ty) {
        const int span=5-std::abs(ty-4);
        for(int tx=0;tx<span;++tx)out[std::size_t(y+ty)*width+(right?x+tx:x+4-tx)]=tan;
    }
}
// The cursor: a golden hand pointing right, 11 wide, 7 tall.
void hand(std::vector<std::uint32_t>& out,int width,int x,int y) {
    static constexpr const char* rows[7]={"....OOO....","OOOOGGGOOO.","OGGGGGGGGGO","OGGGGGGOOO.","OGGGGGO....","OGGGGGO....","OOOOOOO...."};
    for(int ty=0;ty<7;++ty)for(int tx=0;tx<11;++tx) {
        const char c=rows[ty][tx];
        if(c!='.')out[std::size_t(y+ty)*width+x+tx]=c=='G'?gold:goldEdge;
    }
}
}

BattleMenuState& battleMenuState() {static BattleMenuState state;return state;}
int battleScale(int windowHeight) {return std::max(1,windowHeight/270);}
BattleMenuLayout layoutBattleMenu(const BattleMenuState& state,int windowWidth,int windowHeight) {
    BattleMenuLayout layout;layout.scale=battleScale(windowHeight);
    const int s=layout.scale;
    const auto p=plan(state,windowWidth/s);
    if(p.windows.empty())return layout;
    const int top=windowHeight/s-margin-p.picture.h;
    layout.picture={p.picture.x*s,top*s,p.picture.w*s,p.picture.h*s};
    auto place=[&](const Box& b) {return BattleRect{(p.picture.x+b.x)*s,(top+b.y)*s,b.w*s,b.h*s};};
    for(const auto& b:p.windows)layout.windows.push_back(place(b));
    for(const auto& b:p.entries)layout.entries.push_back(place(b));
    return layout;
}
int moveBattleCursor(const BattleMenuState& state,int cursor,int direction) {
    const int count=int(state.entries.size()),columns=std::max(1,state.columns);
    if(count==0||cursor<0||cursor>=count)return cursor;
    const int rows=(count+columns-1)/columns,r=cursor/columns,c=cursor%columns;
    const bool paged=state.pages>1;
    switch(direction) {
    case 0: {const int to=r>0?cursor-columns:((rows-1)*columns+c<count?(rows-1)*columns+c:(rows-2)*columns+c);return to>=0?to:cursor;}
    case 1: {
        if(r==rows-1)return c;                       // past the last row: wrap
        const int to=cursor+columns;return to<count?to:cursor;  // an empty slot below: stay
    }
    case 2:return c>0?cursor-1:paged?-1:cursor;
    case 3:return c<columns-1&&cursor+1<count?cursor+1:paged?-1:cursor;
    }
    return cursor;
}
std::vector<std::uint32_t> composeBattleMenu(const BattleMenuState& state,const BattleMenuLayout& layout,int blinkFrame) {
    if(layout.windows.empty())return {};
    const int s=layout.scale,width=layout.picture.w/s,height=layout.picture.h/s;
    // The same plan as the layout: labels already cut to the picture fit it again.
    const auto p=plan(state,width+2*margin);
    std::vector<std::uint32_t> out(std::size_t(width)*height,0);
    // The layout's windows in DS pixels relative to the picture.
    for(const auto& w:layout.windows)
        frame(out,width,{(w.x-layout.picture.x)/s,(w.y-layout.picture.y)/s,w.w/s,w.h/s});
    const int dy=height-p.picture.h;  // the plan's own bottom line is the picture's
    if(state.title.width>0) {
        const auto& t=p.titleBox;
        if(p.titleOnBorder)  // the name on the top border, over a strip of the panel colour
            for(int y=0;y<state.title.height;++y)for(int x=-2;x<state.title.width+2;++x)
                if(t.x+x>=0&&t.x+x<width&&t.y+dy+y>=0&&t.y+dy+y<height)out[std::size_t(t.y+dy+y)*width+t.x+x]=fill;
        blit(out,width,height,state.title,t.x,t.y+dy,state.kind==BattleMenuKind::message?t.w:state.title.width,state.palette,false);
    }
    for(std::size_t i=0;i<state.entries.size()&&i<p.entries.size();++i) {
        const auto& e=state.entries[i];const auto& b=p.entries[i];
        const int textY=b.y+dy+(row-e.label.height)/2;
        blit(out,width,height,e.label,b.x+cursorWidth,textY,p.labelWidth,state.palette,!e.enabled);
        if(e.right.width>0)
            blit(out,width,height,e.right,b.x+b.w-e.right.width,b.y+dy+(row-e.right.height)/2,e.right.width,state.palette,!e.enabled);
        if(int(i)==state.cursor&&(!state.waiting||blinkFrame%30<20))hand(out,width,b.x+1,b.y+dy+(row-7)/2);
    }
    if(p.footerY>=0) {
        const auto& list=p.windows.back();
        const int y=p.footerY+dy;
        if(state.footer.width>0)blit(out,width,height,state.footer,list.x+pad+cursorWidth,y+(row-state.footer.height)/2,
            list.w-2*pad-cursorWidth,state.palette,false);
        if(state.pages>1) {
            // ◀ one dot per page (the current one larger) ▶, at the right.
            int x=list.x+list.w-pad-(state.pages*4+14);
            triangle(out,width,x,y+(row-9)/2,false);x+=7;
            for(int i=0;i<state.pages;++i,x+=4) {
                const int size=i==state.page?3:1,cy=y+row/2-size/2;
                for(int yy=0;yy<size;++yy)for(int xx=0;xx<size;++xx)out[std::size_t(cy+yy)*width+x+xx]=tan;
            }
            triangle(out,width,x+1,y+(row-9)/2,true);
        }
    }
    return out;
}
namespace {
BattleMenuLayout lastLayout;
int pointerX=-1,pointerY=-1,pointedEntry=-1;bool clicked=false;
}
void rememberBattleLayout(const BattleMenuLayout& layout) {lastLayout=layout;}
bool battleWindowsShown() {return !lastLayout.windows.empty();}
void setBattlePointer(int x,int y,bool leftPressed,bool) {
    pointerX=x;pointerY=y;clicked=clicked||leftPressed;
}
BattlePointer takeBattlePointer() {
    // Resting on an entry points at it once: the keys may move the cursor away.
    const int entry=battleEntryAt(lastLayout,pointerX,pointerY);
    BattlePointer pointer;
    if(entry!=pointedEntry||clicked)pointer.entry=entry;
    pointedEntry=entry;clicked=false;
    return pointer;
}
int battleClickAt(int x,int y) {return battleEntryAt(lastLayout,x,y);}
unsigned BattleMouse::poll(bool shown,bool leftPressed,bool rightPressed,bool leftDown,bool onEntry) {
    if(shown&&leftPressed)held_=true;
    if(!leftDown)held_=false;
    if(pendingA_) {pendingA_=false;press_=1u;polls_=3;}
    if(shown&&leftPressed&&onEntry)pendingA_=true;
    if(shown&&rightPressed) {press_=2u;polls_=3;}
    if(polls_==0)return 0;
    --polls_;
    return press_;
}
int battleEntryAt(const BattleMenuLayout& layout,int x,int y) {
    for(std::size_t i=0;i<layout.entries.size();++i) {
        const auto& r=layout.entries[i];
        if(x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h)return int(i);
    }
    return -1;
}
namespace {
// The party panel (DS pixels): rows of 50, 132 wide: the face (the strip
// fading in behind its left part), then the name and level, HP and MP.
constexpr int face=48,partyRow=50,partyWidth=132,partyFade=0,stripFade=16,partyInfo=partyFade+face+6,partyRight=partyWidth-4;
constexpr std::uint32_t green=0xff90e070,yellow=0xfff0d050,red=0xfff06050,hpBar=0xff60d040,mpBar=0xff4090f0,barBack=0xff202020;
}
int battleBarWidth(int value,int max,int width) {
    if(max<=0||value<=0)return 0;
    if(value>=max)return width;
    return std::max(1,int((long long)value*width/max));
}
BattleGroupLayout layoutBattleParty(const BattleMenuState& state,int windowWidth,int windowHeight) {
    BattleGroupLayout layout;layout.scale=battleScale(windowHeight);
    if(state.party.empty())return layout;
    const int s=layout.scale,h=int(state.party.size())*partyRow;
    // The cards' background runs out to the window's right edge.
    layout.picture={windowWidth-(margin+partyWidth+battleCardSlide)*s,windowHeight-(margin+h)*s,(partyWidth+battleCardSlide+margin)*s,h*s};
    return layout;
}
std::vector<std::uint32_t> composeBattleParty(const BattleMenuState& state,const BattleGroupLayout& layout,std::span<const float> slides,
    std::span<const float> backs) {
    if(layout.picture.w<=0)return {};
    const int pictureWidth=partyWidth+battleCardSlide+margin,pictureHeight=int(state.party.size())*partyRow;
    std::vector<std::uint32_t> picture(std::size_t(pictureWidth)*pictureHeight,0);
    // Each card is drawn on its own, then placed: the active one slid out to the left,
    // its background still out to the edge.
    const int width=pictureWidth,height=partyRow;
    std::vector<std::uint32_t> out(std::size_t(width)*height,0);
    std::array<std::uint32_t,16> white;white.fill(0xffffffff);
    // The palette the text inks map through: the menu text palette of the state.
    const auto& palette=state.palette[1]?state.palette:white;
    auto rect=[&](int x,int y,int w,int h,std::uint32_t colour) {
        for(int yy=std::max(0,y);yy<std::min(height,y+h);++yy)for(int xx=std::max(0,x);xx<std::min(width,x+w);++xx)
            out[std::size_t(yy)*width+xx]=colour;
    };
    for(std::size_t r=0;r<state.party.size();++r) {
        const auto& row=state.party[r];
        const int top=0;
        std::fill(out.begin(),out.end(),0u);
        // The strip, while the member can still act this round: a light dark brown (a little
        // lighter for the member choosing), fading in over its left part.
        const std::uint32_t strip=row.active?0x4a3c2c:0x2a2018;
        const float back=std::clamp(r<backs.size()?backs[r]:row.ready?1.f:0.f,0.f,1.f);
        const unsigned full=unsigned(std::lround(0x80*back));
        if(full)for(int y=top;y<top+partyRow-1;++y)for(int x=0;x<width;++x) {
            const unsigned alpha=x<stripFade?full*unsigned(x)/stripFade:full;
            auto part=[&](unsigned shift){return (((strip>>shift)&255)*alpha/255)<<shift;};
            out[std::size_t(y)*width+x]=(alpha<<24)|part(16)|part(8)|part(0);
        }
        if(row.portrait.size()==face*face)
            for(int y=0;y<face;++y)for(int x=0;x<face;++x)
                if(const auto index=row.portrait[std::size_t(y)*face+x])out[std::size_t(top+1+y)*width+partyFade+x]=state.portraitPalette[index];
        const std::uint32_t tone=row.colour==2?red:row.colour==1?yellow:cream;
        // The top line: the name at the left, the level (or condition) at the right.
        const int levelWidth=row.label.width+2+row.level.width;
        // The small font on the name's baseline.
        const int levelY=top-1+std::max(0,row.name.height-std::max(row.label.height,row.level.height));
        blit(out,width,height,row.label,partyRight-levelWidth,levelY,row.label.width,palette,false,tone);
        blit(out,width,height,row.level,partyRight-row.level.width,levelY,row.level.width,palette,false,tone);
        blit(out,width,height,row.name,partyInfo,top-1,std::max(0,partyRight-levelWidth-4-partyInfo),palette,false,row.colour?tone:green);
        const int barWidth=partyRight-partyInfo;
        for(int k=0;k<2;++k) {
            const int y=top+16+k*17;
            rect(partyInfo,y+11,barWidth,3,barBack);
            rect(partyInfo,y+11,battleBarWidth(k?row.mpValue:row.hpValue,k?row.mpMax:row.hpMax,barWidth),3,k?mpBar:(row.colour?tone:hpBar));
            blit(out,width,height,k?row.mpLabel:row.hpLabel,partyInfo,y-2,barWidth,palette,false,tone);
            const auto& number=k?row.mp:row.hp;
            blit(out,width,height,number,partyRight-number.width,y-2,barWidth,palette,false,tone);
        }
        const float slide=r<slides.size()?std::clamp(slides[r],0.f,1.f):0.f;
        const int x=battleCardSlide-int(std::lround(slide*battleCardSlide));
        for(int y=0;y<height;++y)std::copy_n(out.data()+std::size_t(y)*width,width-x,picture.data()+std::size_t(int(r)*partyRow+y)*pictureWidth+x);
    }
    return picture;
}
BattleGroupLayout layoutBattleMessage(const BattleMenuState& state,int windowWidth,int windowHeight) {
    BattleGroupLayout layout;layout.scale=battleScale(windowHeight);
    if(!state.message.shown)return layout;
    const int s=layout.scale;
    // The room: the window less its margins, and left of the party panel when there is one.
    const int right=state.party.empty()?windowWidth/s-margin:windowWidth/s-margin-partyWidth-battleCardSlide-gap;
    const int room=right-margin;
    int widest=0;
    for(const auto& line:state.message.lines)widest=std::max({widest,line.fullWidth,line.text.width});
    const int w=std::min(std::max(240,widest+2*pad),room),h=std::max(state.message.height,2*pad+row);
    if(w<2*pad+row)return layout;   // no room for it
    // Centred in the room (the whole window without a party panel).
    const int x=state.party.empty()?(windowWidth-w*s)/2:(margin*s+right*s-w*s)/2;
    layout.picture={x+state.message.shakeX*s,windowHeight-(margin+h)*s+state.message.shakeY*s,w*s,h*s};
    return layout;
}
std::vector<std::uint32_t> composeBattleMessage(const BattleMenuState& state,const BattleGroupLayout& layout,int blinkFrame) {
    if(layout.picture.w<=0)return {};
    const int width=layout.picture.w/layout.scale,height=layout.picture.h/layout.scale;
    std::vector<std::uint32_t> out(std::size_t(width)*height,0);
    frame(out,width,{0,0,width,height});
    std::array<std::uint32_t,16> white;white.fill(0xffffffff);
    const auto& palette=state.palette[1]?state.palette:white;
    // The game's lines, cut at the panel's inner width (scrolling lines are cut at its border).
    for(const auto& line:state.message.lines) {
        const int y=line.y;
        if(y+line.text.height<=border||y>=height-border-1)continue;
        const int inner=std::max(0,width-2*pad);
        BattleText text=line.text;
        if(text.width>inner) {  // a hard cut, no "…": the game wraps its own lines
            BattleText cut;cut.width=inner;cut.height=text.height;cut.ink.resize(std::size_t(inner)*text.height);
            for(int ty=0;ty<text.height;++ty)std::copy_n(text.ink.data()+std::size_t(ty)*text.width,inner,cut.ink.data()+std::size_t(ty)*inner);
            text=cut;
        }
        // Rows above or below the inside are left out, so lines scroll under the border.
        for(int ty=0;ty<text.height;++ty) {
            if(y+ty<border+1||y+ty>=height-border-1)std::fill_n(text.ink.data()+std::size_t(ty)*text.width,text.width,0);
        }
        blit(out,width,height,text,pad,y,inner,palette,false);
    }
    if(state.message.name.width>0) {
        const auto& name=state.message.name;
        for(int y=0;y<std::min(height,name.height);++y)for(int x=pad-2;x<std::min(width,pad+name.width+2);++x)out[std::size_t(y)*width+x]=fill;
        blit(out,width,height,name,pad,0,width-2*pad,palette,false);
    }
    // ▼ while it waits for a button.
    if(state.message.waiting&&blinkFrame%30<20) {
        const int x=width-pad-7,y=height-pad-3;
        for(int ty=0;ty<4;++ty)for(int tx=ty;tx<7-ty;++tx)if(y+ty<height)out[std::size_t(y+ty)*width+x+tx]=tan;
    }
    return out;
}
std::vector<std::uint32_t> composeBattlePlate(const BattlePlate& plate,const BattleMenuState& state,int& width,int& height) {
    // The text as the game laid it out in the plate (its margins included), else fitted.
    const bool laidOut=plate.width>0;
    width=laidOut?plate.width:plate.text.width+8;height=16;
    std::vector<std::uint32_t> out(std::size_t(width)*height,0);
    frame(out,width,{0,0,width,height},false);   // too small for the corner curls
    std::array<std::uint32_t,16> white;white.fill(0xffffffff);
    // The plate's own palette (its sprites'), else the menus'.
    const auto& palette=plate.palette[1]?plate.palette:state.palette[1]?state.palette:white;
    const std::uint32_t tone=plate.tone==2?red:plate.tone==1?yellow:cream;
    blit(out,width,height,plate.text,laidOut?0:4,laidOut?0:(height-plate.text.height)/2,width,palette,plate.dim,tone);
    return out;
}
}
