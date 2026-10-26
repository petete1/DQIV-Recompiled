# 32-bit Linux build with the system GCC and its multilib libraries (the
# original game resources contain 32-bit pointers).
# Arch: lib32-glibc lib32-gcc-libs. Debian/Ubuntu: g++-multilib.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_C_FLAGS_INIT "-m32 -msse2 -mfpmath=sse")
set(CMAKE_CXX_FLAGS_INIT "-m32 -msse2 -mfpmath=sse")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-m32")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-m32")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-m32")
# SDL finds the 32-bit libraries' pkg-config files (Arch, Fedora, Debian layouts).
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib32/pkgconfig:/usr/lib/pkgconfig:/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig")
