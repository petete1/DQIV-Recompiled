#pragma once
#include <globaldefs.h>
#include "main/profile/Profile.hpp"

// one save slot as shown by the load/save menus (mobile profile::CatalogView without saveresult_)
struct profile::CatalogView {
    int useFlag_;                               // 0x00
    int id_;                                    // 0x04
    char name_[32];                             // 0x08
    char restart_[32];                          // 0x28
    int savetype_;                              // 0x48
    int chapter_;                               // 0x4C
    int level_;                                 // 0x50
    int town_;                                  // 0x54
    int time_;                                  // 0x58
#ifdef DQIV_NATIVE
    int lead_;                                  // the party's first member (player index), for its face
#endif
};

// backup memory access (header at 0, current bank at 0x200, banks of 0x3c00 bytes from 0x400)
struct profile::SaveLoad {
    static int asyncBank_;
    static int asyncResult_;
    static Profile* asyncBuff_;
    static int lastSleep_;
    static int catalogRecent_;
    static CatalogView catalogView_[3];

    static int checkbank(int bank);
    static void fillCatalogView(PROFILE_RECORD* record, CatalogView* view);
    static CatalogView* getCatalogView();
    static int getCatalogRecent();
#ifdef DQIV_NATIVE
    // Bank 3 (quick save or autosave) as a catalog entry; false when empty.
    static bool getResumeView(CatalogView* view);
#endif
    static int loadbank(int bank);
    static bool savebank(int bank, SAVETYPE type);
    static void savebankAsync(int bank, SAVETYPE type);
    static int savebankAsyncWait();
    static int savebankAsyncResult();
    static bool killbank(int bank);
    static int verifybank(int bank);
    static bool memorysave(unsigned int seek, void* buff, unsigned int size);
    static void memorysaveAsync(unsigned int seek, void* buff, unsigned int size);
    static int memorysaveAsyncWait();
    static int memorysaveAsyncResult();
    static bool memoryload(unsigned int seek, void* buff, unsigned int size);
    static char* unkfunc_0202bbe0(int type, unsigned char* name);
    static void setCatalogMacro(CatalogView* view);
    static int getPlaceNameNo(int chapter, char* name);
    static int getPlaceNameByChapter(int chap);
    static int isThisMap(const char* map, const char* name);
    static int isCardOK();
    static int checkBackup();
    static int setSaveBank(int bank);
    static int getSaveBank();
};
