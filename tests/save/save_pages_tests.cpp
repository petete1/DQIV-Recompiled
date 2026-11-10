// Save pages: two 64 KiB images, routed by offset (build target dqiv_save_tests).
#include "SavePages.hpp"
#include <array>
#include <ctime>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fs=std::filesystem;
namespace {
int failures=0;
void check(bool ok,const char* what) {
    if(!ok) {++failures;std::cerr<<"FAIL: "<<what<<'\n';}
}
fs::path freshDirectory(const char* name) {
    const auto directory=fs::temp_directory_path()/"dqiv_save_tests"/name;
    fs::remove_all(directory);fs::create_directories(directory);
    return directory;
}
std::vector<std::uint8_t> fileBytes(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),{}};
}
void writeFile(const fs::path& path,std::size_t size,std::uint8_t value) {
    std::vector<char> bytes(size,char(value));
    std::ofstream(path,std::ios::binary).write(bytes.data(),std::streamsize(bytes.size()));
}

void createsPage2WithMagic() {
    const auto d=freshDirectory("create");
    {pc::SavePages pages(d/"one.sav",d/"two.sav");}
    const auto bytes=fileBytes(d/"two.sav");
    check(bytes.size()==65536,"page 2 is created with 65536 bytes");
    const std::array<std::uint8_t,8> magic{0x44,0x51,0x34,0x20,0x41,0x52,0x54,0x45};
    bool rest=bytes.size()==65536;
    for(std::size_t i=0;i<bytes.size();++i)
        if(i<8?bytes[i]!=magic[i]:bytes[i]!=0) rest=false;
    check(rest,"page 2 holds the magic and zeros");
}
void keepsExistingPage2() {
    const auto d=freshDirectory("keep");
    writeFile(d/"two.sav",65536,0xab);
    {pc::SavePages pages(d/"one.sav",d/"two.sav");}
    const auto bytes=fileBytes(d/"two.sav");
    bool same=bytes.size()==65536;
    for(auto b:bytes) if(b!=0xab) same=false;
    check(same,"an existing page 2 is kept");
}
void logsFollowShownPage() {
    const auto d=freshDirectory("logs");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    pages.showPage(1);
    const std::array<std::uint8_t,3> data{1,2,3};
    pages.write(0x400,data);
    check(fileBytes(d/"two.sav")[0x401]==2,"a log write on page 2 lands in page 2's file");
    check(!fs::exists(d/"one.sav") || fileBytes(d/"one.sav")[0x401]!=2,"page 1's file is untouched");
    pages.showPage(0);
    std::array<std::uint8_t,3> read{};
    pages.read(0x400,read);
    check(read[1]!=2,"page 1 reads its own log");
    pages.showPage(1);
    pages.read(0x400,read);
    check(read==data,"page 2 reads back its log");
}
void bank3FollowsSlot() {
    const auto d=freshDirectory("bank3");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    const std::array<std::uint8_t,1> quick{0x11},automatic{0x22};
    pages.showPage(1);pages.useResumeSlot(pc::ResumeSlot::quick);
    pages.write(0xb800,quick);
    check(fileBytes(d/"one.sav")[0xb800]==0x11,"the quick save goes to page 1's file");
    pages.showPage(0);pages.useResumeSlot(pc::ResumeSlot::autosave);
    pages.write(0xb800,automatic);
    check(fileBytes(d/"two.sav")[0xb800]==0x22,"the autosave goes to page 2's file");
    check(fileBytes(d/"one.sav")[0xb800]==0x11,"the autosave leaves the quick save alone");
}
void splitsAcrossBank3() {
    const auto d=freshDirectory("split");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    pages.showPage(1);pages.useResumeSlot(pc::ResumeSlot::quick);
    std::array<std::uint8_t,0x20> data;data.fill(0x5a);
    pages.write(0xb7f0,data);
    const auto one=fileBytes(d/"one.sav"),two=fileBytes(d/"two.sav");
    check(two[0xb7f0]==0x5a && two[0xb7ff]==0x5a && two[0xb800]!=0x5a,"the log part lands in page 2");
    check(one[0xb800]==0x5a && one[0xb80f]==0x5a && one[0xb7ff]!=0x5a,"the bank 3 part lands in page 1");
    std::array<std::uint8_t,0x20> read{};
    pages.read(0xb7f0,read);
    check(read==data,"a split read returns both parts");
}
void tagsRoundTrip() {
    const auto d=freshDirectory("tags");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    pages.tagResume(pc::ResumeSlot::autosave,1);
    const auto two=fileBytes(d/"two.sav");
    check(two[0x1fc]==2 && two[0x1fd]==0 && two[0x1fe]==0 && two[0x1ff]==0,"the autosave tag is 2 at 0x1FC");
    check(pages.resumePage(pc::ResumeSlot::autosave)==1,"the autosave reads back page 2");
    check(pages.resumePage(pc::ResumeSlot::quick)==0,"an untagged quick save is page 1");
    const std::array<std::uint8_t,4> seven{7,0,0,0};
    pages.showPage(0);pages.write(0x1fc,seven);
    check(pages.resumePage(pc::ResumeSlot::quick)==0,"an invalid tag is page 1");
}
void rejectsWrongSize() {
    const auto d=freshDirectory("size");
    writeFile(d/"two.sav",100,0);
    bool threw=false;
    try {pc::SavePages pages(d/"one.sav",d/"two.sav");} catch(const std::runtime_error&) {threw=true;}
    check(threw,"a page 2 of the wrong size is refused");
}
// Runs first: nothing is armed before an adventure is loaded (the title's own maps).
void noAutosaveBeforeLoading() {
    pc::autosaveMapChanged();
    pc::autosaveMapChanged();
    check(!pc::autosavePending(),"map changes before any load ask for nothing");
}
void tagFailureIsReported() {
    const auto d=freshDirectory("tagfail");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    fs::permissions(d,fs::perms::owner_read|fs::perms::owner_exec);
    bool ok=true,threw=false;
    try {ok=pages.tagResume(pc::ResumeSlot::autosave,1);} catch(...) {threw=true;}
    fs::permissions(d,fs::perms::owner_all);
    check(!threw&&!ok,"a tag that cannot be written returns false instead of throwing");
}
void autosaveTrigger() {
    pc::autosaveArrived();
    pc::autosaveMapChanged();
    check(!pc::autosavePending(),"the map change of an arrival is absorbed");
    pc::autosaveMapChanged();
    check(pc::autosavePending(),"a later map change asks for an autosave");
    check(pc::takeAutosave(),"taking the pending autosave");
    check(!pc::takeAutosave(),"taken once only");
    pc::autosaveMapChanged();
    pc::autosaveArrived();
    check(!pc::autosavePending(),"loading from the title drops a pending autosave");
}
void stampsLogBank() {
    const auto d=freshDirectory("stamp");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    pages.showPage(1);
    const std::array<std::uint8_t,4> data{1,2,3,4};
    const auto before=std::int64_t(std::time(nullptr));
    pages.write(0x400+0x3c00*2+16,data);           // bank 2, page 2
    const auto t=pages.saveTime(1,2);
    check(t>=before&&t<=std::int64_t(std::time(nullptr)),"a log write stamps its bank with now");
    check(pages.saveTime(1,0)==0&&pages.saveTime(0,2)==0,"other banks and the other page keep no stamp");
    const auto bytes=fileBytes(d/"two.sav");
    std::int64_t onDisk=0;for(int i=7;i>=0;--i)onDisk=onDisk<<8|bytes[0xff00+16+i];
    check(onDisk==t,"the stamp is little-endian at 0xFF00 + 8*bank");
}
void stampsResumeInItsSlotsFile() {
    const auto d=freshDirectory("stampresume");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    pages.useResumeSlot(pc::ResumeSlot::autosave);
    const std::array<std::uint8_t,4> data{1,2,3,4};
    pages.write(0xb800,data);
    check(pages.saveTime(1,3)!=0&&pages.saveTime(0,3)==0,"bank 3 stamps the autosave's file");
}
void headerWriteStampsNothing() {
    const auto d=freshDirectory("stampheader");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    const std::array<std::uint8_t,4> data{1,2,3,4};
    pages.write(0x200,data);
    bool none=true;for(int b=0;b<4;++b)none=none&&pages.saveTime(0,b)==0;
    check(none,"a header write stamps no bank");
}
void stampsBothBanksOfASplitWrite() {
    const auto d=freshDirectory("stampsplit");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    std::vector<std::uint8_t> data(32,7);
    pages.write(0xb800-16,data);
    check(pages.saveTime(0,2)!=0&&pages.saveTime(0,3)!=0,"a write across bank 2 and 3 stamps both");
}
void saveTimeTextFormats() {
    check(pc::saveTimeText(0).empty(),"no stamp, no text");
    check(pc::saveTimeText(std::int64_t(0x7fffffffffffffff)).empty(),"a garbage stamp gives no text instead of crashing");
    check(pc::saveTimeText(-5).empty(),"a negative stamp gives no text");
    const auto text=pc::saveTimeText(1794348781);  // 2026-11-10, some hour of the local day
    check(text.size()==16&&text.rfind("2026-11-",0)==0&&text[10]==' '&&text[13]==':',"YYYY-MM-DD HH:MM");
}
}

int main() {
    noAutosaveBeforeLoading();
    createsPage2WithMagic();
    keepsExistingPage2();
    logsFollowShownPage();
    bank3FollowsSlot();
    splitsAcrossBank3();
    tagsRoundTrip();
    rejectsWrongSize();
    autosaveTrigger();
    tagFailureIsReported();
    stampsLogBank();
    stampsResumeInItsSlotsFile();
    headerWriteStampsNothing();
    stampsBothBanksOfASplitWrite();
    saveTimeTextFormats();
    std::cout<<(failures?"save pages: FAILED ":"save pages: all passed ")<<failures<<'\n';
    return failures?1:0;
}
