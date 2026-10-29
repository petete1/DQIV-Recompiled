// Dragon Quest IV for Windows and Linux: the original game, run natively with
// the PC window, renderer and sound.
#include "Assets.hpp"
#include "AssetPack.hpp"
#include "GameInstall.hpp"
#include "GameServices.hpp"
#include "NitroFilesystem.hpp"
#include "NitroBackup.hpp"
#include "NitroGeometry.hpp"
#include "GxTextures.hpp"
#include "Display.hpp"
#include "GameWindow.hpp"
#include "FramePresenter.hpp"
#include "DualScreen.hpp"
#include "Profile.hpp"
#include "NitroVideo.hpp"
#include "NitroThreads.hpp"
#include "NitroSound.hpp"
#include "MobileSound.hpp"
#include "NitroPower.hpp"
#include "NitroOverlays.hpp"
#include "NitroIdentity.hpp"
#include "NitroWireless.hpp"
#include "Input.hpp"
#include "Settings.hpp"
#include "ErrorWindow.hpp"
#include "LowMemory.hpp"
#ifdef _WIN32
#include <share.h>
// Laptops with two graphics chips run the game on the discrete one.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement=1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1;
}
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
extern "C" {
#include <nitro/os/thread.h>
}
#include "main/global/GameApplication.hpp"
#include "main/global/GlobalDQ4.hpp"
#include "main/achievement/AchievementTriggers.hpp"
#include "Achievements.hpp"
#include "Version.hpp"
#include <array>
#include <cctype>
#include <map>
#include <cstdio>
#include <exception>
#include <iostream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif


namespace {
#ifdef _WIN32
// The game's frame (the DS game, its 3D and the frames between) runs on this
// thread: on processors with fast and efficient cores (Intel 12th generation
// and later) Windows may give it an efficient one, or slow it down to save
// power. It asks for the fast cores and full speed.
void preferFastCores() {
    PROCESS_POWER_THROTTLING_STATE process{PROCESS_POWER_THROTTLING_CURRENT_VERSION,PROCESS_POWER_THROTTLING_EXECUTION_SPEED,0};
    SetProcessInformation(GetCurrentProcess(),ProcessPowerThrottling,&process,sizeof(process));
    THREAD_POWER_THROTTLING_STATE thread{THREAD_POWER_THROTTLING_CURRENT_VERSION,THREAD_POWER_THROTTLING_EXECUTION_SPEED,0};
    SetThreadInformation(GetCurrentThread(),ThreadPowerThrottling,&thread,sizeof(thread));
    SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS);
    ULONG size=0;
    GetSystemCpuSetInformation(nullptr,0,&size,GetCurrentProcess(),0);
    if(!size)return;
    std::vector<unsigned char> buffer(size);
    auto* first=reinterpret_cast<SYSTEM_CPU_SET_INFORMATION*>(buffer.data());
    if(!GetSystemCpuSetInformation(first,size,&size,GetCurrentProcess(),0))return;
    BYTE fastest=0;std::vector<ULONG> all,fast;
    for(int pass=0;pass<2;++pass) {
        for(ULONG at=0;at<size;) {
            const auto* set=reinterpret_cast<const SYSTEM_CPU_SET_INFORMATION*>(buffer.data()+at);
            if(set->Type==CpuSetInformation) {
                const BYTE efficiency=set->CpuSet.EfficiencyClass;
                if(pass==0) {fastest=std::max(fastest,efficiency);all.push_back(set->CpuSet.Id);}
                else if(efficiency==fastest)fast.push_back(set->CpuSet.Id);
            }
            at+=set->Size;
        }
    }
    // Only when some cores are slower: elsewhere Windows chooses best.
    if(!fast.empty()&&fast.size()<all.size()) {
        SetThreadSelectedCpuSets(GetCurrentThread(),fast.data(),ULONG(fast.size()));
        std::printf("Fast cores: %zu of %zu\n",fast.size(),all.size());
    }
}
#else
// Linux schedules the busy game thread on the fast cores itself.
void preferFastCores() {}
#endif
}

static_assert(pc::wideAddresses || sizeof(DataObject) == 0x10);
static_assert(pc::wideAddresses || sizeof(GameApplication) == 0xb4);
std::span<const pc::OverlayImage> gameOverlayImages();

