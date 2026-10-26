if(MSVC)
# MSVC's custom init_seg suppresses CRT startup construction and redirects
# destructor registration. Writable data and local-static guards have their own
# sections, so a scene reload starts from the pre-constructor image.
# https://learn.microsoft.com/en-us/cpp/preprocessor/init-seg
function(dqiv_overlay_source id target)
    set(header "${CMAKE_CURRENT_BINARY_DIR}/overlays/module${id}.hpp")
    file(GENERATE OUTPUT "${header}" CONTENT "#pragma once\n#pragma warning(disable:4075) // Custom initialization is intentional.\nextern \"C\" int __cdecl pcOverlayExit${id}(void (__cdecl*)());\n#pragma init_seg(\".i$${id}m\", pcOverlayExit${id})\n#pragma data_seg(\".d$${id}m\")\n#pragma bss_seg(\".b$${id}m\")\n")
    foreach(source IN LISTS ARGN)
        set_property(SOURCE "${source}" APPEND PROPERTY COMPILE_OPTIONS
            "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,${target}>:/FI${header}>"
            "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,${target}>:/Gw->"
            "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,${target}>:/Zc:threadSafeInit->")
    endforeach()
endfunction()

function(dqiv_overlay_registry target registry)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/overlays/${registry}.cpp")
    set(code "#include \"NitroOverlays.hpp\"\n#include <span>\n")
    set(entries "")
    foreach(id IN LISTS ARGN)
        string(APPEND code "extern \"C\" int __cdecl pcOverlayExit${id}(pc::OverlayFunction f) { return pcRegisterOverlayDestructor(${id},f); }\n")
        foreach(segment IN ITEMS d)
            string(APPEND code "#pragma section(\".${segment}$${id}a\",read,write)\n#pragma section(\".${segment}$${id}z\",read,write)\n__declspec(allocate(\".${segment}$${id}a\")) unsigned char ${segment}${id}Begin=0;\n__declspec(allocate(\".${segment}$${id}z\")) unsigned char ${segment}${id}End=0;\n")
        endforeach()
        string(APPEND code "#pragma bss_seg(\".b$${id}a\")\nunsigned char b${id}Begin;\n#pragma bss_seg(\".b$${id}z\")\nunsigned char b${id}End;\n#pragma bss_seg()\n")
        string(APPEND code "#pragma section(\".i$${id}a\",read)\n#pragma section(\".i$${id}z\",read)\n__declspec(allocate(\".i$${id}a\")) const pc::OverlayFunction i${id}Begin=nullptr;\n__declspec(allocate(\".i$${id}z\")) const pc::OverlayFunction i${id}End=nullptr;\n")
        string(APPEND entries "{${id},{&d${id}Begin+1,&d${id}End},{&b${id}Begin+1,&b${id}End},&i${id}Begin+1,&i${id}End},\n")
    endforeach()
    string(APPEND code "std::span<const pc::OverlayImage> ${registry}() { static const pc::OverlayImage images[]={\n${entries}};return images;}\n")
    file(GENERATE OUTPUT "${source}" CONTENT "${code}")
    target_sources(${target} PRIVATE "${source}")
endfunction()
else()
# GCC: cmake/overlay-object.sh renames each module object's .data, .bss and
# .init_array to dqiv_d<id>, dqiv_b<id> and dqiv_i<id> after compiling it, so
# constructors no longer run at startup and each module's writable image has
# linker-provided bounds. Local-static guards are plain zeroed data. Destructors
# registered while a module is constructed belong to it (NitroOverlays.cpp,
# with the linker's --wrap=__cxa_atexit).
function(dqiv_overlay_source id target)
    foreach(source IN LISTS ARGN)
        set_property(SOURCE "${source}" APPEND PROPERTY COMPILE_OPTIONS
            "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,${target}>:-DPC_OVERLAY_MODULE=${id}>"
            "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,${target}>:-fno-threadsafe-statics>")
    endforeach()
    set_property(TARGET ${target} PROPERTY CXX_COMPILER_LAUNCHER
        sh "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/overlay-object.sh" "${CMAKE_READELF}" "${CMAKE_OBJCOPY}")
endfunction()

function(dqiv_overlay_registry target registry)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/overlays/${registry}.cpp")
    set(code "#include \"NitroOverlays.hpp\"\n#include <span>\n#define DQIV_BOUNDS(type,name) extern \"C\" type __start_##name[] __attribute__((weak)),__stop_##name[] __attribute__((weak));\n")
    set(entries "")
    foreach(id IN LISTS ARGN)
        string(APPEND code "DQIV_BOUNDS(unsigned char,dqiv_d${id})\nDQIV_BOUNDS(unsigned char,dqiv_b${id})\nDQIV_BOUNDS(const pc::OverlayFunction,dqiv_i${id})\n")
        string(APPEND entries "{${id},{__start_dqiv_d${id},__stop_dqiv_d${id}},{__start_dqiv_b${id},__stop_dqiv_b${id}},__start_dqiv_i${id},__stop_dqiv_i${id}},\n")
    endforeach()
    string(APPEND code "std::span<const pc::OverlayImage> ${registry}() { static const pc::OverlayImage images[]={\n${entries}};return images;}\n")
    file(GENERATE OUTPUT "${source}" CONTENT "${code}")
    target_sources(${target} PRIVATE "${source}")
endfunction()
endif()
