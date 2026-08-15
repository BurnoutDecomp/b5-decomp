#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h"   // gpMainGameFlowController, SendEvent
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"   // DebugManager::Update during load
#include "GameSource/Resource/BrnGameDataModule.h"   // GameDataModule + BrnGame::GetMainGameDataModule()
#include "GameSource/Resource/BrnGameDataModuleIO.h" // GameDataIO::InputBuffer/OutputBuffer (LoadSoundModule args)
#include "GameSource/Sound/Module/BrnRootSoundModule.h"   // RootSoundModule + BrnGame::GetMainSoundModule() (stage 4)
#include "GameSource/Game/BrnGameModule.hpp"         // BrnGame::GetMainGameModule() (the update IO stacks)
#include "GameSource/World/BrnWorldModuleIO.h"       // BrnWorldIO::UpdateOutputBuffer (LoadWorldModule scratch buffer)
#include "GameSource/World/BrnWorldModule.h"         // BrnWorld::WorldModule::Update (the per-frame world drive)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h" // CgsMemory::LinearMalloc (the world frame allocator)
#include "GameSource/Game/BrnLoadingScreenRenderer.h" // BrnGame::ELoadingScreenCommand (the command slot values)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp" // DirectorIO::OutputBuffer (stage 3)
#include "GameSource/GameState/BrnGameStateModule.h"    // BrnGameState::GameStateModule::GetOutputBuffer (BridgeGameStateToWorld source)
#include "GameSource/GameState/BrnGameStateModuleIO.h"  // GameStateModuleIO::OutputBuffer (its lock bracket)

// Engine clock (same source the loading-screen renderer animates from). Defined in
// CgsTimeUtils.cpp; used here to pace the (currently stubbed) load so it is visible.
namespace CgsSystem { u32 GetSystemTimerBaseTime(); u32 GetSystemTimerFrequency(); }

// The game-data streaming module the loading screen brings up (X360 Update 0x823EF688 case 8:
// GameDataModule::Prepare via vtable+64). On X360 it lives in the game module; here a file-static
// instance is driven by the load stage until it reports ready. Its Prepare spine runs the REAL
// resource module-tree bring-up (base + ResourceModule::Prepare chaining Memory/Pool/Bundle/
// FileSystem). The rw-allocator-gated CreateBanks/CreatePools/CreateAllocators inside it are still
// stubs that report success, so this exercises the real module lifecycle without yet allocating
// banks/pools (those fills are step 5a/5b). [NEVER run headless -- the user runs the exe.]
// The game's one GameDataModule lives in BrnGameModule (gGameModule.mGameDataModule), constructed by
// BrnGameModule::Construct; the loading flow (case 8) drives THAT instance via GetMainGameDataModule()
// -- no parallel copy. (gGameModule is a file-scope static -> zero-init BSS + ctor-run, so its stage
// fields are valid and its vtables are set, no calloc/placement-new needed.)
static bool g_bLoggedGameDataPrepare = false;

namespace
{
    // The dispatch write buffer the loading-screen commands post onto (the X360 flow state
    // reaches it the same way: the module global -> the manager's write pointer,
    // *(*(off_830102D0 + 2524268) + 39312) @0x823EF688).
    BrnGame::DispatchThreadInputBuffer* GetDispatchWriteBuffer()
    {
        return BrnGame::GetMainGameModule()->GetDispatchThreadInputBufferManager().GetWriteBuffer();
    }
}

// The boot loading screen is owned by the GUI flow's BF_LOADING state (BrnGui::BootLoading), which
// shows it (PlayLoadingScreen) and dismisses it (StopLoadingScreen) through its StateInterface -- the
// faithful path (the X360 GUI flow drives the loading-screen APT movie). When BrnGui::GuiModule has
// that FSM live it sets gBrnGuiDrivesLoadingScreen, and this game-flow state stops touching
// the loading-screen command (the GUI drives it); it only does the loading work and, once finished,
// raises gBrnInitialLoadingComplete so the GUI flow advances BF_LOADING -> BF_VIDEOS. If the GUI FSM
// is NOT live (gBrnGuiDrivesLoadingScreen false), this state keeps managing the screen itself (the
// prior behaviour) so the boot never loses its loading screen.
bool gBrnInitialLoadingComplete = false;
bool gBrnGuiDrivesLoadingScreen = false;   // set by BrnGui::GuiModule when the BF_LOADING Lua FSM is live

// Defined further down with the CheckDiskSpace state (byte_82FAE28E): the disk-check
// entered flag DOUBLES as the scripted-load pause flag -- the load spine below skips its
// stage machine while it is raised, and ResumeLoadingWorld clears it on the title
// pre-accept.
extern bool gBrnCheckDiskSpaceEntered;

// --- MainGameFlowState (abstract base) --------------------------------------------------
MainGameFlowState::MainGameFlowState() {}
void MainGameFlowState::OnEnter() {}
void MainGameFlowState::OnLeave() {}
void MainGameFlowState::Update() {}
void MainGameFlowState::Render() {}

// --- LoadingScriptedState ---------------------------------------------------------------
LoadingScriptedState::LoadingScriptedState()
    : meLoadingStateStage(E_LOADINGSTATESTAGE_START)
    , mbLoadingPaused(false)
{
}
void LoadingScriptedState::OnEnter() {}
void LoadingScriptedState::OnLeave() {}
void LoadingScriptedState::Render() {}
void LoadingScriptedState::FinishLoading() {}

// The scripted world-load stage (X360 dword_82FAE4B0 -- a GLOBAL shared by every flow
// state, NOT a per-instance member; the ARTIST stage set drifted from the PS3-DWARF
// ELoadingStateStage enum: 1 sound-again, 2 effects, 3 gamestate2, 4 GUI second prepare,
// 5 world module, 6 sound world-loaded cue -> 8, 7 world collision, 8 done).
s32 gBrnScriptedLoadStage = 0;

namespace
{
    // One-shot per-stage log (the deferred stages advance immediately; keep the
    // progression visible in BrnGame.log the way InitialLoadingScreen's stages are).
    void LogScriptedStageOnce(s32 liStage, const char* lpcWhat)
    {
        static bool s_abLogged[9] = { false, false, false, false, false, false, false, false, false };
        if (liStage < 0 || liStage > 8 || s_abLogged[liStage])
            return;
        s_abLogged[liStage] = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "ScriptedLoad: stage " << liStage << " (" << lpcWhat << ")\n";
    }
}

// @ 0x823A85B0 -- the title pre-accept resume: assert the scripted load is still parked at
// START, then clear the pause flag (byte_82FAE28E == gBrnCheckDiskSpaceEntered) so the
// per-frame spine below starts advancing the stages.
void LoadingScriptedState::ResumeLoadingWorld()
{
    CGS_ASSERT(gBrnScriptedLoadStage == E_LOADINGSTATESTAGE_START,
               "meLoadingStateStage == E_LOADINGSTATESTAGE_START");   // X360 h:209
    gBrnCheckDiskSpaceEntered = false;
}

// @ 0x823E72F0 -- one frame of the world-module load. Create a scratch BrnWorldIO::
// UpdateOutputBuffer on the update output stack ("World"), drive WorldModule::Prepare
// (vtable +68) with the update IO stacks + the GameData allocator list, and -- while it
// reports "still preparing" -- forward the world's staged resource requests into the
// GameData input buffer (that append is what carries the vault / district-map / PVS /
// surface-list requests into the GameData pump each frame).
bool LoadingScriptedState::LoadWorldModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                                           const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer)
{
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    CgsModule::IOBufferStack* lpUpdateInputStack  = lpGameModule->GetUpdateInputBufferStack();
    CgsModule::IOBufferStack* lpUpdateOutputStack = lpGameModule->GetUpdateOutputBufferStack();

    BrnWorldIO::UpdateOutputBuffer* lpWorldOutput = 0;
    lpUpdateOutputStack->CreateIOBuffer(&lpWorldOutput, "World");
    // (CreateIOBuffer<T> runs UpdateOutputBuffer::Construct @0x827CA0F8 itself, as the X360
    //  instantiation @0x823AD100 does.)

    // X360 passes the output buffer's allocator list (the module's mutable registry);
    // the PC read accessor is const-qualified, hence the cast back to the X360 shape.
    const BrnResource::GameDataIO::AllocatorList* lpAllocatorList =
        lpGameDataOutputBuffer ? lpGameDataOutputBuffer->GetAllocatorList() : 0;

    const bool lbPrepared = lpGameModule->GetWorldModule().Prepare(
        lpUpdateInputStack, lpUpdateOutputStack, lpWorldOutput,
        const_cast<BrnResource::GameDataIO::AllocatorList*>(lpAllocatorList));

    if (!lbPrepared && lpGameDataInputBuffer != 0)
    {
        lpWorldOutput->LockForRead();
        // The X360 reads through the CONST accessor (0x823B5780) under the read lock.
        const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputRead = lpWorldOutput;
        lpGameDataInputBuffer->AppendRequestInterface<4096>(
            *lpWorldOutputRead->GetResourceRequestResourceInterface());
        // X360 (headless-IDA @0x823E72F0): also bulk-append the world output's AttribSys
        // vault request queue (<2048>) into the GameData input's attrib queue
        // (VariableEventQueue<32768,16>::Append<2048,16>) -- this is what carries the
        // world's RegisterVault into the GameData pump's AttribSysModule update.
        lpGameDataInputBuffer->GetAttribSysRequestInterface()->mRequestQueue.Append(
            lpWorldOutputRead->GetAttribSysVaultRequestInterface()->mRequestQueue);
        lpWorldOutput->UnlockForRead();
    }

    lpUpdateOutputStack->DestroyIOBuffer(&lpWorldOutput);
    return lbPrepared;
}

