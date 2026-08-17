#ifdef WIN32
#include <Windows.h>
#include <shlobj.h>   // SHGetSpecialFolderPathA / SHCreateDirectoryExA (getGameSaveDir, TUB 0x53A910)
#endif

#include <GameShared/GameClasses/System/CgsHardwareInit.h>

#include <cstdio>    // std::snprintf (CgsCore::SnPrintf stand-in for the config strings)
#include <cstdlib>   // std::exit (the TUB save-dir failure path)

#include "pc/gcm/renderengine/device.h"
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/PC/CgsAudioOutputPC.h"
#include "GameShared/GameClasses/System/PC/CgsCrashHandlerPC.h"

// ============================================================================================
// The PC boot shell. Two references govern this TU:
//   * BURNOUT_X360_ARTIST.XEX `main` 0x827E60D8 -- the AUTHORITATIVE engine boot spine:
//       stack-test log -> PrintConsoleMemory("Game startup memory usage") ->
//       HardwareInit::InitializeHardware -> Allocators::InitMemoryMap ->
//       BrnRendererModule::DeviceInitialise -> Allocators::Construct ->
//       allocate BrnGameModule (0x9A1300 bytes, align 128, from the boot allocator) ->
//       DebugMemoryInit -> placement-ctor -> store in the game-module global (off_830102D0) ->
//       construct+prepare the FIVE IO buffer stacks (UpdateInput/UpdateOutput 0x780000,
//       ResourceInput 0x40000, ResourceOutput 0x20000, Dispatch 0x18000; align 128) ->
//       gm->Construct() (vtable+0) -> loop gm->Prepare(in,out,resIn,resOut,dispatch)
//       (vtable+64, 0x823DB848) until true -> the frame loop `while (!terminated)
//       gm->vtable+16()` -> loop gm->Release() (vtable+8) until true -> gm->Destruct()
//       (vtable+12).
//   * TUB_Burnout_PC_External.exe WinMain 0x79D580 (+ loadConfig 0x7CC610 / saveConfig
//     0x7CCCD0 / getGameSaveDir 0x53A910) -- the reference for the PC-ONLY shell shape
//     (BPR/TUB are reference-only for platform layers): IsAlreadyRunning guard,
//     Device::Initialize, loadConfig (config.ini), InitializeHardware(lpCmdLine), the
//     memory-map/allocator bring-up, Device::Start, the same game-module allocation +
//     five stacks, Construct, "-skipvideos", the Prepare loop.
// What is still gated here (allocator layer + BrnGameModule::Prepare(5 stacks) + the
// threaded frame pump) is flagged at each site below.
// ============================================================================================

// The top-level game module. [gated] X360 main/TUB WinMain allocate this from the boot
// resource allocator (X360: descriptor 0x9A1300/128 -> off_830102D0; TUB: gHeapResourceAllocator
// -> gpBurnoutGame) after Allocators::Construct; until the allocator layer lands on this path,
// the single instance is a file-static (zero-init BSS + ctor, so vtables/stages are valid).
static BrnGame::BrnGameModule gGameModule;

// The loading flow (case 8) prepares the game's one GameDataModule through this accessor.
namespace BrnGame { BrnResource::GameDataModule* GetMainGameDataModule() { return &gGameModule.GetGameDataModule(); } }
// The loading flow (stage 4) loads the game's one RootSoundModule through this accessor.
namespace BrnGame { BrnSound::Module::RootSoundModule* GetMainSoundModule() { return &gGameModule.GetSoundModule(); } }
// The game-module global itself (X360 off_830102D0 / TUB gpBurnoutGame); the scripted module
// loads read the update IO stacks through it.
namespace BrnGame { BrnGameModule* GetMainGameModule() { return &gGameModule; } }

