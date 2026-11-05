// ============================================================================
// b5-decomp/src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates_wN1_01.cpp
//
// Network wave N1, partfile 01 of BrnGameMainFlowStates.cpp:
//
//   LoadingScriptedState::LoadNetworkModule
//     One frame of the network-module load, run by MainGameFlowStateInitialLoadingScreen::Update
//     at E_LOADINGSTAGE_NETWORK with this frame's GameData IO pair. Same shape as its
//     LoadDirectorModule / LoadSoundModule siblings.
//
// ⛔ This TU compiles only once BrnGameModule.hpp includes the real BrnNetworkModule.h in place
// of its placeholder class (the Prepare below is the real module's slot-17 virtual).
// ============================================================================

#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"

#include "GameSource/Game/BrnGameModule.hpp"                              // gpBurnoutGame
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"            // CgsSystem::EFrameRate
#include "GameSource/Resource/BrnGameDataModuleIO.h"                     // GameDataIO::InputBuffer / OutputBuffer
#include "GameSource/Network/BrnNetworkModule.h"                         // BrnNetwork::BrnNetworkModule::Prepare
#include "GameSource/Network/BrnNetworkModuleIO.h"                       // BrnNetworkModuleIO::OutputBuffer

// Console body, in order:
//   * the network frame rate: 50 Hz when the game timer runs at the 50 Hz step (0.02 s),
//     otherwise 60 Hz;
//   * CreateIOBuffer<BrnNetworkModuleIO::OutputBuffer>(update OUTPUT stack, "Network") -- no
//     assert on this path, exactly like the other loaders;
//   * prepared = mNetworkModule.Prepare(networkOut, frameRate, the launch data held at the head
//     of the game module's System360HW, the GameData allocator list)        (vtable +0x44);
//   * still preparing: LockForRead(networkOut); forward its game-data request interface into
//     the GameData input (AppendRequestInterface<256>); UnlockForRead;
//   * DestroyIOBuffer, return prepared.
//
// GetSoftRebootData exposes System360HW's typed LaunchData subobject at +0, the same 0x98-byte
// region XGetLaunchData fills and ARTIST passes to BrnNetworkModule::Prepare.
bool LoadingScriptedState::LoadNetworkModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                                             const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer)
{
    BrnGame::BrnGameModule* lpGameModule = gpBurnoutGame;

    CgsSystem::EFrameRate leFrameRate = CgsSystem::E_FRAMERATE_50HZ;
    if (lpGameModule->GetGameTimer().GetRate() != 0.02f)
    {
        leFrameRate = CgsSystem::E_FRAMERATE_60HZ;
    }

    CgsModule::IOBufferStack* lpUpdateOutputStack = lpGameModule->GetUpdateOutputBufferStack();

    BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutputBuffer = 0;
    lpUpdateOutputStack->CreateIOBuffer<BrnNetwork::BrnNetworkModuleIO::OutputBuffer>(&lpNetworkOutputBuffer, "Network");

    const BrnHW::LaunchData* lpLaunchData =
        &lpGameModule->GetSoftRebootData()->mLaunchData;

    const bool lbPrepared = lpGameModule->GetNetworkModule().Prepare(
        lpNetworkOutputBuffer, leFrameRate, lpLaunchData, lpGameDataOutputBuffer->GetAllocatorList());

    if (!lbPrepared)
    {
        lpNetworkOutputBuffer->LockForRead();
        {
            const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutputRead = lpNetworkOutputBuffer;
            lpGameDataInputBuffer->AppendRequestInterface<256>(*lpNetworkOutputRead->GetGameDataRequestInterface());
        }
        lpNetworkOutputBuffer->UnlockForRead();
    }

    lpUpdateOutputStack->DestroyIOBuffer<BrnNetwork::BrnNetworkModuleIO::OutputBuffer>(&lpNetworkOutputBuffer);
    return lbPrepared;
}