// @ 0x823E73E0 -- one frame of the WORLD COLLISION load (scripted-load stage 7). Same shape
// as LoadWorldModule above: create a scratch BrnWorldIO::UpdateOutputBuffer on the update
// output stack ("World"), drive WorldModule::PrepareWorldCollision with the update IO stacks,
// and -- while it reports "still preparing" -- forward the world's staged resource requests
// into the GameData input buffer. That forward is what carries the "TRK_COLL" LoadBundle and
// then the 396 "TRK_CLIL<n>" AcquireResourceList requests into the GameData pump.
//
// The X360's true arm calls BrnEffects::EffectsModule::PostWorldPreparePrepare @0x822902F0
// (136 insns). NOT LANDED: that body is 100% AttribSys reads (GetCollectionWithDefault /
// GetAttributePointer / GetCollection) storing into EffectsModule's members, and the PC
// EffectsModule is still `u8 mOpaqueBody[0x2F550]` -- writing into an opaque slice at console
// byte offsets is the memory-bug class this project forbids. It configures debris/particle
// attributes and feeds nothing on the collision path, so its absence cannot block the load.
// [deferred: EffectsModule::PostWorldPreparePrepare -- EffectsModule layout is opaque]
// ⭐ ARITY from the DecFIGS DWARF (BrnGameMainFlowStates.h:52 --
// `bool LoadWorldCollision(InputBuffer*, const OutputBuffer*)`), NOT from the IDA export
// prototype, which reports only `(a1, a2)`: the console body never reads the GameData
// OUTPUT buffer (unlike LoadWorldModule, which pulls the allocator list out of it), so
// Hex-Rays dropped the unread parameter. The call site @0x823F22D8 passes three registers.
// The parameter is kept, and kept unused, so the signature matches the shipped one.
bool LoadingScriptedState::LoadWorldCollision(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                                              const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer)
{
    (void)lpGameDataOutputBuffer;   // read by neither the console body nor this one

    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    CgsModule::IOBufferStack* lpUpdateInputStack  = lpGameModule->GetUpdateInputBufferStack();
    CgsModule::IOBufferStack* lpUpdateOutputStack = lpGameModule->GetUpdateOutputBufferStack();

    BrnWorldIO::UpdateOutputBuffer* lpWorldOutput = 0;
    lpUpdateOutputStack->CreateIOBuffer(&lpWorldOutput, "World");

    const bool lbPrepared = lpGameModule->GetWorldModule().PrepareWorldCollision(
        lpUpdateInputStack, lpUpdateOutputStack, lpWorldOutput);

    if (lbPrepared)
    {
        static bool s_bLoggedPostWorldPrepare = false;
        if (!s_bLoggedPostWorldPrepare)
        {
            s_bLoggedPostWorldPrepare = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "LoadWorldCollision: EffectsModule::PostWorldPreparePrepare (0x822902F0) "
                       "skipped -- EffectsModule body is opaque [FLAG PC boot gate]\n";
        }
    }
    else if (lpGameDataInputBuffer != 0)
    {
        lpWorldOutput->LockForRead();
        // The X360 reads through the CONST accessor (0x823B5780) under the read lock -- the
        // same pair LoadWorldModule uses.
        const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputRead = lpWorldOutput;
        lpGameDataInputBuffer->AppendRequestInterface<4096>(
            *lpWorldOutputRead->GetResourceRequestResourceInterface());
        lpGameDataInputBuffer->GetAttribSysRequestInterface()->mRequestQueue.Append(
            lpWorldOutputRead->GetAttribSysVaultRequestInterface()->mRequestQueue);
        lpWorldOutput->UnlockForRead();
    }

    lpUpdateOutputStack->DestroyIOBuffer(&lpWorldOutput);
    return lbPrepared;
}

// ⭐ X360 0x823EF4D8 -- LoadingScriptedState::LoadGameState2, scripted stage 3.
//
// The console body:
//     CreateIOBuffer<GameStateModuleIO::OutputBuffer>(updateOutStack, &out, "GameState");
//     if (GameStateModule::Prepare2(&gGameModule.mGameStateModule, out)) {
//         DestroyIOBuffer(out); return 1;
//     }
//     LockForRead(out);
//     gameDataIn->AppendRequestInterface<3072>(*out->GetResourceRequestInterface());
//     UnlockForRead(out);
//     DestroyIOBuffer(out); return 0;
//
// ⚠️ FLAG (PC deviation -- ONE persistent buffer instead of a per-pass scratch): identical to the
// one BrnGameModule::GamePrepare stage 4 already carries for the FIRST-pass Prepare. The console
// carves a fresh GameStateModuleIO::OutputBuffer off the update-output IOBufferStack every pass
// (so its request queue starts empty each time and the append moves exactly that pass's
// requests); the PC module owns ONE persistent buffer, so the queue is CLEARED after the append,
// which is the state the console's next pass starts in. Using the module's own buffer here also
// keeps BOTH prepare passes staging onto the SAME queue -- the acquire LoadProgressionData
// issues has to reach the GameData pump through the identical hop the trigger acquire does.
// DELETE-WHEN the module's real CreateOutputDataStructure path lands.
bool LoadingScriptedState::LoadGameState2(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer)
{
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    if (lpGameModule == 0)
        return true;

    BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput =
        lpGameModule->GetGameStateModule().GetOutputBuffer();
    if (lpGameStateOutput == 0)
        return true;

    const bool lbPrepared = lpGameModule->GetGameStateModule().Prepare2(lpGameStateOutput);

    if (!lbPrepared && lpGameDataInputBuffer != 0)
    {
        // The CONST overload is the one the console calls under the read lock (its non-const twin
        // @0x8231D560 asserts the WRITE lock) -- same const-alias idiom as GamePrepare stage 4.
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputRead =
            lpGameStateOutput;
        lpGameStateOutput->LockForRead();
        lpGameDataInputBuffer->AppendRequestInterface<3072>(
            *lpGameStateOutputRead->GetResourceRequestInterface());
        lpGameStateOutput->UnlockForRead();

        // [PC deviation, see the FLAG above] the console's per-pass buffer dies here.
        lpGameStateOutput->LockForWrite();
        lpGameStateOutput->GetResourceRequestInterface()->mRequestQueue.Clear();
        lpGameStateOutput->UnlockForWrite();
    }

    return lbPrepared;
}

// The per-frame world UPDATE leg of the scripted-load spine (X360 0x823F22D8, the
// `dword_82FAE4B0 > 5` block). Split out of Update() for readability; the X360 inlines it.
//
//   * the world update IO pair: the X360 spine creates BrnWorldIO::UpdateInputBuffer and
//     UpdateOutputBuffer on the update stacks near the top of the frame (v40/v39) and
//     threads them through every world bridge; this slice creates the pair here, drives the
//     world, forwards the requests and destroys it -- the same lifetime, narrower scope
//     (no other consumer of the pair is wired on the PC yet).
//   * the frame allocator: X360 gm+10094128, published by GamePrepare @0x823EFBD0 as the
//     RENDERER's reusable loading-screen allocator (RendererIO::OutputBuffer::
//     GetReusableLoadingScreenAllocator @0x823B4088). The PC renderer never publishes it
//     (GamePrepare is gated), so this slice owns an equivalent bump allocator --
//     FLAG PC-platform leaf: the region SIZE is a PC choice (the X360 size belongs to the
//     renderer's loading-screen allocator and is not recovered); WorldModule::Update makes
//     exactly ONE 336896-byte carve per frame and the FreeAll below resets it every frame,
//     so 512 KiB covers the frame with headroom. Swap for the renderer's allocator when
//     GamePrepare lands.
//   * BridgeWorldToResource @0x823E5300: append the world output's resource-request
//     interface (RequestInterface<4096>) into the GameData input, then bulk-append its
//     AttribSys vault request queue (<2048> into VariableEventQueue<32768,16>) -- the same
//     pair LoadWorldModule runs during the prepare stages, now running per frame.
void LoadingScriptedState::UpdateWorldModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer)
{
    DriveWorldUpdateFrame(lpGameDataInputBuffer, KU_LOADING_UPDATE_SET);
}

