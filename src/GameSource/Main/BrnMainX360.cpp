// Native entry recovered from ARTIST main @0x827E60D8.
// DecFIGS attributes main and gpBurnoutGame to GameSource/Main/BrnMain.cpp.
// The suffix separates this native entry from the host platform's startup.
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Resource/BrnResourceAllocator.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include <new>

#if !defined(_XBOX)
#error BrnMainX360.cpp requires the native Xbox 360 target.
#endif

// Do not turn the old partial game-module declaration into a bootable image.
// DebugMemoryInit writes this entire original object before placement construction.
typedef char BrnMain_requires_original_game_module_size[
    sizeof(BrnGame::BrnGameModule) == 0x9A1300 ? 1 : -1];

BrnGame::BrnGameModule* gpBurnoutGame;

int main(int lnArgc, char** lapcArgv)
{
    volatile u32 luStackValue = 0xABABABAB;
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "Stack test:"
            << const_cast<u32*>(&luStackValue) << ", " << luStackValue << "\n";
    }

    BrnResource::PrintConsoleMemory("Game startup memory usage");
    CgsSystem::HardwareInit::InitializeHardware(lnArgc, lapcArgv, 0, 0);
    BrnResource::Allocators::InitMemoryMap();
    BrnRendererModule::DeviceInitialise();
    BrnResource::Allocators::Construct();

    // off_82F2C854 is mRuntimeAllocator, initialized by ConstructChildAllocators
    // @0x82666E1C. Each call builds all five descriptor lanes and dispatches
    // DoAllocate at allocator slot 4; no allocation name is supplied by ARTIST.
    BrnGame::BrnGameModule* lpNewGame = static_cast<BrnGame::BrnGameModule*>(
        BrnResource::GetRuntimeAllocator()->AllocateMemoryResource(0x9A1300, 128, 0));
    BrnGame::BrnGameModule::DebugMemoryInit(lpNewGame);
    gpBurnoutGame = lpNewGame ? new (lpNewGame) BrnGame::BrnGameModule : 0;

    CgsModule::IOBufferStack gUpdateInputBufferStack;
    CgsModule::IOBufferStack gUpdateOutputBufferStack;
    CgsModule::IOBufferStack gResourceInputBufferStack;
    CgsModule::IOBufferStack gResourceOutputBufferStack;
    CgsModule::IOBufferStack gDispatchThreadIOBufferStack;
    gUpdateInputBufferStack.Construct("UpdateInput");
    gUpdateOutputBufferStack.Construct("UpdateOutput");
    gResourceInputBufferStack.Construct("ResourceInput");
    gResourceOutputBufferStack.Construct("ResourceOutput");
    gDispatchThreadIOBufferStack.Construct("Dispatch");

    gUpdateInputBufferStack.Prepare(
        BrnResource::GetRuntimeAllocator()->AllocateMemoryResource(0x780000, 128, 0), 0x780000, 128);
    gUpdateOutputBufferStack.Prepare(
        BrnResource::GetRuntimeAllocator()->AllocateMemoryResource(0x780000, 128, 0), 0x780000, 128);
    gResourceInputBufferStack.Prepare(
        BrnResource::GetRuntimeAllocator()->AllocateMemoryResource(0x40000, 128, 0), 0x40000, 128);
    gResourceOutputBufferStack.Prepare(
        BrnResource::GetRuntimeAllocator()->AllocateMemoryResource(0x20000, 128, 0), 0x20000, 128);
    gDispatchThreadIOBufferStack.Prepare(
        BrnResource::GetRuntimeAllocator()->AllocateMemoryResource(0x18000, 128, 0), 0x18000, 128);

    // Five inlined GetDebugAllocator checks survive at 827E64CC..827E6590.
    // Their returned pointers are unused; do not allocate more buffers here.
    BrnResource::GetDebugAllocator();
    BrnResource::GetDebugAllocator();
    BrnResource::GetDebugAllocator();
    BrnResource::GetDebugAllocator();
    BrnResource::GetDebugAllocator();

    gpBurnoutGame->Construct();
    while (!gpBurnoutGame->Prepare(&gUpdateInputBufferStack, &gUpdateOutputBufferStack,
                                  &gResourceInputBufferStack, &gResourceOutputBufferStack,
                                  &gDispatchThreadIOBufferStack)
           && !gpBurnoutGame->HasGameTerminated())
    {
    }
    while (!gpBurnoutGame->HasGameTerminated())
        gpBurnoutGame->Update();
    while (!gpBurnoutGame->Release())
    {
    }
    gpBurnoutGame->Destruct();
    // ARTIST returns directly: there is no delete, stack Release/Destruct or
    // additional ReleaseHardware call on this entry's return path.
    return 0;
}
