#include "device.h"

#include <Windows.h>
#include <d3d9.h>
#include <cstring>
#include <cstdio>   // [diag] BRN_FRAME_DUMP back-buffer BMP writer
#include <cstdlib>  // [diag] atoi -- BRN_FRAME_DUMP_EVERY period override

#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"   // PCInstallDefaultRenderTargetState

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
// THE ANTI-ALIASING KNOB (given its meaning by the anti-aliasing wave, 2026-08-16).
//
// Sourced exactly like gDisplayWidth/gDisplayHeight: seeded here, overwritten from config.ini by
// BrnMain.cpp's LoadConfig (`GetPrivateProfileIntA("Settings", "AntiAliasing", ...)`, clamped to
// [0,16]) and written back by SaveConfig. LoadConfig runs AFTER Device::Initialize (BrnMain.cpp:260
// then :261), so the file value is what survives.
//
// THE VALUES, as the PC render-target leaf reads them
// (renderengine::RenderTarget::Initialize, pc/gcm/renderengine/PostFxRenderTargetPCLeaf.cpp):
//     0  -- USE THE CONSOLE'S OWN MULTISAMPLE FORMAT (the default, and what the shipped X360 build
//           does): the anti-alias buffer's format comes from BrnGraphics::KMSAA_TILING_PLAN, i.e.
//           format 1 == D3DMULTISAMPLE_2_SAMPLES. No other pool target is multisampled.
//     1  -- force the scene target NOT multisampled (the pre-2026-08-16 PC picture, kept as an
//           escape hatch; the frame bracket is unaffected -- see below).
//     2 / 4 / 8 -- force that many samples on the scene target instead of the console's 2.
// Anything the adapter refuses (CheckDeviceMultiSampleType) falls back to the next lower count and
// SAYS SO on the [postfx-rt] line. This is a KNOB: a value other than 0 is the user asking for
// something the console did not do, and it is never chosen silently.
//
// ⚠ IT DOES NOT SELECT THE FRAME BRACKET'S BRANCH, and must not be made to. That is
// BrnRendererModule::mbMultisampledBackbuffer, which is a recovered console constant (1) and not a
// setting -- BrnRendererModule.h. The tiled branch is correct at any sample count including none:
// its two rectangles partition the surface exactly, so the clears and the per-band resolves cover
// the same pixels either way.
//
// ⚠ IT IS ALSO NOT A "0 == OFF" SWITCH, which is the one reading to be careful of. Its Ultimate-Box
// ancestor most likely meant a plain sample count with 0 == off; here 0 has to mean "whatever the
// console did", because the console's answer is ON and this project's default is the console. That
// re-reading is DELIBERATE and is the reason 1 (not 0) is the "off" value. If the TUB setting's own
// semantics are ever recovered, this comment and the leaf's mapping are the two places to change.
s32  renderengine::gAntiAliasing = 0;

// The alpha-to-coverage knob (rung 9). 1 = honour the console's per-material
// ALPHA_TO_MASK request through the D3D9 vendor hook; 0 = never apply it. Semantics and
// the reason it is an off-switch rather than a force-switch are on the declaration in
// device.h. Default 1 because the console's answer IS the request in the shipped
// material data -- the same principle that makes gAntiAliasing default to "whatever the
// console did".
s32  renderengine::gAlphaToCoverage = 1;
// Vertical sync on by default -- see the declaration in device.h.
s32  renderengine::gVSync = 1;
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

    // 0 == "use the console's own multisample format" (see the banner on the definition above), so
    // this seeding is the console default, not an off switch and not a placeholder. LoadConfig runs
    // after this (BrnMain.cpp:260/:261) and is what a config.ini value overrides it with.
    gAntiAliasing = 0;
    // Honour the console's alpha-to-mask requests unless config.ini says otherwise.
    // Seeded here for the same reason gAntiAliasing is: LoadConfig runs after this
    // (BrnMain.cpp:260/:261) and is what a config.ini value overrides it with.
    gAlphaToCoverage = 1;
    // Vertical sync unless config.ini `[Display] VSync=0` (see device.h).
    gVSync = 1;
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
    lPresentParams.PresentationInterval =
        (gVSync != 0) ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;

    if (FAILED(gD3D9->CreateDevice(gAdapterIndex, D3DDEVTYPE_HAL, hWnd, luBehaviorFlags,
                                   &lPresentParams, &gDevice)))
    {
        return;
    }

    // The engine's "device's own surface" state (rw::graphics::postfx::gpDefaultRenderTargetState,
    // X360 dword_83271614) is installed HERE on the console too -- Device::Start is what publishes
    // the front-buffer descriptor. On PC it is the swap chain's back buffer + the auto
    // depth-stencil created above; captured now, while they are exactly what is bound.
    PCInstallDefaultRenderTargetState(static_cast<u32>(gDisplayWidth), static_cast<u32>(gDisplayHeight));

    ShowWindow(hWnd, SW_SHOWNORMAL);
}

