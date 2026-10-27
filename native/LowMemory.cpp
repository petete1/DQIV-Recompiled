#include "LowMemory.hpp"
#include "NitroThreads.hpp"
#include <exception>
#include <memory>
#include <stdexcept>
#if UINTPTR_MAX>0xffffffffu
#include <malloc.h>
#include <ucontext.h>
#endif

namespace pc {
#if UINTPTR_MAX>0xffffffffu
namespace {
// Before any other constructor: every block, large ones and other threads'
// too, comes from the main heap, which grows up from the end of the
// executable (loaded at a fixed low address), never from mmap.
__attribute__((constructor(101))) void keepHeapLow() {
    mallopt(M_MMAP_MAX,0);
    mallopt(M_ARENA_MAX,1);
}
ucontext_t caller,game;
int (*lowFunction)()=nullptr;
int lowResult=0;
std::exception_ptr lowFailure;
void lowEntry() {
    try {lowResult=lowFunction();}
    catch(...) {lowFailure=std::current_exception();}
}
}
void requireLowAddress(const void* pointer,std::size_t size) {
    const auto address=reinterpret_cast<std::uintptr_t>(pointer);
    if(address>=lowMemoryLimit||size>lowMemoryLimit-address)
        throw std::runtime_error("Game memory above 2 GiB: its addresses do not fit in 32 bits");
}
int runOnLowStack(int (*function)()) {
    constexpr std::size_t size=16u<<20;
    const std::unique_ptr<unsigned char[]> stack(new unsigned char[size]);
    requireLowAddress(stack.get(),size);
    const auto low=reinterpret_cast<std::uintptr_t>(stack.get());
    setThreadNativeStack({low,low+size});
    lowFunction=function;
    getcontext(&game);
    game.uc_stack.ss_sp=stack.get();game.uc_stack.ss_size=size;game.uc_link=&caller;
    makecontext(&game,lowEntry,0);
    swapcontext(&caller,&game);
    setThreadNativeStack({});
    if(lowFailure)std::rethrow_exception(lowFailure);
    return lowResult;
}
#else
void requireLowAddress(const void*,std::size_t) {}
int runOnLowStack(int (*function)()) {return function();}
#endif
}
