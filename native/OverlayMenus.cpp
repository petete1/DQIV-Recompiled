#include "OverlayMenus.hpp"
#include <algorithm>
#include <cmath>

namespace pc {
namespace {
constexpr int columns=32,rows=48,margin=8,gap=4;
// Battle: the commands and their targets at the bottom left, the lists beside them.
constexpr OverlayRule bottomLeft{-1,1,margin,margin};
bool inside(const DsRect& r,int x,int y) {return r.w>0&&x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
}

DsRect drawnArea(const MenuLayerMaps& maps) {
    int left=columns,top=rows,right=-1,bottom=-1;
    for(const std::uint16_t* map:{maps.window,maps.back,maps.icon}) {
        if(!map)continue;
        for(int y=0;y<rows;++y)for(int x=0;x<columns;++x) {
            if(!(map[y*columns+x]&1023))continue;
            left=std::min(left,x);top=std::min(top,y);right=std::max(right,x);bottom=std::max(bottom,y);
        }
    }
    if(right<0)return {};
    return {left*8,top*8,(right-left+1)*8,(bottom-top+1)*8};
}
const OverlayRule* overlayRule(OverlayMenu menu) {
    switch(menu) {
    case OverlayMenu::battleRoot:case OverlayMenu::battleCommands:case OverlayMenu::battleTarget:
    case OverlayMenu::battleSpells:case OverlayMenu::battleItems:case OverlayMenu::battleUseOrEquip:return &bottomLeft;
    default:return nullptr;
    }
}
std::vector<LayerPlacement> placeLayers(std::span<const MenuLayerMaps> layers,int windowWidth,int windowHeight,
    const std::array<ScreenPlace,2>& screens,int ruleScale) {
    std::vector<LayerPlacement> out;
    const int s=std::max(1,ruleScale);
    // The commands' width: the lists open beside them.
    int besideCommands=0;
    for(const auto& layer:layers)if(layer.menu==OverlayMenu::battleCommands)besideCommands=drawnArea(layer).w+gap;
    for(const auto& layer:layers) {
        LayerPlacement p;p.area=drawnArea(layer);
        const OverlayRule* rule=overlayRule(layer.menu);
        if(!rule) {
            const ScreenPlace& screen=screens[p.area.y>=192?1:0];
            const int top=p.area.y>=192?192:0;
            p.x=int(std::lround(screen.x+p.area.x*screen.scale));p.y=int(std::lround(screen.y+(p.area.y-top)*screen.scale));
            p.w=int(std::lround(p.area.w*screen.scale));p.h=int(std::lround(p.area.h*screen.scale));
        } else {
            p.w=p.area.w*s;p.h=p.area.h*s;
            const bool list=layer.menu==OverlayMenu::battleSpells||layer.menu==OverlayMenu::battleItems||
                layer.menu==OverlayMenu::battleUseOrEquip;
            const int offsetX=rule->offsetX+(list?besideCommands:0),w=p.area.w*s,h=p.area.h*s;
            p.x=rule->anchorX<0?offsetX*s:rule->anchorX>0?windowWidth-offsetX*s-w:(windowWidth-w)/2+offsetX*s;
            p.y=rule->anchorY<0?rule->offsetY*s:rule->anchorY>0?windowHeight-rule->offsetY*s-h:(windowHeight-h)/2+rule->offsetY*s;
        }
        out.push_back(p);
    }
    return out;
}
bool pointerOnLayer(std::span<const LayerPlacement> placements,int x,int y,int& dsX,int& dsY) {
    for(auto p=placements.rbegin();p!=placements.rend();++p) {
        const DsRect onWindow{p->x,p->y,p->w,p->h};
        if(!inside(onWindow,x,y))continue;
        dsX=p->area.x+(x-p->x)*p->area.w/p->w;dsY=p->area.y+(y-p->y)*p->area.h/p->h;
        return true;
    }
    return false;
}
int spriteLayer(std::span<const LayerPlacement> placements,int dsX,int dsY) {
    for(int i=int(placements.size())-1;i>=0;--i)if(inside(placements[i].area,dsX,dsY))return i;
    return -1;
}
std::vector<MenuSprite> menuSprites(const std::uint16_t* oamTop,const std::uint16_t* oamBottom) {
    static constexpr int widths[3][4]={{8,16,32,64},{16,32,32,64},{8,8,16,32}};
    static constexpr int heights[3][4]={{8,16,32,64},{8,8,16,32},{16,32,32,64}};
    std::vector<MenuSprite> out;
    const std::uint16_t* screens[2]={oamTop,oamBottom};
    for(int screen=0;screen<2;++screen) {
        const std::uint16_t* oam=screens[screen];
        if(!oam)continue;
        for(int entry=0;entry<128;++entry) {
            const unsigned a=oam[entry*4],b=oam[entry*4+1];
            const bool affine=(a&256)!=0;
            if((!affine&&(a&512))||(a>>14)==3)continue;
            const int w=widths[a>>14][b>>14],h=heights[a>>14][b>>14];
            const int y=(a&255)>=192?int(a&255)-256:int(a&255),x=(b&511)>=256?int(b&511)-512:int(b&511);
            if(y+h<=0||y>=192||x+w<=0||x>=256)continue;
            out.push_back({screen,entry,x+w/2,screen*192+y+h/2});
        }
    }
    return out;
}
OverlayMenuState& overlayMenuState() {static OverlayMenuState state;return state;}
}
