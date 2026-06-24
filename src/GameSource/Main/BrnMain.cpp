#ifdef WIN32
#include <Windows.h>
#endif

#include <GameShared/GameClasses/System/CgsHardwareInit.h>

#include "pc/gcm/renderengine/device.h"
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

// The top-level game module. In the full engine this is owned by the game's module/heap
// system; here it is the single instance the boot path constructs and drives.
static BrnGame::BrnGameModule gGameModule;

// The loading flow (case 8) prepares the game's one GameDataModule through this accessor.
namespace BrnGame { BrnResource::GameDataModule* GetMainGameDataModule() { return &gGameModule.GetGameDataModule(); } }
// The loading flow (stage 4) loads the game's one RootSoundModule through this accessor.
namespace BrnGame { BrnSound::Module::RootSoundModule* GetMainSoundModule() { return &gGameModule.GetSoundModule(); } }

void LoadConfig()
{
    // TODO: Implement LoadConfig
}

void SaveConfig()
{
    // TODO: Implement SaveConfig
}

void EnginePrepare()
{
    // First log line of the run (also guarantees the log file is created on every boot).
    *CgsDev::Log::gpDebugPrint << "==== Burnout Paradise starting ====\n";

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
    // FSM advances its scripted load) -> DispatchThread (render the loading screen). The full
    // engine splits update + dispatch across threads with vsync sync; this runs them inline for
    // the single-threaded boot. Runs until the window closes.
    MSG lMsg;
    ZeroMemory(&lMsg, sizeof(lMsg));
    while (lMsg.message != WM_QUIT)
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
            gGameModule.OnEndOfUpdateFrame();
            gGameModule.DispatchThread();
        }
    }
}

void EngineRelease()
{
    SaveConfig();
    // TODO: Implement EngineRelease
}

void GameRelease()
{
    CgsSystem::HardwareInit::ReleaseHardware();

    // TODO: Implement GameRelease
}

#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
    if (!CgsSystem::HardwareInit::IsAlreadyRunning())
    {
        renderengine::Device::Initialize();
        LoadConfig();

        // TODO: these values are probably supposed to be set in global space or just used directly where needed
        //lpSubKey = "SOFTWARE\\EA Games\\Burnout(TM) Paradise The Ultimate Box\\";
        //lpValueName = "locale";

        CgsSystem::HardwareInit::InitializeHardware(lpCmdLine);

        EnginePrepare();
        EngineUpdate();
        EngineRelease();
        GameRelease();

    }
    return 0;
}
#endif