#ifdef WIN32
namespace
{
    // PC configuration path. Keep the decomp's config alongside its executable so it
    // stays self-contained and never reads or overwrites the retail game's AppData INI.
    //
    // The historical helper name is retained because LoadConfig() and SaveConfig()
    // already call it.
    void getGameSaveDir(char* lpcPath, const char* lpcFile)
    {
        const DWORD luPathLength = GetModuleFileNameA(nullptr, lpcPath, MAX_PATH);
        if (luPathLength == 0 || luPathLength >= MAX_PATH)
        {
            lpcPath[0] = '\0';
            return;
        }

        // Strip the executable filename, retaining the trailing slash.
        DWORD luDirectoryLength = luPathLength;
        while (luDirectoryLength > 0 &&
               lpcPath[luDirectoryLength - 1] != '\\' &&
               lpcPath[luDirectoryLength - 1] != '/')
        {
            --luDirectoryLength;
        }

        if (luDirectoryLength == 0)
        {
            lpcPath[0] = '\0';
            return;
        }

        lpcPath[luDirectoryLength] = '\0';
        strncat(lpcPath, lpcFile, MAX_PATH - luDirectoryLength - 1);
    }
}
#endif

// TUB loadConfig 0x7CC610: ensure the save directory exists (error box + exit(-808) on
// failure), then read config.ini. Implemented for the settings whose globals are
// reconstructed (renderengine::gDisplayWidth/gDisplayHeight/gAdapterIndex/gAntiAliasing).
// [gated] the remaining TUB reads -- AspectRatio (a 7-entry name table + gAspectRatioIndex
// match loop), AdapterName, Shadows/EnvironmentMap/MotionBlur/Textures (0..2 clamps),
// GammaCorrection (0..3.0f), Brightness/Contrast (0..100), SSAO, NumMonitors/CarMonitor,
// HUDFullWidth, OverallQuality, the Telemetry checksum block -- wait on their globals'
// homes in the PC renderer/settings layer.
void LoadConfig()
{
#ifdef WIN32
    char lacPath[MAX_PATH];
    getGameSaveDir(lacPath, "");
    int liResult = SHCreateDirectoryExA(0, lacPath, 0);
    if (liResult != ERROR_SUCCESS && liResult != ERROR_ALREADY_EXISTS && liResult != ERROR_FILE_EXISTS)
    {
        char lacText[260];
        std::snprintf(lacText, sizeof(lacText),
                      "Error trying to create Directory for the Save Game : %s\n\n Error Code : %d",
                      lacPath, liResult);
        MessageBoxA(0, lacText, "Directory Creation Error", MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
        std::exit(-808);
    }

    getGameSaveDir(lacPath, "config.ini");
    renderengine::gDisplayWidth  = GetPrivateProfileIntA("Display", "Width",  renderengine::gDisplayWidth,  lacPath);
    renderengine::gDisplayHeight = GetPrivateProfileIntA("Display", "Height", renderengine::gDisplayHeight, lacPath);
    renderengine::gAdapterIndex  = GetPrivateProfileIntA("Display", "AdapterIndex", renderengine::gAdapterIndex, lacPath);
    // Present sync (device.h gVSync): 1 = vertical sync (default), 0 = immediate presents.
    renderengine::gVSync = (GetPrivateProfileIntA("Display", "VSync", renderengine::gVSync, lacPath) == 0) ? 0 : 1;

    // TUB clamps AntiAliasing to [0,16].
    s32 liAntiAliasing = GetPrivateProfileIntA("Settings", "AntiAliasing", renderengine::gAntiAliasing, lacPath);
    if (liAntiAliasing < 0)  liAntiAliasing = 0;
    if (liAntiAliasing > 16) liAntiAliasing = 16;
    renderengine::gAntiAliasing = liAntiAliasing;

    // The alpha-to-coverage off switch (rung 9); 0/1, anything else clamped to 1 because
    // the setting has no third state -- see the declaration in device.h.
    const s32 liAlphaToCoverage =
        GetPrivateProfileIntA("Settings", "AlphaToCoverage", renderengine::gAlphaToCoverage, lacPath);
    renderengine::gAlphaToCoverage = (liAlphaToCoverage == 0) ? 0 : 1;

    // The environment-map (car-reflection) off switch (reflections step 1); 0/1, clamped the same
    // way and for the same reason as its neighbour -- the setting has no third state. It SEEDS the
    // console's own RendererIO::RenderSwitches::mbRenderEnvmap on the first Render frame; see the
    // declaration in device.h.
    const s32 liEnvironmentMap =
        GetPrivateProfileIntA("Settings", "EnvironmentMap", renderengine::gEnvironmentMap, lacPath);
    renderengine::gEnvironmentMap = (liEnvironmentMap == 0) ? 0 : 1;

    // The env-map REFRESH SCHEDULE (reflections step 2): 1 = three faces per frame (PC default, a
    // documented perf deviation -- see device.h), 0 = the console's all six every frame. 0/1 only.
    const s32 liEnvironmentMap30Hz =
        GetPrivateProfileIntA("Settings", "EnvironmentMap30Hz", renderengine::gEnvironmentMap30Hz, lacPath);
    renderengine::gEnvironmentMap30Hz = (liEnvironmentMap30Hz == 0) ? 0 : 1;
#endif
}

// TUB saveConfig 0x7CCCD0: validateMultiMonitors [gated -- multi-monitor globals], then write
// config.ini. Mirrors LoadConfig's reconstructed subset; the remaining TUB writes are gated
// with their reads (see LoadConfig).
void SaveConfig()
{
#ifdef WIN32
    char lacPath[MAX_PATH];
    getGameSaveDir(lacPath, "config.ini");
    char lacValue[128];
    std::snprintf(lacValue, sizeof(lacValue), "%d", renderengine::gDisplayWidth);
    WritePrivateProfileStringA("Display", "Width", lacValue, lacPath);
    std::snprintf(lacValue, sizeof(lacValue), "%d", renderengine::gDisplayHeight);
    WritePrivateProfileStringA("Display", "Height", lacValue, lacPath);
    std::snprintf(lacValue, sizeof(lacValue), "%d", renderengine::gAdapterIndex);
    WritePrivateProfileStringA("Display", "AdapterIndex", lacValue, lacPath);
    std::snprintf(lacValue, sizeof(lacValue), "%d", renderengine::gVSync);
    WritePrivateProfileStringA("Display", "VSync", lacValue, lacPath);
    std::snprintf(lacValue, sizeof(lacValue), "%d", renderengine::gAntiAliasing);
    WritePrivateProfileStringA("Settings", "AntiAliasing", lacValue, lacPath);
    std::snprintf(lacValue, sizeof(lacValue), "%d", renderengine::gAlphaToCoverage);
    WritePrivateProfileStringA("Settings", "AlphaToCoverage", lacValue, lacPath);
    std::snprintf(lacValue, sizeof(lacValue), "%d", renderengine::gEnvironmentMap);
    WritePrivateProfileStringA("Settings", "EnvironmentMap", lacValue, lacPath);
    std::snprintf(lacValue, sizeof(lacValue), "%d", renderengine::gEnvironmentMap30Hz);
    WritePrivateProfileStringA("Settings", "EnvironmentMap30Hz", lacValue, lacPath);
#endif
}

// The installed locale string TUB's WinMain reads out of the registry (boot audit F-P0-9).
// Published here rather than kept local so the language select has somewhere to read it from
// the day its locale->langid mapping lands; empty means "not installed / not readable", which
// is the state every dev build is in.
char gacInstalledLocale[64] = { 0 };

void EnginePrepare()
{
    // First log line of the run (also guarantees the log file is created on every boot).
    *CgsDev::Log::gpDebugPrint << "==== Burnout Paradise starting ====\n";

    // ⭐ THE STACK TEST, restored 2026-08-17 (boot audit F-P0-11). main @0x827E6100-8C opens
    // the run with it, guarded on message-filter bit 0 exactly as here:
    //     "Stack test:" << &<a stack local> << ", " << <that local's value> << "\n"
    // It prints the ADDRESS of a frame local and then the word already sitting at it -- a
    // one-line probe of where the stack is and what it came up holding. The value is not
    // initialised before the print in the console body either; that is the point of it.
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        s32 liStackProbe;                                   // deliberately uninitialised
        *CgsDev::Log::gpDebugPrint << "Stack test:"
                                   << static_cast<void*>(&liStackProbe)
                                   << ", " << liStackProbe << "\n";
    }

    // [FLAG] the line after it, BrnResource::PrintConsoleMemory("Game startup memory usage")
    // @0x827E6198, is NOT restored: it walks the memory map's carves to report their usage,
    // and the memory map is the absent allocator layer (F-P0-1). It is the first consumer to
    // wire once InitMemoryMap lands, because it is what makes a bad carve visible at boot
    // rather than as an overflow much later.

    // [gated] X360 main 0x827E60D8 / TUB WinMain run the memory-map + allocator bring-up here
    // (Allocators::InitMemoryMap / MemoryMap::FixUp, Allocators::Construct + the global
    // graphics/resource/system/debug carves), then allocate the game module + the five IO
    // buffer stacks from it and drive gm->Prepare(5 stacks) (vtable+64, X360 0x823DB848) in a
    // loop until prepared. Until the allocator layer + that Prepare land, the game module is
    // the file-static above and the update stacks are scratch-backed in BrnGameModule::
    // Construct.

    // Create the D3D9 device on the window opened by InitializeHardware, then construct
    // the game's modules (the renderer module builds the loading-screen renderer).
    renderengine::Device::Start();

    gGameModule.Construct();
}

