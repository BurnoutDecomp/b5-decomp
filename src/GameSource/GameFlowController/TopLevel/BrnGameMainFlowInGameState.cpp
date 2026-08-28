#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"

#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h"   // GameMainFlowController + EMainGameFlowState
#include "GameSource/Game/BrnGameModule.hpp"                                    // BrnGame::GetMainGameModule / DoUpdate / DoDispatch
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"                       // RootOutputBuffer + RootPreUpdateOutputBuffer (the C4b frame trio)
#include "GameSource/Resource/BrnGameDataModuleIO.h"                            // GameDataIO::InputBuffer (the sound->resource forwards)

// MainGameFlowStateInGame per-frame surface -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU is the DWARF home (BrnGameMainFlowInGameState.cpp) for the in-game flow state's
// OnEnter/OnLeave/Render/Update; the ctor stays with the sibling group in
// BrnGameMainFlowStates.cpp.
//
// Bodied here (4 ledger functions):
//   MainGameFlowStateInGame::OnEnter  @ 0x823AACF8
//   MainGameFlowStateInGame::OnLeave  @ 0x823AAD28
//   MainGameFlowStateInGame::Render   @ 0x823E79B8
//   MainGameFlowStateInGame::Update   @ 0x823F3048

namespace BrnGameMainFlowController
{
    // Option B stand-ins for the game-module aggregate fields the X360 body accesses by
    // absolute offset (off_830102D0 == BrnGame::GetMainGameModule()); those fields are not
    // mapped in the incremental game-module layout, so -- following the
    // gBrnLoadingScreenShouldShow bridge-global precedent (the gBrnDiskSpaceCheckComplete
    // one was retired 2026-08-16 -- boot audit F-P4-8 -- as an invented producer) --
    // they are modelled here:
    //   gBrnInGameStateActive       stands in for *(base + 0x9A06BA) (byte) -- "in-game state active"
    //   gBrnReturnToFrontEndRequested stands in for *(base + 0x9A0626) (byte) -- "return to front-end" poll
    bool gBrnInGameStateActive = false;
    bool gBrnReturnToFrontEndRequested = false;
}

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // dev print + gxMessageFilterFlags gate

// @ 0x823AACF8 -- entering the in-game flow state. Two absolute-offset stores into the
// game-module aggregate: raise the "in-game state active" flag and stamp the load-state slot to 5.
void MainGameFlowStateInGame::OnEnter()
{
    BrnGameMainFlowController::gBrnInGameStateActive = true;
    // @0x823AAD18 -- mbInGameState = 1 (the update set becomes 0x88).
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SetInGameState(true);
    // Request GUI FSM stage 5 (the +2523537 pending-stage byte): BridgeGameToGui
    // @0x823DCA10 posts GuiEventRunFsm{BrnScreenFsm@LOADING -> SCREEN} +
    // {BrnFBFsm -> HUD} for it -- the front-end/freeburn handoff. [The prior recon
    // read this store as a "load-state slot" stamp; the bridge's switch on the same
    // offset proves it is the GUI FSM stage request.]
    // The dev-gated stage log every main-flow OnEnter on the boot path carries
    // (MemoryCard/CompleteLoading print the same shape).
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "InGame: OnEnter -> GUI FSM stage 5 (BrnScreenFsm+BrnFBFsm)\n";
    BrnGame::GetMainGameModule()->RequestGuiFsmStage(5);
}

// @ 0x823AAD28 -- leaving the in-game flow state: clear the "in-game state active" flag OnEnter
// raised (*(base + 0x9A06BA) = 0). The load-state word (+0x9A0644) is NOT touched here.
void MainGameFlowStateInGame::OnLeave()
{
    BrnGameMainFlowController::gBrnInGameStateActive = false;
    // @0x823AAD3C -- mbInGameState = 0.
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SetInGameState(false);
}

// @ 0x823E79B8 -- in-game render is a straight tail-forward to the game module's render-dispatch
// spine (BrnGame::BrnGameModule::DoDispatch, off_830102D0 == BrnGame::GetMainGameModule()). The
// void Render slot discards DoDispatch's int.
void MainGameFlowStateInGame::Render()
{
    BrnGame::GetMainGameModule()->DoDispatch();
}

