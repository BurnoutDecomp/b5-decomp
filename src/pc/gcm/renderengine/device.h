#pragma once

#include "types.hpp"
#include <Windows.h>

struct IDirect3D9;
struct IDirect3DDevice9;

namespace renderengine
{
    extern bool gFullscreen;
    extern s32 gDisplayWidth;
    extern s32 gDisplayHeight;
    extern s32 gAdapterIndex;
    extern s32 gAspectRatioIndex;
    extern s32 gAntiAliasing;
    extern HWND hWnd;

    extern IDirect3D9* gD3D9;
    extern IDirect3DDevice9* gDevice;

    class Device
    {
    public:
        static bool Initialize();
        static void Start();
        static bool FrameBegin();
        static bool FrameBeginNoClear();
        static void ShowPixelBuffer();
    };
}