void EngineUpdate()
{
    // Main loop: pump the window messages and, when idle, drive the real per-frame spine -
    // OnCompletionOfVsyncWait (decide this frame's sim-step count) -> UpdateThread (GamePrepare
    // once, then GameMain, which runs the active flow state's per-substep Update -> the loading
    // FSM advances its scripted load) -> DispatchThread (render the loading screen). Runs until
    // the window closes.
    //
    // [gated] the X360 frame loop is `while (!*(gm+10094112)) gm->vtable+16(gm)` (main
    // 0x827E60D8): one virtual per frame that drives the update/dispatch/resource THREADS
    // through CgsSystem::ThreadLayout with vsync sync; the terminated flag at +10094112 ends
    // it. Until the threading core + that vtable entry land, the same IThreadClass hooks run
    // inline single-threaded here.
    //
    // The loop ends on EITHER the WM_QUIT this pump retrieves itself OR the hardware's
    // shutdown request. Both are needed: the request flag (raised by the window procedure on
    // WM_CLOSE/WM_DESTROY -- CgsSystem::HardwareInit::RequestShutdown) is the AUTHORITATIVE
    // end-of-run signal, because a modal host loop that owns its own message pump for a while
    // (the assert screen, CgsDev::Assert::Manager::DisplayAssertScreen) receives the close and
    // therefore the WM_QUIT, which never reaches this pump at all. That is exactly how a closed
    // window used to leave the process running headless with the menu-music voice still sounding.
    MSG lMsg;
    ZeroMemory(&lMsg, sizeof(lMsg));
    while (lMsg.message != WM_QUIT && !CgsSystem::HardwareInit::IsHardwareWantingToShutdown())
    {
        if (PeekMessageA(&lMsg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&lMsg);
            DispatchMessageA(&lMsg);
        }
        else
        {
            gGameModule.OnStartOfUpdateFrame();
            gGameModule.OnCompletionOfVsyncWait();
            gGameModule.UpdateThread();
            // NOTE: the render feed (X360 BrnGameModule::DoDispatch @0x823DC458) is NOT
            // driven from here. Its only console caller is MainGameFlowStateInGame::Render
            // @0x823E79B8, i.e. the active main-flow state's Render slot, which
            // BrnGameModule::GameMain already runs once per frame from inside UpdateThread()
            // above -- so it is reached exactly in the IN_GAME state and nowhere else. That
            // still satisfies the ordering the GDL ring needs (the lists are written after
            // the world update and before OnEndOfUpdateFrame's swap publishes the frame to
            // the render side). Calling it here as well drove the world producer through the
            // boot logos, the legal screens, the title and the menus, where the console
            // renders no world at all.
            gGameModule.OnEndOfUpdateFrame();
            gGameModule.DispatchThread();
        }
    }
}

