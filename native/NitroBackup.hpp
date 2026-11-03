#pragma once
#include "SavePages.hpp"
namespace pc {
class NitroBackup {
public:
    explicit NitroBackup(SavePages&);
    ~NitroBackup();
    NitroBackup(const NitroBackup&)=delete;
    NitroBackup& operator=(const NitroBackup&)=delete;
};
}
