#pragma once
#include "SaveStore.hpp"
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace pc {
// Which file the game's bank 3 (its quick save) reads and writes.
enum class ResumeSlot {quick,autosave};
// Two cartridge images seen by the game as one: the header and the three logs
// come from the page shown (page 1: logs 1-3, page 2: logs 4-6); bank 3 is the
// quick save in page 1's file or the autosave in page 2's. Pages are 0-based.
class SavePages {
public:
    static constexpr int pageCount=2;
    SavePages(std::filesystem::path page1,std::filesystem::path page2);
    void read(std::uint32_t offset,std::span<std::uint8_t> destination) const;
    void write(std::uint32_t offset,std::span<const std::uint8_t> source);
    void showPage(int page);
    int shownPage() const {return shown_;}
    // The page of the adventure being played; also shown.
    void setSessionPage(int page);
    int sessionPage() const {return session_;}
    void useResumeSlot(ResumeSlot slot) {slot_=slot;}
    ResumeSlot resumeSlot() const {return slot_;}
    // The page a quick save or autosave was made on, kept beside it (0x1FC);
    // tagResume returns false when the file cannot be written.
    int resumePage(ResumeSlot slot) const;
    bool tagResume(ResumeSlot slot,int page);
    // When bank 0-3 of page 0-1's file was last written (Unix seconds, 0: never, also when erased): each
    // write stamps the banks it touches at 0xFF00 + 8*bank, a part the game never reads.
    // Bank 3 is the resume save: page 0's file holds the quick save, page 1's the autosave.
    std::int64_t saveTime(int page,int bank) const;
private:
    SaveStore& resumeStore(ResumeSlot slot) {return slot==ResumeSlot::quick?first_:second_;}
    const SaveStore& resumeStore(ResumeSlot slot) const {return slot==ResumeSlot::quick?first_:second_;}
    SaveStore& pageStore(int page) {return page?second_:first_;}
    const SaveStore& pageStore(int page) const {return page?second_:first_;}
    SaveStore first_,second_;
    int shown_=0,session_=0;
    ResumeSlot slot_=ResumeSlot::quick;
};
// A stamp as "YYYY-MM-DD HH:MM" in local time; empty for 0 or a value outside 2000-2199
// (a file from elsewhere may hold anything there).
std::string saveTimeText(std::int64_t seconds);
// The pages the game uses (NitroBackup), or null.
SavePages* mountedSavePages();
void mountSavePages(SavePages* pages);
// When to autosave: each map change once an adventure is loaded asks for one,
// except the change that follows the load (already at a save point).
void autosaveMapChanged();
void autosaveArrived();
bool autosavePending();
bool takeAutosave(); // true once per request
// Writes the autosave if one is asked for (Autosave.cpp: the game's quick save
// capture, into page 2's bank 3). Called when the player is back in control.
void autosaveIfPending();
}
