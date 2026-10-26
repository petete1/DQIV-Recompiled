#pragma once
#include <cstdint>
#include <functional>
#include <memory>
namespace pc {
struct ThreadRuntime;
class NitroThreads {
public:
    NitroThreads();
    ~NitroThreads();
    NitroThreads(const NitroThreads&)=delete;
    NitroThreads& operator=(const NitroThreads&)=delete;
    void checkErrors();
    void setIdleHandler(std::function<void()> handler);
private:
    std::unique_ptr<ThreadRuntime> impl_;
};
unsigned suspendNativeInterrupts();
void resumeNativeInterrupts(unsigned depth);
#ifndef _WIN32
// The stack the game runs on: an SDK thread's own while it runs, else the
// OS thread's (Windows keeps them in the thread's own information block).
struct StackRange {std::uintptr_t low=0,high=0;};
StackRange currentNativeStack();
void setCurrentNativeStack(StackRange fiber);
#endif
}