// ⭐ 2026-08-16 (boot audit F-P1-9) -- the PC realisation of the console's display-mode
// refresh-rate read. GetAdapterDisplayMode reports 0 for some windowed modes, which is
// D3D's way of saying "whatever the desktop is doing"; the caller keeps its own default in
// that case rather than inventing one here.
namespace renderengine
{
    u32 GetDisplayRefreshRate()
    {
        if (gD3D9 == nullptr)
            return 0u;
        D3DDISPLAYMODE lMode;
        std::memset(&lMode, 0, sizeof(lMode));
        if (FAILED(gD3D9->GetAdapterDisplayMode(static_cast<UINT>(gAdapterIndex), &lMode)))
            return 0u;
        return static_cast<u32>(lMode.RefreshRate);
    }
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

// [diag] present counter shared with the draw-trace diagnostics (CgsIm2d.cpp reads it to
// stamp traced draws with their frame). Plain u32; single render thread on the PC boot.
namespace renderengine { u32 guPresentCount = 0; }

// [diag] BRN_FRAME_DUMP=<dir>: save the back buffer as BMP into <dir> every Nth present
// (PrintWindow returns black against this device, so the game dumps its own frames).
//
// N defaults to 30 and is overridden by BRN_FRAME_DUMP_EVERY=<n>.  ⭐ WHY THIS IS TUNABLE:
// a 30-present period is ~0.4 s on an uncapped PC boot, which is COARSER THAN THE UI
// TRANSITIONS IT IS USED TO JUDGE -- a GUI animation that plays over ~1 s lands in two or
// three samples and is indistinguishable from a pop.  Judging "does this animate?" from a
// 30-present dump is measuring the sampler, not the game.  Set BRN_FRAME_DUMP_EVERY=1 for
// a per-present capture of a transition; leave it unset for the ordinary flow run (657
// frames / 2.3 GB at 30 -- a period of 1 is ~30x that, so use it for short windows).
// ⛔ THE PERIOD IS SHARED, NOT COPIED. Other diagnostics (the CXFORM batch trace in
// CgsImRenderBufferTemplate.cpp) deliberately gate on the SAME presents this writer does, so
// that a logged number and a dumped pixel come from ONE frame -- a trace correlated against a
// dump of a DIFFERENT frame has already produced a false lead in this tree. A second hardcoded
// 30 in those consumers would silently desync the moment the period is overridden, so they call
// this accessor instead.
namespace renderengine
{
    u32 FrameDumpEvery()
    {
        static u32 suEvery = 0u;
        if (suEvery == 0u)
        {
            suEvery = 30u;
            char lacEvery[32];
            DWORD luEveryLen =
                GetEnvironmentVariableA("BRN_FRAME_DUMP_EVERY", lacEvery, sizeof(lacEvery));
            if (luEveryLen != 0 && luEveryLen < sizeof(lacEvery))
            {
                const int liEvery = atoi(lacEvery);
                if (liEvery > 0) { suEvery = static_cast<u32>(liEvery); }
            }
        }
        return suEvery;
    }
}

static void DumpBackBufferIfRequested()
{
    static char sacDir[512];
    static int siChecked = 0;
    if (siChecked == 0)
    {
        siChecked = 1;
        DWORD luLen = GetEnvironmentVariableA("BRN_FRAME_DUMP", sacDir, sizeof(sacDir));
        if (luLen == 0 || luLen >= sizeof(sacDir)) { sacDir[0] = 0; }
    }
    if (sacDir[0] == 0 || (renderengine::guPresentCount % renderengine::FrameDumpEvery()) != 0u)
    {
        return;
    }

    IDirect3DSurface9* lpBack = nullptr;
    if (FAILED(renderengine::gDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &lpBack)) || lpBack == nullptr)
    {
        return;
    }
    D3DSURFACE_DESC lDesc;
    lpBack->GetDesc(&lDesc);
    IDirect3DSurface9* lpSys = nullptr;
    if (SUCCEEDED(renderengine::gDevice->CreateOffscreenPlainSurface(
            lDesc.Width, lDesc.Height, lDesc.Format, D3DPOOL_SYSTEMMEM, &lpSys, nullptr)) &&
        SUCCEEDED(renderengine::gDevice->GetRenderTargetData(lpBack, lpSys)))
    {
        D3DLOCKED_RECT lLock;
        if (SUCCEEDED(lpSys->LockRect(&lLock, nullptr, D3DLOCK_READONLY)))
        {
            char lacPath[600];
            std::snprintf(lacPath, sizeof(lacPath), "%s\\bb_%06u.bmp",
                          sacDir, renderengine::guPresentCount);
            FILE* lpFile = std::fopen(lacPath, "wb");
            if (lpFile != nullptr)
            {
                const u32 luW = lDesc.Width, luH = lDesc.Height;
                const u32 luImageBytes = luW * luH * 4u;
                u8 laHdr[54] = { 'B','M' };
                *reinterpret_cast<u32*>(laHdr + 2)  = 54u + luImageBytes;
                *reinterpret_cast<u32*>(laHdr + 10) = 54u;
                *reinterpret_cast<u32*>(laHdr + 14) = 40u;
                *reinterpret_cast<s32*>(laHdr + 18) = static_cast<s32>(luW);
                *reinterpret_cast<s32*>(laHdr + 22) = -static_cast<s32>(luH);   // top-down
                *reinterpret_cast<u16*>(laHdr + 26) = 1;
                *reinterpret_cast<u16*>(laHdr + 28) = 32;
                *reinterpret_cast<u32*>(laHdr + 34) = luImageBytes;
                fwrite(laHdr, 1, sizeof(laHdr), lpFile);
                for (u32 y = 0; y < luH; ++y)
                {
                    fwrite(static_cast<const u8*>(lLock.pBits) + y * lLock.Pitch, 1, luW * 4u, lpFile);
                }
                fclose(lpFile);
            }
            lpSys->UnlockRect();
        }
    }
    if (lpSys != nullptr) { lpSys->Release(); }
    lpBack->Release();
}

// End the scene and present the back buffer to the window.
void renderengine::Device::ShowPixelBuffer()
{
    if (gDevice == nullptr)
    {
        return;
    }
    gDevice->EndScene();
    DumpBackBufferIfRequested();
    gDevice->Present(nullptr, nullptr, nullptr, nullptr);
    ++renderengine::guPresentCount;
}