// The same leg, callable from ANY flow state.
//
// WHY THIS EXISTS: the world was only ever driven from the scripted-load spine, so it stopped
// updating the instant the flow left the loading screen. WorldModule::Update had exactly one
// reachable caller (LoadingScriptedState::UpdateWorldModule above), and the in-game state's
// whole per-frame body is BrnGameModule::DoUpdate(), a PC-platform leaf returning 0; the
// console's in-game world leg (BrnGameModule::DoUpdate_World @0x823E8BD0) is reconstructed
// but reached from nowhere. Net effect: the PVS query ran ONCE, from world-space (0,0,0),
// during the loading screen, and its answer -- zone 174 plus 24 neighbours, 25 of the 396
// track units -- stayed frozen for the whole session, so driving the camera across the city
// streamed nothing new in or out.
//
// This is the narrow fix: drive the WORLD leg per frame from the in-game state as well. It
// deliberately does NOT re-run the rest of DoUpdate's cascade -- the PC host loop
// (EngineUpdate -> DispatchThread) still owns the module walk, and duplicating that here
// would double-update every module, which is exactly why DoUpdate is a leaf. Fold this back
// into the real DoUpdate cascade when the module scheduler moves under the game module's own
// spines.
void DriveWorldUpdateFrame(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                           BrnUpdateSet lUpdateSet)
{
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    CgsModule::IOBufferStack* lpUpdateInputStack  = lpGameModule->GetUpdateInputBufferStack();
    CgsModule::IOBufferStack* lpUpdateOutputStack = lpGameModule->GetUpdateOutputBufferStack();

    // FLAG PC-platform leaf (see the note above): the world's per-frame linear allocator.
    static const size_t KN_WORLD_FRAME_ALLOCATOR_SIZE = 512 * 1024;
    static CgsMemory::LinearMalloc s_WorldFrameAllocator;
    static bool s_bFrameAllocatorCreated = false;
    if (!s_bFrameAllocatorCreated)
    {
        s_bFrameAllocatorCreated = true;
        s_WorldFrameAllocator.Construct();
        s_WorldFrameAllocator.Create(new u8[KN_WORLD_FRAME_ALLOCATOR_SIZE],
                                     KN_WORLD_FRAME_ALLOCATOR_SIZE);
    }

    BrnWorldIO::UpdateInputBuffer*  lpWorldInput  = 0;
    lpUpdateInputStack->CreateIOBuffer(&lpWorldInput, "World");

    // ⭐⭐ THE WORLD'S FRAME TIMER, STAGED -- 2026-08-10 (root-cause wave).
    // The console stages it here (X360 DoUpdate_World @0x823E8BD0:
    //     UpdateInputBuffer::SetTimerStatusInterface(worldIn, gm+10095372)),
    // and gm+10095372 is BrnGameModule::mTimerStatusInterface, which is REAL and already live
    // on this build -- UpdateTimers @0x823BCFD0 ticks mGameTimer/mSimTimer once per sim
    // sub-step and BridgeTimers @0x823BD150 snapshots the pair into it (it is what makes every
    // director camera behaviour advance). So this is real data, not a stand-in.
    //
    // WorldModule::Update hands this block to WorldModule::BridgeInputToPhysicsModule
    // @0x827AB830, which copies it into the physics input buffer; PhysicsModule::Update
    // @0x825B0640 short-circuits ("sim timer not running -- inert this frame") for as long as
    // the block stays Construct-cleared. With the timer staged the module runs for real.
    //
    // ⛔ THE 2026-08-09 MEASUREMENT THAT GATED THIS LINE, AND WHAT ANSWERED IT.
    // Staging it on the previous build produced 906 asserts per session and a boot that never
    // reached the fly-by -- six distinct identities, every one inside PhysicsModule::Update.
    // Both root causes behind those six are CLOSED as of this wave:
    //   (a) the solver iteration count was zero because its ONLY producer in the image,
    //       WorldModule::BridgeEntityModulesToPhysicsModule_PreScene @0x827AADB8, was an inert
    //       link stub -- it is reconstructed now (Bridges/WorldBridgeEntityModulesToPhysics.cpp)
    //       and calls SetSolverMaxIterations @0x8279F240. That was gates 2, 3 and 4.
    //   (b) the write-lock/Construct family: the const accessor overloads the console actually
    //       calls (VehicleManagerOutputBuffer @0x825A0FB0, PhysicsSimulationIO::OutputBuffer::
    //       GetUpdateRigidBodyQueue @0x8259EFD0) and the missing
    //       PhysicsModuleIO::OutputBuffer::Construct @0x825ABB10. That was gates 1, 5 and 6.
    // ⚠️ The vehicle CREATE path is still absent, so the module conducts over an empty body
    // set -- an empty tick is the expected, honest outcome, not a regression.
    lpWorldInput->LockForWrite();
    lpWorldInput->SetTimerStatusInterface(
        reinterpret_cast<const BrnWorldIO::TimerStatusInterface*>(
            lpGameModule->GetTimerStatusInterface()));
    lpWorldInput->UnlockForWrite();

    // ⭐⭐ THE CONTROLLER -> WORLD BRIDGE (X360 BridgeControllerToWorld @0x823CD890), first of
    // DoUpdate_World's five source bridges and the ONLY producer of the world input's
    // PlayerVehicleControls -- the block BridgeInputToEntityModules hands the race car as
    // the player's steering/throttle. Until this line (driving-input wave 2026-08-11) the
    // block stayed Construct-cleared every frame and the player car was deaf to input.
    // FLAG PC placement: staged here for exactly the reason the game-state block below
    // gives -- DoUpdate_World is reconstructed but reached from nowhere; this function is
    // the live world drive, and both sites carry the same call and move together. The pad
    // fill (InputPadsPC::UpdatePlayer0) ran earlier this sub-step under GameMain's GUI
    // staging, so the buffer carries this frame's controls; the console's DoUpdate locks
    // all five sources in one LockBuffersForIO, reproduced here as the per-source bracket
    // this file already uses for the game-state leg.
    {
        CgsInput::InputIO::OutputBuffer* lpInputOutput = lpGameModule->GetPcInputOutputBuffer();
        lpInputOutput->LockForRead();
        lpWorldInput->LockForWrite();
        lpGameModule->BridgeControllerToWorld(lpWorldInput, lpInputOutput);
        lpWorldInput->UnlockForWrite();
        lpInputOutput->UnlockForRead();
    }

    // ⭐ THE OUTPUT BUFFER IS THE GAME MODULE'S, not this function's (2026-08-01, camera wave).
    // The console's DoUpdate @0x823F0AF8 creates ONE BrnWorldIO::UpdateOutputBuffer per
    // sub-step and threads it through DoUpdate_World, DoUpdate_Director and every other leg.
    // This function used to create AND DESTROY it inside one call, so nothing downstream could
    // ever read what the world published -- in particular BridgeWorldToDirector, the only
    // route from a race car's pose to the director's cameras. It now uses the game module's
    // per-sub-step buffer (CreateStaticIOBuffers), falling back to a local one only if that
    // has not been created (the flow states are always driven from inside the sub-step, so the
    // fallback is defensive).
    BrnWorldIO::UpdateOutputBuffer* lpWorldOutput = lpGameModule->GetWorldUpdateOutputBuffer();
    const bool lbOwnsOutputBuffer = (lpWorldOutput == 0);
    if (lbOwnsOutputBuffer)
    {
        lpUpdateOutputStack->CreateIOBuffer(&lpWorldOutput, "World");
    }

    // ⭐⭐ THE GAME-STATE -> WORLD BRIDGE (X360 BridgeGameStateToWorld @0x823E1890), staged into
    // the input buffer before the world drive reads it -- the console's own order inside
    // DoUpdate_World @0x823E8BD0 (which is where this call ALSO lives; see the note there).
    // Its game-action Append is the ONLY producer of the queue WorldModule::HandleGameActions
    // and WorldModule::BridgeInputToEntityModules drain, so before this the world could not see
    // a single game action -- including ResetPlayerCarAction, the record that places the
    // player's car at a junkyard spawn.
    // FLAG PC placement: this leg lives here rather than in DoUpdate_World for exactly the
    // reason the banner above gives -- DoUpdate is a PC-platform leaf, so this function is the
    // live world drive. Both sites carry the same call and move together.
    {
        BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput =
            lpGameModule->GetGameStateModule().GetOutputBuffer();
        if (lpGameStateOutput != 0)
        {
            lpGameStateOutput->LockForRead();
            lpWorldInput->LockForWrite();
            lpGameModule->BridgeGameStateToWorld(lpWorldInput, lpGameStateOutput);
            lpWorldInput->UnlockForWrite();
            lpGameStateOutput->UnlockForRead();
        }
    }

    // X360: LinearMalloc::FreeAll(gm->mpWorldUpdateFrameAllocator) then the vtable+76
    // dispatch. The loading update set is 0x80 (frustum testing on, nothing else).
    s_WorldFrameAllocator.FreeAll();
    lpGameModule->GetWorldModule().Update(lUpdateSet,
                                          lpUpdateInputStack, lpUpdateOutputStack,
                                          lpWorldInput, lpWorldOutput,
                                          &s_WorldFrameAllocator);

    // BridgeWorldToResource @0x823E5300 -- the streamer's request forward.
    if (lpGameDataInputBuffer != 0)
    {
        // The X360 brackets the bridge with the standard destination-write /
        // source-read pair; the GameData input's own accessors assert the write lock.
        lpGameDataInputBuffer->LockForWrite();
        lpWorldOutput->LockForRead();
        const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputRead = lpWorldOutput;
        lpGameDataInputBuffer->AppendRequestInterface<4096>(
            *lpWorldOutputRead->GetResourceRequestResourceInterface());
        lpGameDataInputBuffer->GetAttribSysRequestInterface()->mRequestQueue.Append(
            lpWorldOutputRead->GetAttribSysVaultRequestInterface()->mRequestQueue);
        lpWorldOutput->UnlockForRead();
        lpGameDataInputBuffer->UnlockForWrite();
    }

    if (lbOwnsOutputBuffer)
    {
        lpUpdateOutputStack->DestroyIOBuffer(&lpWorldOutput);
    }
    lpUpdateInputStack->DestroyIOBuffer(&lpWorldInput);
}

