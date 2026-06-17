#include "device.h"

#include <Windows.h>
#include <d3d9.h>
#include <cstring>

// PC / D3D9 renderengine device bring-up, reversed from TUB (Burnout Paradise: The
// Ultimate Box):
//   renderengine::Device::Initialize  @ TUB 0x7CC080  - display + settings init
//   renderengine::Device::Start       @ TUB 0x53E130  - drives initdx (-> initdx9
//                                                       0x9479A0, Direct3DCreate9) and
//                                                       the CreateDevice path (sub_947F10)
// The settings/resolution and the GetDeviceCaps -> CreateDevice flow are faithful; the
// original Device::Start also ran the per-frame render loop, which lives above the
// device and is not reproduced here.

bool renderengine::gFullscreen = false;
s32  renderengine::gDisplayWidth = 640;
s32  renderengine::gDisplayHeight = 480;
s32  renderengine::gAdapterIndex = 0;
s32  renderengine::gAspectRatioIndex = 0;
s32  renderengine::gAntiAliasing = 0;
HWND renderengine::hWnd = nullptr;

IDirect3D9*       renderengine::gD3D9 = nullptr;
IDirect3DDevice9* renderengine::gDevice = nullptr;

// @ TUB 0x7CC080 - detect the desktop resolution and seed the default graphics
// settings. (TUB additionally seeds motion-blur / shadow / env-map / SSAO / texture
// detail etc.; the display-critical subset is reconstructed here.)
bool renderengine::Device::Initialize()
{
    gAspectRatioIndex = 0;
    gDisplayWidth = 800;
    gDisplayHeight = 600;
    gAdapterIndex = 0;

    DEVMODEA lDevMode;
    std::memset(&lDevMode, 0, sizeof(lDevMode));
    lDevMode.dmSize = sizeof(DEVMODEA);
    if (EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &lDevMode))
    {
        gDisplayWidth = static_cast<s32>(lDevMode.dmPelsWidth);
        gDisplayHeight = static_cast<s32>(lDevMode.dmPelsHeight);
    }

    gAntiAliasing = 0;
    // TUB seeds fullscreen=true; forced windowed during the PC bring-up.
    gFullscreen = false;

    // Windowed bring-up renders at the 1280x720 window client size so the back buffer is
    // 1:1 with the window (the desktop res detected above is what TUB uses for fullscreen).
    // The Im2d works in this same 1280x720 logical space, so its map is identity.
    gDisplayWidth = 1280;
    gDisplayHeight = 720;
    return true;
}

// @ TUB 0x53E130 (+ initdx9 0x9479A0 + sub_947F10) - create the D3D9 object and device,
// then show the window.
void renderengine::Device::Start()
{
    gD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (gD3D9 == nullptr)
    {
        return;
    }

    D3DCAPS9 lCaps;
    if (FAILED(gD3D9->GetDeviceCaps(gAdapterIndex, D3DDEVTYPE_HAL, &lCaps)))
    {
        return;
    }

    // Hardware vertex processing when the GPU supports T&L, else software - matching the
    // TUB caps check that selects HARDWARE / SOFTWARE / PURE vertex processing.
    DWORD luBehaviorFlags = (lCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
                                ? D3DCREATE_HARDWARE_VERTEXPROCESSING
                                : D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    D3DPRESENT_PARAMETERS lPresentParams;
    std::memset(&lPresentParams, 0, sizeof(lPresentParams));
    lPresentParams.Windowed = gFullscreen ? FALSE : TRUE;
    // COPY (not DISCARD) preserves the back buffer across Present, so an on-screen overlay drawn
    // outside the normal render loop (the assert dialog: FrameBeginNoClear -> draw -> present) can
    // composite over the last presented frame instead of garbage. Invisible to normal rendering -
    // every game frame opens with FrameBegin's Clear anyway.
    lPresentParams.SwapEffect = D3DSWAPEFFECT_COPY;
    lPresentParams.BackBufferWidth = static_cast<UINT>(gDisplayWidth);
    lPresentParams.BackBufferHeight = static_cast<UINT>(gDisplayHeight);
    lPresentParams.BackBufferFormat = D3DFMT_X8R8G8B8;
    lPresentParams.BackBufferCount = 1;
    lPresentParams.EnableAutoDepthStencil = TRUE;
    lPresentParams.AutoDepthStencilFormat = D3DFMT_D24S8;
    lPresentParams.hDeviceWindow = hWnd;
    lPresentParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    if (FAILED(gD3D9->CreateDevice(gAdapterIndex, D3DDEVTYPE_HAL, hWnd, luBehaviorFlags,
                                   &lPresentParams, &gDevice)))
    {
        return;
    }

    ShowWindow(hWnd, SW_SHOWNORMAL);
}

// Begin a frame: clear to black (the loading screen fades up from black) and open the
// scene so immediate-mode draws are accepted. Returns false if the device is not ready.
bool renderengine::Device::FrameBegin()
{
    if (gDevice == nullptr)
    {
        return false;
    }
    gDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    return SUCCEEDED(gDevice->BeginScene());
}

// Open a scene WITHOUT clearing, so a draw composites over whatever is already in the back buffer
// (the last presented frame, preserved by D3DSWAPEFFECT_COPY). Used by the assert dialog to overlay
// the frozen frame. Returns false if the device is not ready or a scene is already open.
bool renderengine::Device::FrameBeginNoClear()
{
    if (gDevice == nullptr)
    {
        return false;
    }
    return SUCCEEDED(gDevice->BeginScene());
}

// End the scene and present the back buffer to the window.
void renderengine::Device::ShowPixelBuffer()
{
    if (gDevice == nullptr)
    {
        return;
    }
    gDevice->EndScene();
    gDevice->Present(nullptr, nullptr, nullptr, nullptr);
}