void EngineRelease()
{
    // Silence the output device FIRST -- before anything slower in the teardown gets a chance
    // to run. AudioOutputPC::Close stops and destroys the source voice (so the XAudio2 audio
    // thread stops pulling the movie / menu-music / speech fills), destroys the mastering
    // voice, releases the engine and unloads xaudio2_9.dll. Without it the voice kept sounding
    // for as long as the process lived.
    CgsSystem::AudioOutputPC::Close();

    SaveConfig();

    // [gated] X360 main 0x827E60D8 ends with `while (!gm->Release());` (vtable+8 --
    // BrnGameModule::Release, reconstructed) then gm->Destruct() (vtable+12 -- also
    // reconstructed). Driving them now would hang: the placeholder engine modules
    // (Input/World/GameState/Director/Effects/Network) release through the module base,
    // whose ReleaseDataStructures placeholder reports false forever (and their real
    // release ordering needs the un-modelled game-module state words). Wire the loop in
    // when the placeholder modules grow real Release paths.
}

void GameRelease()
{
    CgsSystem::HardwareInit::ReleaseHardware();

    // [gated] the X360 gm->Destruct() (0x823BC868, reconstructed as BrnGameModule::Destruct)
    // belongs here, but it destructs every engine module including the placeholders (see
    // EngineRelease). The process exits right after, so the teardown is deferred with it.
}

