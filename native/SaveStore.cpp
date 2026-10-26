#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <chrono>
#endif
#include "SaveStore.hpp"
#include "AssetPack.hpp"
#include <algorithm>
#include <atomic>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <string>

namespace pc {
std::filesystem::path saveDirectory() {return executablePath().parent_path()/L"saves";}
std::filesystem::path defaultSavePath() {return saveDirectory()/L"dqiv_eur.sav";}
std::filesystem::path previousSaveDirectory() {
#ifndef _WIN32
    return {}; // no earlier version kept its files elsewhere
#else
    const DWORD count=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);
    if(!count) return {};
    std::wstring value(count,L'\0');
    const DWORD copied=GetEnvironmentVariableW(L"LOCALAPPDATA",value.data(),count);
    if(!copied || copied>=count) return {};
    value.resize(copied);
    return std::filesystem::path(value)/L"DQIV-PC";
#endif
}
namespace {
#ifdef _WIN32
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    ~Handle() { if(value!=INVALID_HANDLE_VALUE) CloseHandle(value); }
};
#else
// The lock file is removed when released, as on Windows.
struct Lock {
    int file=-1;
    std::filesystem::path path;
    Lock(int file,std::filesystem::path path):file(file),path(std::move(path)) {}
    Lock(const Lock&)=delete;
    Lock& operator=(const Lock&)=delete;
    ~Lock() { if(file>=0) { ::unlink(path.c_str()); ::close(file); } }
};
bool writeFile(const std::filesystem::path& path,std::span<const std::uint8_t> bytes,bool& owned) {
    const int file=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0644);
    if(file<0) return false;
    owned=true;
    std::size_t done=0;
    while(done<bytes.size()) {
        const auto count=::write(file,bytes.data()+done,bytes.size()-done);
        if(count<=0) break;
        done+=std::size_t(count);
    }
    const bool flushed=done==bytes.size() && ::fsync(file)==0;
    return ::close(file)==0 && flushed;
}
#endif
void checkRange(std::uint32_t offset,std::size_t count) {
    if(offset>SaveStore::size || count>SaveStore::size-offset) throw std::out_of_range("Backup range exceeds 64 KiB");
}
std::array<std::uint8_t,SaveStore::size> readImage(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    if(!stream || stream.tellg()!=SaveStore::size) throw std::runtime_error("Invalid save file: expected exactly 65536 bytes");
    std::array<std::uint8_t,SaveStore::size> bytes;
    stream.seekg(0);
    if(!stream.read(reinterpret_cast<char*>(bytes.data()),bytes.size())) throw std::runtime_error("Cannot read save file");
    return bytes;
}
}
SaveStore::SaveStore(std::filesystem::path path):path_(std::filesystem::absolute(std::move(path))) {
    std::filesystem::create_directories(path_.parent_path());
    auto lockPath=path_; lockPath += L".lock";
#ifdef _WIN32
    Handle lock{CreateFileW(lockPath.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,
                          FILE_ATTRIBUTE_HIDDEN|FILE_FLAG_DELETE_ON_CLOSE,nullptr)};
    if(lock.value==INVALID_HANDLE_VALUE) throw std::runtime_error("Save file is in use or its directory is not writable");
#else
    auto lock=std::make_unique<Lock>(::open(lockPath.c_str(),O_RDWR|O_CREAT|O_CLOEXEC,0644),lockPath);
    if(lock->file<0 || ::flock(lock->file,LOCK_EX|LOCK_NB)!=0) {
        if(lock->file>=0) { ::close(lock->file); lock->file=-1; }
        throw std::runtime_error("Save file is in use or its directory is not writable");
    }
#endif
    bytes_.fill(0xff);
    if(std::filesystem::exists(path_)) bytes_=readImage(path_);
#ifdef _WIN32
    lock_=lock.value; lock.value=INVALID_HANDLE_VALUE;
#else
    lock_=lock.release();
#endif
}
#ifdef _WIN32
SaveStore::~SaveStore() { if(lock_) CloseHandle(lock_); }
#else
SaveStore::~SaveStore() { delete static_cast<Lock*>(lock_); }
#endif
void SaveStore::read(std::uint32_t offset,std::span<std::uint8_t> destination) const {
    checkRange(offset,destination.size());
    std::copy_n(bytes_.begin()+offset,destination.size(),destination.begin());
}
void SaveStore::write(std::uint32_t offset,std::span<const std::uint8_t> source) {
    checkRange(offset,source.size());
    if(source.empty()) return;
    auto candidate=bytes_;
    std::copy(source.begin(),source.end(),candidate.begin()+offset);
    static std::atomic<unsigned> sequence=0;
    auto temporary=path_;
#ifdef _WIN32
    temporary += L".pending-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(sequence++);
#else
    const auto ticks=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    temporary += ".pending-"+std::to_string(::getpid())+"-"+std::to_string(ticks)+"-"+std::to_string(sequence++);
#endif
    bool owned=false;
    try {
#ifndef _WIN32
        if(!writeFile(temporary,candidate,owned)) throw std::runtime_error("Cannot write save transaction");
        if(readImage(temporary)!=candidate) throw std::runtime_error("Save readback verification failed");
        std::filesystem::rename(temporary,path_);
#else
        {
            Handle file{CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
            if(file.value==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create save transaction");
            owned=true;
            DWORD written=0;
            if(!WriteFile(file.value,candidate.data(),static_cast<DWORD>(candidate.size()),&written,nullptr) || written!=candidate.size() || !FlushFileBuffers(file.value))
                throw std::runtime_error("Cannot flush save transaction");
        }
        if(readImage(temporary)!=candidate) throw std::runtime_error("Save readback verification failed");
        if(!MoveFileExW(temporary.c_str(),path_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot publish save transaction");
#endif
        bytes_=candidate;
    } catch(...) {
        if(owned) { std::error_code ignored; std::filesystem::remove(temporary,ignored); }
        throw;
    }
}
}
