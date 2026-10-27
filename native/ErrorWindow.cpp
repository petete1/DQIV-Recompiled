#include "ErrorWindow.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include "TextCanvas.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <vector>
#endif

namespace pc {
#ifdef _WIN32
void showErrorBox(const std::string& message) {
    std::wstring wide(std::size_t(MultiByteToWideChar(CP_UTF8,0,message.c_str(),-1,nullptr,0)),L'\0');
    MultiByteToWideChar(CP_UTF8,0,message.c_str(),-1,wide.data(),int(wide.size()));
    MessageBoxW(nullptr,wide.c_str(),L"Dragon Quest IV",MB_OK|MB_ICONERROR);
}
#else
namespace {
// Lines of at most `width` pixels: broken after a space, or between two
// Japanese characters (which have none), or anywhere in a longer word.
std::vector<std::string> wrap(TextCanvas& canvas,const std::string& text,int size,int width) {
    std::vector<std::string> lines;
    std::size_t start=0;
    while(start<=text.size()) {
        const auto end=std::min(text.find('\n',start),text.size());
        const std::string paragraph=text.substr(start,end-start);
        std::string line;
        std::size_t breakAt=std::string::npos; // in line: where it may be cut
        for(std::size_t i=0;i<paragraph.size();) {
            const unsigned char lead=paragraph[i];
            const std::size_t length=lead<0x80?1:lead<0xe0?2:lead<0xf0?3:4;
            const std::string character=paragraph.substr(i,length);i+=length;
            const bool wide=length>=3; // Japanese and other wide characters
            if(wide&&!line.empty())breakAt=line.size();
            if(!line.empty()&&canvas.measure(line+character,size)>width) {
                const auto cut=breakAt!=std::string::npos&&breakAt>0?breakAt:line.size();
                std::string rest=line.substr(cut);
                line.resize(cut);
                while(!line.empty()&&line.back()==' ')line.pop_back();
                lines.push_back(line);
                line=rest;
                const auto space=line.rfind(' ');
                breakAt=space!=std::string::npos?space+1:std::string::npos;
            }
            line+=character;
            if(character==" ")breakAt=line.size();
        }
        lines.push_back(line);
        if(end==text.size())break;
        start=end+1;
    }
    return lines;
}
bool drawnBox(const std::string& message) {
    if(!SDL_InitSubSystem(SDL_INIT_VIDEO))return false;
    struct Quit {~Quit(){SDL_QuitSubSystem(SDL_INIT_VIDEO);}} quit;
    const float scale=std::max(1.f,SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay()));
    const int size=int(std::lround(15*scale)),margin=int(std::lround(20*scale)),lineHeight=size*7/5;
    const int width=int(std::lround(560*scale)),buttonWidth=int(std::lround(96*scale)),buttonHeight=int(std::lround(30*scale));
    TextCanvas measure(1,1);
    const auto lines=wrap(measure,message,size,width-2*margin);
    const int height=margin+int(lines.size())*lineHeight+margin+buttonHeight+margin;
    TextCanvas canvas(width,height);
    constexpr std::uint32_t background=0xfff2f2f2,ink=0xff202020,button=0xffdcdcdc,edge=0xff9a9a9a;
    canvas.clear(background);
    for(std::size_t i=0;i<lines.size();++i)canvas.text(margin,margin+int(i)*lineHeight,lines[i],size,ink);
    const int buttonX=width-margin-buttonWidth,buttonY=height-margin-buttonHeight;
    canvas.fill(buttonX,buttonY,buttonWidth,buttonHeight,edge);
    canvas.fill(buttonX+1,buttonY+1,buttonWidth-2,buttonHeight-2,button);
    canvas.text(buttonX,buttonY+(buttonHeight-size*6/5)/2,"OK",size,ink,true,1,buttonWidth);

    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    if(!SDL_CreateWindowAndRenderer("Dragon Quest IV",width,height,0,&window,&renderer))return false;
    struct Close {SDL_Window* w;SDL_Renderer* r;~Close(){SDL_DestroyRenderer(r);SDL_DestroyWindow(w);}} close{window,renderer};
    SDL_Texture* texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,width,height);
    if(!texture)return false;
    SDL_UpdateTexture(texture,nullptr,canvas.pixels().data(),width*4);
    SDL_RaiseWindow(window);
    for(bool open=true;open;) {
        SDL_RenderClear(renderer);SDL_RenderTexture(renderer,texture,nullptr,nullptr);SDL_RenderPresent(renderer);
        SDL_Event event;
        if(!SDL_WaitEvent(&event))break;
        switch(event.type) {
        case SDL_EVENT_QUIT:case SDL_EVENT_WINDOW_CLOSE_REQUESTED:open=false;break;
        case SDL_EVENT_KEY_DOWN:
            if(event.key.key==SDLK_RETURN||event.key.key==SDLK_KP_ENTER||event.key.key==SDLK_ESCAPE||event.key.key==SDLK_SPACE)open=false;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if(event.button.x>=buttonX&&event.button.x<buttonX+buttonWidth&&event.button.y>=buttonY&&event.button.y<buttonY+buttonHeight)open=false;
            break;
        default:break;
        }
    }
    SDL_DestroyTexture(texture);
    return true;
}
}
void showErrorBox(const std::string& message) {
    if(!drawnBox(message))SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Dragon Quest IV",message.c_str(),nullptr);
}
#endif
}
