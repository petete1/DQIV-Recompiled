#pragma once
#include <cstddef>
#include <cstdint>

namespace pc {
// The original game keeps addresses in 32-bit integers and in the 32-bit
// fields of its files. A 64-bit build keeps the game's memory (the executable,
// the heap and the stacks it runs on) below 2 GiB, where an address fits in
// 32 bits and comes back unchanged from a signed or unsigned integer.
constexpr bool wideAddresses=sizeof(void*)>4;
constexpr std::uintptr_t lowMemoryLimit=0x80000000u;
// Throws when [pointer, pointer + size) does not lie below the limit (64-bit).
void requireLowAddress(const void* pointer,std::size_t size);
// 64-bit: restarts the program once without address randomization, so the
// heap starts right after the executable instead of up to 1 GiB further and
// has room to grow below the limit. Returns when nothing needs doing.
void keepHeapNearExecutable(char** argv);
// Runs `function` on a stack below the limit (64-bit), on the calling thread.
int runOnLowStack(int (*function)());
}
