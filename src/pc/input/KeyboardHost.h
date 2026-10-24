#pragma once

#include <cstdint>

// FLAG PC-platform leaf: lean Win32 imports for the SDL adapter. Keep winuser.h
// out of game TUs that already include windows.h with NOUSER/NOGDI.
extern "C" __declspec(dllimport) void* __stdcall GetForegroundWindow();
extern "C" __declspec(dllimport) int __stdcall PostMessageW(void*, unsigned, std::uintptr_t, std::intptr_t);
extern "C" __declspec(dllimport) unsigned __stdcall MapVirtualKeyW(unsigned, unsigned);
extern "C" __declspec(dllimport) unsigned long __stdcall GetPrivateProfileSectionA(const char*, char*, unsigned long, const char*);
