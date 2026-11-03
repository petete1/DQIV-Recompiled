#pragma once
#include "SaveStore.hpp"
#include <cstdint>
#include <filesystem>
#include <span>

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
    // The page a quick save or autosave was made on, kept beside it (0x1FC).
    int resumePage(ResumeSlot slot) const;
    void tagResume(ResumeSlot slot,int page);
private:
    SaveStore& resumeStore(ResumeSlot slot) {return slot==ResumeSlot::quick?first_:second_;}
    const SaveStore& resumeStore(ResumeSlot slot) const {return slot==ResumeSlot::quick?first_:second_;}
    SaveStore& pageStore(int page) {return page?second_:first_;}
    const SaveStore& pageStore(int page) const {return page?second_:first_;}
    SaveStore first_,second_;
    int shown_=0,session_=0;
    ResumeSlot slot_=ResumeSlot::quick;
};
// The pages the game uses (NitroBackup), or null.
SavePages* mountedSavePages();
void mountSavePages(SavePages* pages);
// When to autosave: each map change asks for one, except the change that
// follows loading from the title (the player is already at a save point).
void autosaveMapChanged();
void autosaveArrived();
bool autosavePending();
bool takeAutosave(); // true once per request
// Writes the autosave if one is asked for (Autosave.cpp: the game's quick save
// capture, into page 2's bank 3). Called when the player is back in control.
void autosaveIfPending();
}
