#ifdef WIN32
#include <Windows.h>
#endif

#include <GameShared/GameClasses/System/CgsHardwareInit.h>

#include "pc/gcm/renderengine/device.h"
#include "GameSource/Game/BrnGameModule.hpp"

// The top-level game module. In the full engine this is owned by the game's module/heap
// system; here it is the single instance the boot path constructs and drives.
static BrnGame::BrnGameModule gGameModule;

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
    // Create the D3D9 device on the window opened by InitializeHardware, then construct
    // the game's modules (the renderer module builds the loading-screen renderer).
    renderengine::Device::Start();

    gGameModule.Construct();
}

void EngineUpdate()
{
    // Main loop: pump the window messages and, when idle, render a frame through the game
    // module's dispatch (which renders the loading screen). Runs until the window closes.
    // The full engine runs the module update set + a separate dispatch thread here; the
    // threaded module loop is reconstructed with the threading core.
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