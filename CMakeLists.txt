cmake_minimum_required(VERSION 3.24)
project(DQIV_PC VERSION 1.0.0 LANGUAGES C CXX)
if(WIN32)
    enable_language(RC)
endif()
# The original game resources contain 32-bit pointers. A 64-bit Linux build
# keeps the game's memory below 2 GiB, where its addresses fit in 32 bits.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$"))
    message(FATAL_ERROR "The original game resources contain 32-bit pointers: configure a 32-bit build (cmake --preset windows-game or cmake --preset linux-game), or a 64-bit build on x86-64 Linux (cmake --preset linux64-game).")
endif()
if(MSVC)
    # Native C ABI adapters can report errors with C++ exceptions.
    string(REPLACE "/EHsc" "/EHs" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
else()
    # Native C ABI adapters can report errors with C++ exceptions, through the
    # SDK's C code too.
    add_compile_options("$<$<COMPILE_LANGUAGE:C>:-fexceptions>")
    # The decompiled code was written for compilers that do not exploit these
    # rules (type punning, signed wrap-around, out-of-range array indices,
    # objects filled before their constructor), as MSVC does not.
    add_compile_options(-fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks
        -fno-aggressive-loop-optimizations -fno-lifetime-dse -Wno-unknown-pragmas)
    # Constant tables: brace-initialized with values of the other signedness.
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wno-narrowing>")
    # Fixed addresses: the game tells its own text pointers from message IDs.
    set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
    add_compile_options(-fno-pie)
    add_link_options(-no-pie)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # The decompiled code casts addresses to 32-bit integers; they fit, as
        # the game's memory stays below 2 GiB (native/LowMemory.hpp).
        add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-fpermissive>")
    endif()
    string(REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
# Gamepads are reached through the kernel (hidraw, evdev); no 32-bit libusb needed.
set(SDL_HIDAPI_LIBUSB OFF CACHE BOOL "" FORCE)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # SDL's own check looks for them without _GNU_SOURCE; glibc has both.
    set(HAVE_GETRESUID 1)
    set(HAVE_GETRESGID 1)
endif()
FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
    URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(SDL3)

# Ogg Vorbis decoder (public domain) for the smartphone version's music.
set(DQIV_STB_VORBIS "${CMAKE_BINARY_DIR}/_deps/stb/stb_vorbis.c")
if(NOT EXISTS "${DQIV_STB_VORBIS}")
    file(DOWNLOAD https://raw.githubusercontent.com/nothings/stb/2c980bb59875b0d32144a71867fbdebb2f77cd20/stb_vorbis.c
        "${DQIV_STB_VORBIS}"
        EXPECTED_HASH SHA256=4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636)
endif()
if(NOT WIN32)
    # Interface text (settings, FPS) is drawn with a system font (public domain renderer).
    set(DQIV_STB_TRUETYPE "${CMAKE_BINARY_DIR}/_deps/stb/stb_truetype.h")
    if(NOT EXISTS "${DQIV_STB_TRUETYPE}")
        file(DOWNLOAD https://raw.githubusercontent.com/nothings/stb/2c980bb59875b0d32144a71867fbdebb2f77cd20/stb_truetype.h
            "${DQIV_STB_TRUETYPE}"
            EXPECTED_HASH SHA256=ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab)
    endif()
endif()
add_library(dqiv_vorbis STATIC native/VorbisDecoder.c)
target_include_directories(dqiv_vorbis PRIVATE "${CMAKE_BINARY_DIR}/_deps/stb")
if(MSVC)
    target_compile_options(dqiv_vorbis PRIVATE /W0)
endif()

# PC platform: memory, math, video, 3D, display, sound mixing, files, settings.
add_library(dqiv_core STATIC
    game/src/main/dss/Random.cpp
    game/src/main/dss/Utf8Iterator.cpp
    game/src/main/data/DataObject.cpp
    game/src/main/dss/Pad.cpp
    game/src/main/global/GlobalDQ4.cpp
    game/src/main/dss/DssFix.cpp
    game/src/main/dss/DssVector.cpp
    native/Entropy.cpp
    native/FixedMath.cpp
    native/NitroMath.cpp
    native/NitroMemory.cpp
    native/NitroAlgorithms.cpp
    game/libs/NitroSDK/src/os/os_printf.c
    native/NitroOs.cpp
    native/LowMemory.cpp
    native/GxGeometry.cpp
    native/GxTextures.cpp
    native/GxRasterizer.cpp
    native/NitroVideo.cpp
    native/Display.cpp
    native/Parallel.cpp
    native/HdPictures.cpp
    game/libs/NitroSDK/src/gx/gx.c
    game/libs/NitroSDK/src/gx/gx_vramcnt.c
    game/libs/NitroSDK/src/gx/gx_bgcnt.c
    game/libs/NitroSDK/src/gx/gx_load2d.c
    game/libs/NitroSDK/src/gx/gx_load3d.c
    game/libs/NitroSDK/src/gx/g2.c
    native/NitroGeometry.cpp
    game/libs/NitroSDK/src/gx/g3_util.c
    native/SaveStore.cpp
    native/Achievements.cpp
    native/AchievementTexts.cpp
    native/SoundWave.cpp
    native/SoundMixer.cpp
    game/libs/NitroSDK/src/fx/fx_atan.c
    game/libs/NitroSDK/src/fx/fx_trig.c
    game/libs/NitroSDK/src/fx/fx_mtx22.c
    game/libs/NitroSDK/src/fx/fx_mtx33.c
    game/libs/NitroSDK/src/fx/fx_mtx43.c
    game/libs/NitroSDK/src/fx/fx_mtx44.c
    game/libs/NitroSDK/src/fx/fx_vec.c
    game/libs/NitroSDK/src/std/std_string.c
    native/Assets.cpp
    native/AssetPack.cpp
    native/GameInstall.cpp
    native/Archive.cpp
    native/Compression.cpp
    native/GameServices.cpp
    native/Input.cpp
    native/Texture.cpp
    native/DualScreen.cpp
    native/Settings.cpp
    native/MobileSound.cpp
    native/Profile.cpp)
target_include_directories(dqiv_core PUBLIC native native/compat game/include game/libs/NitroSDK/include game/libs/NitroSystem/include)
target_compile_definitions(dqiv_core PUBLIC DQIV_NATIVE=1)
target_link_libraries(dqiv_core PUBLIC dqiv_vorbis)
add_library(dqiv_audio STATIC native/AudioOutput.cpp)
target_link_libraries(dqiv_audio PUBLIC dqiv_core SDL3::SDL3)

# Packs the game resources into the executable (dist\DragonQuestIV.exe).
add_executable(dqiv_pack native/PackMain.cpp)
target_link_libraries(dqiv_pack PRIVATE dqiv_core)

# The original game (decompiled sources), each DS overlay in its own sections.
include(cmake/NativeOverlays.cmake)
file(GLOB_RECURSE DQIV_GAME_SOURCES CONFIGURE_DEPENDS "game/src/*.cpp")
list(FILTER DQIV_GAME_SOURCES EXCLUDE REGEX "/ModulePadding_[^/]+\\.cpp$")
list(FILTER DQIV_GAME_SOURCES EXCLUDE REGEX "/main/main\\.cpp$")
list(FILTER DQIV_GAME_SOURCES EXCLUDE REGEX "/dss/(Random|Utf8Iterator|Pad|DssFix|DssVector|DssCore|UnkMemory)\\.cpp$")
list(FILTER DQIV_GAME_SOURCES EXCLUDE REGEX "/(DataObject|GlobalDQ4)\\.cpp$")
add_library(dqiv_game STATIC EXCLUDE_FROM_ALL ${DQIV_GAME_SOURCES})
target_link_libraries(dqiv_game PUBLIC dqiv_core)
add_library(dqiv_platform STATIC native/NitroFilesystem.cpp native/NitroBackup.cpp native/NitroPower.cpp
    native/NitroThreads.cpp native/NitroOverlays.cpp native/NitroIdentity.cpp native/NitroWireless.cpp game/libs/NitroSDK/src/os/os_message.c
    game/libs/NitroSDK/src/wm/wm_gameinfo.c)
target_link_libraries(dqiv_platform PUBLIC dqiv_core)
if(NOT MSVC)
    target_link_options(dqiv_platform INTERFACE -Wl,--wrap=__cxa_atexit)
endif()
add_library(dqiv_sound STATIC native/NitroSound.cpp native/SoundVoices.cpp native/SoundSequence.cpp
    game/libs/NitroSDK/src/snd/snd_main.c
    game/libs/NitroSDK/src/snd/snd_interface.c
    game/libs/NitroSDK/src/snd/snd_command.c
    game/libs/NitroSDK/src/snd/snd_alarm.c
    game/libs/NitroSDK/src/snd/snd_work.c
    game/libs/NitroSDK/src/snd/snd_bank.c
    game/libs/NitroSDK/src/snd/snd_util.c)
target_link_libraries(dqiv_sound PUBLIC dqiv_platform)
file(GLOB_RECURSE DQIV_NNS_SOURCES CONFIGURE_DEPENDS "game/libs/NitroSystem/src/*.c")
add_library(dqiv_nitro_system STATIC EXCLUDE_FROM_ALL ${DQIV_NNS_SOURCES} native/NitroModel.cpp)
target_link_libraries(dqiv_nitro_system PUBLIC dqiv_core)

# The game: a window without console; output and crash reports go to a
# journal beside the save.
add_executable(DragonQuestIV WIN32 native/GameBoot.cpp native/GameWindow.cpp native/GpuRenderer.cpp native/GlLoader.cpp
    native/TextCanvas.cpp native/InputBindings.cpp native/FramePresenter.cpp native/NitroSymbols.cpp native/DssRuntime.cpp
    native/CrashHandler.cpp)
if(WIN32)
    target_sources(DragonQuestIV PRIVATE native/DragonQuestIV.rc)
    target_link_libraries(DragonQuestIV PRIVATE dqiv_game dqiv_nitro_system dqiv_sound dqiv_audio)
else()
    target_include_directories(DragonQuestIV PRIVATE "${CMAKE_BINARY_DIR}/_deps/stb")
    target_link_libraries(DragonQuestIV PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,dqiv_game>" dqiv_nitro_system dqiv_sound dqiv_audio)
endif()
if(NOT MSVC)
    # The PC platform's own code (display composition, 3D, textures, sound
    # mixing) takes the full optimization level; the decompiled game keeps -O2.
    file(GLOB DQIV_NATIVE_SOURCES CONFIGURE_DEPENDS "native/*.cpp")
    set_property(SOURCE ${DQIV_NATIVE_SOURCES} APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Release>:-O3>")
endif()
set(overlay_ids "")
foreach(id RANGE 0 37)
    if(id LESS 10)
        set(directory "ov00${id}")
    else()
        set(directory "ov0${id}")
    endif()
    file(GLOB_RECURSE overlay_sources CONFIGURE_DEPENDS "game/src/${directory}/*.cpp")
    dqiv_overlay_source(${id} dqiv_game ${overlay_sources})
    list(APPEND overlay_ids ${id})
endforeach()
set(main_sources ${DQIV_GAME_SOURCES})
list(FILTER main_sources INCLUDE REGEX "/main/")
dqiv_overlay_source(38 dqiv_game ${main_sources})
dqiv_overlay_registry(DragonQuestIV gameOverlayImages ${overlay_ids} 38)
# Every module initializer must survive archive extraction even when no
# currently linked scene refers to an individual global object by name.
# The DS keeps read-only data in RAM and the game writes to some of it
# (battle effect parameters, for one): constant data stays writable.
if(MSVC)
    target_link_options(DragonQuestIV PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:dqiv_game>" /SECTION:.rdata,RW
        /ENTRY:mainCRTStartup /DEBUG:FULL /INCREMENTAL:NO)
else()
    # Read-only data is made writable at startup (CrashHandler.cpp), past the
    # loader's relocation protection. Symbol names go to the crash journal.
    # The C++ runtime is linked in, as on Windows.
    target_link_options(DragonQuestIV PRIVATE -Wl,-z,norelro -rdynamic -static-libstdc++ -static-libgcc)
    target_link_options(dqiv_pack PRIVATE -static-libstdc++ -static-libgcc)
endif()

if(MSVC)
    target_compile_options(dqiv_core PRIVATE /utf-8 /W4 /wd4068)
    target_compile_options(dqiv_game PRIVATE /utf-8 /wd4068 /Gy /MP8)
    target_compile_options(dqiv_nitro_system PRIVATE /utf-8 /wd4068 /Gy /Gw /MP8)
    target_compile_options(DragonQuestIV PRIVATE /utf-8)
endif()