// @ 0x823E74C0 -- LoadDirectorModule, the E_LOADINGSTAGE_DIRECTORMODULE leg of
// InitialLoadingScreen's stage machine, in the same shape as LoadSoundModule/LoadWorldModule.
// The X360 body, statement for statement:
//   * CreateIOBuffer<DirectorIO::OutputBuffer>(gm->mpUpdateOutputBufferStack, &lpDirectorOut,
//     "Director")
//   * prepared = gm->mDirectorModule.Prepare(lpDirectorOut,
//                    lpGameDataOutputBuffer->GetAllocatorList())              (vtable +68)
//   * still preparing: LockForRead(lpDirectorOut);
//        VariableEventQueue<32768,16>::Append<512,16>( <the GameData input's AttribSys queue>,
//                                                      <the director output's @0x510 queue> );
//        InputBuffer::AppendRequestInterface<512>(lpGameDataInputBuffer,
//                                                 lpDirectorOut->Get());
//        UnlockForRead(lpDirectorOut);
//   * destroy the buffer and return `prepared`.
//
// ⭐ THIS IS THE PER-FRAME GAMEDATA IO BRACKET the lane load was blocked on. The requests
// WorldMap::LoadData stages onto the director output's RequestInterface<512> during
// DirectorModule::Prepare's stage 3 leave through the AppendRequestInterface<512> below and
// are serviced by the resource pump (BrnGameModule's per-frame GameDataModule::Update tick,
// the console's ResourceUpdateThread @0x823BC9B8). Without it the state machine waited for a
// reply that could never arrive and the loading flow wedged at stage 3.
//
// DirectorModule::Prepare @0x822712D8 is a 6-stage machine (register the debug component ->
// base ModuleSingleBuffered::Prepare -> DirectorResourceManager::Prepare -> WorldMap::LoadData
// -> MainDirector::Prepare -> done); it write-locks the output buffer for the whole call and
// returns true only at the last stage, so the caller pumps it until it does.
//
// ⭐ BOTH APPENDS ARE NOW REAL (2026-08-01, Prepare wave). The X360's first append moves the
// director output's @0x510 VariableEventQueue<512,16> into the GameData input's AttribSys
// request queue (+0x8014 -- the same destination LoadWorldModule's own AttribSys append uses).
// That member is re-homed to its real AttribSysRequestInterface<512> type, so the append is
// written out below; DirectorResourceManager::Prepare's RegisterVault is what rides it.
bool LoadingScriptedState::LoadDirectorModule(
    BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
    const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer)
{
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    CgsModule::IOBufferStack* lpUpdateOutputStack = lpGameModule->GetUpdateOutputBufferStack();

    BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutput = 0;
    lpUpdateOutputStack->CreateIOBuffer(&lpDirectorOutput, "Director");
    // (CreateIOBuffer<T> runs DirectorIO::OutputBuffer::Construct itself -- X360 instantiation
    //  @0x823ACBF8. That Construct also brings up the embedded RequestInterface<512> queue,
    //  which WorldMap::LoadData stages onto.)

    // The X360 reads the allocator list out of the GameData OUTPUT buffer (read-locked by the
    // caller) and passes it as DirectorModule::Prepare's second argument, which
    // MainDirector::Prepare forwards straight to ICEWrapper::Prepare. The committed
    // DirectorModule::Prepare signature still types that argument as the plain s32 the older
    // recon named `liPrepareArg`; see the FLAG below.
    const BrnResource::GameDataIO::AllocatorList* lpAllocatorList =
        lpGameDataOutputBuffer ? lpGameDataOutputBuffer->GetAllocatorList() : 0;

    // FLAG signature debt: DirectorModule::Prepare's 2nd parameter IS the allocator list
    // (X360 0x823E74C0 `lwz r9,0x44(vtable); r5 = GetAllocatorList(...)`), not an s32 replay
    // token. Its only consumer -- ICEWrapper::Prepare -- is a DirectorLinkStubs no-op, so the
    // value is currently unused either way; retype the whole chain
    // (DirectorModule::Prepare -> MainDirector::Prepare -> ICEWrapper::Prepare) when the ICE
    // wrapper is bodied.
    (void)lpAllocatorList;
    const bool lbPrepared = lpGameModule->GetDirectorModule().Prepare(lpDirectorOutput, 0);

    if (!lbPrepared && lpGameDataInputBuffer != 0)
    {
        // Still preparing: forward the module's staged resource requests into the GameData
        // input. The X360 brackets this with the source READ lock only -- the destination
        // write lock is held by the caller (InitialLoadingScreen::Update opens the frame with
        // LockForWrite on the GameData input) for the whole stage switch.
        lpDirectorOutput->LockForRead();
        // X360's FIRST append: the director output's @0x510 AttribSys vault queue into the
        // GameData input's own AttribSys queue (+0x8014) -- the destination
        // LoadWorldModule/DriveWorldUpdateFrame's AttribSys append already uses. Landed
        // 2026-08-01 with the +0x510 re-home; DirectorResourceManager::Prepare's RegisterVault
        // rides exactly this hop, and without it that request could never reach the module.
        {
            const BrnDirector::DirectorIO::OutputBuffer* lpDirectorOutputRead = lpDirectorOutput;
            lpGameDataInputBuffer->GetAttribSysRequestInterface()->mRequestQueue.Append(
                lpDirectorOutputRead->GetVaultRequestInterface()->mRequestQueue);
        }
        lpGameDataInputBuffer->AppendRequestInterface<512>(*lpDirectorOutput->Get());
        lpDirectorOutput->UnlockForRead();
    }

    lpUpdateOutputStack->DestroyIOBuffer(&lpDirectorOutput);
    return lbPrepared;
}

// @ 0x823F22D8 -- the shared scripted-load spine EVERY titled flow state's Update runs
// first (Marketing/StartScreen/MemoryCard/CompleteLoading on the X360 all open with
// LoadingScriptedState::Update()). Reconstructed slice: the scripted world-load stage
// machine + the per-frame GameData IO bracket + the GameDataModule pump. The rest of the
// X360 spine (the per-module input/sound/network/gamestate/GUI drives + RenderGUI) already
// runs through BrnGameModule::GameMain's inline hookup on the PC -- running it here too
// would double-drive those modules, so it is [deferred] to the module-scheduler move.
// The per-frame GameData IO pair. [PC placement] the X360 keeps these as game-module
// members (gm+10055440 / +10055444) created by the update spine each frame; the PC reuses
// one constructed-once pair (the precedent is GameDataModule::Update's own static
// ResourceIO input). Published through BrnGameMainFlowController::GetScriptedLoadGameData*
// because BrnGameModule::GamePrepare @0x823EFBD0 brackets the SAME pair on the X360.
namespace
{
    BrnResource::GameDataIO::InputBuffer  s_GameDataInput;
    BrnResource::GameDataIO::OutputBuffer s_GameDataOutput;
    bool                                  s_bIOConstructed = false;

    void EnsureScriptedLoadGameDataIO()
    {
        if (s_bIOConstructed)
            return;
        s_bIOConstructed = true;
        // The IOBuffer base status must be raised before any lock (LockForWrite asserts
        // eStatusConstructed). GameDataIO's own lifecycle members are still the deferred
        // slice, so run the base Construct explicitly, then bring up the request queue.
        s_GameDataInput.CgsModule::IOBuffer::Construct();
        s_GameDataInput.LockForWrite();
        s_GameDataInput.GetRequestInterface()->mRequestQueue.Construct();
        // The AttribSys request queue (DWARF BrnGameDataModuleIO.cpp:53: InputBuffer::
        // Construct also runs VariableEventQueue<32768,16>::Construct on it).
        s_GameDataInput.GetAttribSysRequestInterface()->mRequestQueue.Construct();
        s_GameDataInput.UnlockForWrite();
        s_GameDataOutput.CgsModule::IOBuffer::Construct();
        s_GameDataOutput.LockForWrite();
        s_GameDataOutput.Construct();   // the member-clearing OutputBuffer::Construct
        s_GameDataOutput.UnlockForWrite();
    }
}

namespace BrnGameMainFlowController
{
    BrnResource::GameDataIO::InputBuffer* GetScriptedLoadGameDataInput()
    {
        EnsureScriptedLoadGameDataIO();
        return &s_GameDataInput;
    }

    BrnResource::GameDataIO::OutputBuffer* GetScriptedLoadGameDataOutput()
    {
        EnsureScriptedLoadGameDataIO();
        return &s_GameDataOutput;
    }
}

