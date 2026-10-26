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
#include "NitroIdentity.hpp"
#include <cstring>
#include <fstream>
#include <random>
#include <stdexcept>

namespace pc {
namespace {
thread_local NitroIdentity* current=nullptr;
#ifdef _WIN32
struct Handle {
    HANDLE value;
    ~Handle() {if(value!=INVALID_HANDLE_VALUE) CloseHandle(value);}
};
#else
// Held while the identity is created; removed when released, as on Windows.
struct Lock {
    int file;
    std::filesystem::path path;
    Lock(int file,std::filesystem::path path):file(file),path(std::move(path)) {}
    Lock(const Lock&)=delete;
    Lock& operator=(const Lock&)=delete;
    ~Lock() {if(file>=0) {::unlink(path.c_str());::close(file);}}
};
#endif
std::array<std::uint8_t,6> read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    std::array<std::uint8_t,6> result;
    if(!file || file.tellg()!=result.size()) throw std::runtime_error("Invalid native device identity file");
    file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(result.data()),result.size()) || (result[0]&3)!=2)
        throw std::runtime_error("Invalid native device identity address");
    return result;
}
}
NitroIdentity::NitroIdentity(const std::filesystem::path& input) {
    if(current) throw std::runtime_error("A native identity context is already active");
    const auto path=std::filesystem::absolute(input);
    std::filesystem::create_directories(path.parent_path());
    auto lockPath=path;lockPath+=L".lock";
#ifdef _WIN32
    Handle lock{CreateFileW(lockPath.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,
        FILE_ATTRIBUTE_HIDDEN|FILE_FLAG_DELETE_ON_CLOSE,nullptr)};
    if(lock.value==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot lock native identity profile");
#else
    Lock lock{::open(lockPath.c_str(),O_RDWR|O_CREAT|O_CLOEXEC,0644),lockPath};
    if(lock.file<0 || ::flock(lock.file,LOCK_EX|LOCK_NB)!=0) {
        if(lock.file>=0) {::close(lock.file);lock.file=-1;}
        throw std::runtime_error("Cannot lock native identity profile");
    }
#endif
    if(std::filesystem::exists(path)) address_=read(path);
    else {
        std::random_device random;
        for(auto& byte:address_) byte=static_cast<std::uint8_t>(random());
        address_[0]=(address_[0]&0xfc)|2;
        auto temporary=path;
#ifdef _WIN32
        temporary+=L".pending-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
#else
        temporary+=".pending-"+std::to_string(::getpid())+"-"+std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
        bool owned=false;
        try {
#ifndef _WIN32
            {
                const int file=::open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0644);
                if(file<0) throw std::runtime_error("Cannot create native device identity");
                owned=true;
                const bool written=::write(file,address_.data(),6)==6 && ::fsync(file)==0;
                if(::close(file)!=0 || !written) throw std::runtime_error("Cannot flush native device identity");
            }
            if(read(temporary)!=address_) throw std::runtime_error("Native device identity verification failed");
            // Never replaces an identity created meanwhile (Windows: MoveFileEx without replacing).
            if(::link(temporary.c_str(),path.c_str())!=0) throw std::runtime_error("Cannot publish native device identity");
            std::error_code ignored;std::filesystem::remove(temporary,ignored);
#else
            {
                Handle file{CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
                if(file.value==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create native device identity");
                owned=true;DWORD written=0;
                if(!WriteFile(file.value,address_.data(),6,&written,nullptr) || written!=6 || !FlushFileBuffers(file.value))
                    throw std::runtime_error("Cannot flush native device identity");
            }
            if(read(temporary)!=address_) throw std::runtime_error("Native device identity verification failed");
            if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Cannot publish native device identity");
#endif
        } catch(...) {
            if(owned) {std::error_code ignored;std::filesystem::remove(temporary,ignored);}
            throw;
        }
    }
    current=this;
}
NitroIdentity::~NitroIdentity() {current=nullptr;}
}
extern "C" void func_02079e24(unsigned char* destination) {
    if(!pc::current || !destination) throw std::runtime_error("Device identity requested without its native profile");
    std::memcpy(destination,pc::current->address().data(),6);
}
