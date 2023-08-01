
// include imgui if the specified platform uses it
#if defined(PLATFORM_WIN32) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS) || defined(PLATFORM_WASM) || defined(PLATFORM_GENERIC)
#include "imgui.h"
#include "imgui.cpp"
#include "imgui_demo.cpp"
#include "imgui_draw.cpp"
#include "imgui_tables.cpp"
#include "imgui_widgets.cpp"
#define USE_IMGUI
#endif

#include "chip8emu_platform.h"
#include "chip8emu.h"
#include "chip8emu.cpp"

#if defined(PLATFORM_WIN32)
#include "chip8emu_win32.cpp"
#elif defined(PLATFORM_LINUX)

#elif defined(PLATFORM_MACOS)

#elif defined(PLATFORM_WASM)

#elif defined(PLATFORM_GENERIC)
// some generic 3rd-party cross-platform library impl goes here
#else
#error You must define a platform before compiling!
#endif