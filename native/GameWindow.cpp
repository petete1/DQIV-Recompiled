#include "GameWindow.hpp"
#include "BattleWindows.hpp"
#include "Display.hpp"
#include "NitroVideo.hpp"
#include "DebugTools.hpp"
#include "AudioOutput.hpp"
#include "GlLoader.hpp"
#include "Input.hpp"
#include "DualScreen.hpp"
#include "Profile.hpp"
#include "Settings.hpp"
#include "Achievements.hpp"
#include "HdPictures.hpp"
#include "Version.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace pc {
namespace {
void require(bool ok) {if(!ok)throw std::runtime_error(SDL_GetError());}
enum Overlay : unsigned {overlayFps=1,overlayToast=2,overlayDebug=3,overlayVersion=4,overlayBattle=5,overlayMenu0=6,overlayBattleParty=30,overlayBattleMessage=31,overlayBattlePlate0=32,overlayBattleGold=36}; // overlayMenu0..+17: menu layers (a picture per screen part), +16 and +17: the sprites of no layer
// The achievement notification: slides in, stays, slides out (nanoseconds).
constexpr std::uint64_t toastSlide=350'000'000,toastShown=5'000'000'000;
}
std::vector<std::uint32_t> decodePng(const std::vector<std::uint8_t>& data,int& width,int& height) {
    std::vector<std::uint32_t> pixels;
    if(data.empty())return pixels;
    SDL_Surface* loaded=SDL_LoadPNG_IO(SDL_IOFromConstMem(data.data(),data.size()),true);
    if(!loaded)return pixels;
    if(SDL_Surface* surface=SDL_ConvertSurface(loaded,SDL_PIXELFORMAT_ARGB8888)) {
        width=surface->w;height=surface->h;pixels.resize(std::size_t(width)*height);
        for(int row=0;row<height;++row)
            std::memcpy(pixels.data()+std::size_t(row)*width,static_cast<const std::uint8_t*>(surface->pixels)+std::size_t(row)*surface->pitch,std::size_t(width)*4);
        SDL_DestroySurface(surface);
    }
    SDL_DestroySurface(loaded);
    return pixels;
}
namespace {
// Trigger travel (0..32767) past which it counts as pressed.
constexpr int triggerThreshold=16000;
}

