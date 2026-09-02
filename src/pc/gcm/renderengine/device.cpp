#include "device.h"

#include <Windows.h>
#include <d3d9.h>
#include <cstring>
#include <cstdio>   // [diag] BRN_FRAME_DUMP back-buffer BMP writer
#include <cstdlib>  // [diag] atoi -- BRN_FRAME_DUMP_EVERY period override
#include <string.h> // [diag] _stricmp -- BRN_FRAME_DUMP_ARM mode select (MSVC canonical)

#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"   // PCInstallDefaultRenderTargetState
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [diag] Device::Start failure paths
#include "GameSource/Jobs/Traffic/BrnTrafficSwerveWatch.h"  // [diag] BRN_FRAME_DUMP_ARM
#include "GameShared/GameClasses/Development/BrnDiagFilmLatch.h" // [diag] BRN_FRAME_DUMP_ARM=slomo

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
// The environment-map (car-reflection) pass knob. 1 = run the console's own six-face pass and bind
// sampler 13; 0 = neither. Semantics, and why this is a SEED for the console's own
// RenderSwitches::mbRenderEnvmap rather than a second switch, are on the declaration in device.h.
// Default 1 because the console's answer is 1 (BrnRendererModule::ConstructRenderSwitches) -- the
// same principle that makes gAntiAliasing default to "whatever the console did".
s32  renderengine::gEnvironmentMap = 1;
// Three faces per frame by default on PC -- the perf deviation is documented on the declaration.
s32  renderengine::gEnvironmentMap30Hz = 1;
// The corona (light-flare) pass knob. 1 = draw it; 0 = never. Semantics, and why this is a SEED for
// BrnRendererModule::mbRenderCoronas rather than a second switch, are on the declaration in
// device.h. Default 1 because the console's answer is 1 (ConstructRenderSwitches) -- the same
// principle that makes gAntiAliasing default to "whatever the console did".
s32  renderengine::gCoronas = 1;
// The sun-corona pass knob. 1 = run it; 0 = never. Semantics, and why this is a SEED for
// BrnSunCorona::mbRenderSunCorona rather than a second switch, are on the declaration in device.h.
// Default 1 because the console's answer is 1 (BrnSunCorona::Construct @0x824009EC).
s32  renderengine::gSunCorona = 1;
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
    // Run the environment-map pass unless config.ini `[Settings] EnvironmentMap=0`. Seeded here for
    // the same reason the two above are: LoadConfig runs after this (BrnMain.cpp:260/:261) and is
    // what a config.ini value overrides it with.
    gEnvironmentMap = 1;
    // Half-schedule the env-map refresh unless config.ini `[Settings] EnvironmentMap30Hz=0`.
    gEnvironmentMap30Hz = 1;
    // Draw the corona pass unless config.ini `[Settings] Coronas=0`. Seeded here for the same
    // reason its neighbours are: LoadConfig runs after this (BrnMain.cpp:260/:261) and is what a
    // config.ini value overrides it with.
    gCoronas = 1;
    // Run the sun-corona pass unless config.ini `[Settings] SunCorona=0`. Seeded here for the same
    // reason its neighbours are: LoadConfig runs after this and is what a config.ini value
    // overrides it with.
    gSunCorona = 1;
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
    // [diag 2026-08-27] every early-return below used to be SILENT, and a failed Start is
    // unrecoverable (nothing retries CreateDevice): the run boots to the end of the load
    // ladder with gDevice null, renders nothing, and the first movie never acquires --
    // indistinguishable in the log from a flow bug. Name the exact step that failed.
    if (hWnd == nullptr)
    {
        CgsDev::Log::WriteToLog("[device] Start: hWnd is null (InitializeHardware made no window) -- NO DEVICE this run\n");
        return;
    }

    gD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (gD3D9 == nullptr)
    {
        CgsDev::Log::WriteToLog("[device] Start: Direct3DCreate9 returned null -- NO DEVICE this run\n");
        return;
    }

    D3DCAPS9 lCaps;
    HRESULT lhCapsResult = gD3D9->GetDeviceCaps(gAdapterIndex, D3DDEVTYPE_HAL, &lCaps);
    if (FAILED(lhCapsResult))
    {
        char lacMsg[128];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[device] Start: GetDeviceCaps failed hr=0x%08X -- NO DEVICE this run\n",
                      static_cast<unsigned>(lhCapsResult));
        CgsDev::Log::WriteToLog(lacMsg);
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

    HRESULT lhCreateResult = gD3D9->CreateDevice(gAdapterIndex, D3DDEVTYPE_HAL, hWnd,
                                                 luBehaviorFlags, &lPresentParams, &gDevice);
    if (FAILED(lhCreateResult))
    {
        char lacMsg[128];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[device] Start: CreateDevice failed hr=0x%08X -- NO DEVICE this run\n",
                      static_cast<unsigned>(lhCreateResult));
        CgsDev::Log::WriteToLog(lacMsg);
        return;
    }
    CgsDev::Log::WriteToLog("[device] Start: device created, window shown.\n");

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

    // [diag] BRN_FRAME_DUMP_ARM=1 -- HOLD the writer until the traffic swerve camera latches,
    // and BRN_FRAME_DUMP_MAX=<n> -- stop after n frames. Both default off, so an ordinary
    // BRN_FRAME_DUMP run is byte-for-byte unchanged.
    //
    // ⭐ WHY, MEASURED. Dumping every 2nd present for a 130 s run writes ~16 GB, and this
    // simulation is FRAME-COUPLED: the run that filmed itself that hard produced ZERO traffic
    // swerves and zero junction-FUP action, where the SAME BINARY unfilmed produced seven
    // swerves and nine RemoveVehicle removals. Filming the whole run destroys the event the
    // film exists to show. Armed + capped, the capture is a few seconds at every present.
    {
        // BRN_FRAME_DUMP_ARM selects WHICH latch holds the writer:
        //   unset / "0"  -- no arm; dump from the first present (the original behaviour)
        //   "slomo"      -- hold until the simulation timestep leaves real time
        //                   (BrnDiag::gFilmLatch, raised by BrnGameModule::UpdateTimers)
        //   anything else truthy -- hold until the traffic swerve camera latches (the
        //                   original arm; unchanged, so every existing recipe still works)
        static int siArm = -1;      // 0 none, 1 swerve camera, 2 slomo
        static u32 suMax = 0u;
        if (siArm < 0)
        {
            char lacArm[32];
            DWORD luArmLen = GetEnvironmentVariableA("BRN_FRAME_DUMP_ARM", lacArm, sizeof(lacArm));
            if (luArmLen == 0 || luArmLen >= sizeof(lacArm) || lacArm[0] == '0')
            {
                siArm = 0;
            }
            else if (_stricmp(lacArm, "slomo") == 0)
            {
                siArm = 2;
            }
            else
            {
                siArm = 1;
            }

            char lacMax[32];
            DWORD luMaxLen = GetEnvironmentVariableA("BRN_FRAME_DUMP_MAX", lacMax, sizeof(lacMax));
            if (luMaxLen != 0 && luMaxLen < sizeof(lacMax))
            {
                const int liMax = atoi(lacMax);
                if (liMax > 0) { suMax = static_cast<u32>(liMax); }
            }
        }
        if (siArm == 1 && BrnTraffic::gSwerveWatch.muCameraLatched == 0u)
        {
            return;
        }
        if (siArm == 2 && BrnDiag::gFilmLatch.muSlomoLatched == 0u)
        {
            return;
        }
        if (suMax != 0u)
        {
            static u32 suWritten = 0u;
            if (suWritten >= suMax) { return; }
            ++suWritten;
        }
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
                *reinterpret_cast<u32*>(laHdr + 2)  = 54u + luImageBytes;       // BMP file-format header blob
                *reinterpret_cast<u32*>(laHdr + 10) = 54u;                      // BMP file-format header blob
                *reinterpret_cast<u32*>(laHdr + 14) = 40u;                      // BMP file-format header blob
                *reinterpret_cast<s32*>(laHdr + 18) = static_cast<s32>(luW);    // BMP file-format header blob
                *reinterpret_cast<s32*>(laHdr + 22) = -static_cast<s32>(luH);   // top-down BMP file-format header blob
                *reinterpret_cast<u16*>(laHdr + 26) = 1;                        // BMP file-format header blob
                *reinterpret_cast<u16*>(laHdr + 28) = 32;                       // BMP file-format header blob
                *reinterpret_cast<u32*>(laHdr + 34) = luImageBytes;             // BMP file-format header blob
                fwrite(laHdr, 1, sizeof(laHdr), lpFile);
                for (u32 y = 0; y < luH; ++y)
                {
                    fwrite(static_cast<const u8*>(lLock.pBits) + y * lLock.Pitch, 1, luW * 4u, lpFile);
                }
                fclose(lpFile);

                // [diag] Stamp the SIMULATION timestep this frame was rendered under into a
                // sidecar CSV beside the dump. ⭐ WITHOUT IT A CAPTURE OF A TIME DILATION
                // CANNOT BE READ: a drive-thru / crash dilation scales the SIM timer only, so
                // the camera and the HUD keep running at full rate and no eye can tell a
                // dilated frame from an ordinary one. With it, every frame names its own
                // timestep and a strip is self-labelling.
                char lacCsv[620];
                std::snprintf(lacCsv, sizeof(lacCsv), "%s\\frames.csv", sacDir);
                FILE* lpCsv = std::fopen(lacCsv, "a");
                if (lpCsv != nullptr)
                {
                    // Column 4 is the LIVE BOOST/SHOWTIME METER FRACTION the GUI was last
                    // handed (-1 == none published yet). Same argument as columns 2-3: a
                    // bitmap of a bar cannot say whether the bar is tracking anything, and
                    // the log ticks on SIM frames while this ticks on PRESENTS, so pairing
                    // them by time means guessing a frame rate. Stamped here, each frame
                    // names the value it is supposed to be drawing.
                    // Columns 5-10 are THE CHAIN-CRASHING TRAFFIC CAR: its index, its
                    // sympathetic-crash state (1 HEADON 2 ACCEL 3 HANDBRAKE 4 LOCKUP), its
                    // live world position, and the publish counter. Same argument as columns
                    // 2-4 and the same reason they exist: a bitmap of a pile-up cannot say
                    // WHICH car chose to crash, and pairing a log line to a frame means
                    // guessing a frame rate (the log ticks on SIM frames, this ticks on
                    // PRESENTS). With the position stamped into the row, a marker can be
                    // projected from the car's own coordinates into its own frame -- the
                    // trick that turned "a traffic car swerved" into a measured 56 deg over
                    // 13.2 m. Column 10 makes staleness visible: a row whose count equals the
                    // previous row's carries a position nothing refreshed that frame.
                    std::fprintf(lpCsv, "%u,%.6f,%.6f,%.6f,%d,%d,%.3f,%.3f,%.3f,%u\n",
                                 renderengine::guPresentCount,
                                 BrnDiag::gFilmLatch.mfLiveSimScale,
                                 BrnDiag::gFilmLatch.mfLiveSimStep,
                                 BrnDiag::gFilmLatch.mfLiveBoostFraction,
                                 BrnTraffic::gSwerveWatch.miSympVehicle,
                                 BrnTraffic::gSwerveWatch.miSympState,
                                 BrnTraffic::gSwerveWatch.mfSympPosX,
                                 BrnTraffic::gSwerveWatch.mfSympPosY,
                                 BrnTraffic::gSwerveWatch.mfSympPosZ,
                                 BrnTraffic::gSwerveWatch.muSympPublishes);
                    std::fclose(lpCsv);
                }
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
