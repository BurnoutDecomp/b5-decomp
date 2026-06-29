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

    // The bound D3D surface-state object (the colour/depth surfaces + their format/size). On X360
    // this is the GPU D3DSURFACES descriptor renderengine::Device::SetState installs; declared here
    // (forward) so the render-target / immediate-mode layers reference it by name. Layout lives in
    // its own renderengine TU.
    class RenderTargetState;

    class Device
    {
    public:
        static bool Initialize();
        static void Start();
        static bool FrameBegin();
        static bool FrameBeginNoClear();
        static void ShowPixelBuffer();

        // Bind a render-target (surface) state on the device (DWARF renderengine::device.h:1042;
        // X360 guest renderengine__Device__SetState). The post-fx render-target wrapper calls this to
        // install its colour/depth surfaces before drawing.
        static void SetState(const RenderTargetState* lpState);
    };
}