// @ 0x823F3048 -- in-game per-frame update. Run the game module's update spine; then, if the
// "return to front-end" byte is set, clear it and drive an inline IN_GAME -> START_SCREEN
// transition through the flow controller.
//
// The X360 reads meCurrentState (controller + 0x50), asserts it is in [0, E_MGS_COUNT), then
// switches: states 0-5 fall through unchanged; state 6 (IN_GAME) INLINES OnLeave(IN_GAME) +
// meCurrentState = START_SCREEN(3) + OnEnter(START_SCREEN) -- NOT a SetState call (SetState would
// additionally guard != E_MGS_INVALID); any other value hits the assert.
void MainGameFlowStateInGame::Update()
{
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    lpGameModule->DoUpdate();

    // ---- the in-game SOUND frame (faithful-audio-engine phase C4b) ----------------------
    // The console DoUpdate @0x823F0AF8 owns these legs (the full walk is decoded in
    // progress/scratch_dossiers/doupdate_spine_codex.md): the per-frame "Sound"
    // RootOutputBuffer @0x823F0C98 and "SoundRootPreUpdateOutput" buffer @0x823F0D88
    // carved in the frame's buffer batch; DoPreUpdate_Sound @0x823F1300 (guiIn =
    // *(gm+0x9A0BCC), the PC static GUI input) BEFORE the world; BridgeSoundToWorld
    // staged inside DoUpdate_World; DoUpdate_Sound @0x823F19E8 after effects; the two
    // sound->resource forwards in the tail @0x823F1F08-34; buffers destroyed in reverse.
    // FLAG PC placement: DoUpdate above is still the PC-platform leaf, so this frame's
    // sound legs live here with the world drive -- all sites move together when the
    // module scheduler moves under the game module's own spines.
    CgsModule::IOBufferStack* lpOutputStack = lpGameModule->GetUpdateOutputBufferStack();
    BrnSound::Module::Io::RootOutputBuffer*          lpSoundRootOutput      = 0;
    BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutput = 0;
    lpOutputStack->CreateIOBuffer<BrnSound::Module::Io::RootOutputBuffer>(
        &lpSoundRootOutput, "Sound");
    lpOutputStack->CreateIOBuffer<BrnSound::Module::Io::RootPreUpdateOutputBuffer>(
        &lpSoundPreUpdateOutput, "SoundRootPreUpdateOutput");

    if (lpSoundPreUpdateOutput != 0 && lpGameModule->GetGuiInputBuffer() != 0)
    {
        lpGameModule->DoPreUpdate_Sound(lpOutputStack, lpSoundPreUpdateOutput,
                                        lpGameModule->GetGuiInputBuffer());
    }

    // Drive the world module per frame while in-game. DoUpdate above is a PC-platform leaf
    // (the host loop owns the module walk) and its world leg, DoUpdate_World @0x823E8BD0, is
    // reached from nowhere -- so without this the world module, its PVS query and the streamer
    // all stop the moment the flow leaves the loading screen, leaving the query frozen at the
    // world-space (0,0,0) it was last taken from during loading. The sound pre-update buffer
    // threads through to the BridgeSoundToWorld staging (phase C4b), the same seam the
    // console's DoUpdate_World carries.
    DriveInGameWorldUpdate(lpSoundPreUpdateOutput);

    // The post-world sound leg (console @0x823F19E8): the full DoUpdate_Sound with the PC's
    // live sources. Replay pre-sim + effects outputs are null until those modules are driven
    // per-frame (FLAG'd inside the leg); the update set is the same FSM derivation the world
    // drive uses.
    if (lpSoundRootOutput != 0)
    {
        const BrnUpdateSet lUpdateSet = lpGameModule->ConstructUpdateSetFromFsm();
        lpGameModule->DoUpdate_Sound(
            lpGameModule->GetUpdateInputBufferStack(),
            lpOutputStack,
            lpGameModule->GetGameStateModule().GetOutputBuffer(),
            lpGameModule->GetWorldUpdateOutputBuffer(),
            lpGameModule->GetDirectorOutputBuffer(),
            0,   // replays pre-sim output: module not driven per-frame on PC yet
            lpSoundRootOutput,
            lpGameModule->GetGuiOutputBuffer(),
            0,   // effects output: module unmounted (lock-only participant on console)
            lUpdateSet);

        // The caller-side sound->resource forwards (console DoUpdate tail @0x823F1F08-34;
        // the leg itself does NOT forward): the root output's AttribSys <2048> queue +
        // <4096> request interface into the GameData input, under the standard
        // W(gameDataIn)+R(rootOut) bracket -- the same pair the loading spine forwards.
        BrnResource::GameDataIO::InputBuffer* lpGameDataInput =
            BrnGameMainFlowController::GetScriptedLoadGameDataInput();
        if (lpGameDataInput != 0)
        {
            lpGameDataInput->LockForWrite();
            lpSoundRootOutput->LockForRead();
            {
                const BrnSound::Module::Io::RootOutputBuffer* lpSoundRootOutputRead = lpSoundRootOutput;
                lpGameDataInput->GetAttribSysRequestInterface()->mRequestQueue.Append(
                    lpSoundRootOutputRead->GetAttribSysRequestInterface()->mRequestQueue);
                lpGameDataInput->GetRequestInterface()->mRequestQueue.Append(
                    lpSoundRootOutputRead->GetResourceRequestInterface()->mRequestQueue);
            }
            lpSoundRootOutput->UnlockForRead();
            lpGameDataInput->UnlockForWrite();
        }
    }

    // Teardown in reverse creation order (console @0x823F20E0 preUpdateOut ... @0x823F21B4 rootOut).
    if (lpSoundPreUpdateOutput != 0)
        lpOutputStack->DestroyIOBuffer<BrnSound::Module::Io::RootPreUpdateOutputBuffer>(
            &lpSoundPreUpdateOutput);
    if (lpSoundRootOutput != 0)
        lpOutputStack->DestroyIOBuffer<BrnSound::Module::Io::RootOutputBuffer>(
            &lpSoundRootOutput);

    if (!BrnGameMainFlowController::gBrnReturnToFrontEndRequested)
        return;
    BrnGameMainFlowController::gBrnReturnToFrontEndRequested = false;

    BrnGameMainFlowController::GameMainFlowController* lpController =
        BrnGameMainFlowController::gpMainGameFlowController;
    if (lpController == 0)   // Option B null guard (X360 reaches the live controller off the module global)
        return;

    const BrnGameMainFlowController::EMainGameFlowState leState = lpController->GetCurrentState();
    CGS_ASSERT(leState >= 0 && leState < BrnGameMainFlowController::E_MGS_COUNT,
               "meCurrentState >= 0 && meCurrentState < E_MGS_COUNT");

    switch (leState)
    {
    case BrnGameMainFlowController::E_MGS_INITIAL_LOADING_SCREEN:
    case BrnGameMainFlowController::E_MGS_CHECK_DISK_SPACE:
    case BrnGameMainFlowController::E_MGS_MARKETING_SCREENS:
    case BrnGameMainFlowController::E_MGS_START_SCREEN:
    case BrnGameMainFlowController::E_MGS_MEMORY_CARD:
    case BrnGameMainFlowController::E_MGS_COMPLETE_LOADING:
        return;
    case BrnGameMainFlowController::E_MGS_IN_GAME:
        // Inline IN_GAME -> START_SCREEN transition (X360 unrolls OnLeave + stamp + OnEnter here).
        lpController->GetState(BrnGameMainFlowController::E_MGS_IN_GAME)->OnLeave();
        lpController->SetCurrentStateRaw(BrnGameMainFlowController::E_MGS_START_SCREEN);
        lpController->GetState(BrnGameMainFlowController::E_MGS_START_SCREEN)->OnEnter();
        break;
    default:
        CGS_ASSERT(false, "Unknown main game state.\n");
        break;
    }
}