GameWindow::GameWindow(bool hidden,bool mute):hidden_(hidden) {
    require(SDL_InitSubSystem(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD));
    try {
        require(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3));
        require(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3));
        require(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE));
        require(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1));
        window_=SDL_CreateWindow("Dragon Quest IV",1280,720,
            SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|(hidden?SDL_WINDOW_HIDDEN:0));
        require(window_!=nullptr);
        context_=SDL_GL_CreateContext(window_);
        require(context_!=nullptr);
        require(SDL_GL_MakeCurrent(window_,static_cast<SDL_GLContext>(context_)));
        SDL_GL_SetSwapInterval(1);
        gl::load();
        gpu_=std::make_unique<GpuRenderer>();
        std::printf("OpenGL: %s / %s\n",reinterpret_cast<const char*>(gl::GetString(GL_RENDERER)),reinterpret_cast<const char*>(gl::GetString(GL_VERSION)));
        if(!mute) {audio_=std::make_unique<AudioOutput>();audio_->pause(false);}
        int count=0;auto* ids=SDL_GetGamepads(&count);if(count>0)gamepad_=SDL_OpenGamepad(ids[0]);SDL_free(ids);
        deadline_=fpsStart_=SDL_GetTicksNS();
    } catch(...) {
        audio_.reset();gpu_.reset();
        if(context_)SDL_GL_DestroyContext(static_cast<SDL_GLContext>(context_));
        SDL_DestroyWindow(window_);
        SDL_QuitSubSystem(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD);throw;
    }
}
GameWindow::~GameWindow() {
    setButtons(0);setTouch(false,0,0);audio_.reset();SDL_CloseGamepad(gamepad_);
    gpu_.reset();
    SDL_GL_DestroyContext(static_cast<SDL_GLContext>(context_));SDL_DestroyWindow(window_);
    SDL_QuitSubSystem(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD);
}
void GameWindow::setFullscreen(bool on) {
    SDL_SetWindowFullscreen(window_,on);SDL_SyncWindow(window_);
}
void GameWindow::applySettings() {
    const auto& s=gameSettings();
    settingsUsed_=true;
    if(!s.fullscreen) {
        setFullscreen(false);
        fitWindowToPicture(windowSizes[s.windowSize][1]);
        SDL_SetWindowPosition(window_,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
    } else setFullscreen(true);
    // Strict vertical sync: the adaptive one (-1) shows a frame that misses
    // the refresh at once, and the game's own 60 Hz pace makes that steady,
    // a tear across the middle of the screen.
    SDL_GL_SetSwapInterval(verticalSync(s)?1:0);
    int interval=0;SDL_GL_GetSwapInterval(&interval);
    std::printf("Vertical sync: %d\n",interval);
    updatePictureWidth();
    stretch_=s.stretch;
    gpu_->settings.autoQuality=s.quality==0;
    gpu_->settings.supersample=s.quality==0?4:s.quality;
    gpu_->settings.smoothTextures=s.smoothTextures;
    // Textures not smoothed: the DS pixels as they are, without Scale2x.
    gpu_->settings.textureScale=s.smoothTextures?4:1;
    const int rate=frameRates[std::clamp(s.frameRate,0,frameRateCount-1)];
    gpu_->settings.frameMilliseconds=1000.0/(rate?rate:240);
    fpsChanged_=true;
}
void GameWindow::drawableSize(int& width,int& height) const {require(SDL_GetWindowSizeInPixels(window_,&width,&height));}
void GameWindow::canvasPixels(const GpuLayout& layout,float& width,float& height) const {
    int w=0,h=0;drawableSize(w,h);
    if(stretch_) {width=float(w);height=float(h);return;}
    const float scale=std::min(w/layout.canvasWidth,h/layout.canvasHeight);
    width=layout.canvasWidth*scale;height=layout.canvasHeight*scale;
}
// Windowed: the window takes the picture's shape at this height (16:9 wide,
// the DS screens' 2:3), so there are no bars beside it. A maximized window
// keeps the size the desktop gave it.
void GameWindow::fitWindowToPicture(int height) {
    if(gameSettings().fullscreen||(SDL_GetWindowFlags(window_)&SDL_WINDOW_MAXIMIZED))return;
    SDL_SetWindowSize(window_,gameSettings().wide?height*16/9:height*2/3,height);
}
void GameWindow::updatePictureWidth() {
    // The two DS screens are 384 pixels tall: the wide picture takes the
    // window's (or the screen's) shape around them, from 4:3 to 2.4:1 (the
    // "21:9" screens: 2560x1080, 3440x1440; the DS world was not drawn wider:
    // bars beyond), without stretching.
    if(!gameSettings().wide) {dualScreen().canvasWidth=256.f;return;}
    int w=0,h=0;SDL_GetWindowSizeInPixels(window_,&w,&h);
    const float shape=h>0?std::clamp(float(w)/float(h),4.f/3.f,2.4f):16.f/9.f;
    dualScreen().canvasWidth=384*shape;
}
bool GameWindow::poll() {
    // Settings changed in the game's own menu.
    if(takeSettingsApply())applySettings();
    // The game's own way out (its "turn the power off" screens, its menu).
    if(takeQuitRequest())running_=false;
    SDL_Event e;
    while(SDL_PollEvent(&e)) {
        if(e.type==SDL_EVENT_QUIT)running_=false;
        // Full screen, another window size: the wide picture follows its shape.
        if(e.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)updatePictureWidth();
        if(e.type==SDL_EVENT_MOUSE_WHEEL&&e.wheel.y!=0) {
            // The mouse wheel zooms the camera.
            zoomCamera(e.wheel.y>0?10:-10);
            if(settingsUsed_)saveSettings();
        }

        if(e.type==SDL_EVENT_GAMEPAD_ADDED&&!gamepad_)gamepad_=SDL_OpenGamepad(e.gdevice.which);
        if(e.type==SDL_EVENT_GAMEPAD_REMOVED&&gamepad_&&SDL_GetGamepadID(gamepad_)==e.gdevice.which) {
            SDL_CloseGamepad(gamepad_);gamepad_=nullptr;
        }
        if(e.type==SDL_EVENT_KEY_DOWN&&rebindWaiting()) {
            // A control being bound: Escape cancels, a key binds the keyboard page.
            if(!e.key.repeat) {
                if(e.key.key==SDLK_ESCAPE)cancelRebind();
                else if(rebindPage()==pageKeyboard) {
                    gameSettings().keys[rebindAction()]=static_cast<unsigned>(e.key.key);
                    cancelRebind();saveSettings();suppressInput_=true;
                }
            }
            continue;
        }
        if(e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN&&rebindWaiting()) {
            if(rebindPage()==pageGamepad) {
                gameSettings().pads[rebindAction()]=e.gbutton.button;
                cancelRebind();saveSettings();suppressInput_=true;
            }
            continue;
        }
        // A trigger pressed past half its travel binds like a button.
        if(e.type==SDL_EVENT_GAMEPAD_AXIS_MOTION&&rebindWaiting()) {
            const bool trigger=e.gaxis.axis==SDL_GAMEPAD_AXIS_LEFT_TRIGGER||e.gaxis.axis==SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
            if(rebindPage()==pageGamepad&&trigger&&e.gaxis.value>triggerThreshold) {
                gameSettings().pads[rebindAction()]=e.gaxis.axis==SDL_GAMEPAD_AXIS_LEFT_TRIGGER?padLeftTrigger:padRightTrigger;
                cancelRebind();saveSettings();suppressInput_=true;
            }
            continue;
        }
        if(e.type==SDL_EVENT_KEY_DOWN) {
            const auto key=e.key.key;
            if(e.key.repeat)continue;
            auto& s=gameSettings();
            if(key==SDLK_F11) {s.fullscreen=!s.fullscreen;setFullscreen(s.fullscreen);if(settingsUsed_)saveSettings();}
            if(key==SDLK_F10) {s.stretch=stretch_=!stretch_;if(settingsUsed_)saveSettings();}
            if(key==SDLK_F12)screenshot_=true;
            // Camera: keypad + / - zoom.
            if(key==SDLK_KP_PLUS||key==SDLK_KP_MINUS) {zoomCamera(key==SDLK_KP_PLUS?10:-10);if(settingsUsed_)saveSettings();}
            if(key==SDLK_F8) {
                // The wide picture, or the DS screens' own width.
                s.wide=!s.wide;
                int w=0,h=0;SDL_GetWindowSize(window_,&w,&h);
                fitWindowToPicture(h);
                updatePictureWidth();
                if(settingsUsed_)saveSettings();
            }
            if(key==SDLK_F9) {
                s.windowSize=(s.windowSize+1)%windowSizeCount;s.fullscreen=false;
                setFullscreen(false);
                fitWindowToPicture(windowSizes[s.windowSize][1]);
                if(settingsUsed_)saveSettings();
            }
        }
    }
    std::uint16_t buttons=0;bool partyChat=false,fast=false;
    const auto* keys=SDL_GetKeyboardState(nullptr);
    const auto& controls=gameSettings();
    auto held=[&](SDL_Keycode key){const auto scan=SDL_GetScancodeFromKey(key,nullptr);return scan!=SDL_SCANCODE_UNKNOWN&&keys[scan];};
    auto bound=[&](SDL_Keycode key){for(const auto k:controls.keys)if(k==static_cast<unsigned>(key))return true;return false;};
    const bool focus=(SDL_GetWindowFlags(window_)&SDL_WINDOW_INPUT_FOCUS)!=0;
    if(focus) {
        // The player's keys (settings), with Space for A and Backspace and
        // Escape for B while they are bound to nothing else.
        for(int i=0;i<buttonControlCount;++i)if(held(static_cast<SDL_Keycode>(controls.keys[i])))buttons|=1u<<controlBits[i];
        partyChat=held(static_cast<SDL_Keycode>(controls.keys[controlPartyChat]));
        fast=held(static_cast<SDL_Keycode>(controls.keys[controlFastForward]));
        if(!bound(SDLK_SPACE)&&keys[SDL_SCANCODE_SPACE])buttons|=1;
        if(!bound(SDLK_BACKSPACE)&&keys[SDL_SCANCODE_BACKSPACE])buttons|=2;
        if(!bound(SDLK_ESCAPE)&&keys[SDL_SCANCODE_ESCAPE])buttons|=2;
    }
    if(gamepad_&&focus) {
        // A bound button, or a trigger pressed past half its travel.
        auto pressed=[&](int pad) {
            if(pad==padLeftTrigger)return SDL_GetGamepadAxis(gamepad_,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)>triggerThreshold;
            if(pad==padRightTrigger)return SDL_GetGamepadAxis(gamepad_,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)>triggerThreshold;
            return SDL_GetGamepadButton(gamepad_,static_cast<SDL_GamepadButton>(pad));
        };
        for(int i=0;i<buttonControlCount;++i)if(pressed(controls.pads[i]))buttons|=1u<<controlBits[i];
        if(pressed(controls.pads[controlPartyChat]))partyChat=true;
        if(pressed(controls.pads[controlFastForward]))fast=true;
        // The left stick moves too.
        const int x=SDL_GetGamepadAxis(gamepad_,SDL_GAMEPAD_AXIS_LEFTX),y=SDL_GetGamepadAxis(gamepad_,SDL_GAMEPAD_AXIS_LEFTY);
        if(x>16000)buttons|=16;if(x<-16000)buttons|=32;if(y<-16000)buttons|=64;if(y>16000)buttons|=128;
        // The right stick, up and down, zooms the camera.
        const int zoom=SDL_GetGamepadAxis(gamepad_,SDL_GAMEPAD_AXIS_RIGHTY);
        if(zoom<-16000||zoom>16000) {zoomCamera(zoom<0?1:-1);stickZoom_=true;}
        else if(stickZoom_) {stickZoom_=false;if(settingsUsed_)saveSettings();}
        // Left and right, it turns the camera as R and L do (in towns, while held).
        const int turn=SDL_GetGamepadAxis(gamepad_,SDL_GAMEPAD_AXIS_RIGHTX);
        if(turn>16000)buttons|=256;if(turn<-16000)buttons|=512;
    }
    // While a control is being bound, and until the key that bound it is
    // released, the game gets no input.
    if(rebindWaiting()) {buttons=0;partyChat=false;fast=false;}
    else if(suppressInput_) {if(buttons)buttons=0;else suppressInput_=false;partyChat=false;fast=false;}
    setFastForward(fast);
    setPartyChatRequest(partyChat&&!partyChatHeld_);partyChatHeld_=partyChat;
    // Touch: the bottom DS screen, centred at the foot of the canvas.
    float mouseX=0,mouseY=0;const auto mouse=SDL_GetMouseState(&mouseX,&mouseY);
    int width=0,height=0;require(SDL_GetWindowSize(window_,&width,&height));
    float canvasWidth=float(width),canvasHeight=float(height);
    if(!stretch_) {
        const float scale=std::min(width/layout_.canvasWidth,height/layout_.canvasHeight);
        canvasWidth=layout_.canvasWidth*scale;canvasHeight=layout_.canvasHeight*scale;
    }
    float x=(mouseX-(width-canvasWidth)/2)/std::max(canvasWidth,1.f)*layout_.canvasWidth-(layout_.canvasWidth-256)/2;
    float y=(mouseY-(height-canvasHeight)/2)/std::max(canvasHeight,1.f)*layout_.canvasHeight;
    if(layout_.placed) {
        // The bottom screen's own rectangle.
        const auto& r=layout_.screens[1];
        const float cx=x+(layout_.canvasWidth-256)/2;
        x=r[2]>0?(cx-r[0])/r[2]*256:-1;y=r[3]>0?(y-r[1])/r[3]*192+192:-1;
    }
    const bool down=focus&&(mouse&SDL_BUTTON_LMASK)&&x>=0&&x<256&&y>=192&&y<384;
    // Classic battle windows take the mouse (window pixels); the DS touch
    // screen gets nothing while they show, not even over the battle scene.
    int pixelWidth=0,pixelHeight=0;drawableSize(pixelWidth,pixelHeight);
    const std::uint32_t pressed=focus?mouse&~mouseButtons_:0;mouseButtons_=mouse;
    const int pointerX=int(mouseX*pixelWidth/std::max(width,1)),pointerY=int(mouseY*pixelHeight/std::max(height,1));
    setBattlePointer(pointerX,pointerY,(pressed&SDL_BUTTON_LMASK)!=0,(pressed&SDL_BUTTON_RMASK)!=0);
    const bool shown=battleWindowsShown();
    buttons|=battleMouse_.poll(shown,(pressed&SDL_BUTTON_LMASK)!=0,(pressed&SDL_BUTTON_RMASK)!=0,focus&&(mouse&SDL_BUTTON_LMASK),
        battleClickAt(pointerX,pointerY)>=0);
    if(gameSettings().battleMenus==2&&overlayMenuState().active) {
        // Overlay menus: the touch goes through the layer under the pointer, at
        // its DS place (the game's touch areas), and nowhere else.
        // A tap acts where it is released: the last place touched stays.
        int dsX=0,dsY=0;
        const bool on=overlayTouch(menuPlacements_,menuScreens_,pointerX,pointerY,dsX,dsY);
        if(on) {menuTouchX_=dsX;menuTouchY_=dsY-192;}
        setTouch(focus&&(mouse&SDL_BUTTON_LMASK)&&on,menuTouchX_,menuTouchY_);
    } else setTouch(down&&!battleMouse_.touchBlocked(shown),static_cast<int>(x),static_cast<int>(y)-192);
    setButtons(buttons);return running_;
}
static int gapPixels(int scale) {return 4*scale;}
void GameWindow::drawBattleWindows(int width,int height) {
    const auto& state=battleMenuState();
    if(!classicBattleMenusOn(gameSettings())) {rememberBattleLayout({});return;}
    // The party panel, whenever the battle shows its status (small: drawn anew each time).
    const auto party=layoutBattleParty(state,width,height);
    if(party.picture.w>0) {
        // The active member's card slides out (and back) over about an eighth of a second;
        // the backgrounds fade in and out over a quarter.
        const auto now=SDL_GetTicksNS();
        const float step=cardTime_?float(double(now-cardTime_)/125e6):0.f;
        cardTime_=now;
        auto toward=[](float& value,float target,float step){value=target>value?std::min(target,value+step):std::max(target,value-step);};
        for(std::size_t i=0;i<cardSlides_.size();++i) {
            const bool shown=i<state.party.size();
            toward(cardSlides_[i],shown&&state.party[i].active?1.f:0.f,step);
            toward(cardBacks_[i],shown&&state.party[i].ready?1.f:0.f,step/2);
        }
        const auto pixels=composeBattleParty(state,party,cardSlides_,cardBacks_);
        const int s=party.scale;
        gpu_->drawOverlay(overlayBattleParty,pixels,party.picture.w/s,party.picture.h/s,true,
            party.picture.x,party.picture.y,width,height,party.picture.w,party.picture.h);
    }
    // The monster name plates, where the battle places them over the scene (its DS pixel size).
    if(!state.plates.empty()) {
        float canvasWidth=0,canvasHeight=0;canvasPixels(layout_,canvasWidth,canvasHeight);
        const float k=canvasHeight/layout_.canvasHeight;
        const float originX=(width-canvasWidth)/2+(layout_.canvasWidth-256)/2*k,originY=(height-canvasHeight)/2+192*k;
        for(std::size_t i=0;i<state.plates.size()&&i<4;++i) {
            int w=0,h=0;
            const auto pixels=composeBattlePlate(state.plates[i],state,w,h);
            gpu_->drawOverlay(overlayBattlePlate0+unsigned(i),pixels,w,h,true,int(std::lround(originX+state.plates[i].x*k)),
                int(std::lround(originY+state.plates[i].y*k)),width,height,int(std::lround(w*k)),int(std::lround(h*k)));
        }
    }
    // The message window, above the command panel when both show.
    auto message=layoutBattleMessage(state,width,height);
    if(message.picture.w>0) {
        if(state.kind!=BattleMenuKind::none&&!state.field) {
            const auto menu=layoutBattleMenu(state,width,height);
            if(!menu.windows.empty())message.picture.y=std::min(message.picture.y,menu.picture.y-message.picture.h-gapPixels(message.scale));
        }
        const auto pixels=composeBattleMessage(state,message,int(SDL_GetTicks()*60/1000));
        const int s=message.scale;
        gpu_->drawOverlay(overlayBattleMessage,pixels,message.picture.w/s,message.picture.h/s,true,
            message.picture.x,message.picture.y,width,height,message.picture.w,message.picture.h);
    }
    if(state.kind==BattleMenuKind::none) {rememberBattleLayout({});return;}
    // The field menu's gold, at the top right.
    const auto gold=layoutBattleGold(state,width,height);
    if(gold.picture.w>0) {
        const int s=gold.scale;
        gpu_->drawOverlay(overlayBattleGold,composeBattleGold(state,gold),gold.picture.w/s,gold.picture.h/s,true,
            gold.picture.x,gold.picture.y,width,height,gold.picture.w,gold.picture.h);
    }
    const auto layout=layoutBattleMenu(state,width,height);
    rememberBattleLayout(layout);
    if(layout.windows.empty())return;
    // The cursor blinks at the game's 60 frames per second: on 20, off 10.
    const int blinkFrame=int(SDL_GetTicks()*60/1000);
    const bool blink=blinkFrame%30<20;
    const bool changed=battlePicture_.empty()||state.revision!=battleRevision_||width!=battleWidth_||height!=battleHeight_||blink!=battleBlink_;
    if(changed) {
        battlePicture_=composeBattleMenu(state,layout,blinkFrame);
        battleRevision_=state.revision;battleWidth_=width;battleHeight_=height;battleBlink_=blink;
    }
    const int s=layout.scale;
    gpu_->drawOverlay(overlayBattle,battlePicture_,layout.picture.w/s,layout.picture.h/s,changed,
        layout.picture.x,layout.picture.y,width,height,layout.picture.w,layout.picture.h);
}
void GameWindow::drawOverlayMenus(int width,int height) {
    const auto& state=overlayMenuState();
    if(gameSettings().battleMenus!=2||!state.active) {menuPlacements_.clear();return;}
    // Where the presented picture shows each DS screen (as the presenter
    // draws them: stacked in the canvas centre, placed, or the top one smaller).
    float canvasWidth=0,canvasHeight=0;canvasPixels(layout_,canvasWidth,canvasHeight);
    const float k=canvasHeight/layout_.canvasHeight,cx=(width-canvasWidth)/2,cy=(height-canvasHeight)/2;
    std::array<ScreenPlace,2> screens;
    for(int screen=0;screen<2;++screen) {
        const auto& r=layout_.screens[screen];
        screens[screen]=layout_.placed&&r[2]>0?ScreenPlace{cx+r[0]*k,cy+r[1]*k,r[2]/256*k}:
            ScreenPlace{cx+(layout_.canvasWidth-256)/2*k,cy+screen*192*k,k};
    }
    if(!layout_.placed&&layout_.topScale<1)
        screens[0]={cx+(layout_.canvasWidth-256*layout_.topScale)/2*k,cy,layout_.topScale*k};
    menuScreens_=screens;
    menuPlacements_=placeLayers(std::span(state.layers,state.count),width,height,screens,std::max(1,int(std::lround(k))));
    // Each sprite with the layer whose drawn area holds it; the rest at the DS place.
    std::vector<std::vector<MenuSprite>> sprites(state.count+1);
    for(const auto& sprite:menuSprites(state.oam[0],state.oam[1])) {
        const int layer=spriteLayer(menuPlacements_,sprite.centreX,sprite.centreY);
        sprites[layer<0?state.count:layer].push_back(sprite);
    }
    const std::span<const std::uint16_t> top(state.oam[0],state.oam[0]?512:0),bottom(state.oam[1],state.oam[1]?512:0);
    auto& video=activeVideo();
    // The port's HD portraits (placed over the DS screens, no DS pixels of their own), likewise.
    std::vector<std::vector<HdPlacedPicture>> portraits(state.count+1);
    for(int screen=0;screen<2;++screen)for(auto& p:placedHdPictures(screen,video.palette().subspan(0,0x200))) {
        const int layer=spriteLayer(menuPlacements_,p.x+p.width/2,screen*192+p.y+p.height/2);
        p.y+=screen*192;
        portraits[layer<0?state.count:layer].push_back(std::move(p));
    }
    std::vector<std::uint32_t> crop;
    auto draw=[&](unsigned id,const std::vector<std::uint32_t>& picture,const DsRect& area,int x,int y,int w,int h) {
        if(area.w<=0||area.h<=0)return;
        crop.resize(std::size_t(area.w)*area.h);
        for(int row=0;row<area.h;++row)std::copy_n(picture.data()+(area.y+row)*256+area.x,area.w,crop.data()+std::size_t(row)*area.w);
        gpu_->drawOverlay(id,crop,area.w,area.h,true,x,y,width,height,w,h);
    };
    for(int i=0;i<=state.count;++i) {
        const bool loose=i==state.count;
        if(loose&&sprites[i].empty()&&portraits[i].empty())break;
        const MenuLayerMaps maps=loose?MenuLayerMaps{}:state.layers[i];
        auto picture=Display::menuLayerPicture(video,maps,state.windowBg,state.backBg,state.iconBg,top,bottom,sprites[i]);
        for(const auto& p:portraits[i])
            for(int y=std::max(0,p.y);y<std::min(384,p.y+p.height);++y)for(int x=std::max(0,p.x);x<std::min(256,p.x+p.width);++x) {
                const auto colour=p.pixels[std::size_t((y-p.y)*p.size/p.height)*p.size+(x-p.x)*p.size/p.width];
                if(colour>>24)picture[std::size_t(y)*256+x]=colour;
            }
        if(!loose&&menuPlacements_[i].moved) {
            const auto& p=menuPlacements_[i];
            draw(overlayMenu0+i*2,picture,p.area,p.x,p.y,p.w,p.h);
            continue;
        }
        // A menu left at its DS place, and what no layer holds: each screen's
        // part where the presenter shows that screen.
        const DsRect area=loose?DsRect{0,0,256,384}:menuPlacements_[i].area;
        for(int screen=0;screen<2;++screen) {
            const int top=std::max(area.y,screen*192),bottom=std::min(area.y+area.h,screen*192+192);
            if(bottom<=top)continue;
            const auto& place=screens[screen];
            const DsRect part{area.x,top,area.w,bottom-top};
            draw(overlayMenu0+(loose?16:i*2)+screen,picture,part,int(std::lround(place.x+part.x*place.scale)),
                int(std::lround(place.y+(part.y-screen*192)*place.scale)),int(std::lround(part.w*place.scale)),int(std::lround(part.h*place.scale)));
        }
    }
}
void GameWindow::drawInterface(int width,int height) {
    drawOverlayMenus(width,height);
    drawBattleWindows(width,height);
    const float scale=std::max(0.6f,height/720.f);
    if(gameSettings().showFps&&settingsUsed_) {
        const int supersample=gpu_->supersample();
        const bool rebuild=fpsChanged_||supersample!=supersampleShown_||!fpsPicture_||fpsPicture_->height()!=int(std::lround(30*scale));
        if(rebuild) {
            supersampleShown_=supersample;
            const std::string text=std::to_string(fpsShown_)+" FPS  "+std::to_string(supersample)+"x"+(gpu_->settings.autoQuality?" (auto)":"");
            const int size=int(std::lround(18*scale));
            const int textWidth=TextCanvas(1,1).measure(text,size,true);
            fpsPicture_=std::make_unique<TextCanvas>(textWidth+int(std::lround(20*scale)),int(std::lround(30*scale)));
            fpsPicture_->clear(0xa0000000);
            fpsPicture_->text(0,int(std::lround(4*scale)),text,size,
                fpsShown_>=58?0xff8ef08e:fpsShown_>=45?0xfff0d070:0xfff07a6a,true,1,fpsPicture_->width());
            fpsChanged_=false;
        }
        gpu_->drawOverlay(overlayFps,fpsPicture_->pixels(),fpsPicture_->width(),fpsPicture_->height(),rebuild,
            int(std::lround(10*scale)),int(std::lround(10*scale)),width,height);
    }
    // Debug menu: the position overlay, below the FPS.
    if(const auto debug=debugOverlayText();!debug.empty()) {
        std::vector<std::string> lines;
        for(std::size_t start=0;start<debug.size();) {
            const auto end=debug.find('\n',start);
            lines.push_back(debug.substr(start,end==std::string::npos?std::string::npos:end-start));
            if(end==std::string::npos)break;
            start=end+1;
        }
        const int size=int(std::lround(14*scale)),line=size*5/4;
        if(!debugPicture_||debugPicture_->height()!=int(lines.size())*line+size/2)
            debugPicture_=std::make_unique<TextCanvas>(int(std::lround(330*scale)),int(lines.size())*line+size/2);
        debugPicture_->clear(0xa0000000);
        for(std::size_t i=0;i<lines.size();++i)
            debugPicture_->text(size/2,size/4+int(i)*line,lines[i],size,0xffe8e8e8,false,0,debugPicture_->width()-size);
        gpu_->drawOverlay(overlayDebug,debugPicture_->pixels(),debugPicture_->width(),debugPicture_->height(),true,
            int(std::lround(10*scale)),int(std::lround(46*scale)),width,height);
    }
    if(versionShown_) {
        const int size=int(std::lround(14*scale)),margin=int(std::lround(10*scale));
        const bool rebuild=!versionPicture_||versionPicture_->height()!=size*3/2;
        if(rebuild) {
            const int textWidth=TextCanvas(1,1).measure(version(),size);
            versionPicture_=std::make_unique<TextCanvas>(textWidth+size,size*3/2);
            versionPicture_->clear(0xa0000000);
            versionPicture_->text(size/2,size/4,version(),size,0xffb0b0b0);
        }
        gpu_->drawOverlay(overlayVersion,versionPicture_->pixels(),versionPicture_->width(),versionPicture_->height(),rebuild,
            width-versionPicture_->width()-margin,height-versionPicture_->height()-margin,width,height);
    }
    drawToast(width,height,scale);
}
void GameWindow::buildToast(float scale) {
    const int language=gameSettings().language;
    const auto px=[&](float value) {return int(std::lround(value*scale));};
    const int w=px(340),h=px(64),border=std::max(1,px(2)),badge=px(48),margin=px(8);
    toastPicture_=std::make_unique<TextCanvas>(w,h);
    auto& picture=*toastPicture_;
    picture.clear(0);
    // A dark window with a golden frame, like the game's.
    picture.fill(0,0,w,h,0xffd9b44a);
    picture.fill(border,border,w-2*border,h-2*border,0xf0101a33);
    if(const auto pixels=achievementBadge(toast_,false)) {
        picture.image(margin,margin,badge,badge,pixels->data(),hdCellSize,hdCellSize);
    } else {
        // No badge in the resources: a golden medal.
        picture.fill(margin,margin,badge,badge,0xff6b4a12);
        picture.fill(margin+border,margin+border,badge-2*border,badge-2*border,0xffd9b44a);
        picture.text(margin,margin+badge/2-px(14),"\xe2\x98\x85",px(26),0xff6b4a12,true,1,badge);
    }
    const int textX=margin*2+badge,textWidth=w-textX-margin;
    picture.text(textX,px(7),achievementUnlockedBanner(language),px(11),0xffffd75e,true,0,textWidth);
    picture.text(textX,px(20),achievementTitle(toast_,language),px(15),0xffffffff,true,0,textWidth);
    picture.text(textX,px(40),achievementDescription(toast_,language),px(11),0xffc8cede,false,0,textWidth);
}
void GameWindow::drawToast(int width,int height,float scale) {
    const auto now=SDL_GetTicksNS();
    if(toast_<0) {
        toast_=takeAchievementToast();
        if(toast_<0)return;
        toastStart_=now;
        buildToast(scale);
    }
    const auto elapsed=now-toastStart_;
    if(elapsed>=toastShown) {toast_=-1;toastPicture_.reset();return;}
    // Slides up from below the window's bottom right corner, then back down.
    const float in=std::min(1.f,float(elapsed)/toastSlide),out=std::min(1.f,float(toastShown-elapsed)/toastSlide);
    const float shown=std::min(in,out),eased=1-(1-shown)*(1-shown);
    const auto& picture=*toastPicture_;
    toastFaded_.resize(picture.pixels().size());
    const unsigned alpha=unsigned(std::lround(255*shown));
    for(std::size_t i=0;i<toastFaded_.size();++i) {
        const auto p=picture.pixels()[i];
        std::uint32_t faded=0;
        for(unsigned shift:{0u,8u,16u,24u})faded|=(((p>>shift)&255)*alpha/255)<<shift;
        toastFaded_[i]=faded;
    }
    const int x=width-picture.width()-int(std::lround(16*scale));
    const int y=height-int(std::lround((picture.height()+16*scale)*eased));
    gpu_->drawOverlay(overlayToast,toastFaded_,picture.width(),picture.height(),true,x,y,width,height);
}
void GameWindow::present(const GpuLayout& layout,std::span<const std::uint32_t> recipes) {
    layout_=layout;recipes_=recipes;
    int width=0,height=0;drawableSize(width,height);
    {ProfileScope timing("present-compose");gpu_->present(layout,recipes,width,height,stretch_);}
    if(screenshot_) {
        // F12: PNG beside the save file.
        screenshot_=false;
        int w=0,h=0;auto pixels=gpu_->readPresented(w,h);
        if(auto* surface=SDL_CreateSurfaceFrom(w,h,SDL_PIXELFORMAT_ARGB8888,pixels.data(),w*4)) {
            if(char* base=SDL_GetPrefPath(nullptr,"DQIV-PC")) {
                const auto path=std::string(base)+"capture-"+std::to_string(SDL_GetTicks())+".png";
                SDL_SavePNG(surface,path.c_str());SDL_free(base);
            }
            SDL_DestroySurface(surface);
        }
    }
    // Frames per second, counted over each second.
    ++fpsFrames_;
    const auto now=SDL_GetTicksNS();
    if(now-fpsStart_>=1'000'000'000) {
        const unsigned shown=unsigned(std::lround(fpsFrames_*1e9/double(now-fpsStart_)));
        if(shown!=fpsShown_) {fpsShown_=shown;fpsChanged_=true;}
        fpsFrames_=0;fpsStart_=now;
    }
    drawInterface(width,height);
    ProfileScope timing("present-swap");
    require(SDL_GL_SwapWindow(window_));
}
void GameWindow::audio(std::span<const std::int16_t> samples) {
    if(!audio_)return;
    const int volume=settingsUsed_?gameSettings().volume:100;
    if(volume>=100) {audio_->write(samples);return;}
    std::vector<std::int16_t> scaled(samples.begin(),samples.end());
    for(auto& s:scaled)s=static_cast<std::int16_t>(s*volume/100);
    audio_->write(scaled);
}
int GameWindow::frameRateTarget() const {return frameRates[std::clamp(gameSettings().frameRate,0,frameRateCount-1)];}
void GameWindow::measureBetween(std::uint64_t cost) {
    betweenCost_=std::clamp<std::uint64_t>((betweenCost_*7+cost)/8,500'000,8'000'000);
}
bool GameWindow::interpolating() const {
    if(hidden_||!interpolator_||frameRateTarget()==60)return false;
    // With vertical sync, the screen's own rate bounds the frames shown.
    if(!verticalSync(gameSettings()))return true;
    const auto* mode=SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window_));
    return mode&&mode->refresh_rate>61;
}
void GameWindow::pace() {
    if(hidden_)return; // diagnostics run as fast as possible
    constexpr std::uint64_t period=1'000'000'000/60;
    const auto start=deadline_;
    deadline_+=period;
    if(interpolating()) {
        // Frames between the game's, as often as the chosen rate allows and
        // as long as one more fits before the next game frame.
        const int target=frameRateTarget();
        const std::uint64_t step=target?1'000'000'000ull/std::uint64_t(target):0;
        // The frames keep one cadence across the game's frames: the time left
        // after a game frame is used from where the cadence stands, not from a
        // fresh start (which would fit only two frames of 144 in each 60th).
        const auto keepCadence=[&](std::uint64_t now) {nextShown_=now-nextShown_<step?nextShown_+step:now+step;};
        bool shown=false;
        for(;;) {
            const auto now=SDL_GetTicksNS();
            if(now+betweenCost_>=deadline_)break;
            // One that would end after the game's next frame starts comes
            // early instead (up to half a step), as long as it fits now.
            if(now<nextShown_&&(nextShown_+betweenCost_<deadline_||nextShown_-now>step/2)) {
                if(nextShown_+betweenCost_>=deadline_)break;
                SDL_DelayPrecise(nextShown_-now);continue;
            }
            const float alpha=std::clamp(float(double(std::int64_t(now-start))/double(period)),0.f,1.f);
            if(!interpolator_(alpha))break;
            measureBetween(SDL_GetTicksNS()-now);
            shown=true;keepCadence(now);
        }
        if(!shown) {
            const auto now=SDL_GetTicksNS();
            if(interpolator_(1.f))measureBetween(SDL_GetTicksNS()-now),keepCadence(now);
            // An estimate made by a slow frame (a map loading) comes back down.
            betweenCost_=betweenCost_*3/4;
        }
    }
    const auto now=SDL_GetTicksNS();
    ProfileScope timing("pace-wait");
    if(deadline_>now)SDL_DelayPrecise(deadline_-now);
    else if(now-deadline_>100'000'000)deadline_=now;
}
}