void LoadingScriptedState::Update()
{
    EnsureScriptedLoadGameDataIO();

    if (gBrnScriptedLoadStage != 8)
    {
        s_GameDataInput.LockForWrite();
        s_GameDataOutput.LockForRead();

        // byte_82FAE28E -- the scripted-load pause flag (set by CheckDiskSpace::OnEnter,
        // cleared by the title pre-accept ResumeLoadingWorld). NOTE the PC flow currently
        // skips E_MGS_CHECK_DISK_SPACE (initial loading advances straight to marketing),
        // so the load runs unpaused from the marketing screens.
        if (!gBrnCheckDiskSpaceEntered)
        {
            switch (gBrnScriptedLoadStage)
            {
            case 0:
            case 1:
                gBrnScriptedLoadStage = 1;
                // X360: LoadSoundModuleAgain -- the sound module's second prepare pass.
                // [deferred: the first pass already ran in the initial loading; advance]
                LogScriptedStageOnce(1, "LoadSoundModuleAgain [deferred]");
                // fall through
            case 2:
                gBrnScriptedLoadStage = 2;
                // X360: LoadEffectsModule. [deferred: effects module placeholder]
                LogScriptedStageOnce(2, "LoadEffectsModule [deferred]");
                // fall through
            case 3:
                gBrnScriptedLoadStage = 3;
                // ⭐ REAL since 2026-08-11 (was `[deferred]`, which is why PROGRESSION.DAT never
                // loaded and OnPlayerCarChange fired "lpProgressionData != NULL"). X360:
                // `if (!LoadGameState2(this, gameDataIn)) break;`
                LogScriptedStageOnce(3, "LoadGameState2 -- real");
                if (!LoadGameState2(&s_GameDataInput))
                    break;
                // fall through
            case 4:
                gBrnScriptedLoadStage = 4;
                // X360: the GUI module's second prepare (GuiModule vtable +92).
                // [deferred: the PC GUI module prepared through the inline hookup]
                LogScriptedStageOnce(4, "GUI second prepare [deferred]");
                // fall through
            case 5:
                gBrnScriptedLoadStage = 5;
                LogScriptedStageOnce(5, "LoadWorldModule -- real");
                if (!LoadWorldModule(&s_GameDataInput, &s_GameDataOutput))
                    break;
                // fall through
            case 6:
                gBrnScriptedLoadStage = 6;
                // X360: post sound event 297 (the "world loaded" cue) into this frame's
                // Root sound input buffer, then jump straight to DONE (8). [deferred: the
                // per-frame sound IO bracket is not threaded through this spine yet]
                LogScriptedStageOnce(6, "sound world-loaded cue [deferred] -> DONE");
                gBrnScriptedLoadStage = 8;
                break;
            case 7:
                gBrnScriptedLoadStage = 7;
                // X360: LoadWorldCollision @0x823E73E0 -- WorldModule::PrepareWorldCollision,
                // then EffectsModule::PostWorldPreparePrepare on completion (that last hop
                // is still deferred; see LoadWorldCollision's own note).
                LogScriptedStageOnce(7, "LoadWorldCollision -- real");
                if (!LoadWorldCollision(&s_GameDataInput, &s_GameDataOutput))
                    break;
                gBrnScriptedLoadStage = 8;
                break;
            case 8:
                break;
            default:
                CGS_ASSERT(false, "Unhandled meLoadingStage");   // X360 BrnLoadingScriptedState.cpp:237
                break;
            }
        }

        s_GameDataOutput.UnlockForRead();
        s_GameDataInput.UnlockForWrite();
    }

    {
        // ---- the per-frame WORLD UPDATE leg (X360 0x823F22D8, stage > 5) ------------
        // Once the scripted load is past LoadWorldModule (stage > 5) the spine drives the
        // world module's per-frame Update and then forwards its staged resource requests
        // into the GameData input -- this is what turns the world streamer's per-frame
        // PVS/zone work into TRK/PVS/prop LoadBundle requests on the GameData pump.
        //
        // X360 order (reproduced):
        //   if (stage > 5) { BridgeSoundToWorld(worldIn, soundOut);
        //                    if (!(updateSet & 0x20) && !(updateSet & 0x40)) {
        //                        LinearMalloc::FreeAll(gm->mpWorldUpdateFrameAllocator);
        //                        world->vtbl+76(updateSet, inStack, outStack,
        //                                       worldIn, worldOut, frameAllocator); } }
        //   ... later ...  if (stage > 5) BridgeWorldToResource(gameDataIn, worldOut);
        // The loading update set is 0x80 (ConstructUpdateSetFromFsm @0x823BD420 base),
        // so neither the boot-video (0x20) nor the paused (0x40) bit is set here.
        //
        // [deferred] BridgeSoundToWorld: the per-frame Root sound IO bracket is not
        // threaded through this spine yet (same deferral as the stage-6 sound cue), and
        // BridgeWorldToSound likewise. Neither feeds the streamer.
        if (gBrnScriptedLoadStage > 5)
        {
            UpdateWorldModule(&s_GameDataInput);
        }

        // The GameData pump used to run here. It has MOVED to the frame level -- the game
        // module's per-frame resource tick (BrnGameModule::ResourceUpdateThread, the console's
        // IThreadClass::ResourceUpdateThread @0x823BC9B8) -- so that EVERY flow state's staged
        // requests are serviced, not just this spine's. The X360's scripted-load spine
        // @0x823F22D8 does not pump the module either: the resource thread does, concurrently.
    }
    // stage 8 (DONE): the X360 runs the full module cascade (BrnGameModule::DoUpdate);
    // the PC host loop owns the module drive -- see DoUpdate's PC-platform note.
}

// @ 0x823E75A8 - one frame of the sound-module load. The X360 body:
//   * CreateIOBuffer<Io::RootInputBuffer>(gm->mpUpdateInputBufferStack,  &lpRootIn,  "Sound")
//     CreateIOBuffer<Io::RootOutputBuffer>(gm->mpUpdateOutputBufferStack, &lpRootOut, "Sound")
//   * prepared = gm->mSoundModule.Prepare(lpGameDataOutputBuffer->GetAllocatorList(),
//                    gm->mpUpdateInputBufferStack, gm->mpUpdateOutputBufferStack,
//                    lpRootIn, lpRootOut)                                   (vtable +64)
//   * still preparing: LockForRead(lpRootOut); append the RootOutputBuffer's AttribSys queue
//     (VariableEventQueue<32768,16>::Append<2048,16>) + its resource-request interface
//     (AppendRequestInterface<4096>) into lpGameDataInputBuffer; UnlockForRead.
//   * destroy the two buffers (output first) and return `prepared`.
bool LoadingScriptedState::LoadSoundModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                                           const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer)
{
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    CgsModule::IOBufferStack* lpUpdateInputStack  = lpGameModule->GetUpdateInputBufferStack();
    CgsModule::IOBufferStack* lpUpdateOutputStack = lpGameModule->GetUpdateOutputBufferStack();

    BrnSound::Module::Io::RootInputBuffer*  lpRootInputBuffer  = 0;
    BrnSound::Module::Io::RootOutputBuffer* lpRootOutputBuffer = 0;
    lpUpdateInputStack->CreateIOBuffer<BrnSound::Module::Io::RootInputBuffer>(&lpRootInputBuffer, "Sound");
    lpUpdateOutputStack->CreateIOBuffer<BrnSound::Module::Io::RootOutputBuffer>(&lpRootOutputBuffer, "Sound");

    // [follow-on] The loading state's per-frame GameData IO bracket (the X360 Update creates the
    // GameData input/output buffers each frame and threads them through every LoadXxxModule) is
    // not reconstructed yet, so the buffers arrive null here; the allocator list is then null and
    // Prepare's carve stages stay allocator-gated (see BrnRootSoundModule.cpp).
    // GetAllocatorList() const now returns const AllocatorList* (the +4 member is a pointer, not an
    // embedded list -- see the OutputBuffer layout fix in BrnGameDataModuleIO.h), so no address-of here.
    const BrnResource::GameDataIO::AllocatorList* lpAllocatorList =
        lpGameDataOutputBuffer ? lpGameDataOutputBuffer->GetAllocatorList() : 0;

    bool lbPrepared = BrnGame::GetMainSoundModule()->Prepare(
        lpAllocatorList, lpUpdateInputStack, lpUpdateOutputStack,
        lpRootInputBuffer, lpRootOutputBuffer);

    if (!lbPrepared && lpGameDataInputBuffer)
    {
        // Still preparing: forward the module's resource requests into the GameData input.
        // [gated] the RootOutputBuffer request interfaces + getters (the X360's
        // GetAttribSysRequestInterface / GetResourceRequestInterface reads under LockForRead)
        // are still the minimal Io slice; the forwarding lands when those members do.
    }

    lpUpdateOutputStack->DestroyIOBuffer<BrnSound::Module::Io::RootOutputBuffer>(&lpRootOutputBuffer);
    lpUpdateInputStack->DestroyIOBuffer<BrnSound::Module::Io::RootInputBuffer>(&lpRootInputBuffer);
    return lbPrepared;
}

// --- MainGameFlowStateInitialLoadingScreen (the boot loading screen) --------------------
MainGameFlowStateInitialLoadingScreen::MainGameFlowStateInitialLoadingScreen()
    : meLoadingScreenStage(E_LOADINGSTAGE_START)
    , mpInputModuleAllocator(0)
    , mbGuiPreloadDone(false)
{
}

