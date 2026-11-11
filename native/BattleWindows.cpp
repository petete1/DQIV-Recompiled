#include "BattleWindows.hpp"
#include <algorithm>
#include <cmath>
#include <memory>

namespace pc {
namespace {
// DS pixels.
constexpr int margin=8,gap=4,pad=8,row=16,cursorWidth=14,columnGap=16,border=2,rightGap=8;
// A list's header (title, pager, a line under them); the description (10-dot lines, its
// text 200 wide at most, beside the list when that leaves this much, else above it).
constexpr int headerHeight=row+4,helpRow=12,helpSpace=3,helpWidth=200,helpBeside=96;
// The party panel's room at the right (BattleWindows' party constants below).
constexpr int partyRoom=132+12;
// Icons (the commands' 24x24, the items' 32x32), shrunk to fit a row.
constexpr int iconSize=16,iconGap=4;
int iconSide(const std::vector<std::uint32_t>& icon) {
    const int side=int(std::lround(std::sqrt(double(icon.size()))));
    return side>=iconSize&&std::size_t(side)*side==icon.size()?side:0;
}
// The DQ11-like look: see-through brown panels (premultiplied), a tan border inset 2 pixels, cream text.
constexpr std::uint32_t fill=0xe0281810,tan=0xffb89870,cream=0xfff4ead8,gold=0xfff0c040,goldEdge=0xff806010;
// What a window shows, in DS pixels relative to the picture.
struct Box {int x=0,y=0,w=0,h=0;};
struct Plan {
    Box picture;
    std::vector<Box> windows,entries;
    Box titleBox;bool titleOnBorder=false;   // where the title text goes
    int labelWidth=0,rightWidth=0;          // a column's label and right parts
    int iconWidth=0;                         // the icons' column before the labels (0 without icons)
    int footerY=-1;                          // the footer row (picture coordinates)
    int headerY=-1;                          // a list's header row
    Box help;bool described=false;           // the description's text area; the window after the list's
    int listIndex=0;                         // the list's window (1 under field parent commands)
    std::shared_ptr<const Plan> parent;      // a field list's parent commands (their own plan, at the top)
    // The Items window: windows 0 the window, 1 whose, 2 list, 3 detail, 4 the popup; each pane's rows.
    bool items=false,detailBelow=false,whoseHeader=false;
    std::vector<Box> whoseRows,listRows;
    std::shared_ptr<const Plan> popup;       // the popup's own plan (placed at windows[4])
    // The launch list: windows 0 the window, then the cards shown (from card firstCard).
    bool launch=false,moreAbove=false,moreBelow=false;
    int firstCard=0;
};
bool listed(BattleMenuKind kind) {return kind!=BattleMenuKind::none&&kind!=BattleMenuKind::message;}
// The commands and root windows carry their entries; the others show the
// member's name in a window of its own at the left.
bool ownWindow(BattleMenuKind kind) {return kind==BattleMenuKind::root||kind==BattleMenuKind::commands;}
Plan plan(const BattleMenuState& state,int width,int height=0);
// The Items window (the field, Classic): see BattleItemsWindow.
constexpr int itemsRow=20,whoseMin=76,whoseMax=110,listMin=130,detailMin=100,faceStrip=32;
Plan planItems(const BattleMenuState& state,int width,int height) {
    Plan p;p.items=true;
    const auto focus=state.items.focus;
    const auto& whose=focus==BattleItemsFocus::whose?state.entries:state.items.whose.entries;
    const auto& list=focus==BattleItemsFocus::list?state.entries:state.items.list.entries;
    const int right=state.party.empty()?width-margin:width-margin-partyRoom-gap;
    const Box big{0,0,right-margin,height-2*margin};
    p.windows.push_back(big);
    const int inner=big.w-2*pad,top=headerHeight+gap,bottom=big.h-pad;
    int labels=0;for(const auto& e:whose)labels=std::max(labels,e.label.width);
    const int whoseW=std::clamp(2*pad+cursorWidth+labels+4+faceStrip,whoseMin,whoseMax);
    int names=0;for(const auto& e:list)names=std::max(names,e.label.width);
    p.detailBelow=whoseW+listMin+detailMin+2*gap>inner;
    int listW=inner-whoseW-gap;
    if(!p.detailBelow)listW=std::clamp(2*pad+cursorWidth+8+iconSize+iconGap+names,listMin,inner-whoseW-detailMin-2*gap);
    const Box whoseBox{pad,top,whoseW,bottom-top};
    const Box listBox{pad+whoseW+gap,top,listW,headerHeight+2*pad+std::max(6,int(list.size()))*row};  // towns: 8 a page
    const Box detail=p.detailBelow?Box{listBox.x,listBox.y+listBox.h+gap,listW,bottom-(listBox.y+listBox.h+gap)}
        :Box{listBox.x+listW+gap,top,inner+pad-(listBox.x+listW+gap),bottom-top};
    // Too short for the panes (the picture would not hold them): no window.
    if(listBox.y+listBox.h+pad>big.h||(p.detailBelow&&detail.h<40+pad))return Plan{};
    p.windows.push_back(whoseBox);p.windows.push_back(listBox);p.windows.push_back(detail);
    // Use's targets: their item and pager in a header of their own.
    p.whoseHeader=focus==BattleItemsFocus::whose&&(state.pages>1||state.title.width>0);
    const int whoseTop=whoseBox.y+pad+(p.whoseHeader?headerHeight:0);
    for(std::size_t i=0;i<whose.size();++i)
        p.whoseRows.push_back({whoseBox.x+pad,whoseTop+int(i)*itemsRow,whoseBox.w-2*pad,itemsRow});
    for(std::size_t i=0;i<list.size();++i)
        p.listRows.push_back({listBox.x+pad,listBox.y+headerHeight+pad+int(i)*row,listBox.w-2*pad,row});
    p.headerY=listBox.y+pad;
    p.picture={margin,0,width-2*margin,big.h};
    if(focus==BattleItemsFocus::popup) {
        BattleMenuState commands;
        commands.kind=BattleMenuKind::list;commands.field=true;commands.entries=state.entries;commands.title=state.title;
        auto popup=plan(commands,width);
        if(popup.picture.h>big.h)return Plan{};
        if(!popup.windows.empty()) {
            const int x=width-2*margin-popup.picture.w;
            auto& box=popup.windows.front();box.x+=x;
            for(auto& e:popup.entries)e.x+=x;
            popup.titleBox.x+=x;
            p.windows.push_back(box);p.entries=popup.entries;
            p.popup=std::make_shared<const Plan>(popup);
        }
    } else p.entries=focus==BattleItemsFocus::whose?p.whoseRows:p.listRows;
    return p;
}
// The launch list (Classic): the save cards in one window, scrolled to the cursor's card.
constexpr int cardHeight=31,cardGap=2,cardFace=24;  // 31: a row under the face, as the portrait's own top row leaves above it
Plan planLaunch(const BattleMenuState& state,int width,int height) {
    Plan p;p.launch=true;
    const int right=state.party.empty()?width-margin:width-margin-partyRoom-gap;
    Box big{0,0,right-margin,height-2*margin};
    const int top=state.title.width>0?headerHeight+gap:pad,count=int(state.launch.cards.size());
    const int visible=std::min(count,(big.h-top-pad+cardGap)/(cardHeight+cardGap));
    if(state.launch.fit)big.h=std::min(big.h,top+visible*(cardHeight+cardGap)-cardGap+pad);
    if(visible<1||big.w<2*pad+cursorWidth+cardFace)return Plan{};
    int cursorCard=0;
    for(int i=0;i<count;++i)if(state.launch.cards[std::size_t(i)].entry==state.cursor&&state.cursor>=0)cursorCard=i;
    p.firstCard=std::clamp(cursorCard-visible/2,0,count-visible);
    p.moreAbove=p.firstCard>0;p.moreBelow=p.firstCard+visible<count;
    p.windows.push_back(big);
    p.entries.assign(state.entries.size(),Box{});
    for(int i=0;i<visible;++i) {
        const Box card{pad+cursorWidth-2,top+i*(cardHeight+cardGap),big.w-2*pad-cursorWidth+2,cardHeight};
        p.windows.push_back(card);
        const int entry=state.launch.cards[std::size_t(p.firstCard+i)].entry;
        if(entry>=0&&entry<int(p.entries.size()))p.entries[std::size_t(entry)]=card;
    }
    p.picture={margin,0,big.w,big.h};
    return p;
}
Plan plan(const BattleMenuState& state,int width,int height) {
    Plan p;
    if(state.kind==BattleMenuKind::none)return p;
    if(state.launch.shown&&!state.launch.cards.empty())return planLaunch(state,width,height);
    if(state.items.shown)return planItems(state,width,height);
    if(state.kind==BattleMenuKind::message) {
        const int w=std::min(state.title.width+2*pad,width-2*margin),h=state.title.height+2*pad;
        p.windows.push_back({0,0,std::max(w,2*pad),h});p.titleBox={pad,pad,w-2*pad,state.title.height};
        p.picture={margin,0,p.windows[0].w,h};
        return p;
    }
    const int columns=std::max(1,state.columns),count=int(state.entries.size());
    for(const auto& e:state.entries) {
        p.labelWidth=std::max(p.labelWidth,e.label.width);p.rightWidth=std::max(p.rightWidth,e.right.width);
        if(iconSide(e.icon))p.iconWidth=iconSize+iconGap;
    }
    auto columnWidth=[&] {return cursorWidth+p.iconWidth+p.labelWidth+(p.rightWidth?rightGap+p.rightWidth:0);};
    // Cut the labels so the window fits the game window.
    const int room=width-2*margin-2*pad-(columns-1)*columnGap;
    if(columnWidth()*columns>room)p.labelWidth=std::max(0,room/columns-cursorWidth-p.iconWidth-(p.rightWidth?rightGap+p.rightWidth:0));
    const int rows=(count+columns-1)/columns;
    const bool footer=state.footer.width>0;
    // Lists: the title and the pager in a row of their own; root and commands: the title on the border.
    const bool header=!ownWindow(state.kind)&&(state.title.width>0||state.pages>1);
    const int pagerWidth=state.pages>1?state.pageText.width+18:0;
    int titleHeight=0;
    if(ownWindow(state.kind)&&state.title.width>0) {p.titleOnBorder=true;titleHeight=state.title.height/2;}
    const int top=titleHeight+(header?headerHeight:0);
    const int innerWidth=std::max({columns*columnWidth()+(columns-1)*columnGap,ownWindow(state.kind)?state.title.width+4:0,
        header?state.title.width+(pagerWidth?pagerWidth+columnGap:0):0});
    const int w=innerWidth+2*pad,h=top+2*pad+std::max(rows,1)*row+(footer?row:0);
    p.windows.push_back({0,0,w,h});
    if(p.titleOnBorder)p.titleBox={pad,0,state.title.width,state.title.height};
    if(header) {p.headerY=titleHeight+pad;p.titleBox={pad,p.headerY+(row-state.title.height)/2,state.title.width,state.title.height};}
    for(int i=0;i<count;++i)
        p.entries.push_back({pad+(i%columns)*(columnWidth()+columnGap),top+pad+(i/columns)*row,columnWidth(),row});
    if(footer)p.footerY=top+pad+std::max(rows,1)*row;
    // The description: at the right of the list when there is room left of the party panel, else above it.
    if(!state.help.empty()||state.cost.width>0) {
        const int right=state.party.empty()?width-margin:width-margin-partyRoom-gap;
        const int beside=std::min(helpWidth,right-margin-w-gap-2*pad);
        const int textWidth=beside>=helpBeside?beside:w-2*pad;
        const int lines=state.help.empty()?0:int(wrapBattleWords(state.help,textWidth,helpSpace).size());
        const int dh=2*pad+lines*helpRow+(state.cost.width>0?row+4:0);
        p.described=true;
        if(beside>=helpBeside)p.windows.push_back({w+gap,0,textWidth+2*pad,std::max(h,dh)});
        else if(state.field)p.windows.push_back({0,h+gap,w,dh});  // the field: below the list
        else p.windows.push_back({0,-(dh+gap),w,dh});
        const auto& d=p.windows.back();
        p.help={d.x+pad,d.y+pad,textWidth,d.h-2*pad};
    }
    // Battle: windows side by side end on the same bottom line; then all moves down to start
    // at 0. The field: they start on the same top line.
    auto shiftAll=[&](int by) {
        for(auto& box:p.windows)box.y+=by;
        for(auto& e:p.entries)e.y+=by;
        p.titleBox.y+=by;p.help.y+=by;
        if(p.footerY>=0)p.footerY+=by;
        if(p.headerY>=0)p.headerY+=by;
    };
    if(!state.field) {
        int bottom=0;
        for(const auto& box:p.windows)if(box.y>=0)bottom=std::max(bottom,box.h);
        auto& list=p.windows.front();
        const int shift=bottom-list.h;
        list.y=shift;
        if(p.described&&p.windows[1].y>=0) {p.windows[1].y=bottom-p.windows[1].h;p.help.y=p.windows[1].y+pad;}
        for(auto& e:p.entries)e.y+=shift;
        if(p.titleOnBorder||header)p.titleBox.y+=shift;
        if(p.footerY>=0)p.footerY+=shift;
        if(p.headerY>=0)p.headerY+=shift;
        if(p.described&&p.windows[1].y<0) {p.windows[1].y+=shift;p.help.y+=shift;}
    }
    int topmost=0;
    for(const auto& box:p.windows)topmost=std::min(topmost,box.y);
    shiftAll(-topmost);
    // A field list: the root commands it came from stay above it, the hand still on the choice.
    if(state.field&&!state.parent.empty()) {
        BattleMenuState commands;
        commands.kind=BattleMenuKind::commands;commands.columns=2;commands.field=true;
        commands.entries=state.parent;commands.cursor=state.parentCursor;commands.waiting=false;
        const auto above=std::make_shared<const Plan>(plan(commands,width));
        if(!above->windows.empty()) {
            shiftAll(above->picture.h+gap);
            p.windows.insert(p.windows.begin(),above->windows.front());
            p.listIndex=1;p.parent=above;
        }
    }
    int rightmost=0,bottom=0;
    for(const auto& box:p.windows) {rightmost=std::max(rightmost,box.x+box.w);bottom=std::max(bottom,box.y+box.h);}
    p.picture={margin,0,rightmost,bottom};
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
// An icon shrunk to 16x16: each pixel the average of the source area it covers
// (premultiplied, so the edges blend with the panel).
void icon(std::vector<std::uint32_t>& out,int width,int height,const std::vector<std::uint32_t>& src,int x,int y,bool dim) {
    const int n=iconSide(src);
    for(int oy=0;oy<iconSize;++oy)for(int ox=0;ox<iconSize;++ox) {
        double sum[4]{};
        // The area in source pixels, times iconSize: [o*n, (o+1)*n).
        for(int sy=oy*n/iconSize;sy*iconSize<(oy+1)*n;++sy)for(int sx=ox*n/iconSize;sx*iconSize<(ox+1)*n;++sx) {
            const int wx=std::min((sx+1)*iconSize,(ox+1)*n)-std::max(sx*iconSize,ox*n);
            const int wy=std::min((sy+1)*iconSize,(oy+1)*n)-std::max(sy*iconSize,oy*n);
            const std::uint32_t c=src[std::size_t(sy)*n+sx];
            const double a=(c>>24)/255.0*wx*wy;
            sum[0]+=a;sum[1]+=((c>>16)&255)*a;sum[2]+=((c>>8)&255)*a;sum[3]+=(c&255)*a;
        }
        if(sum[0]<=0)continue;
        const int px=x+ox,py=y+oy;
        if(px<0||py<0||px>=width||py>=height)continue;
        const double area=double(n)*n,alpha=sum[0]/area,shade=dim?0.5:1.0;
        const std::uint32_t under=out[std::size_t(py)*width+px];
        auto mix=[&](int i,unsigned shift){return std::uint32_t(std::lround(sum[i]/area*shade+((under>>shift)&255)*(1-alpha)))<<shift;};
        const std::uint32_t a=std::uint32_t(std::lround(255*alpha+(under>>24)*(1-alpha)));
        out[std::size_t(py)*width+px]=(a<<24)|mix(1,16)|mix(2,8)|mix(3,0);
    }
}
// A small curl in each corner, inside the border (top-left; mirrored for the others).
constexpr const char* curl[5]={".XXX.","X....","X.XX.","X.X..","....."};
void frame(std::vector<std::uint32_t>& out,int width,const Box& box,bool curls=true,std::uint32_t inside=fill) {
    for(int y=0;y<box.h;++y)for(int x=0;x<box.w;++x) {
        const int dx=std::min(x,box.w-1-x),dy=std::min(y,box.h-1-y);
        if(dx+dy<1)continue;  // the rounded outer corners
        std::uint32_t colour=inside;
        if((dx==border&&dy>=border)||(dy==border&&dx>=border))colour=tan;
        else if(curls&&dx>border&&dy>border&&dx-border-1<5&&dy-border-1<5&&curl[dy-border-1][dx-border-1]=='X')colour=tan;
        out[std::size_t(box.y+y)*width+box.x+x]=colour;
    }
}
// ▶ (or ◀) of the pager: 5 wide, 9 tall, gold.
void triangle(std::vector<std::uint32_t>& out,int width,int x,int y,bool right) {
    for(int ty=0;ty<9;++ty) {
        const int span=5-std::abs(ty-4);
        for(int tx=0;tx<span;++tx)out[std::size_t(y+ty)*width+(right?x+tx:x+4-tx)]=gold;
    }
}
// A tan line across a window's inside.
void rule(std::vector<std::uint32_t>& out,int width,int x,int y,int w) {
    for(int i=0;i<w;++i)out[std::size_t(y)*width+x+i]=tan;
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
BattleGroupLayout layoutBattleGold(const BattleMenuState& state,int windowWidth,int windowHeight) {
    BattleGroupLayout layout;layout.scale=battleScale(windowHeight);
    if(state.gold.width<=0||state.items.shown||state.launch.shown)return layout;
    const int s=layout.scale,w=2*pad+state.gold.width+3+state.goldUnit.width,h=row+2*pad;
    layout.picture={windowWidth-(margin+w)*s,margin*s,w*s,h*s};
    return layout;
}
std::vector<std::uint32_t> composeBattleGold(const BattleMenuState& state,const BattleGroupLayout& layout) {
    if(layout.picture.w<=0)return {};
    const int s=layout.scale,w=layout.picture.w/s,h=layout.picture.h/s;
    std::vector<std::uint32_t> out(std::size_t(w)*h,0);
    frame(out,w,{0,0,w,h});
    // The amount, then the unit, at the right.
    const int unitX=w-pad-state.goldUnit.width,goldX=unitX-3-state.gold.width;
    blit(out,w,h,state.gold,goldX,(h-state.gold.height)/2,state.gold.width,state.palette,false);
    blit(out,w,h,state.goldUnit,unitX,(h-state.goldUnit.height)/2,state.goldUnit.width,state.palette,false);
    return out;
}
bool classicFieldMapShown(const BattleMenuState& state) {
    return state.field&&state.kind!=BattleMenuKind::none&&!state.message.shown&&!state.items.shown&&!state.launch.shown;
}
std::array<float,4> fieldMapInset(float canvasHeight,float insetWidth,float insetHeight) {
    return {6.f,canvasHeight-6.f-insetHeight,insetWidth,insetHeight};
}
std::vector<std::uint32_t> battleSpellIcon(BattleSpellKind kind) {
    // A coloured disc with a light rim at the top left, a dark one at the bottom right, and a
    // white symbol (10x10) with a shadow: flame, cross, arrow up, arrow down, wing.
    static constexpr const char* symbols[5][10]={
        {"....X.....","...XX.....","...XXX....","..XXXX.X..","..XXXXXX..",".XXXXXXXX.",".XXX.XXXX.",".XX...XXX.","..X...XX..","...XXXX..."},
        {"..........","....XX....","....XX....","....XX....",".XXXXXXXX.",".XXXXXXXX.","....XX....","....XX....","....XX....",".........."},
        {"..........","....XX....","...XXXX...","..XXXXXX..",".XXXXXXXX.","....XX....","....XX....","....XX....","....XX....",".........."},
        {"..........","....XX....","....XX....","....XX....","....XX....",".XXXXXXXX.","..XXXXXX..","...XXXX...","....XX....",".........."},
        {"........XX","......XXX.","....XXXX..","..XXXXX...",".XXXXX....",".XXXX.X...","XXX..X....","X..XX.....","..........",".........."}};
    static constexpr std::uint32_t colours[5]={0xe04838,0x38b048,0x3870e0,0x9048d0,0xd8b028};
    const int k=int(kind);
    if(k<0||k>4)return {};
    std::vector<std::uint32_t> out(16*16,0);
    auto shade=[](std::uint32_t c,double f) {
        auto part=[&](unsigned shift){return std::uint32_t(std::clamp(((c>>shift)&255)*f,0.0,255.0))<<shift;};
        return part(16)|part(8)|part(0);
    };
    for(int y=0;y<16;++y)for(int x=0;x<16;++x) {
        const double dx=x+0.5-8,dy=y+0.5-8,d=std::sqrt(dx*dx+dy*dy);
        const double alpha=std::clamp(8.0-d,0.0,1.0);
        if(alpha<=0)continue;
        // The rim: lighter where it faces the top left, darker elsewhere.
        const double f=d>6.5?(dx+dy<0?1.35:0.55):1.0;
        const std::uint32_t c=shade(colours[k],f*alpha);
        out[std::size_t(y)*16+x]=(std::uint32_t(std::lround(alpha*255))<<24)|c;
    }
    for(int pass=0;pass<2;++pass)for(int y=0;y<10;++y)for(int x=0;x<10;++x) {
        if(symbols[k][y][x]!='X')continue;
        const int px=3+x+(pass?0:1),py=3+y+(pass?0:1);
        if(px<16&&py<16)out[std::size_t(py)*16+px]=pass?0xfff8f0e0u:(0xff000000u|shade(colours[k],0.45));
    }
    return out;
}
std::vector<int> wrapBattleWords(std::span<const BattleText> words,int width,int space) {
    std::vector<int> starts;
    int x=0;
    for(int i=0;i<int(words.size());++i) {
        const int w=words[std::size_t(i)].width;
        if(w==0) {starts.push_back(i+1);x=0;continue;}  // a break: the next word starts a line
        if(starts.empty()||(x>0&&x+space+w>width)) {
            if(starts.empty()||starts.back()!=i)starts.push_back(i);
            x=w;
        } else x+=(x>0?space:0)+w;
    }
    // A break at the very end starts no line.
    while(!starts.empty()&&starts.back()>=int(words.size()))starts.pop_back();
    return starts;
}
int battleScale(int windowHeight) {return std::max(1,windowHeight/270);}
BattleMenuLayout layoutBattleMenu(const BattleMenuState& state,int windowWidth,int windowHeight) {
    BattleMenuLayout layout;layout.scale=battleScale(windowHeight);
    const int s=layout.scale;
    const auto p=plan(state,windowWidth/s,windowHeight/s);
    if(p.windows.empty())return layout;
    layout.planWidth=windowWidth/s;layout.planHeight=windowHeight/s;
    const int top=state.field||state.items.shown?margin:windowHeight/s-margin-p.picture.h;
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
namespace {
// A 32x32 icon at full size (ARGB, straight alpha) over the panel.
void bigIcon(std::vector<std::uint32_t>& out,int width,int height,const std::vector<std::uint32_t>& src,int x,int y) {
    if(src.size()!=32*32)return;
    for(int ty=0;ty<32;++ty)for(int tx=0;tx<32;++tx) {
        const auto c=src[std::size_t(ty)*32+tx];const unsigned a=c>>24;
        const int px=x+tx,py=y+ty;
        if(!a||px<0||py<0||px>=width||py>=height)continue;
        auto& under=out[std::size_t(py)*width+px];
        if(a==255) {under=c;continue;}
        auto mix=[&](unsigned shift){return ((((c>>shift)&255)*a+((under>>shift)&255)*(255-a))/255)<<shift;};
        under=(std::min(255u,a+((under>>24)*(255-a))/255)<<24)|mix(16)|mix(8)|mix(0);
    }
}
// A member's face strip: the portrait's 32x16 at the eyes, its left 8 columns fading in.
void drawFace(std::vector<std::uint32_t>& out,int width,int height,const BattleMenuState& state,const std::vector<std::uint8_t>& face,int x,int y) {
    if(face.size()!=48*48)return;
    for(int ty=0;ty<16;++ty)for(int tx=0;tx<32;++tx) {
        const auto index=face[std::size_t(14+ty)*48+8+tx];
        const int px=x+tx,py=y+ty;
        if(!index||px<0||py<0||px>=width||py>=height)continue;
        const auto c=state.portraitPalette[index];
        auto& under=out[std::size_t(py)*width+px];
        if(tx>=8) {under=c;continue;}
        const unsigned k=255*unsigned(tx+1)/9;
        auto mix=[&](unsigned shift){return ((((c>>shift)&255)*k+((under>>shift)&255)*(255-k))/255)<<shift;};
        under=mix(24)|mix(16)|mix(8)|mix(0);
    }
}
template<class Pointer,class Entries>
void composeItems(std::vector<std::uint32_t>& out,int width,int height,const BattleMenuState& state,const Plan& p,
    Pointer& pointer,Entries& entries) {
    const auto& items=state.items;const auto focus=items.focus;
    const auto& big=p.windows[0];
    frame(out,width,big);
    for(std::size_t i=1;i<4&&i<p.windows.size();++i)frame(out,width,p.windows[i],false);
    blit(out,width,height,items.title,big.x+(big.w-items.title.width)/2,big.y+(headerHeight-items.title.height)/2+1,big.w-2*pad,state.palette,false);
    rule(out,width,big.x+pad,big.y+headerHeight,big.w-2*pad);
    // Whose items: names and faces; the focused pane's cursor moves, the others' stay still.
    const bool whoseFocus=focus==BattleItemsFocus::whose;
    const auto& whose=whoseFocus?state.entries:items.whose.entries;
    const int whoseCursor=whoseFocus?state.cursor:items.whose.cursor;
    if(p.whoseHeader) {
        const auto& w=p.windows[1];const int y=w.y+pad;
        const int pagerWidth=state.pages>1?state.pageText.width+22:0;
        blit(out,width,height,state.title,w.x+pad,y+(row-state.title.height)/2,w.w-2*pad-pagerWidth,state.palette,false);
        rule(out,width,w.x+pad-2,y+row+1,w.w-2*pad+4);
        if(state.pages>1) {
            int x=w.x+w.w-pad-state.pageText.width-18;
            triangle(out,width,x,y+(row-9)/2,false);x+=9;
            blit(out,width,height,state.pageText,x,y+(row-state.pageText.height)/2,state.pageText.width,state.palette,false);
            triangle(out,width,x+state.pageText.width+4,y+(row-9)/2,true);
        }
    }
    for(std::size_t i=0;i<whose.size()&&i<p.whoseRows.size();++i) {
        const auto& e=whose[i];const auto& b=p.whoseRows[i];
        drawFace(out,width,height,state,e.face,b.x+b.w-faceStrip,b.y+(itemsRow-16)/2);
        blit(out,width,height,e.label,b.x+cursorWidth,b.y+(itemsRow-e.label.height)/2,b.w-cursorWidth-faceStrip-2,state.palette,!e.enabled);
        if(int(i)==whoseCursor)pointer(b.x-3,b.y+(itemsRow-16)/2,whoseFocus&&state.waiting,!whoseFocus);
    }
    // The list: its header, then "E", the icon and the name.
    const bool listFocus=focus==BattleItemsFocus::list;
    const auto& list=listFocus?state.entries:items.list.entries;
    const int listCursor=listFocus?state.cursor:items.list.cursor;
    const auto& title=listFocus?state.title:items.list.title;
    const auto& pageText=listFocus?state.pageText:items.list.pageText;
    const int pages=listFocus?state.pages:items.list.pages;
    const auto& box=p.windows[2];
    blit(out,width,height,title,box.x+pad,p.headerY+(row-title.height)/2,box.w-2*pad-(pages>1?pageText.width+22:0),state.palette,false);
    rule(out,width,box.x+pad-2,p.headerY+row+1,box.w-2*pad+4);
    if(pages>1) {
        int x=box.x+box.w-pad-pageText.width-18;
        triangle(out,width,x,p.headerY+(row-9)/2,false);x+=9;
        blit(out,width,height,pageText,x,p.headerY+(row-pageText.height)/2,pageText.width,state.palette,false);
        triangle(out,width,x+pageText.width+4,p.headerY+(row-9)/2,true);
    }
    for(std::size_t i=0;i<list.size()&&i<p.listRows.size();++i) {
        const auto& e=list[i];const auto& b=p.listRows[i];
        const int x=b.x+cursorWidth;
        if(e.right.width>0)blit(out,width,height,e.right,x,b.y+(row-e.right.height)/2,e.right.width,state.palette,!e.enabled);
        if(iconSide(e.icon))icon(out,width,height,e.icon,x+10,b.y+(row-iconSize)/2,!e.enabled);
        const int labelX=x+10+iconSize+iconGap;
        blit(out,width,height,e.label,labelX,b.y+(row-e.label.height)/2,b.x+b.w-labelX,state.palette,!e.enabled);
        if(int(i)==listCursor)pointer(b.x-3,b.y+(row-16)/2,listFocus&&state.waiting,!listFocus);
    }
    // The detail: the kind, the icon in a frame, the description, the count and the comparison.
    if(!items.icon.empty()&&p.windows.size()>3) {
        const auto& d=p.windows[3];
        const int kindY=d.y+pad/2;
        blit(out,width,height,items.kind,d.x+std::max(pad,(d.w-items.kind.width)/2),kindY+(row-items.kind.height)/2,d.w-2*pad,state.palette,false);
        rule(out,width,d.x+pad-2,kindY+row+1,d.w-2*pad+4);
        const int contentTop=kindY+row+5;
        const Box iconBox=p.detailBelow?Box{d.x+pad,contentTop,40,40}:Box{d.x+(d.w-40)/2,contentTop,40,40};
        frame(out,width,iconBox,false);
        bigIcon(out,width,height,items.icon,iconBox.x+4,iconBox.y+4);
        const Box text=p.detailBelow?Box{d.x+pad+44,contentTop,d.w-2*pad-44,0}:Box{d.x+pad,contentTop+44,d.w-2*pad,0};
        const bool compare=items.statLabel.width>0;
        int bottom=d.y+d.h-pad;
        const int left=text.x;  // below the list: right of the icon, clear of it
        if(compare) {
            const int y=bottom-row;
            rule(out,width,left-2,y-3,d.x+d.w-pad-left+4);
            blit(out,width,height,items.statLabel,left,y+(row-items.statLabel.height)/2,d.x+d.w-pad-left,state.palette,false);
            int x=d.x+d.w-pad;
            if(items.statNew.width>0) {
                x-=items.statNew.width;blit(out,width,height,items.statNew,x,y+(row-items.statNew.height)/2,items.statNew.width,state.palette,false);
                x-=9;triangle(out,width,x+2,y+(row-9)/2,true);
            } else if(items.statNote.width>0&&items.statNow.width>0) {
                x-=items.statNow.width;blit(out,width,height,items.statNow,x,y+(row-items.statNow.height)/2,items.statNow.width,state.palette,false);
                x-=4;
            }
            const auto& first=items.statNew.width>0?items.statNow:items.statNote;
            x-=first.width;blit(out,width,height,first,x,y+(row-first.height)/2,first.width,state.palette,false);
            bottom=y-4;
        }
        if(items.count.width>0) {
            bottom-=row;
            blit(out,width,height,items.count,d.x+d.w-pad-items.count.width,bottom+(row-items.count.height)/2,items.count.width,state.palette,false);
        }
        const auto lines=wrapBattleWords(state.help,text.w,helpSpace);
        for(std::size_t l=0;l<lines.size();++l) {
            const int y=text.y+int(l)*helpRow;
            if(y+helpRow>bottom)break;
            const int end=l+1<lines.size()?lines[l+1]:int(state.help.size());
            for(int i=lines[l],x=text.x;i<end;x+=state.help[std::size_t(i)].width+helpSpace,++i)
                blit(out,width,height,state.help[std::size_t(i)],x,y,text.x+text.w-x,state.palette,false);
        }
    }
    // Attributes: label / value rows, under the header when there is one.
    if(items.icon.empty()&&!items.rows.empty()&&p.windows.size()>3) {
        const auto& d=p.windows[3];
        int y=d.y+pad/2;
        if(items.kind.width>0) {
            blit(out,width,height,items.kind,d.x+std::max(pad,(d.w-items.kind.width)/2),y+(row-items.kind.height)/2,d.w-2*pad,state.palette,false);
            rule(out,width,d.x+pad-2,y+row+1,d.w-2*pad+4);
            y+=row+5;
        }
        for(const auto& r:items.rows) {
            if(y+row>d.y+d.h-pad)break;
            blit(out,width,height,r[1],d.x+d.w-pad-r[1].width,y+(row-r[1].height)/2,r[1].width,state.palette,false);
            blit(out,width,height,r[0],d.x+pad,y+(row-r[0].height)/2,std::max(0,d.w-3*pad-r[1].width),state.palette,false);
            y+=row;
        }
    }
    // The item's commands, in their own window at the top right.
    if(p.popup&&p.windows.size()>4) {
        const auto& q=*p.popup;const auto& w=p.windows[4];
        frame(out,width,w);
        blit(out,width,height,state.title,q.titleBox.x,q.titleBox.y,w.w-2*pad,state.palette,false);
        if(q.headerY>=0)rule(out,width,w.x+pad-2,q.headerY+row+1,w.w-2*pad+4);
        entries(q,state.entries,state.cursor,state.waiting,false);
    }
}
}
// A face halved to 24x24 (every other pixel of the 48x48 portrait).
void smallFace(std::vector<std::uint32_t>& out,int width,int height,const BattleMenuState& state,const std::vector<std::uint8_t>& face,int x,int y) {
    if(face.size()!=48*48)return;
    for(int ty=0;ty<cardFace;++ty)for(int tx=0;tx<cardFace;++tx) {
        const auto index=face[std::size_t(ty*2)*48+tx*2];
        const int px=x+tx,py=y+ty;
        if(index&&px>=0&&py>=0&&px<width&&py<height)out[std::size_t(py)*width+px]=state.portraitPalette[index];
    }
}
// ▲ (or ▼) of a scrolled list: 9 wide, 5 tall, gold.
void arrow(std::vector<std::uint32_t>& out,int width,int height,int x,int y,bool up) {
    for(int ty=0;ty<5;++ty)for(int tx=4-(up?ty:4-ty);tx<=4+(up?ty:4-ty);++tx)
        if(x+tx>=0&&x+tx<width&&y+ty>=0&&y+ty<height)out[std::size_t(y+ty)*width+x+tx]=gold;
}
// The chosen card's panel, lighter than the others.
constexpr std::uint32_t chosenFill=0xe0503420;
template<class Pointer>
void composeLaunch(std::vector<std::uint32_t>& out,int width,int height,const BattleMenuState& state,const Plan& p,Pointer& pointer) {
    const auto& big=p.windows[0];
    frame(out,width,big);
    if(state.title.width>0) {
        blit(out,width,height,state.title,big.x+(big.w-state.title.width)/2,big.y+(headerHeight-state.title.height)/2+1,big.w-2*pad,state.palette,false);
        rule(out,width,big.x+pad,big.y+headerHeight,big.w-2*pad);
    }
    if(p.moreAbove)arrow(out,width,height,big.x+big.w-pad-9,big.y+headerHeight-7,true);
    if(p.moreBelow)arrow(out,width,height,big.x+big.w-pad-9,big.y+big.h-pad+1,false);
    int labels=0,chapters=0;
    for(const auto& c:state.launch.cards) {labels=std::max(labels,c.label.width);chapters=std::max(chapters,c.chapter.width);}
    for(std::size_t i=1;i<p.windows.size();++i) {
        const auto& b=p.windows[i];
        const auto& c=state.launch.cards[std::size_t(p.firstCard)+i-1];
        const bool chosen=c.entry>=0&&c.entry==state.cursor,dim=c.entry<0;
        frame(out,width,b,false,chosen?chosenFill:fill);
        smallFace(out,width,height,state,c.face,b.x+4,b.y+(b.h-cardFace)/2);
        const int x=b.x+4+cardFace+6,right=b.x+b.w-pad,y1=b.y+3,y2=b.y+15;
        blit(out,width,height,c.label,x,y1,labels,state.palette,dim);
        blit(out,width,height,c.time,right-c.time.width,y1,c.time.width,state.palette,dim);
        const int nameRoom=std::max(0,right-c.time.width-8-(x+labels+8)-(c.level.width?c.level.width+8:0));
        blit(out,width,height,c.name,x+labels+8,y1,nameRoom,state.palette,dim);
        blit(out,width,height,c.level,x+labels+8+std::min(nameRoom,c.name.width)+8,y1,c.level.width,state.palette,dim);
        blit(out,width,height,c.date,right-c.date.width,y2,c.date.width,state.palette,dim);
        blit(out,width,height,c.chapter,x,y2,chapters,state.palette,dim);
        blit(out,width,height,c.place,x+chapters+8,y2,std::max(0,right-c.date.width-8-(x+chapters+8)),state.palette,dim);
        if(chosen)pointer(b.x-cursorWidth+1,b.y+(b.h-16)/2,state.waiting,false);
    }
}
std::vector<std::uint32_t> composeBattleMenu(const BattleMenuState& state,const BattleMenuLayout& layout,int blinkFrame) {
    if(layout.windows.empty())return {};
    const int s=layout.scale,width=layout.picture.w/s,height=layout.picture.h/s;
    // The same plan as the layout.
    const auto p=plan(state,layout.planWidth>0?layout.planWidth:width+2*margin,layout.planHeight);
    std::vector<std::uint32_t> out(std::size_t(width)*height,0);
    // The layout's windows in DS pixels relative to the picture.
    if(!p.items&&!p.launch)for(const auto& w:layout.windows)
        frame(out,width,{(w.x-layout.picture.x)/s,(w.y-layout.picture.y)/s,w.w/s,w.h/s});
    const int dy=height-p.picture.h;  // the plan's own bottom line is the picture's
    if(state.title.width>0&&!p.items&&!p.launch) {
        const auto& t=p.titleBox;
        if(p.titleOnBorder)  // the name on the top border, over a strip of the panel colour
            for(int y=0;y<state.title.height;++y)for(int x=-2;x<state.title.width+2;++x)
                if(t.x+x>=0&&t.x+x<width&&t.y+dy+y>=0&&t.y+dy+y<height)out[std::size_t(t.y+dy+y)*width+t.x+x]=fill;
        blit(out,width,height,state.title,t.x,t.y+dy,state.kind==BattleMenuKind::message?t.w:state.title.width,state.palette,false);
    }
    // The cursor: the game's animated one (still on the parent commands), else the hand.
    auto pointer=[&](int x,int y,bool blinking,bool still) {
        const auto& frame=state.cursorFrames[std::size_t(still?0:std::clamp(state.cursorFrame,0,4))];
        if(frame.size()==16*16) {
            for(int ty=0;ty<16;++ty)for(int tx=0;tx<16;++tx) {
                const int px=x+tx,py=y+ty;
                const auto colour=frame[std::size_t(ty)*16+tx];
                if((colour>>24)&&px>=0&&py>=0&&px<width&&py<height)out[std::size_t(py)*width+px]=colour;
            }
        } else if(!blinking||blinkFrame%30<20)hand(out,width,x+4,y+(16-7)/2);
    };
    auto entries=[&](const Plan& q,const std::vector<BattleMenuEntry>& list,int cursor,bool blinking,bool still) {
        for(std::size_t i=0;i<list.size()&&i<q.entries.size();++i) {
            const auto& e=list[i];const auto& b=q.entries[i];
            const int textY=b.y+dy+(row-e.label.height)/2;
            if(iconSide(e.icon))
                icon(out,width,height,e.icon,b.x+cursorWidth,b.y+dy+(row-iconSize)/2,!e.enabled);
            blit(out,width,height,e.label,b.x+cursorWidth+q.iconWidth,textY,q.labelWidth,state.palette,!e.enabled);
            if(e.right.width>0)
                blit(out,width,height,e.right,b.x+b.w-e.right.width,b.y+dy+(row-e.right.height)/2,e.right.width,state.palette,!e.enabled);
            if(int(i)==cursor)pointer(b.x-3,b.y+dy+(row-16)/2,blinking,still);
        }
    };
    if(p.items) {composeItems(out,width,height,state,p,pointer,entries);return out;}
    if(p.launch) {composeLaunch(out,width,height,state,p,pointer);return out;}
    entries(p,state.entries,state.cursor,state.waiting,false);
    if(p.parent)entries(*p.parent,state.parent,state.parentCursor,false,true);
    const auto& list=p.windows[std::size_t(p.listIndex)];
    if(p.footerY>=0&&state.footer.width>0)
        blit(out,width,height,state.footer,list.x+pad+cursorWidth,p.footerY+dy+(row-state.footer.height)/2,
            list.w-2*pad-cursorWidth,state.palette,false);
    if(p.headerY>=0) {
        const int y=p.headerY+dy;
        rule(out,width,list.x+pad-2,y+row+1,list.w-2*pad+4);
        if(state.pages>1) {
            // ◀ 1/3 ▶ at the right of the header.
            int x=list.x+list.w-pad-state.pageText.width-18;
            triangle(out,width,x,y+(row-9)/2,false);x+=9;
            blit(out,width,height,state.pageText,x,y+(row-state.pageText.height)/2,state.pageText.width,state.palette,false);
            triangle(out,width,x+state.pageText.width+4,y+(row-9)/2,true);
        }
    }
    if(p.described) {
        const auto& box=p.help;const int y0=box.y+dy;
        const int costHeight=state.cost.width>0?row+4:0,textBottom=y0+box.h-costHeight;
        const auto lines=wrapBattleWords(state.help,box.w,helpSpace);
        for(std::size_t l=0;l<lines.size();++l) {
            const int y=y0+int(l)*helpRow;
            if(y+helpRow>textBottom)break;
            const int end=l+1<lines.size()?lines[l+1]:int(state.help.size());
            for(int i=lines[l],x=box.x;i<end;x+=state.help[std::size_t(i)].width+helpSpace,++i)
                blit(out,width,height,state.help[std::size_t(i)],x,y,box.x+box.w-x,state.palette,false);
        }
        if(costHeight) {
            const int y=y0+box.h-row;
            rule(out,width,box.x-2,y-3,box.w+4);
            blit(out,width,height,state.costLabel,box.x,y+(row-state.costLabel.height)/2,box.w,state.palette,false);
            blit(out,width,height,state.cost,box.x+box.w-state.cost.width,y+(row-state.cost.height)/2,state.cost.width,state.palette,false);
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
