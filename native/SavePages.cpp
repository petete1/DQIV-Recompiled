#include "SavePages.hpp"
#include <algorithm>
#include <array>
#include <ctime>
#include <exception>
#include <utility>

namespace pc {
namespace {
constexpr std::uint32_t resumeStart=0xb800,resumeEnd=0xf400,tagOffset=0x1fc;
constexpr std::uint32_t bankStart=0x400,bankSize=0x3c00,stampOffset=0xff00;
SavePages* mounted=nullptr;
bool pending=false,arriving=false,playing=false;
// An empty image the game reads as three empty logs: its header magic, then zeros.
void createEmpty(SaveStore& store) {
    std::array<std::uint8_t,SaveStore::size> image{};
    const std::array<std::uint8_t,8> magic{0x44,0x51,0x34,0x20,0x41,0x52,0x54,0x45};
    std::copy(magic.begin(),magic.end(),image.begin());
    store.write(0,image);
}
}
SavePages::SavePages(std::filesystem::path page1,std::filesystem::path page2)
    :first_(std::move(page1)),second_(page2) {
    if(!std::filesystem::exists(page2)) createEmpty(second_);
}
void SavePages::read(std::uint32_t offset,std::span<std::uint8_t> destination) const {
    // Split at bank 3's edges; each part comes from its own file.
    while(!destination.empty()) {
        const bool resume=offset>=resumeStart&&offset<resumeEnd;
        const std::uint32_t edge=offset<resumeStart?resumeStart:resume?resumeEnd:SaveStore::size;
        const auto count=std::min<std::size_t>(destination.size(),edge>offset?edge-offset:destination.size());
        (resume?resumeStore(slot_):pageStore(shown_)).read(offset,destination.first(count));
        offset+=std::uint32_t(count);destination=destination.subspan(count);
    }
}
void SavePages::write(std::uint32_t offset,std::span<const std::uint8_t> source) {
    while(!source.empty()) {
        const bool resume=offset>=resumeStart&&offset<resumeEnd;
        const std::uint32_t edge=offset<resumeStart?resumeStart:resume?resumeEnd:SaveStore::size;
        const auto count=std::min<std::size_t>(source.size(),edge>offset?edge-offset:source.size());
        SaveStore& store=resume?resumeStore(slot_):pageStore(shown_);
        store.write(offset,source.first(count));
        const std::int64_t now=std::time(nullptr);
        for(std::uint32_t bank=0;bank<4;++bank) {
            const std::uint32_t start=bankStart+bank*bankSize;
            if(offset>=start+bankSize||offset+count<=start) continue;
            std::array<std::uint8_t,8> stamp{};
            for(int i=0;i<8;++i) stamp[i]=std::uint8_t(std::uint64_t(now)>>(8*i));
            store.write(stampOffset+8*bank,stamp);
        }
        offset+=std::uint32_t(count);source=source.subspan(count);
    }
}
std::int64_t SavePages::saveTime(int page,int bank) const {
    if(bank<0||bank>3) return 0;
    const SaveStore& store=bank==3?resumeStore(page?ResumeSlot::autosave:ResumeSlot::quick):pageStore(page);
    std::array<std::uint8_t,8> stamp{};
    store.read(stampOffset+8*std::uint32_t(bank),stamp);
    std::uint64_t value=0;
    for(int i=7;i>=0;--i) value=value<<8|stamp[i];
    return value==~std::uint64_t(0)?0:std::int64_t(value); // an erased cartridge reads 0xFF
}
void SavePages::showPage(int page) {shown_=std::clamp(page,0,pageCount-1);}
void SavePages::setSessionPage(int page) {showPage(page);session_=shown_;}
int SavePages::resumePage(ResumeSlot slot) const {
    std::array<std::uint8_t,4> tag{};
    resumeStore(slot).read(tagOffset,tag);
    return tag[0]==2&&!tag[1]&&!tag[2]&&!tag[3]?1:0;
}
bool SavePages::tagResume(ResumeSlot slot,int page) {
    const std::array<std::uint8_t,4> tag{std::uint8_t(std::clamp(page,0,pageCount-1)+1),0,0,0};
    // Called from game code, which has no handler: a failed write is reported.
    try {resumeStore(slot).write(tagOffset,tag);} catch(const std::exception&) {return false;}
    return true;
}
SavePages* mountedSavePages() {return mounted;}
void mountSavePages(SavePages* pages) {mounted=pages;}
void autosaveMapChanged() {
    if(arriving) arriving=false;
    else pending=playing; // the title's own maps ask for nothing
}
void autosaveArrived() {pending=false;arriving=true;playing=true;}
bool autosavePending() {return pending;}
bool takeAutosave() {return std::exchange(pending,false);}
}