// @ 0x823AA950 - reset the load stages to START and raise the renderer's loading-screen
// signal (Option B bridge for the game-module global the X360 writes).
void MainGameFlowStateInitialLoadingScreen::OnEnter()
{
    meLoadingStateStage  = E_LOADINGSTATESTAGE_START;
    meLoadingScreenStage = E_LOADINGSTAGE_START;
    mbGuiPreloadDone     = false;
    gBrnInitialLoadingComplete = false;
    // The show command posts from Update each tick (the X360 writes the dispatch write
    // buffer's command every Update @0x823EF688; Update runs this same frame).

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: OnEnter - loading screen shown\n";
}

// @ 0x823AA9E8 - clear the loading-screen signal on exit (unless the GUI BF_LOADING state owns it,
// in which case BootLoading::OnLeave -> StopLoadingScreen drops it).
void MainGameFlowStateInitialLoadingScreen::OnLeave()
{
    if (!gBrnGuiDrivesLoadingScreen)
        GetDispatchWriteBuffer()->HideLoadingScreen();
}

// @ 0x823EF688 - the scripted module-by-module load. The X360 body loads one module per stage
// (controller -> GUI -> director -> sound -> network -> juice -> massive -> replays) via the
// LoadXxxModule helpers, advancing meLoadingScreenStage as each completes. Reconstructed here
// as the real stage-machine control flow; the per-stage module loads are filled in as each
// module TU is reconstructed. Advances one stage per update and holds at DONE (the screen
// stays up until the next flow state is reconstructed).
namespace
{
    // The real DWARF/PS3 stage identities (E_LOADINGSTAGE_*). On the X360 ARTIST spine the late
    // stages (Juice/Massive/Replays) DON'T load those subsystems -- see the per-case notes below.
    const char* const kapcStageNames[] = {
        "START", "ControllerModule", "GUIModule", "DirectorModule", "SoundModule",
        "Network", "Juice", "Massive", "Replays", "DONE",
    };

    // [INTERIM PACING] The per-stage LoadXxxModule loads are still stubs, so gate each stage on a
    // short engine-clock dwell (~0.4s) to keep the loading screen visible. The real X360 advances a
    // stage when its async module load reports complete; this dwell is removed per stage as each real
    // LoadXxxModule is reconstructed (the real load then provides the natural timing). Resets itself
    // each time it elapses so the next stage starts a fresh dwell.
    u32 g_uStageStartTick = 0;
    bool StageDwellElapsed()
    {
        const u32 luNow  = CgsSystem::GetSystemTimerBaseTime();
        const u32 luFreq = CgsSystem::GetSystemTimerFrequency();
        if (g_uStageStartTick == 0)
            g_uStageStartTick = luNow;
        if (luFreq != 0 && (luNow - g_uStageStartTick) < (luFreq * 4u / 10u))
            return false;
        g_uStageStartTick = luNow;
        return true;
    }
}

// @ 0x823EF688 - the real boot-load stage machine: load one engine module per stage
// (Controller -> GUI -> Director -> Sound -> Network), then the GameDataModule prepare, then
// FinishLoading. The X360 also creates the GUI IO buffers and renders + bridges the GUI each frame
// while loading (RendererIO + BrnRendererModule::Update + BridgeGameToGui/BridgeGuiToResource/
// BridgeGuiToGame + RenderGUI + GuiEventTimeInfo). That render/bridge loop needs the per-module IO
// types (CgsGui::CgsGuiModuleIO / ViewIO / ModelIO / RendererIO) which aren't reconstructed yet, so
// it is a [follow-on]; for now the GUI + MovieManager keep running through BrnGameModule's inline
// hookup. The per-stage LoadXxxModule bodies are also [stubs] (interim dwell) until reconstructed.
void MainGameFlowStateInitialLoadingScreen::Update()
{
    // ⭐ THE PER-FRAME GAMEDATA IO BRACKET (X360 @0x823EF688, its first statements).
    // The console opens this Update by reading the game module's GameData buffer pair
    // (gm+10055440 input / gm+10055444 output -- the SAME pair GamePrepare and the scripted
    // spine bracket), taking the input's WRITE lock and the output's READ lock, and holding
    // both across the whole stage switch. Every LoadXxxModule is then threaded with the pair:
    // that is how a module's staged resource requests reach the streaming pump while the boot
    // loading screen is up. Reproduced exactly here.
    BrnResource::GameDataIO::InputBuffer*  lpGameDataInput =
        BrnGameMainFlowController::GetScriptedLoadGameDataInput();
    BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput =
        BrnGameMainFlowController::GetScriptedLoadGameDataOutput();

    // X360 @0x823EF688: the state re-posts the show command onto the dispatch write buffer
    // every update tick (the one-shot slot is wiped by each end-of-frame swap).
    GetDispatchWriteBuffer()->ShowLoadingScreen();

    // The debug manager updates every frame while loading (X360 gates this on stage > Controller).
    if (meLoadingScreenStage > E_LOADINGSTAGE_CONTROLLERMODULE)
    {
        if (CgsDev::DebugManager* lpDebug = CgsDev::DebugManager::ThreadSafeAquire())
        {
            lpDebug->Update(1.0f / 60.0f);
            CgsDev::DebugManager::ThreadSafeRelease(lpDebug);
        }
    }

    lpGameDataInput->LockForWrite();
    lpGameDataOutput->LockForRead();

    switch (meLoadingScreenStage)
    {
    case E_LOADINGSTAGE_START:
    case E_LOADINGSTAGE_CONTROLLERMODULE:
        // X360: LoadControllerModule (0x823C68C0) -- allocates the Controller module's input
        // rw::core::GeneralResourceAllocator from the GameData allocator. [stub: interim dwell;
        // real load = Phase 3]
        meLoadingScreenStage = E_LOADINGSTAGE_CONTROLLERMODULE;
        if (StageDwellElapsed())
            AdvanceLoadingStage(E_LOADINGSTAGE_GUIMODULE);
        break;
    case E_LOADINGSTAGE_GUIMODULE:
        // X360: LoadGUIModule (0x823EF310) -- the GUI module load stage ALSO posts the
        // initial flow FSMs: GuiEventRunFsm{BrnBFPreFsm -> HUD} (BF_PRELOAD, the first
        // boot state) and GuiEventRunFsm{BrnOverlay -> OVERLAY} (the popup overlay flow,
        // record {fsmId, stateId 0, flow 2} @0x823EF3A0). The GUI module itself is
        // prepared by BrnGameModule's inline hookup; the RunFsm posts are the real kick.
        {
            BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
            CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput = lpGameModule->GetGuiInputBuffer();
            if (lpGuiInput != 0)
            {
                BrnGui::GuiEventRunFsm lEvent;
                lEvent.mFsmId          = CgsIDCompress("BrnBFPreFsm");
                lEvent.mInitialStateId = 0;
                lEvent.meFsmToRun      = BrnGui::E_GUI_HUD_BOOT;
                lEvent.meFlowToUse     = BrnGui::E_GUIFLOW_HUD;
                lpGuiInput->LockForWrite();
                lpGuiInput->GetGuiEvents()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lEvent), 144,
                    static_cast<s32>(sizeof(lEvent)));

                // The overlay kick (the X360 posts it back-to-back in the same write
                // bracket; the state id stays 0 = the script's initial state).
                lEvent.mFsmId          = CgsIDCompress("BrnOverlay");
                lEvent.mInitialStateId = 0;
                lEvent.meFsmToRun      = BrnGui::E_GUI_HUD_BOOT;
                lEvent.meFlowToUse     = BrnGui::E_GUIFLOW_OVERLAY;
                lpGuiInput->GetGuiEvents()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lEvent), 144,
                    static_cast<s32>(sizeof(lEvent)));
                lpGuiInput->UnlockForWrite();
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint
                        << "InitialLoadingScreen: posted RunFsm(BrnBFPreFsm -> HUD) + RunFsm(BrnOverlay -> OVERLAY)\n";
                AdvanceLoadingStage(E_LOADINGSTAGE_DIRECTORMODULE);
            }
        }
        break;
    case E_LOADINGSTAGE_DIRECTORMODULE:
        // X360 0x823E74C0: LoadDirectorModule -- REAL, and now threaded with this frame's
        // GameData IO pair (2026-07-29). DirectorModule::Prepare's stage 3 is
        // WorldMap::LoadData, whose TriggerData / traffic-lane / AI-lane requests ride the
        // director OUTPUT buffer; LoadDirectorModule appends them into the GameData input on
        // every not-yet-prepared tick, and the resource pump services them.
        {
            static bool s_bLoggedDirectorLoad = false;
            if (!s_bLoggedDirectorLoad)
            {
                s_bLoggedDirectorLoad = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint
                        << "InitialLoadingScreen: loading stage 3 (DirectorModule) -- real load\n";
            }
            if (LoadDirectorModule(lpGameDataInput, lpGameDataOutput))
                AdvanceLoadingStage(E_LOADINGSTAGE_SOUND_MODULE);
        }
        break;
    case E_LOADINGSTAGE_SOUND_MODULE:
        // X360: LoadSoundModule (0x823E75A8) -- the real per-frame load: create the Root sound IO
        // buffer pair on the update IO stacks, drive RootSoundModule::Prepare (vtable+64) until it
        // reports prepared, forwarding its resource requests meanwhile, then advance. Now threaded
        // with this frame's GameData IO pair, exactly as the X360 threads every LoadXxxModule
        // (the request forward inside LoadSoundModule is still gated on the RootOutputBuffer's
        // request interfaces -- see that body).
        {
            static bool s_bLoggedSoundLoad = false;
            if (!s_bLoggedSoundLoad)
            {
                s_bLoggedSoundLoad = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: loading stage 4 (SoundModule) -- real load\n";
            }
            if (LoadSoundModule(lpGameDataInput, lpGameDataOutput))
                AdvanceLoadingStage(E_LOADINGSTAGE_NETWORK);
        }
        break;
    case E_LOADINGSTAGE_NETWORK:
        // X360: LoadNetworkModule. [stub]
        if (StageDwellElapsed())
            AdvanceLoadingStage(E_LOADINGSTAGE_JUICE);
        break;
    case E_LOADINGSTAGE_JUICE:
        // [X360 ARTIST divergence] The PS3 DecFIGS build loads Juice here; the X360 ARTIST Update
        // does NOT -- stage 6 is a passthrough. (The enum name is the DWARF/PS3 identity.)
        AdvanceLoadingStage(E_LOADINGSTAGE_MASSIVE);
        break;
    case E_LOADINGSTAGE_MASSIVE:
        // [X360 ARTIST divergence] No Massive load here; the X360 sets the "engine modules loaded"
        // flag (off_830102D0 + 10097260 = 1) at this stage. [follow-on: that game-module flag isn't
        // mapped in this incremental layout]
        AdvanceLoadingStage(E_LOADINGSTAGE_REPLAYS);
        break;
    case E_LOADINGSTAGE_REPLAYS:
        // ⚠️ CORRECTED 2026-07-29. This case's old note claimed the X360 prepares the
        // GameDataModule here. It does NOT: the asm is
        // `(*(*(gm+0x8BD300) + 0x40))(gm+0x8BD300, GetAllocatorList(gameDataOutput))`, and
        // gm+0x8BD300 is the REPLAYS module (the only other functions that touch that offset
        // are BrnGameModule::Construct/Destruct/GameRelease and DoUpdate_Replays{Pre,Post}Sim).
        // So stage 8 really is a Replays prepare taking just the allocator list.
        // The GameDataModule is prepared MUCH earlier on the console -- BrnGameModule::Prepare
        // @0x823DB848 stage 3 pumps `(*(gm[1561280]+64))(gm+6245120, inStack, outStack,
        // gameDataInput, gameDataOutput)` until it returns true, before the flow controller
        // exists -- and BrnGameModule::GamePrepare already reproduces that on the PC (it drives
        // mGameDataModule.Prepare to completion before GameMain runs). Keeping the call here is
        // therefore harmless (the module's own resumable machine reports done immediately) and
        // it is the Replays prepare that is [deferred].
        if (!g_bLoggedGameDataPrepare)
        {
            g_bLoggedGameDataPrepare = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: stage 8 (Replays prepare [deferred])\n";
        }
        if (BrnGame::GetMainGameDataModule()->Prepare(0, 0))
            AdvanceLoadingStage(E_LOADINGSTAGE_DONE);
        break;
    case E_LOADINGSTAGE_DONE:
    default:
        // Load complete (once): raise the completion signal + advance the flow (FinishLoading ->
        // SendEvent(STATEEND) -> MARKETING_SCREENS). The GUI BF_LOADING state watches
        // gBrnInitialLoadingComplete and dismisses the loading screen itself (StopLoadingScreen) as it
        // proceeds to BF_VIDEOS; only when the GUI isn't driving do we drop the screen here. Guarded so
        // the held DONE stage doesn't re-fire every frame.
        if (!gBrnInitialLoadingComplete)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: loading complete\n";
            FinishLoading();
            gBrnInitialLoadingComplete = true;
            if (!gBrnGuiDrivesLoadingScreen)
                GetDispatchWriteBuffer()->HideLoadingScreen();
        }
        break;
    }

    // Close the frame's GameData IO bracket (X360 @0x823EF688's tail: UnlockForWrite(input),
    // UnlockForRead(output), then the GUI IO buffer teardown). The resource pump takes its own
    // locks, so it can only run once these are released -- see BrnGameModule's per-frame
    // resource tick.
    lpGameDataOutput->UnlockForRead();
    lpGameDataInput->UnlockForWrite();
}

