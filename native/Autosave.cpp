#include "SavePages.hpp"
#include "main/profile/SaveLoad.hpp"
#include <iostream>

namespace pc {
void autosaveIfPending() {
    SavePages* pages=mountedSavePages();
    if(!pages||!takeAutosave()) return;
    // The quick save's own capture, sent to page 2's bank 3; the header (last
    // log used) stays the adventure's page, as for a quick save.
    pages->showPage(pages->sessionPage());
    pages->useResumeSlot(ResumeSlot::autosave);
    if(profile::SaveLoad::savebank(3,profile::SAVETYPE_RESUME)) pages->tagResume(ResumeSlot::autosave,pages->sessionPage());
    else std::cout<<"Autosave failed\n";
    pages->useResumeSlot(ResumeSlot::quick);
}
}
