#include "BattleWindows.hpp"
#include <algorithm>

namespace pc {
namespace {
// DS pixels.
constexpr int margin=8,gap=4,pad=8,row=16,cursorWidth=12,columnGap=8,border=2,rightGap=8;
constexpr std::uint32_t navy=0xff101830,white=0xffffffff;
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
    const std::array<std::uint32_t,16>& palette,bool dim) {
    const bool cut=text.width>maxWidth;
    const int shown=cut?std::max(0,maxWidth-7):text.width;
    for(int ty=0;ty<text.height;++ty)for(int tx=0;tx<shown;++tx) {
        const auto index=text.ink[std::size_t(ty)*text.width+tx];
        const int px=x+tx,py=y+ty;
        if(!index||px<0||py<0||px>=width||py>=height)continue;
        auto colour=palette[index&15];
        if(dim)colour=(colour&0xff000000)|((colour>>1)&0x007f7f7f);
        out[std::size_t(py)*width+px]=colour;
    }
    // "…": three dots on the text's baseline.
    if(cut)for(int d=0;d<3;++d)for(int dy=0;dy<2;++dy)for(int dx=0;dx<1;++dx) {
        const int px=x+shown+1+d*2+dx,py=y+text.height-3+dy;
        if(px>=0&&py>=0&&px<width&&py<height)out[std::size_t(py)*width+px]=dim?0xff7f7f7f:white;
    }
}
void frame(std::vector<std::uint32_t>& out,int width,const Box& box) {
    for(int y=0;y<box.h;++y)for(int x=0;x<box.w;++x) {
        const int dx=std::min(x,box.w-1-x),dy=std::min(y,box.h-1-y);
        if(dx+dy<border)continue;  // the rounded corners
        out[std::size_t(box.y+y)*width+box.x+x]=dx<border||dy<border||dx+dy==border?white:navy;
    }
}
void triangle(std::vector<std::uint32_t>& out,int width,int x,int y,bool right) {
    for(int ty=0;ty<7;++ty) {
        const int span=4-std::abs(ty-3);
        for(int tx=0;tx<span;++tx)out[std::size_t(y+ty)*width+(right?x+tx:x+3-tx)]=white;
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
                if(t.x+x>=0&&t.x+x<width&&t.y+dy+y>=0&&t.y+dy+y<height)out[std::size_t(t.y+dy+y)*width+t.x+x]=navy;
        blit(out,width,height,state.title,t.x,t.y+dy,state.kind==BattleMenuKind::message?t.w:state.title.width,state.palette,false);
    }
    for(std::size_t i=0;i<state.entries.size()&&i<p.entries.size();++i) {
        const auto& e=state.entries[i];const auto& b=p.entries[i];
        const int textY=b.y+dy+(row-e.label.height)/2;
        blit(out,width,height,e.label,b.x+cursorWidth,textY,p.labelWidth,state.palette,!e.enabled);
        if(e.right.width>0)
            blit(out,width,height,e.right,b.x+b.w-e.right.width,b.y+dy+(row-e.right.height)/2,e.right.width,state.palette,!e.enabled);
        if(int(i)==state.cursor&&(!state.waiting||blinkFrame%30<20))triangle(out,width,b.x+2,b.y+dy+(row-7)/2,true);
    }
    if(p.footerY>=0) {
        const auto& list=p.windows.back();
        const int y=p.footerY+dy;
        if(state.footer.width>0)blit(out,width,height,state.footer,list.x+pad+cursorWidth,y+(row-state.footer.height)/2,
            list.w-2*pad-cursorWidth,state.palette,false);
        if(state.pages>1) {
            // ◀ one dot per page (the current one larger) ▶, at the right.
            int x=list.x+list.w-pad-(state.pages*4+12);
            triangle(out,width,x,y+(row-7)/2,false);x+=6;
            for(int i=0;i<state.pages;++i,x+=4) {
                const int size=i==state.page?3:1,cy=y+row/2-size/2;
                for(int yy=0;yy<size;++yy)for(int xx=0;xx<size;++xx)out[std::size_t(cy+yy)*width+x+xx]=white;
            }
            triangle(out,width,x+1,y+(row-7)/2,true);
        }
    }
    return out;
}
namespace {
BattleMenuLayout lastLayout;
int pointerX=-1,pointerY=-1;bool leftPress=false,rightPress=false;
}
void rememberBattleLayout(const BattleMenuLayout& layout) {lastLayout=layout;}
bool battleWindowsShown() {return !lastLayout.windows.empty();}
void setBattlePointer(int x,int y,bool leftPressed,bool rightPressed) {
    pointerX=x;pointerY=y;leftPress=leftPress||leftPressed;rightPress=rightPress||rightPressed;
}
BattlePointer takeBattlePointer() {
    BattlePointer pointer;
    pointer.entry=battleEntryAt(lastLayout,pointerX,pointerY);
    pointer.confirm=leftPress&&pointer.entry>=0;
    pointer.cancel=rightPress;
    leftPress=rightPress=false;
    return pointer;
}
int battleEntryAt(const BattleMenuLayout& layout,int x,int y) {
    for(std::size_t i=0;i<layout.entries.size();++i) {
        const auto& r=layout.entries[i];
        if(x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h)return int(i);
    }
    return -1;
}
}