#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
    // TUB WinMain 0x79D580 shape: single-instance guard, device enumeration, config.ini,
    // hardware/window init, then the engine spine.
    // Host-only: last-chance crash reporter (BrnGame.log block + BrnCrash.png) -- first,
    // so even an init-path fault is captured.
    CgsSystem::CrashHandler::Install();
    if (!CgsSystem::HardwareInit::IsAlreadyRunning())
    {
        renderengine::Device::Initialize();
        LoadConfig();

        // ⭐ 2026-08-16 (boot audit F-P0-9). TUB's WinMain @0x79D580 reads the installed
        // locale out of the registry here -- HKLM "SOFTWARE\\EA Games\\Burnout(TM) Paradise
        // The Ultimate Box\\", value "locale" -- and the language select keys off it. We did
        // not read it at all.
        //
        // [FLAG] the read is restored; the MAPPING from that locale string to a
        // language-table id is not. The GUI's bundle choice is still the host-static
        // "LANGUAGE/0002.bundle" (BrnGuiModule.cpp), and the locale->langid table is not
        // attested in anything I can read -- inventing one would put the wrong strings on
        // screen in a way nobody would notice until a non-English install. So the value is
        // read and published for the consumer that will need it, and the gap is named.
        {
            HKEY lhKey = 0;
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                              "SOFTWARE\\EA Games\\Burnout(TM) Paradise The Ultimate Box",
                              0, KEY_READ, &lhKey) == ERROR_SUCCESS)
            {
                char  lacLocale[64] = { 0 };
                DWORD luType = 0;
                DWORD luSize = sizeof(lacLocale) - 1;
                if (RegQueryValueExA(lhKey, "locale", 0, &luType,
                                     reinterpret_cast<LPBYTE>(lacLocale), &luSize) == ERROR_SUCCESS &&
                    luType == REG_SZ)
                {
                    gacInstalledLocale[0] = 0;
                    for (DWORD lu = 0; lu < sizeof(gacInstalledLocale) - 1 && lacLocale[lu] != 0; ++lu)
                    {
                        gacInstalledLocale[lu]     = lacLocale[lu];
                        gacInstalledLocale[lu + 1] = 0;
                    }
                }
                RegCloseKey(lhKey);
            }
        }

        CgsSystem::HardwareInit::InitializeHardware(lpCmdLine);

        // The XAudio2 PC backend (CgsSystem::AudioOutputPC) is opened on demand by the first
        // real audio source -- the boot-movie sound streams (BrnGui::MovieManager ->
        // CgsSystem::MovieAudioPC).

        EnginePrepare();

        // ⭐ 2026-08-16 (boot audit F-P0-10). TUB's WinMain @0x79D580 latches "-skipvideos"
        // off lpCmdLine into the game module right after Construct. This was gated on "the
        // mbSkipVideos member and its boot-video consumer are not in this layout yet" -- both
        // exist now, so the latch is real: BrnGui::BootVideos short-circuits to DONE and
        // posts the phase-complete command, the same exit it already takes for a soft reboot.
        // Placed after EnginePrepare because that is what runs Construct on this build.
        if (lpCmdLine != 0)
        {
            for (const char* lpc = lpCmdLine; *lpc != 0; ++lpc)
            {
                if (_strnicmp(lpc, "-skipvideos", 11) == 0)
                {
                    gGameModule.SetSkipVideos(true);
                    break;
                }
            }
        }
        EngineUpdate();
        EngineRelease();
        GameRelease();

    }
    return 0;
}
#endif
