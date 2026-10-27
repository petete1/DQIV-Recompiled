#pragma once
#include <cstdint>
#include <type_traits>

// A pointer kept in a 32-bit field of a game file (a file offset the game
// turns into an address in place), or of a structure the game sizes as on the
// DS. A 64-bit build keeps the field 32 bits wide: the game's memory stays
// below 2 GiB there (native/LowMemory.hpp). A 32-bit build uses a pointer.
#if UINTPTR_MAX > 0xffffffffu
template<class T> class FilePtr {
public:
    FilePtr() = default;
    FilePtr(T* pointer) : value_(std::uint32_t(reinterpret_cast<std::uintptr_t>(pointer))) {}
    operator T*() const { return reinterpret_cast<T*>(std::uintptr_t(value_)); }
    T* operator->() const { return *this; }
    // The casts the game writes on its pointers: to another pointer, or to an integer.
    template<class U> explicit operator U*() const { return reinterpret_cast<U*>(std::uintptr_t(value_)); }
    template<class I, class = std::enable_if_t<std::is_integral_v<I>>> explicit operator I() const { return I(value_); }
private:
    std::uint32_t value_;
};
#else
template<class T> using FilePtr = T*;
#endif
