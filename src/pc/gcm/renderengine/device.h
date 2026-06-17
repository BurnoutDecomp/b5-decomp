#pragma once

#include "types.hpp"
#include <Windows.h>

// COM device interfaces (full definitions pulled in by device.cpp via <d3d9.h>).
struct IDirect3D9;
struct IDirect3DDevice9;

namespace renderengine
{
    extern bool gFullscreen;
    extern s32  gDisplayWidth;
    extern s32  gDisplayHeight;
    extern s32  gAdapterIndex;
    extern s32  gAspectRatioIndex;
    extern s32  gAntiAliasing;
    extern HWND hWnd;

    // The D3D9 object + device created by Device::Start (the PC/D3D9 backend, modelled
    // on TUB's renderengine).
    extern IDirect3D9*       gD3D9;
    extern IDirect3DDevice9* gDevice;

    class Device
    {
    public:
        static bool Initialize();   // detect the display + seed default graphics settings
        static void Start();        // create the D3D9 device + show the window

        // Per-frame bracket. On console these go through the shadowing command-buffer
        // device; on the PC immediate D3D9 backend FrameBegin clears + BeginScene, and
        // ShowPixelBuffer EndScene + Present. (The shadow::Device layer is reconstructed
        // with the threaded dispatch.)
        static bool FrameBegin();       // begin a frame (clears to black); false if the device is not ready
        static bool FrameBeginNoClear();// begin a scene WITHOUT clearing (overlay over the last frame)
        static void ShowPixelBuffer();  // end the scene and present the back buffer
    };
}