// Advance to the next load stage + log it (the X360 stages are visible in BrnGame.log so the real
// progression can be followed). Kept separate so each stage's real LoadXxxModule (Phase 3+) just
// calls this on its own completion instead of the interim dwell.
void MainGameFlowStateInitialLoadingScreen::AdvanceLoadingStage(ELoadingScreenStage leNextStage)
{
    meLoadingScreenStage = leNextStage;
    if ((CgsDev::Message::gxMessageFilterFlags & 1) &&
        leNextStage >= E_LOADINGSTAGE_START && leNextStage <= E_LOADINGSTAGE_DONE)
    {
        *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: loading stage "
                                   << (s32)leNextStage << " ("
                                   << kapcStageNames[leNextStage] << ")\n";
    }
}

// @ 0x823C6AC8 - the load is complete: advance the flow. The X360 gates on a completion flag
// (this+0xC) and stamps a game-module load-state field to 5 before firing; here the Update stage
// machine gates the call (it fires FinishLoading once, at DONE) and that game-module field isn't
// mapped in this incremental layout. SendEvent(STATEEND) takes LOADING -> MARKETING_SCREENS.
void MainGameFlowStateInitialLoadingScreen::FinishLoading()
{
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SendEvent(BrnGameMainFlowController::E_MGE_STATEEND);
}

// --- MainGameFlowStateStartScreen (the legal/title screen phase) -------------------------
MainGameFlowStateStartScreen::MainGameFlowStateStartScreen() : meLoadingStage(E_STARTSCREEN_START) {}

// @ 0x823AAB18 -- request the BF_LEGAL GUI FSM stage (BrnLegalFsm; the X360 writes the
// pending-stage byte = 2) and clear the load-stage entered flag.
void MainGameFlowStateStartScreen::OnEnter()
{
    BrnGame::GetMainGameModule()->RequestGuiFsmStage(2);
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "StartScreen: OnEnter -> GUI FSM stage 2 (BrnLegalFsm)\n";
}
void MainGameFlowStateStartScreen::OnLeave() {}

// @ 0x823F2D78 -- the title screen's flow decisions: command 71 (pre-accept) resumes the
// world load while the accept-dwell animation plays; command 70 (accepted) advances the
// main flow (-> MEMORY_CARD / BF_PROFILE). The per-frame GUI drive itself runs in
// BrnGameModule::GameMain (the console's LoadingScriptedState::Update shared spine).
void MainGameFlowStateStartScreen::Update()
{
    // X360 0x823F2D78 opens with the shared scripted-load spine.
    LoadingScriptedState::Update();

    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    if (lpGameModule->ConsumeGuiPreAccept())
    {
        // LoadingScriptedState::ResumeLoadingWorld @0x823A85B0 -- unpause the scripted
        // world load during the accept dwell. Guarded on the pause flag: the PC flow
        // skips CHECK_DISK_SPACE, so the load usually runs unpaused (calling the
        // unconditional X360 resume would fire its stage==START assert mid-load).
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "StartScreen: pre-accept (71) -> resume world load\n";
        if (gBrnCheckDiskSpaceEntered)
            ResumeLoadingWorld();
    }
    if (lpGameModule->IsGuiPhaseComplete())
    {
        if (BrnGameMainFlowController::gpMainGameFlowController != 0)
            BrnGameMainFlowController::gpMainGameFlowController->SendEvent(
                BrnGameMainFlowController::E_MGE_STATEEND);
    }
}
void MainGameFlowStateStartScreen::Render() {}

// --- MainGameFlowStateMarketingScreens (the boot-logo phase) -----------------------------
// The logos themselves are owned by the GUI module's HUD flow (BF_VIDEOS / BrnGui::
// BootVideos plays the EA/Criterion logos); this state requests the FSM stage and waits
// for the flow's phase-complete command.
MainGameFlowStateMarketingScreens::MainGameFlowStateMarketingScreens() {}

// @ 0x823AAA08 -- request the BF_VIDEOS GUI FSM stage (BrnVideoFsm; pending-stage byte = 1).
void MainGameFlowStateMarketingScreens::OnEnter()
{
    BrnGame::GetMainGameModule()->RequestGuiFsmStage(1);
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "MarketingScreens: OnEnter -> GUI FSM stage 1 (BrnVideoFsm)\n";
}

void MainGameFlowStateMarketingScreens::OnLeave() {}

