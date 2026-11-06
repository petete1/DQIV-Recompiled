#include "OverlayMenus.hpp"
#include <algorithm>

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
            if(!map[y*columns+x])continue;
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
    int dsOriginX,int dsOriginY,int dsScale) {
    std::vector<LayerPlacement> out;
    const int s=std::max(1,dsScale);
    // The commands' width: the lists open beside them.
    int besideCommands=0;
    for(const auto& layer:layers)if(layer.menu==OverlayMenu::battleCommands)besideCommands=drawnArea(layer).w+gap;
    for(const auto& layer:layers) {
        LayerPlacement p;p.area=drawnArea(layer);p.scale=s;
        const OverlayRule* rule=overlayRule(layer.menu);
        if(!rule) {
            p.x=dsOriginX+p.area.x*s;p.y=dsOriginY+p.area.y*s;
        } else {
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
        const DsRect onWindow{p->x,p->y,p->area.w*p->scale,p->area.h*p->scale};
        if(!inside(onWindow,x,y))continue;
        dsX=p->area.x+(x-p->x)/p->scale;dsY=p->area.y+(y-p->y)/p->scale;
        return true;
    }
    return false;
}
int spriteLayer(std::span<const LayerPlacement> placements,int dsX,int dsY) {
    for(int i=int(placements.size())-1;i>=0;--i)if(inside(placements[i].area,dsX,dsY))return i;
    return -1;
}
OverlayMenuState& overlayMenuState() {static OverlayMenuState state;return state;}
}