namespace {
// The game has no console: its output goes to a journal beside the save.
std::string journalPath() {return (pc::saveDirectory()/"journal.txt").string();}
// The first start of this version brings the save and the settings of an
// earlier one (kept in %LOCALAPPDATA%) into the saves folder.
void adoptPreviousSave() {
    const auto previous=pc::previousSaveDirectory();
    if(previous.empty()||std::filesystem::exists(pc::defaultSavePath()))return;
    for(const char* name:{"dqiv_eur.sav","dqiv_eur.sav.identity","settings.ini"}) {
        std::error_code ignored;
        if(std::filesystem::exists(previous/name,ignored))
            std::filesystem::copy_file(previous/name,pc::saveDirectory()/name,std::filesystem::copy_options::skip_existing,ignored);
    }
}
// An error the player can fix: the journal keeps it in English, the error box
// shows it in the game language.
struct PlayerError:std::runtime_error {
    std::string shown;
    PlayerError(const std::string& logged,std::string text):std::runtime_error(logged),shown(std::move(text)) {}
};
void showError(const std::exception& error) {
    const int language=pc::gameSettings().language;
    const auto* player=dynamic_cast<const PlayerError*>(&error);
    const auto message=std::string(pc::errorText(pc::ErrorText::closing,language))+"\n\n"+(player?player->shown:error.what())+
        "\n\n"+pc::errorText(pc::ErrorText::details,language)+journalPath();
    pc::showErrorBox(message);
}
// The game's resources: packed inside the executable or, in a build tree, the
// assets folder (the game's data) and the addons folder (the PC version's
// additions) beside it or beside one of its parent folders. Without the
// game's data (an executable built without the ROM), it is read in place from
// the player's ROM and .obb in the ROMandOBBfileHere folder beside it.
pc::Assets openResources() {
    const auto executable=pc::executablePath();
    pc::Assets assets;
    if(pc::hasAssetPack(executable))assets.add(executable);
    else
        for(const char* name:{"assets","addons"})
            for(auto directory=executable.parent_path();!directory.empty();) {
                if(std::filesystem::is_directory(directory/name)) {assets.add(directory/name);break;}
                const auto parent=directory.parent_path();if(parent==directory)break;directory=parent;
            }
    const char* sound="data/SOUND/sound_data.sdat";
    if(!assets.contains(sound)) {
        const auto inbox=executable.parent_path()/"ROMandOBBfileHere";
        const auto withExtension=[&inbox](const char* extension) {
            std::filesystem::path found;
            std::error_code ignored;
            for(const auto& entry:std::filesystem::directory_iterator(inbox,ignored)) {
                auto name=entry.path().extension().string();
                for(char& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
                if(name==extension&&entry.is_regular_file()&&(found.empty()||entry.path()<found))found=entry.path();
            }
            return found;
        };
        const auto toAssets=[](const pc::FileRanges& files) {
            std::map<std::string,std::pair<std::uint64_t,std::uint64_t>> ranges;
            for(const auto& [name,range]:files)ranges.emplace(name,std::pair{range.offset,range.size});
            return ranges;
        };
        if(const auto rom=withExtension(".nds");!rom.empty()) {
            assets.addRanges(rom,toAssets(pc::romFileRanges(rom)));
            std::cout<<"Game data: "<<rom.filename().string()<<'\n';
            if(const auto obb=withExtension(".obb");!obb.empty()) {
                assets.addRanges(obb,toAssets(pc::mobileSoundRanges(obb)));
                std::cout<<"HD sound: "<<obb.filename().string()<<'\n';
            }
        } else if(!withExtension(".zip").empty())
            throw PlayerError("Extract the .nds ROM from its .zip into "+inbox.string(),
                pc::errorText(pc::ErrorText::zippedRom,pc::gameSettings().language)+("\n"+inbox.string()));
    }
    if(!assets.contains(sound)) {
        const auto inbox=executable.parent_path()/"ROMandOBBfileHere";
        throw PlayerError("Game data not found: put your European Dragon Quest IV ROM (.nds) in "+inbox.string()+
            " (and the Android version's .obb for the HD sound)",
            pc::errorText(pc::ErrorText::missingData,pc::gameSettings().language)+("\n"+inbox.string()));
    }
    return assets;
}
}

int runGame() {
    {
        std::filesystem::create_directories(pc::saveDirectory());
        adoptPreviousSave();
        // The previous session's journal (its error, after a crash) stays readable.
        std::error_code ignored;
        std::filesystem::copy_file(journalPath(),pc::saveDirectory()/"journal-precedent.txt",
            std::filesystem::copy_options::overwrite_existing,ignored);
        // Shared opening: a second running copy of the game must not lose its
        // output (an exclusive journal would leave it without a stdout).
#ifdef _WIN32
        if(FILE* file=_fsopen(journalPath().c_str(),"w",_SH_DENYWR))std::fclose(file);
        if(!std::freopen(journalPath().c_str(),"a",stdout))std::freopen("NUL","w",stdout);
        if(!std::freopen(journalPath().c_str(),"a",stderr))std::freopen("NUL","w",stderr);
#else
        // Each running copy holds a shared lock on the journal it writes to.
        if(const int file=::open(journalPath().c_str(),O_WRONLY|O_CREAT|O_CLOEXEC,0644);file>=0) {
            if(::flock(file,LOCK_EX|LOCK_NB)==0&&::ftruncate(file,0)!=0)std::perror("journal");
            ::close(file);
        }
        if(!std::freopen(journalPath().c_str(),"a",stdout))std::freopen("/dev/null","w",stdout);
        else ::flock(fileno(stdout),LOCK_SH|LOCK_NB);
        if(!std::freopen(journalPath().c_str(),"a",stderr))std::freopen("/dev/null","w",stderr);
#endif
    }
    try {
        std::cout.setf(std::ios::unitbuf);
        std::cout<<"Version: "<<pc::version()<<'\n';
        pc::loadSettings();
        pc::loadAchievements();
        preferFastCores();
        pc::Assets assets=openResources();
        pc::GameWindow window(false,false);
        pc::setPictureReader([&assets](const std::string& name,int& width,int& height) {
            return assets.contains(name)?pc::decodePng(assets.read(name),width,height):std::vector<std::uint32_t>{};
        });
        // Both DS screens become one picture, presented by the GPU.
        pc::dualScreen().unified=true;
        window.applySettings(); // the picture's width (GameWindow::updatePictureWidth)
        bool running=true;
        pc::GameServices services(assets);
        pc::NitroFilesystem filesystem(assets);
        const auto savePath=pc::defaultSavePath();
        pc::SaveStore save(savePath);
        auto identityPath=savePath;identityPath+=L".identity";
        pc::NitroIdentity identity(identityPath);
        pc::NitroBackup backup(save);
        pc::NitroVideo video;
        pc::NitroPower power;
        pc::GxGeometry geometry;pc::NitroGeometry geometryContext(geometry);
        pc::GxTextures textures;
        pc::Display display;
        unsigned renderedSwap=0;
        pc::NitroSound sound;
        pc::MobileSound mobileSound(assets,48000);
        OSThreadQueue frameQueue{};
        std::array<std::int16_t,1600> samples{};
        pc::NitroThreads threads;
        pc::NitroOverlays overlays(gameOverlayImages());
        if(!overlays.load(38)) throw std::runtime_error("Cannot construct main game module");
        pc::FramePresenter presenter(window,display,textures);
        // Faster battles (settings), and the whole game while fast forward is
        // held: only one game frame in 2, 3 or 4 is shown, paced and heard;
        // the music keeps its tempo.
        bool fastFrame=false;unsigned boostFrames=0;
        pc::setFrameCallback([&] {
            pc::ProfileScope frameTiming("render-callback");
            if(geometry.swaps()!=renderedSwap) {
                renderedSwap=geometry.swaps();
                auto state=geometry.raster;
                state.fogColor=*static_cast<unsigned*>(video.reg(0x04000358,4));
                pc::GxRenderOptions options;
                options.displayControl=*static_cast<unsigned short*>(video.reg(0x04000060,2));
                options.wBuffer=(geometry.swapParameters()&2)!=0;options.manualSort=(geometry.swapParameters()&1)!=0;
                presenter.frame(video,geometry,state,options,false);
            }
            if(!fastFrame)presenter.present();
        });
        threads.setIdleHandler([&] {
            running=window.poll();
            const bool shown=!fastFrame;
            int boost=g_GlobalDQ4.unkfunc_0205810c()==13?pc::gameSettings().battleBoost+1:1;
            if(pc::fastForward())boost=std::max(boost,pc::fastForwardSpeed);
            fastFrame=boost>1&&++boostFrames%unsigned(boost)!=0;
            if(shown) {
                pc::ProfileScope timing("sound");
                sound.render(samples);mobileSound.mix(samples);
                // Fast forward is silent; the sound goes on underneath.
                if(pc::fastForward())samples.fill(0);
            }
            power.advanceFrame();pc::advanceWireless();
            if(shown) {window.audio(samples);window.pace();}
            OS_UnpauseThread(&frameQueue);
        });
        pc::setWaitCallback([&] {OS_PauseThread(&frameQueue);threads.checkErrors();});
        struct ClearWait {~ClearWait(){pc::setWaitCallback({});pc::setFrameCallback({});}} clearWait;
        std::cout << "Original application initialization\n";
        g_GameApplication.unkfunc_02057f8c(0);
        g_GlobalDQ4.nativeStart(g_GameApplication.vf10());
        int scene=-1;
        for(unsigned frame=0;running;++frame) {
            // The journal keeps the scene changes (battle, town, field...).
            const auto currentScene=int(g_GlobalDQ4.unkfunc_0205810c());
            if(scene!=currentScene) {
                if(scene==3)window.hideVersion(); // leaving the adventure log menu
                scene=currentScene;std::cout<<"Scene "<<scene<<", frame "<<frame<<'\n';}
            threads.checkErrors();
            {
                pc::ProfileScope step("frame");
                if(!g_GlobalDQ4.nativeStep())break;
            }
            achievement::onFrame();
            pc::profileFrame();
            geometry.clearTriangles();
        }
        threads.checkErrors();
        g_GlobalDQ4.nativeStop();
        pc::setButtons(0);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Dragon Quest IV stopped: " << error.what() << '\n';
        std::cerr.flush();showError(error);
        return 1;
    }
}

// 64-bit: with the heap and the stack below 2 GiB (LowMemory.hpp).
int main(int,char** argv) {
    pc::keepHeapNearExecutable(argv);
    return pc::runOnLowStack(runGame);
}