// @ 0x823F2CD8 -- advance when the GUI posts phase-complete (command 70: the Criterion
// logo finished).
void MainGameFlowStateMarketingScreens::Update()
{
    // X360 0x823F2CD8 opens with the shared scripted-load spine.
    LoadingScriptedState::Update();

    if (BrnGame::GetMainGameModule()->IsGuiPhaseComplete() &&
        BrnGameMainFlowController::gpMainGameFlowController != 0)
    {
        BrnGameMainFlowController::gpMainGameFlowController->SendEvent(
            BrnGameMainFlowController::E_MGE_STATEEND);
    }
}

void MainGameFlowStateMarketingScreens::Render() {}

// --- MainGameFlowStateCheckDiskSpace ----------------------------------------------------
// Standalone byte global the X360 OnEnter clears-then-sets (byte_82FAE28E). It is NOT a member
// (the X360 stores to an absolute .data address, not a `this`-relative slot): it is set to 0 on
// entry and 1 once OnEnter has finished its stage stamp -- a "disk-check OnEnter has run" flag.
bool gBrnCheckDiskSpaceEntered = false;

MainGameFlowStateCheckDiskSpace::MainGameFlowStateCheckDiskSpace() {}

// @ 0x823AAA98 - clear the entered-flag, assert we arrived at the START stage, then stamp the
// game-module load-state slot to 4 and raise the entered-flag. (No disk/storage query is issued
// here -- the X360 body is just these flag writes + the START-stage assert + the stage stamp.)
void MainGameFlowStateCheckDiskSpace::OnEnter()
{
    gBrnCheckDiskSpaceEntered = false;

    // X360 gated the assert on the global assert-enabled flag (dword_82FAE4B0); CGS_ASSERT subsumes
    // that gate. The X360 reports this from BrnGameMainFlowStates.h:195 (the OnEnter decl site).
    CGS_ASSERT(meLoadingStateStage == E_LOADINGSTATESTAGE_START,
               "meLoadingStateStage == E_LOADINGSTATESTAGE_START");

    gBrnCheckDiskSpaceEntered = true;

    // X360: *(off_830102D0 + 0x9A0644) = 4 -- stamp the value 4 into a game-module-aggregate slot
    // (the same off_830102D0 base that holds the flow controller at +0x9A0664 and the loading-screen
    // signal at +0x99FE38). The recovered immediate is 4. [follow-on] that game-module field isn't
    // mapped in this incremental layout (same status as the +10097260 modules-loaded flag above), so
    // the stamp has no modelled target yet; the flag writes + assert above are faithful.
}
void MainGameFlowStateCheckDiskSpace::OnLeave() {}
// Update @ 0x823F2D28 lives in its DWARF home TU, BrnGameMainFlowCheckDiskSpace.cpp.
void MainGameFlowStateCheckDiskSpace::Render() {}

// --- MainGameFlowStateMemoryCard (the profile / save-device phase) -----------------------
MainGameFlowStateMemoryCard::MainGameFlowStateMemoryCard() {}

// @ 0x823AAC48 -- request the BF_PROFILE GUI FSM stage (BrnBFProFsm; pending-stage byte = 4;
// the X360 skips to 5 under the autotest flags -- no autotest on PC).
void MainGameFlowStateMemoryCard::OnEnter()
{
    BrnGame::GetMainGameModule()->RequestGuiFsmStage(4);
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "MemoryCard: OnEnter -> GUI FSM stage 4 (BrnBFProFsm)\n";
}
void MainGameFlowStateMemoryCard::OnLeave() {}

// @ 0x823F2F98 -- gated on the world-load stage (X360 meLoadingStateStage global == 8);
// advance when the GUI posts phase-complete (the profile task resolved). [FLAG world
// gate: the PC initial load completes synchronously; gBrnInitialLoadingComplete is the
// stand-in for the world-loaded stage.]
void MainGameFlowStateMemoryCard::Update()
{
    // X360 0x823F2F98 opens with the shared scripted-load spine (its advance gate is the
    // scripted stage == 8; the PC keeps the gBrnInitialLoadingComplete stand-in gate
    // below until the world load can really complete).
    LoadingScriptedState::Update();

    if (!gBrnInitialLoadingComplete)
        return;
    if (BrnGame::GetMainGameModule()->IsGuiPhaseComplete() &&
        BrnGameMainFlowController::gpMainGameFlowController != 0)
    {
        BrnGameMainFlowController::gpMainGameFlowController->SendEvent(
            BrnGameMainFlowController::E_MGE_STATEEND);
    }
}
void MainGameFlowStateMemoryCard::Render() {}

// --- MainGameFlowStateCompleteLoading (the post-title / compound-load phase) -------------
MainGameFlowStateCompleteLoading::MainGameFlowStateCompleteLoading() : mbIsCollisionWorldPrepared(false) {}

// @ 0x823AABD0 -- request the BF_COMPLOAD GUI FSM stage (BrnCmpLdFsm; pending-stage
// byte = 3) and reset the settle latch (X360 this+4 = 0).
void MainGameFlowStateCompleteLoading::OnEnter()
{
    BrnGame::GetMainGameModule()->RequestGuiFsmStage(3);
    mbIsCollisionWorldPrepared = false;
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "CompleteLoading: OnEnter -> GUI FSM stage 3 (BrnCmpLdFsm)\n";
}
void MainGameFlowStateCompleteLoading::OnLeave() {}

// @ 0x823F2E08 -- when the GUI posts phase-complete (the post-title intro finished) and
// the world is loaded: first pass stamps the load-state + latches (X360 sets the global
// load stage to 7), second pass advances to IN_GAME. (The quit-to-dash branch and the
// X360 load-state global are platform/world follow-ons.)
void MainGameFlowStateCompleteLoading::Update()
{
    // X360 0x823F2E08 opens with the shared scripted-load spine, then -- once the GUI
    // phase completes with the scripted load DONE -- first pass kicks the world-collision
    // stage (dword_82FAE4B0 = 7) and latches, second pass advances to IN_GAME.
    LoadingScriptedState::Update();

    if (!gBrnInitialLoadingComplete)
        return;
    if (BrnGame::GetMainGameModule()->IsGuiPhaseComplete())
    {
        // ⭐ RESTORED 2026-08-10 to the console gate. The X360 @0x823F2E08 tests
        // `guiPhaseComplete && meLoadingStateStage == 8` on BOTH passes:
        //     pass 1  stage==8, latch clear  -> stage = 7, latch = 1
        //     passes 2..N  stage==7          -> the whole `if` is FALSE: the state HOLDS
        //                                       here while LoadingScriptedState::Update()
        //                                       (called at the top of this function every
        //                                       frame) drives the world-collision load
        //     pass N+1  stage back to 8, latch set -> SendEvent(E_MGE_STATEEND)
        // The PC had made the gate permissive because stage 7 was a no-op deferral and a
        // strict gate would have hung the boot. Stage 7 is real now, and the hold is the
        // whole point: it is the only place the collision load gets its frames (nothing
        // drives LoadingScriptedState::Update() once the flow reaches IN_GAME).
        //
        // ⚠️ [marked deviation -- anti-wedge bound] the console can hold here forever; a PC
        // build that fails to stream WORLDCOL.BIN would then never boot. The budget below
        // releases the flow after KI_MAX_COLLISION_LOAD_FRAMES and logs loudly, so a broken
        // collision load costs the collision, not the session. It is not hit on a good boot.
        static const s32 KI_MAX_COLLISION_LOAD_FRAMES = 1800;   // ~30 s at 60 Hz
        static s32 s_iCollisionLoadFrames = 0;

        if (gBrnScriptedLoadStage == 8)
        {
            s_iCollisionLoadFrames = 0;
            if (mbIsCollisionWorldPrepared)
            {
                if (BrnGameMainFlowController::gpMainGameFlowController != 0)
                    BrnGameMainFlowController::gpMainGameFlowController->SendEvent(
                        BrnGameMainFlowController::E_MGE_STATEEND);
            }
            else
            {
                gBrnScriptedLoadStage = 7;   // kick LoadWorldCollision
                mbIsCollisionWorldPrepared = true;
            }
        }
        else if (mbIsCollisionWorldPrepared && ++s_iCollisionLoadFrames > KI_MAX_COLLISION_LOAD_FRAMES)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "CompleteLoading: world-collision load did not finish in "
                    << KI_MAX_COLLISION_LOAD_FRAMES
                    << " frames (stage " << gBrnScriptedLoadStage
                    << ") -- releasing the flow [FLAG PC anti-wedge bound]\n";
            gBrnScriptedLoadStage = 8;
        }
    }
}
void MainGameFlowStateCompleteLoading::Render() {}

MainGameFlowStateInGame::MainGameFlowStateInGame() {}
// OnEnter/OnLeave/Update/Render are homed in BrnGameMainFlowInGameState.cpp (the DWARF
// home for the in-game state's per-frame surface); only the ctor stays with this group.

// ---------------------------------------------------------------------------
// The in-game per-frame world tick, called by MainGameFlowStateInGame::Update.
// Requests go into the same GameData input the scripted spine uses; the frame-level
// resource tick (BrnGameModule::ResourceUpdateThread) drains it for every flow state,
// so no extra pump is needed here.
// ---------------------------------------------------------------------------
void DriveInGameWorldUpdate()
{
    if (BrnGame::GetMainGameModule() == 0)
        return;
    DriveWorldUpdateFrame(&s_GameDataInput, KU_INGAME_UPDATE_SET);
}
