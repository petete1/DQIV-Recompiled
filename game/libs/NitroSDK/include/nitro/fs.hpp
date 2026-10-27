#pragma once

// C++ subset of nitro/fs.h
// Storage for an FS_File (nitro/fs/file.h), whose pointer fields are 8 bytes
// in a 64-bit build.
struct FSFile {
    void* unk_00[0x48 / 4];                     // 0x00
};

extern "C" {
    int FS_LoadOverlay(void* overlay, unsigned int overlayID);
    int FS_UnloadOverlay(void* overlay, unsigned int overlayID);
    unsigned int FS_TryLoadTable(void* buf, unsigned int size);
    void func_02060e04(FSFile* file);           // FS_InitFile
    int func_020610e4(FSFile* file, const char* path);  // FS_OpenFile
    int func_0206112c(FSFile* file);            // FS_CloseFile
    int func_02061270(FSFile* file, void* buf, int size);  // FS_ReadFile
    int func_02061280(FSFile* file, int pos, int mode);     // FS_SeekFile
}
