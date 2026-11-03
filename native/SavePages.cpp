#include "SavePages.hpp"
#include <algorithm>
#include <array>
#include <utility>

namespace pc {
namespace {
constexpr std::uint32_t resumeStart=0xb800,resumeEnd=0xf400,tagOffset=0x1fc;
SavePages* mounted=nullptr;
bool pending=false,arriving=false;
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
        (resume?resumeStore(slot_):pageStore(shown_)).write(offset,source.first(count));
        offset+=std::uint32_t(count);source=source.subspan(count);
    }
}
void SavePages::showPage(int page) {shown_=std::clamp(page,0,pageCount-1);}
void SavePages::setSessionPage(int page) {showPage(page);session_=shown_;}
int SavePages::resumePage(ResumeSlot slot) const {
    std::array<std::uint8_t,4> tag{};
    resumeStore(slot).read(tagOffset,tag);
    return tag[0]==2&&!tag[1]&&!tag[2]&&!tag[3]?1:0;
}
void SavePages::tagResume(ResumeSlot slot,int page) {
    const std::array<std::uint8_t,4> tag{std::uint8_t(std::clamp(page,0,pageCount-1)+1),0,0,0};
    resumeStore(slot).write(tagOffset,tag);
}
SavePages* mountedSavePages() {return mounted;}
void mountSavePages(SavePages* pages) {mounted=pages;}
void autosaveMapChanged() {
    if(arriving) arriving=false;
    else pending=true;
}
void autosaveArrived() {pending=false;arriving=true;}
bool autosavePending() {return pending;}
bool takeAutosave() {return std::exchange(pending,false);}
}
