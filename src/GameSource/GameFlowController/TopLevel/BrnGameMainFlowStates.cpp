#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h"   // gpMainGameFlowController, SendEvent
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"   // DebugManager::Update during load
#include "GameSource/Resource/BrnGameDataModule.h"   // GameDataModule + BrnGame::GetMainGameDataModule()
#include "GameSource/Resource/BrnGameDataModuleIO.h" // GameDataIO::InputBuffer/OutputBuffer (LoadSoundModule args)
#include "GameSource/Sound/Module/BrnRootSoundModule.h"   // RootSoundModule + BrnGame::GetMainSoundModule() (stage 4)
#include "GameSource/Game/BrnGameModule.hpp"         // BrnGame::GetMainGameModule() (the update IO stacks)
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_OutputBuffer.h"   // EffectsIO::OutputBuffer (LoadEffectsModule scratch buffer)
#include "GameSource/World/BrnWorldModuleIO.h"       // BrnWorldIO::UpdateOutputBuffer (LoadWorldModule scratch buffer)
#include "GameSource/World/BrnWorldModule.h"         // BrnWorld::WorldModule::Update (the per-frame world drive)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h" // CgsMemory::LinearMalloc (the world frame allocator)
#include "GameSource/Game/BrnLoadingScreenRenderer.h" // BrnGame::ELoadingScreenCommand (the command slot values)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp" // DirectorIO::OutputBuffer (stage 3)
#include "GameSource/GameState/BrnGameStateModule.h"    // BrnGameState::GameStateModule::GetOutputBuffer (BridgeGameStateToWorld source)
#include "GameSource/GameState/BrnGameStateModuleIO.h"  // GameStateModuleIO::OutputBuffer (its lock bracket)
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"  // CgsGuiModuleIO::Input/OutputBuffer (the C4 sound-leg GUI endpoints)

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

// X360 byte_82FAE28E -- the scripted-load PARK flag. Defined further down with the
// CheckDiskSpace state (its historical home; it is not that state's private flag).
// ⭐ CORRECTED 2026-08-14 (boot audit F-P4-1): this is not a "CheckDiskSpace entered"
// marker -- EVERY pre-title OnEnter raises it (LOADING/MARKETING/CHECK_DISK/START each
// do clear -> assert stage==START -> set), MemoryCard/CompleteLoading enter with it
// cleared, and the single runtime clear is ResumeLoadingWorld on the title pre-accept.
// That discipline is what parks the whole world load behind the title on the console.
extern bool gBrnScriptedLoadPaused;

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
// @ 0x823C6AB0 -- the base FinishLoading is a tail-call into the flow controller's
// SendEvent(STATEEND); it is vtable-only (no static callers on either side). The PC body
// was empty, so a dispatch through it silently did nothing (boot audit F-P6-19).
void LoadingScriptedState::FinishLoading()
{
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SendEvent(
            BrnGameMainFlowController::E_MGE_STATEEND);
}

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
// START, then clear the park flag (byte_82FAE28E) so the per-frame spine below starts
// advancing the stages. The body is UNCONDITIONAL on the console; the guard lives at the
// call site (StartScreen::Update's 0x9A0649 pre-accept latch).
void LoadingScriptedState::ResumeLoadingWorld()
{
    CGS_ASSERT(gBrnScriptedLoadStage == E_LOADINGSTATESTAGE_START,
               "meLoadingStateStage == E_LOADINGSTATESTAGE_START");   // X360 h:209
    gBrnScriptedLoadPaused = false;
}

// @ 0x823E7820 -- one frame of the EFFECTS-module load (scripted-load stage 2).
//
// ⭐ REAL since 2026-09-02 (tyre-mark wave). It was `LogScriptedStageOnce(2,
// "LoadEffectsModule [deferred]")` and a fall-through, which is why EffectsModule::Prepare had
// never run: the trail system was never Constructed, the FX bundle was never requested, and
// TrailSystem::mbIsReady could not have been raised by anything.
//
// Console body, statement for statement:
//     CreateIOBuffer<EffectsIO::OutputBuffer>(updateOutputStack, &effectsOut)   (h:52 assert)
//     if (effectsModule->vtbl+0x40(gameDataOut->GetAllocatorList(), updateOutputStack, effectsOut))
//         { DestroyIOBuffer (h:57 assert); return true; }
//     LockForRead(effectsOut);
//     gameDataIn->attribQueue.Append<2048,16>(effectsOut->GetVaultRequestInterface()->queue);
//     gameDataIn->AppendRequestInterface<4096>(*effectsOut->GetResourceRequestInterface());
//     UnlockForRead(effectsOut);
//     DestroyIOBuffer; return false;
bool LoadingScriptedState::LoadEffectsModule(
        BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer)
{
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    CgsModule::IOBufferStack* lpUpdateOutputStack = lpGameModule->GetUpdateOutputBufferStack();

    BrnEffects::EffectsIO::OutputBuffer* lpEffectsOutput = 0;
    const bool lbCreated = lpUpdateOutputStack->CreateIOBuffer(&lpEffectsOutput, "Effects");
    CGS_ASSERT(lbCreated, "mpStack->CreateIOBuffer( &mpBuffer, lpcName )");   // CgsModuleIOHelper.h:52
    (void)lbCreated;
    if (lpEffectsOutput == 0)
        return false;

    const BrnResource::GameDataIO::AllocatorList* lpAllocatorList =
        lpGameDataOutputBuffer ? lpGameDataOutputBuffer->GetAllocatorList() : 0;

    const bool lbPrepared = lpGameModule->GetEffectsModule().Prepare(
        lpAllocatorList, lpUpdateOutputStack, lpEffectsOutput);

    // [effects-load] WHERE THE LADDER IS, once per stage change. This stage BLOCKS the whole
    // scripted load (the console's own `if (!LoadEffectsModule(...)) break;`), so a ladder that
    // stops advancing stops the boot -- and the first symptom is a null-progression crash three
    // stages later, not anything that names the effects module. Say it out loud instead.
    if (!lbPrepared)
    {
        static s32 siLastEffectsStage  = -1;
        static s32 siLastParticleStage = -1;
        const s32 liEffectsStage  = static_cast<s32>(lpGameModule->GetEffectsModule().GetPrepareStage());
        const s32 liParticleStage = static_cast<s32>(
            lpGameModule->GetEffectsModule().ParticleModuleRef().GetInitialLoadStage());
        if (liEffectsStage != siLastEffectsStage || liParticleStage != siLastParticleStage)
        {
            siLastEffectsStage  = liEffectsStage;
            siLastParticleStage = liParticleStage;
            char lacMsg[192];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[effects-load] EffectsModule::Prepare stage=%d  ParticleModule FX-load stage=%d "
                "(19 == DONE)\n", liEffectsStage, liParticleStage);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    if (!lbPrepared && lpGameDataInputBuffer != 0)
    {
        lpEffectsOutput->LockForRead();
        lpGameDataInputBuffer->GetAttribSysRequestInterface()->mRequestQueue.Append(
            lpEffectsOutput->GetVaultRequestInterface()->mRequestQueue);
        lpGameDataInputBuffer->AppendRequestInterface<4096>(
            *lpEffectsOutput->GetResourceRequestInterface());
        lpEffectsOutput->UnlockForRead();
    }

    const bool lbDestroyed = lpUpdateOutputStack->DestroyIOBuffer(&lpEffectsOutput);
    CGS_ASSERT(lbDestroyed, "mpStack->DestroyIOBuffer( &mpBuffer )");   // CgsModuleIOHelper.h:57
    (void)lbDestroyed;
    return lbPrepared;
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
// ⭐⭐ THE X360's TRUE ARM CALLS BrnEffects::EffectsModule::PostWorldPreparePrepare @0x822902F0,
// AND IT NOW RUNS. The deferral note that stood here said "the PC EffectsModule is still
// `u8 mOpaqueBody[0x2F550]` -- writing into an opaque slice at console byte offsets is the
// memory-bug class this project forbids". That was true when it was written and is STALE: the
// real EffectsModule, with named members (mSurfaceList at EffectsModule.h:381), has been on the
// build list since the effects wave, and this file's own stage 2 already drives its Prepare.
//
// The note also said its absence "cannot block the load" -- true, and beside the point. It is
// the ONLY writer of mSurfaceList, and the surface list is the tyre mark's LAST GATE:
// HandleWheels @0x82296C80 reads SkidMarksEnabled / the threshold / the type out of the
// surface's visualfxsurface sub-collection, and falls back to a ZEROED
// Attrib::DefaultDataArea(24) when the lookup fails. MEASURED with it deferred (runs 14-16,
// BRN_SKID_PROBE, 368 gate lines on the road):
//     [skid] ... surf=1/0 ref=0 en=0 skid=0.0440 > thr=0.0000 type=0 ready=1
//     [skid] ... surf=2/0 ref=0 en=0 skid=0.0452 > thr=0.0000 type=0 ready=1
// `/0` is Num_Surfaces() and `ref=0` is Surfaces(id) returning null -- an EMPTY list, because
// nothing had ever pointed the instance at the world's surfacelist collection. Three adjacent
// zeros that read exactly like a surface with skid marks deliberately switched off.
//
// A "gate is stale, not dead" case, and the question that found it is the one that note could
// not answer: WHEN DID THIS LAST RUN? Never.
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
        // ⛔ THE CALL IS BACK OUT, AND FOR A NEW REASON -- NOT THE OLD ONE. Turning it on
        // (runs 17 and 18) takes the boot down every time:
        //     Attrib::Instance::GetClass + 0x11
        //     BrnEffects::EffectsModule::PostWorldPreparePrepare + 0x298
        //     LoadingScriptedState::Update -> MainGameFlowStateCompleteLoading::Update
        //     exit 0xC0000005, phase=BOOT, strfin never
        // Run 18 carried a guard that returns early when
        // Attrib::FindCollectionWithDefault(surfacelist, StringToKey("340654")) yields nothing;
        // the guard did NOT fire (no announcement in the log) and the fault simply moved from
        // +0x22F to +0x298 -- i.e. Change() hands back a collection and the instance is STILL
        // unusable, so the first Num_Surfaces()/Surfaces() walks a null class inside AttribSys.
        // That is an AttribSys instance-binding problem, a subsystem away from effects, and it
        // is where the tyre mark now stands or falls.
        //
        // Everything else about this arm is now correct and stays: the collection key is
        // Attrib::StringToKey("340654") (sub_82C4A1F8 -> qword_82FAB7A8, read out of the image),
        // and the "EffectsModule layout is opaque" reason the old note gave is stale.
        // ⇒ NEXT STEP, precisely: make Attrib::Instance::Change bind a usable class for
        //   mSurfaceList, then delete this block and call PostWorldPreparePrepare here. Until
        //   then Num_Surfaces() is 0, HandleWheels reads Attrib::DefaultDataArea's zeros
        //   (en=0 thr=0.0 type=0) and no tyre mark can be laid -- measured, 368 gate lines.
        static bool s_bLoggedPostWorldPrepare = false;
        if (!s_bLoggedPostWorldPrepare)
        {
            s_bLoggedPostWorldPrepare = true;
            CgsDev::Log::WriteToLog(
                "[skid-ready] PostWorldPreparePrepare NOT CALLED -- it faults inside "
                "Attrib::Instance::GetClass on this build (measured runs 17/18). The surface "
                "list stays EMPTY, so HandleWheels reads the zeroed default data area and no "
                "tyre mark can be laid. This is the last gate.\n");
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
bool LoadingScriptedState::LoadGameState2(
        BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
        const BrnResource::GameDataIO::OutputBuffer* /*lpGameDataOutputBuffer*/)
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
void LoadingScriptedState::UpdateWorldModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                                             BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutput)
{
    // ⭐ CORRECTED 2026-08-16 (boot audit F-P3-5 / F-P6-13). The update set was the
    // hard-coded constant KU_LOADING_UPDATE_SET (0x80) for the whole boot. The console
    // derives it per frame from the flow state (ConstructUpdateSetFromFsm @0x823BD420 --
    // one of its four call sites is this very spine, @0x823F26D8) and then GUARDS the world
    // leg on it: @0x823F282C-44 tests !(set & 0x20) && !(set & 0x40), i.e. the world module
    // is NOT driven at all while a video phase or the save-load phase is up. The boot
    // progression is 0x80 (initial load) -> 0xA0 (marketing/title) -> 0xE1 (memory card)
    // -> 0xA0 (complete loading) -> 0x88 (in game).
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    const BrnUpdateSet lUpdateSet = lpGameModule->ConstructUpdateSetFromFsm();

    // Dev trace: one line per distinct set the boot walks through. The console progression
    // is 0x80 -> 0xA0 -> 0xE1 -> 0xA0 -> 0x88; seeing it here is the proof that the flow
    // states' save-load/video/in-game bytes are being written at all.
    {
        static u32 s_uLastUpdateSet = 0xFFFFFFFFu;
        if ((u32)lUpdateSet != s_uLastUpdateSet)
        {
            s_uLastUpdateSet = (u32)lUpdateSet;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "[flow] update set -> "
                                           << CgsDev::E_PRINTMODE_HEXONCE << (u32)lUpdateSet << "\n";
        }
    }

    // On 0x20/0x40 frames the console still runs BridgeSoundToWorld (@0x823F2818 sits
    // BEFORE the bit test @0x823F282C) -- but into a worldIn nothing then reads: the world
    // vcall is suppressed and the frame-spanned buffer dies unread at the teardown batch.
    // Skipping the whole drive here is therefore behaviourally identical, not a divergence.
    if ((lUpdateSet & 0x20) != 0 || (lUpdateSet & 0x40) != 0)
        return;

    DriveWorldUpdateFrame(lpGameDataInputBuffer, lUpdateSet, lpSoundPreUpdateOutput);
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
                           BrnUpdateSet lUpdateSet,
                           BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutput)
{
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    CgsModule::IOBufferStack* lpUpdateInputStack  = lpGameModule->GetUpdateInputBufferStack();
    CgsModule::IOBufferStack* lpUpdateOutputStack = lpGameModule->GetUpdateOutputBufferStack();

    // ⭐ THE INVENTED WORLD FRAME ALLOCATOR IS GONE, 2026-08-17 (boot audit F-P6-12). This
    // used to `new` a 512 KiB LinearMalloc behind a file-static bool, because the console's
    // allocator -- gm+0x9A0630, which LoadingScriptedState::Update FreeAll's before the world
    // drive and passes as the world virtual's 7th argument -- was never latched here.
    // It is now: BrnRendererModule::Update lends it (renderer+0xC8FC) through
    // RendererIO::OutputBuffer and GamePrepare's tail stores it, so the world drive uses the
    // renderer's allocator the way the console does.
    // The null fallback covers the frames before GamePrepare's first not-done pass has run.
    CgsMemory::LinearMalloc* lpWorldFrameAllocator =
        lpGameModule->GetReusableLoadingScreenAllocator();
    if (lpWorldFrameAllocator == 0)
        return;

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
    // @0x827AB830, which copies its 48 bytes into the physics input buffer, and every timestep
    // PhysicsModule::Update @0x825B0640 uses comes out of that copy. Without this line the block
    // stays Construct-cleared, GetCurrentTimeStep() reads 0, and the module conducts frames with
    // a zero timestep. With the timer staged it runs for real.
    // ⚠️ STALE HALF CORRECTED 2026-08-27: this note used to add that PhysicsModule::Update
    // "short-circuits ('sim timer not running -- inert this frame') for as long as the block
    // stays Construct-cleared". IT NO LONGER DOES -- that PC-only early return was an invented
    // arm with no console counterpart (the X360 body never reads the status block's mbRunning at
    // all) and it broke the sim-pause RESUME, so it was deleted in the pauseresume wave. Its full
    // obituary, asm proof and measurement are at the site in
    // BrnPhysicsModuleUpdateFunctions.cpp. This staging line is now the ONLY thing standing
    // between the module and a zero-timestep frame, which makes it more load-bearing, not less.
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

    // ⭐⭐ THE SOUND -> WORLD BRIDGE (X360 BridgeSoundToWorld @0x823CDC98, staged from the
    // loading spine @0x823F2818 and from DoUpdate_World's five-source staging on the full
    // spine -- LAST of the input bridges, immediately before the FreeAll + world dispatch).
    // One append: the world input's audio-car-loaded queue takes the sound pre-update
    // block's queue. The console bracket is R(preUpdateOut) + W(worldIn) (spec
    // progress/scratch_dossiers/spine_sound_leg_0x823F22D8.md section 5), the same
    // per-source pair this function already uses for the controller and game-state legs.
    // Null means the caller has no sound pre-update this frame (phase C4 threads it from
    // the scripted spine; the in-game leg arrives with the DoUpdate_Sound slice).
    if (lpSoundPreUpdateOutput != 0)
    {
        lpSoundPreUpdateOutput->LockForRead();
        lpWorldInput->LockForWrite();
        lpGameModule->BridgeSoundToWorld(lpWorldInput, lpSoundPreUpdateOutput);
        lpWorldInput->UnlockForWrite();
        lpSoundPreUpdateOutput->UnlockForRead();
    }

    // X360: LinearMalloc::FreeAll(gm->mpWorldUpdateFrameAllocator) then the vtable+76
    // dispatch. The loading update set is 0x80 (frustum testing on, nothing else).
    lpWorldFrameAllocator->FreeAll();   // @0x823F2858 -- once per world drive, as the console does
    if ((lUpdateSet & 0x20) != 0)
    {
        // The boot-video arm (X360 DoUpdate_World @0x823E8BD0: `(updateSet & 0x20) ?
        // UpdateForBootUpVideo(...)`). Reachable for the first time now that the flow
        // states write the video byte -- the reconstruction has existed unreached.
        static bool s_bLoggedBootUpVideoArm = false;
        if (!s_bLoggedBootUpVideoArm)
        {
            s_bLoggedBootUpVideoArm = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "[world] update set has the video bit -- UpdateForBootUpVideo arm entered\n";
        }
        lpGameModule->GetWorldModule().UpdateForBootUpVideo(lUpdateSet,
                                                           lpUpdateInputStack, lpUpdateOutputStack,
                                                           lpWorldInput, lpWorldOutput);
    }
    else
    {
        lpGameModule->GetWorldModule().Update(lUpdateSet,
                                              lpUpdateInputStack, lpUpdateOutputStack,
                                              lpWorldInput, lpWorldOutput,
                                              lpWorldFrameAllocator);
    }

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

    // ⭐ SIGNATURE DEBT PAID 2026-08-16 (boot audit F-P6-16). DirectorModule::Prepare's 2nd
    // parameter IS the allocator list (X360 @0x823E74C0: `lwz r9,0x44(vtable);
    // r5 = GetAllocatorList(...)`), not an s32 replay token. It had been typed s32 and
    // called with a literal 0 while the real list sat right here, computed and thrown away
    // with a `(void)` cast. The whole chain -- DirectorModule::Prepare ->
    // MainDirector::Prepare -> ICEWrapper::Prepare -- is retyped, so the list now reaches
    // its consumer. That consumer is still a DirectorLinkStubs no-op, so nothing observable
    // changes today; what changes is that when the ICE wrapper IS bodied it receives the
    // console's argument instead of a zero nobody would have questioned.
    const bool lbPrepared =
        lpGameModule->GetDirectorModule().Prepare(lpDirectorOutput, lpAllocatorList);

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
// The GameData IO pair.
//
// ⭐ DESCRIPTION CORRECTED 2026-08-17 (boot audit F-P6-10 / F-P5-13). This used to say the
// X360 pair is "created by the update spine each frame". It is not, and the asm is explicit:
// they are PERSISTENT game-module members at gm+0x996F10/+0x996F14, LOADED each pass
// (`lwzx` @0x823F2358/5C) and locked across the whole spine -- only the sixteen scratch IO
// buffers around them are created and destroyed per frame. Our constructed-once pair is
// therefore the RIGHT lifetime, not a deviation from a per-frame one; what remains a
// deviation is only that it is a file-static rather than a member of the game module.
//
// The distinction matters because "created each frame" invites someone to "fix" this by
// carving the pair per pass, which would be a regression away from the console, not toward
// it. Published through BrnGameMainFlowController::GetScriptedLoadGameData* because
// BrnGameModule::GamePrepare @0x823EFBD0 brackets the SAME pair on the X360.
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

    // ---- the frame's SOUND IO trio (faithful-audio-engine phase C4) --------------------
    // X360 @0x823F23CC/0x823F23E0/0x823F23F8 -- creation positions 3-5 of the spine's
    // 16-buffer batch, ahead of the GameData lock bracket, destroyed in exact reverse
    // order at the tail (@0x823F2C88-0x823F2CA8). The RootInputBuffer comes off the
    // update-INPUT stack, the RootOutputBuffer and the pre-update buffer off the
    // update-OUTPUT stack, each destroyed through the stack that carved it (LIFO holds:
    // every inner carve this frame -- the world pair, the module's own scratch buffers --
    // nests inside the trio's lifetime). Full decode:
    // progress/scratch_dossiers/spine_sound_leg_0x823F22D8.md.
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    BrnSound::Module::Io::RootInputBuffer*           lpSoundRootInput       = 0;
    BrnSound::Module::Io::RootOutputBuffer*          lpSoundRootOutput      = 0;
    BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutput = 0;
    {
        const bool lbSoundIn = lpGameModule->GetUpdateInputBufferStack()
            ->CreateIOBuffer<BrnSound::Module::Io::RootInputBuffer>(&lpSoundRootInput, "Sound");
        const bool lbSoundOut = lpGameModule->GetUpdateOutputBufferStack()
            ->CreateIOBuffer<BrnSound::Module::Io::RootOutputBuffer>(&lpSoundRootOutput, "Sound");
        const bool lbSoundPre = lpGameModule->GetUpdateOutputBufferStack()
            ->CreateIOBuffer<BrnSound::Module::Io::RootPreUpdateOutputBuffer>(
                &lpSoundPreUpdateOutput, "SoundRootPreUpdateOutput");
        CGS_ASSERT(lbSoundIn && lbSoundOut && lbSoundPre,
                   "mpStack->CreateIOBuffer( &mpBuffer, lpcName )");  // CgsModuleIOHelper.h:52
        (void)lbSoundIn; (void)lbSoundOut; (void)lbSoundPre;
    }

    if (gBrnScriptedLoadStage != 8)
    {
        s_GameDataInput.LockForWrite();
        s_GameDataOutput.LockForRead();

        // byte_82FAE28E -- the scripted-load PARK flag. Raised by every pre-title OnEnter
        // (loading screen, marketing logos, title) and cleared by the title pre-accept's
        // ResumeLoadingWorld, so the whole world load streams BEHIND the title exactly as
        // it does on the console. The park skips only this stage ladder; the per-frame
        // world leg below still runs.
        if (!gBrnScriptedLoadPaused)
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
                // ⭐ REAL since 2026-09-02 (tyre-mark wave). X360:
                // `if (!LoadEffectsModule(this, gameDataIn, gameDataOut)) break;`
                // Was `[deferred: effects module placeholder]` -- and it was, literally: the
                // module was a 1-byte ODR stub in BrnGameModule.hpp until this change.
                LogScriptedStageOnce(2, "LoadEffectsModule -- real");
                if (!LoadEffectsModule(&s_GameDataInput, &s_GameDataOutput))
                    break;
                // fall through
            case 3:
                gBrnScriptedLoadStage = 3;
                // ⭐ REAL since 2026-08-11 (was `[deferred]`, which is why PROGRESSION.DAT never
                // loaded and OnPlayerCarChange fired "lpProgressionData != NULL"). X360:
                // `if (!LoadGameState2(this, gameDataIn)) break;`
                LogScriptedStageOnce(3, "LoadGameState2 -- real");
                if (!LoadGameState2(&s_GameDataInput, &s_GameDataOutput))
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
                // ⭐ THE CUE POSTS, 2026-08-17 (boot audit F-P6-7). X360 @0x823F2650-70:
                //     LockForWrite(rootIn);
                //     VariableEventQueue<13312,16>::AddEvent(rootIn->GetGameActionQueue(),
                //                                            &rec, /*id*/ 0x129, /*size*/ 1);
                //     UnlockForWrite(rootIn);   then stage := 8
                // The console posts into the FRAME's RootInputBuffer (r15, carved in the
                // spine's 16-buffer batch) -- since phase C4 threads that trio through this
                // function, the cue now uses it directly; the interim per-cue carve is
                // retired. The queue is the GameActionQueue: the AddEvent symbol names its
                // template arguments outright, and the member's attested span 0x3410 is
                // 13312+16, which is exactly VariableEventQueue<13312,16>.
                //
                // The 1-byte payload's stack slot is never written before the call in the
                // console body either -- the consumer keys on the id, not the content -- so a
                // zero byte is the faithful record, not a stand-in value. The cue now ALSO
                // reaches the engine the same frame: RootSoundModule::Update below consumes
                // this very buffer, exactly as the console spine does.
                LogScriptedStageOnce(6, "sound world-loaded cue (event 297) -> DONE");
                if (lpSoundRootInput != 0)
                {
                    lpSoundRootInput->LockForWrite();
                    u8 lu8CueRecord = 0;
                    lpSoundRootInput->GetGameActionQueue().AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lu8CueRecord), 0x129, 1);
                    lpSoundRootInput->UnlockForWrite();
                }
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

    // ---- the SOUND pre-update leg (X360 @0x823F2714 -- phase C4) ------------------------
    // Console position: after DoUpdate_InputPreWorld, before the network / game-state /
    // world drives. DoPreUpdate_Sound @0x823EE4D8 brackets the sound perfmon pair, runs
    // RootSoundModule::PreUpdate into the frame's pre-update buffer, appends its GuiOut
    // queue into the GUI INPUT buffer and walks BridgeSoundToTraining.
    // FLAG PC seam (buffer source only): the console spine carves its OWN "Gui" input
    // buffer in the same 16-buffer batch; the PC GUI module is driven from GameMain, so
    // the leg targets the game module's static per-sub-step GUI input buffer -- the same
    // object the GUI module will consume this frame. Skipped defensively if the static
    // pair is not up (the flow states always run inside the sub-step, so it is).
    if (lpSoundPreUpdateOutput != 0 && lpGameModule->GetGuiInputBuffer() != 0)
    {
        lpGameModule->DoPreUpdate_Sound(lpGameModule->GetUpdateOutputBufferStack(),
                                        lpSoundPreUpdateOutput,
                                        lpGameModule->GetGuiInputBuffer());
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
        // BridgeSoundToWorld rides inside DriveWorldUpdateFrame now (phase C4) -- the
        // pre-update buffer threads through UpdateWorldModule to the staging site.
        if (gBrnScriptedLoadStage > 5)
        {
            UpdateWorldModule(&s_GameDataInput, lpSoundPreUpdateOutput);
        }

        // The GameData pump used to run here. It has MOVED to the frame level -- the game
        // module's per-frame resource tick (BrnGameModule::ResourceUpdateThread, the console's
        // IThreadClass::ResourceUpdateThread @0x823BC9B8) -- so that EVERY flow state's staged
        // requests are serviced, not just this spine's. The X360's scripted-load spine
        // @0x823F22D8 does not pump the module either: the resource thread does, concurrently.
    }

    // ---- the post-world SOUND legs (X360 @0x823F2A14-0x823F2B74 -- phase C4) ------------
    // Console order after the GUI module vcall: BridgeGuiToSound (unconditional),
    // BridgeWorldToSound (stage > 5), RootSoundModule::Update (unconditional, no
    // caller-held locks -- the module takes its own), then the two sound->resource
    // forwards under W(gameDataIn)+R(rootOut). The PC GUI/world sources are the game
    // module's static per-sub-step buffers (GameMain drove the GUI earlier this sub-step;
    // DriveWorldUpdateFrame above published into the static world output buffer).
    if (lpSoundRootInput != 0 && lpSoundRootOutput != 0)
    {
        const BrnUpdateSet lUpdateSet = lpGameModule->ConstructUpdateSetFromFsm();

        // BridgeGuiToSound @0x823C0A58 (console @0x823F2A2C): R(guiOut) + W(rootIn).
        {
            CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput = lpGameModule->GetGuiOutputBuffer();
            if (lpGuiOutput != 0)
            {
                lpGuiOutput->LockForRead();
                lpSoundRootInput->LockForWrite();
                lpGameModule->BridgeGuiToSound(lpSoundRootInput, lpGuiOutput);
                lpSoundRootInput->UnlockForWrite();
                lpGuiOutput->UnlockForRead();
            }
        }

        // BridgeWorldToSound @0x823CD580 (console @0x823F2A6C, stage > 5 only):
        // R(worldOut) + W(rootIn); the update set feeds the bit-0x100 replay-source select.
        if (gBrnScriptedLoadStage > 5)
        {
            BrnWorldIO::UpdateOutputBuffer* lpWorldOutput = lpGameModule->GetWorldUpdateOutputBuffer();
            if (lpWorldOutput != 0)
            {
                lpWorldOutput->LockForRead();
                lpSoundRootInput->LockForWrite();
                lpGameModule->BridgeWorldToSound(lpSoundRootInput, lpWorldOutput, lUpdateSet);
                lpSoundRootInput->UnlockForWrite();
                lpWorldOutput->UnlockForRead();
            }
        }

        // RootSoundModule::Update @0x826FB238 (console @0x823F2AC8). The two f32 time
        // steps are the console's exact products (@0x823F2AB0-C4): gameTimer/simTimer
        // [+0x10]*[+0xC] == mfScaleCurrent * mfRate -- scaled delta seconds off the pair
        // UpdateTimers ticks each sub-step.
        {
            const CgsSystem::Timer& lrGameTimer = lpGameModule->GetGameTimer();
            const CgsSystem::Timer& lrSimTimer  = lpGameModule->GetSimTimer();
            lpGameModule->GetSoundModule().Update(
                lrGameTimer.GetScaleCurrent() * lrGameTimer.GetRate(),
                lrSimTimer.GetScaleCurrent() * lrSimTimer.GetRate(),
                lpGameModule->GetUpdateInputBufferStack(),
                lpGameModule->GetUpdateOutputBufferStack(),
                lpSoundRootInput,
                lpSoundRootOutput,
                lUpdateSet);
        }

        // The sound->resource forwards (console @0x823F2B40 + @0x823F2B54): the root
        // output's AttribSys event queue (<2048> @+0x1014) into the GameData input's
        // 32768 queue, then its resource RequestInterface (<4096> @+0x4) merged in --
        // the same pair LoadSoundModule forwards during the prepare stages, now per
        // frame. Bracket: W(gameDataIn) + R(rootOut), the DriveWorldUpdateFrame idiom.
        {
            s_GameDataInput.LockForWrite();
            lpSoundRootOutput->LockForRead();
            {
                const BrnSound::Module::Io::RootOutputBuffer* lpSoundRootOutputRead = lpSoundRootOutput;
                s_GameDataInput.GetAttribSysRequestInterface()->mRequestQueue.Append(
                    lpSoundRootOutputRead->GetAttribSysRequestInterface()->mRequestQueue);
                s_GameDataInput.GetRequestInterface()->mRequestQueue.Append(
                    lpSoundRootOutputRead->GetResourceRequestInterface()->mRequestQueue);
            }
            lpSoundRootOutput->UnlockForRead();
            s_GameDataInput.UnlockForWrite();
        }
    }

    // ---- the sound trio teardown (X360 @0x823F2C88/0x823F2C98/0x823F2CA8) ---------------
    // Exact reverse creation order, each through the stack that carved it.
    if (lpSoundPreUpdateOutput != 0)
        lpGameModule->GetUpdateOutputBufferStack()
            ->DestroyIOBuffer<BrnSound::Module::Io::RootPreUpdateOutputBuffer>(&lpSoundPreUpdateOutput);
    if (lpSoundRootOutput != 0)
        lpGameModule->GetUpdateOutputBufferStack()
            ->DestroyIOBuffer<BrnSound::Module::Io::RootOutputBuffer>(&lpSoundRootOutput);
    if (lpSoundRootInput != 0)
        lpGameModule->GetUpdateInputBufferStack()
            ->DestroyIOBuffer<BrnSound::Module::Io::RootInputBuffer>(&lpSoundRootInput);
    // stage 8 (DONE): the X360 runs the full module cascade (BrnGameModule::DoUpdate);
    // the PC host loop owns the module drive -- see DoUpdate's PC-platform note. The
    // sound legs above ARE that cascade's sound slice, staged here for the same reason
    // the world leg is (both sites move together).
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

    // (The per-frame GameData IO bracket this used to say was "not reconstructed yet, so the
    // buffers arrive null here" IS reconstructed -- InitialLoadingScreen::Update opens every
    // frame with the pair and threads it through each LoadXxxModule. Stale note corrected
    // 2026-08-17. Prepare's carve stages remain allocator-gated for their own reason, the
    // absent per-bank CreateAllocators, not for want of a buffer.)
    // GetAllocatorList() const now returns const AllocatorList* (the +4 member is a pointer, not an
    // embedded list -- see the OutputBuffer layout fix in BrnGameDataModuleIO.h), so no address-of here.
    const BrnResource::GameDataIO::AllocatorList* lpAllocatorList =
        lpGameDataOutputBuffer ? lpGameDataOutputBuffer->GetAllocatorList() : 0;

    bool lbPrepared = BrnGame::GetMainSoundModule()->Prepare(
        lpAllocatorList, lpUpdateInputStack, lpUpdateOutputStack,
        lpRootInputBuffer, lpRootOutputBuffer);

    if (!lbPrepared && lpGameDataInputBuffer && lpRootOutputBuffer)
    {
        // ⭐ THE FORWARDING ARM IS LIVE, 2026-08-17 (boot audit F-P6-17 / F-P5-10). X360
        // @0x823E7684/98, same source-read-lock bracket LoadDirectorModule above uses -- the
        // destination's write lock is held by the caller for the whole stage switch.
        //
        // This took two passes. It was gated on "the RootOutputBuffer request interfaces +
        // getters"; the getters were already here, so the first attempt wired against them
        // and failed to compile -- what was actually missing was one level down, the
        // RequestInterface<4096> / AttribSysRequestInterface<2048> template DEFINITIONS.
        // Those are now defined (BrnRootSoundModuleIo.h), so this can finally run. Until it
        // did, every resource request the sound module staged during its initial load was
        // dropped: nothing moved it into the GameData input where the pump could see it.
        lpRootOutputBuffer->LockForRead();
        {
            const BrnSound::Module::Io::RootOutputBuffer* lpRootOutputRead = lpRootOutputBuffer;
            lpGameDataInputBuffer->GetAttribSysRequestInterface()->mRequestQueue.Append(
                lpRootOutputRead->GetAttribSysRequestInterface()->mRequestQueue);
            lpGameDataInputBuffer->GetRequestInterface()->mRequestQueue.Append(
                lpRootOutputRead->GetResourceRequestInterface()->mRequestQueue);
        }
        lpRootOutputBuffer->UnlockForRead();
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

// @ 0x823AA950 - park the scripted world load, reset the load stages to START and raise
// the renderer's loading-screen signal (Option B bridge for the game-module global the
// X360 writes). Store order is the console's: park=0, assert stage==START, park=1,
// pending-GUI-FSM-stage=0, then the per-instance resets.
void MainGameFlowStateInitialLoadingScreen::OnEnter()
{
    // The pre-title park triplet (@0x823AA96C / assert :0xC3 / @0x823AA9B4).
    gBrnScriptedLoadPaused = false;
    CGS_ASSERT(gBrnScriptedLoadStage == E_LOADINGSTATESTAGE_START,
               "meLoadingStateStage == E_LOADINGSTATESTAGE_START");
    gBrnScriptedLoadPaused = true;

    // @0x823AA9C0 -- the pending-GUI-FSM-stage slot (gm+0x9A0644) is stamped 0 here; the
    // PC models that slot as RequestGuiFsmStage (BrnGameModule.hpp:483).
    BrnGame::GetMainGameModule()->RequestGuiFsmStage(0);

    meLoadingStateStage  = E_LOADINGSTATESTAGE_START;
    meLoadingScreenStage = E_LOADINGSTAGE_START;
    mbGuiPreloadDone     = false;
    gBrnInitialLoadingComplete = false;

    // @0x823AA9D8 -- mbSaveLoadState = 1. NB the console never EXECUTES this body (SendEvent
    // never targets state 0; Construct seeds it raw), so the initial-load update set stays
    // 0x80 there. The PC no longer calls OnEnter at boot either -- see the removed SetState
    // in BrnGameModule::Construct -- but the body is reproduced faithfully for re-entry.
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SetSaveLoadState(true);
    // The show command posts from Update each tick (the X360 writes the dispatch write
    // buffer's command every Update @0x823EF688; Update runs this same frame).

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: OnEnter - loading screen shown\n";
}

// @ 0x823AA9E8 - clear the loading-screen signal on exit (unless the GUI BF_LOADING state owns it,
// in which case BootLoading::OnLeave -> StopLoadingScreen drops it).
void MainGameFlowStateInitialLoadingScreen::OnLeave()
{
    // @0x823AA9FC -- mbSaveLoadState = 0. The console body is this ONE store and nothing
    // else; it never posts a loading-screen show or hide from OnEnter or OnLeave.
    //
    // ⭐ THE PC-ONLY HIDE IS GONE, 2026-08-17 (boot audit F-P4-9). This used to follow the
    // store with a `if (!gBrnGuiDrivesLoadingScreen) HideLoadingScreen()`, from the era when
    // nothing else could dismiss the screen. Something else does now: the GUI owns the
    // loading-screen visual through the real 19/20 command protocol, and since the GUI hoist
    // (79950026) it is always prepared by the time this state is left -- so the branch was
    // dead as well as unfaithful. The screen lifecycle belongs to the dispatch-input
    // publishes (GamePrepare's tail shows it, BridgeGuiToGame's 19/20 hide it), not to the
    // flow state's transitions.
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SetSaveLoadState(false);
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

    // ⭐ GATED 2026-08-17 (boot audit F-P5-7). The old note claimed the console "re-posts the
    // show command every update tick". It does not: the `stw 1 -> +0x9990` lives in the
    // stage<=2 RENDER leg only (@0x823EFB04), and once the stage machine is past 2 the state
    // stops posting it -- by then the GUI owns the loading-screen visual through the 19/20
    // command protocol, and GamePrepare's not-done tail publishes it as well (restored with
    // F-P2-4). Posting it unconditionally from here on every tick fought both of those for
    // the slot, on a one-shot word that each end-of-frame swap wipes.
    if (meLoadingScreenStage <= E_LOADINGSTAGE_GUIMODULE)
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
        // ⭐ THIS IS WHERE THE GUI IS BUILT, 2026-08-16 (boot audit F-P1-1/F-P5-2). X360
        // LoadGUIModule @0x823EF310:
        //
        //     prepared = (gui->vtable+0x58)(gui, ...);          // GuiModule::Prepare
        //     if (!prepared) {
        //         (gui->vtable+0x60)(gui, 0x40, updIn, updOut, ...);   // the GUI update leg
        //         LockForRead(both); BridgeGuiToResource(gm, ...); UnlockForRead(both);
        //         return 0;                                    // stay in this stage
        //     }
        //     LockForWrite(guiIn);
        //     AddEvent({CgsIDCompress("BrnBFPreFsm"), 0, flow 1}, 0x90, 0x18);
        //     AddEvent({CgsIDCompress("BrnOverlay"),  0, flow 2}, 0x90, 0x18);
        //     UnlockForWrite(guiIn); return 1;
        //
        // The two RunFsm posts were already here and already carry the console's payloads
        // (id 0x90, size 0x18, flow words 1 and 2). What was missing was everything they
        // are supposed to be GATED ON: this stage posted them immediately and advanced,
        // because the GUI had already been prepared back in BrnGameModule::Construct.
        //
        // [FLAG] The retry leg is not reproduced, and does not need to be YET: this build's
        // GuiModule::Prepare is the one-shot synchronous collapse of the console's 16-stage
        // resumable ladder (@0x82518D68), so it answers true on the first pump and there is
        // no not-done pass to service. That makes stage 2 one long frame with the loading
        // screen up rather than N frames of a live GUI -- the right ORDER, not yet the right
        // PACING. Adopting the real ladder (and with it this retry leg, the vtable+0x60
        // update-set-0x40 pump and the BridgeGuiToResource forward) is boot-audit F-P8a-1.
        {
            BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();

            if (!lpGameModule->GetGuiModule().IsPrepared())
            {
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint
                        << "InitialLoadingScreen: loading stage 2 (GUIModule) -- preparing\n";
                lpGameModule->GetGuiModule().Prepare();
                break;   // do NOT post or advance until it reports done
            }

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
        // NO LONGER DEFERRED (2026-09-02, tyre-mark wave). The note above already said what
        // this stage is -- `(*(*(gm+0x8BD300) + 0x40))(gm+0x8BD300, GetAllocatorList(gameDataOut))`
        // is ReplayModule::Prepare @0x82652768 -- and it now runs. It is not a replay feature:
        // Prepare acquires the module's LINEAR REGION, and StoreSerialisers @0x8264B600 is the
        // ONLY place in the engine that gives any BaseSerialiser its stream and static buffers.
        // With no region, BrnEffects::EffectsModule::Update @0x8229EC28 returns at its
        // `GetStaticLayout() == 0` guard before the wheel loop -- so every tyre mark, spark,
        // piece of debris and Lion effect was behind this one stage. Measured before the change
        // (BRN_SKID_PROBE): "[skid-gate] EffectsModule::Update RETURNED EARLY at:
        // mEffectsSerialiser.GetStaticLayout() == 0".
        if (!g_bLoggedGameDataPrepare)
        {
            g_bLoggedGameDataPrepare = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: stage 8 (Replays prepare -- real)\n";
        }
        {
            const BrnResource::GameDataIO::AllocatorList* lpReplayAllocators =
                lpGameDataOutput ? lpGameDataOutput->GetAllocatorList() : 0;
            BrnGame::GetMainGameModule()->GetReplayModule().Prepare(lpReplayAllocators);
        }
        // The GameDataModule pump keeps its place in the ladder: BrnGameModule::GamePrepare has
        // already driven it to completion, so it reports done immediately (see the note above).
        if (BrnGame::GetMainGameDataModule()->Prepare(0, 0))
            AdvanceLoadingStage(E_LOADINGSTAGE_DONE);
        break;
    case E_LOADINGSTAGE_DONE:
    default:
        // ⭐ CORRECTED 2026-08-16 (boot audit F-P5-4). The console's stage 9 vcalls
        // FinishLoading on EVERY frame, unconditionally; FinishLoading @0x823C6AC8 opens
        // with `lbz r11,0xC(r3); beqlr` and returns straight away until mbGuiPreloadDone is
        // set. So reaching stage 9 is NOT what ends the loading screen -- the GUI's own
        // done signal is. We used to fire it once, the instant the stage machine arrived,
        // which is why LOADING -> MARKETING never waited for anything.
        FinishLoading();

        // The GUI BF_LOADING state watches gBrnInitialLoadingComplete and dismisses the
        // loading screen itself (StopLoadingScreen) as it proceeds to BF_VIDEOS; only when
        // the GUI isn't driving do we drop the screen here.
        if (mbGuiPreloadDone && !gBrnInitialLoadingComplete)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: loading complete\n";
            gBrnInitialLoadingComplete = true;
            if (!gBrnGuiDrivesLoadingScreen)
                GetDispatchWriteBuffer()->HideLoadingScreen();
        }
        break;
    }

    // @0x823EFA48-6C -- the GUI-PRELOAD-DONE LATCH, run every frame right after
    // BridgeGuiToGame:
    //     if (!this->mbGuiPreloadDone && gm[0x9A0648]) this->mbGuiPreloadDone = 1;
    // gm+0x9A0648 is the byte BridgeGuiToGame @0x823CB758 sets from GUI command 70 (the
    // HUD flow's "phase complete"); on PC that is BrnGameModule::mbGuiPhaseComplete
    // (+10094152), set by the same command in the same bridge. The member was WRITE-ONLY
    // before this -- reset in Construct and never read -- so FinishLoading had nothing to
    // gate on. It is a sticky latch: once set it stays set for the life of the state.
    // The PC's BridgeGuiToGame runs later in the same sub-step (from GameMain rather than
    // from this body), so the value read here is the previous sub-step's -- one sub-step of
    // latency on a signal that is latched anyway.
    if (!mbGuiPreloadDone)
    {
        BrnGame::BrnGameModule* lpGameModuleForLatch = BrnGame::GetMainGameModule();
        if (lpGameModuleForLatch != 0 && lpGameModuleForLatch->IsGuiPhaseComplete())
        {
            mbGuiPreloadDone = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "InitialLoadingScreen: GUI preload done (command 70) -- loading may finish\n";
        }
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

// @ 0x823C6AC8 - the load is complete: advance the flow. Store for store:
//     lbz  r11, 0xC(r3); cmplwi r11, 0; beqlr        <- the mbGuiPreloadDone gate
//     gm[0x9A119C] = 5
//     SendEvent(gm + 0x9A0664, 2 /* E_MGE_STATEEND */)   (a tail call)
//
// ⭐ THE GATE IS RESTORED, 2026-08-16 (boot audit F-P5-4). The caller vcalls this every
// frame from stage 9; this early return is what makes LOADING -> MARKETING wait for the
// GUI's own done signal instead of firing the instant the stage machine arrives.
//
// [FLAG] the gm+0x9A119C := 5 stamp is not reproduced: that word is not mapped in this
// incrementally-populated game-module layout, and nothing on the PC reads it yet. It is a
// loading-state publication for the dispatch side; restore it with the rest of gm's
// 0x9A11xx block.
void MainGameFlowStateInitialLoadingScreen::FinishLoading()
{
    if (!mbGuiPreloadDone)
        return;

    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SendEvent(BrnGameMainFlowController::E_MGE_STATEEND);
}

// --- MainGameFlowStateStartScreen (the legal/title screen phase) -------------------------
MainGameFlowStateStartScreen::MainGameFlowStateStartScreen() : meLoadingStage(E_STARTSCREEN_START) {}

// @ 0x823AAB18 -- park the scripted load (the title holds it at START until the player's
// pre-accept) and request the BF_LEGAL GUI FSM stage (BrnLegalFsm; the X360 writes the
// pending-stage byte = 2).
void MainGameFlowStateStartScreen::OnEnter()
{
    // The pre-title park triplet (@0x823AAB30 / assert / @0x823AAB7C). This is the raise
    // that StartScreen::Update's pre-accept latch later clears via ResumeLoadingWorld.
    gBrnScriptedLoadPaused = false;
    CGS_ASSERT(gBrnScriptedLoadStage == E_LOADINGSTATESTAGE_START,
               "meLoadingStateStage == E_LOADINGSTATESTAGE_START");
    gBrnScriptedLoadPaused = true;

    BrnGame::GetMainGameModule()->RequestGuiFsmStage(2);

    // @0x823AAB94 -- mbVideoState = 1 (the title is still a video-state phase; OnLeave
    // writes none on the console).
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SetVideoState(true);
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
        // world load during the accept dwell. UNCONDITIONAL, as on the console: OnEnter
        // above raised the park, so the assert inside holds by construction.
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "StartScreen: pre-accept (71) -> resume world load\n";
        ResumeLoadingWorld();

        // @0x823F2DD0 -- the console RETURNS here: the resume frame never also runs the
        // phase-complete advance below.
        return;
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

// @ 0x823AAA08 -- park the scripted load (the logos play with the world load held at
// START) and request the BF_VIDEOS GUI FSM stage (BrnVideoFsm; pending-stage byte = 1).
void MainGameFlowStateMarketingScreens::OnEnter()
{
    // The pre-title park triplet (@0x823AAA20 / assert / @0x823AAA6C).
    gBrnScriptedLoadPaused = false;
    CGS_ASSERT(gBrnScriptedLoadStage == E_LOADINGSTATESTAGE_START,
               "meLoadingStateStage == E_LOADINGSTATESTAGE_START");
    gBrnScriptedLoadPaused = true;

    BrnGame::GetMainGameModule()->RequestGuiFsmStage(1);

    // @0x823AAA80 -- mbVideoState = 1: the logos run with the world on the boot-video arm.
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SetVideoState(true);
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "MarketingScreens: OnEnter -> GUI FSM stage 1 (BrnVideoFsm)\n";
}

void MainGameFlowStateMarketingScreens::OnLeave() {
    // @0x823AABC4 -- mbVideoState = 0.
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SetVideoState(false);
}

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
// The scripted-load PARK flag (X360 byte_82FAE28E) lives here for historical reasons -- it
// is a shared global, not this state's private flag: every pre-title OnEnter raises it and
// ResumeLoadingWorld clears it (see the note at the top of this file).
bool gBrnScriptedLoadPaused = false;

MainGameFlowStateCheckDiskSpace::MainGameFlowStateCheckDiskSpace() {}

// @ 0x823AAA98 - the same pre-title park triplet the other states run, then stamp the
// pending-GUI-FSM-stage slot to 4. (No disk/storage query is issued here -- the X360 body
// is just these flag writes + the START-stage assert + the stage stamp.) NB neither build
// ever ENTERS this state: SendEvent's transition set never targets it (boot audit P4).
void MainGameFlowStateCheckDiskSpace::OnEnter()
{
    // The park triplet (@0x823AAAB0 / assert / @0x823AAAF4).
    // ⭐ CORRECTED 2026-08-14 (boot audit F-P4-11): the assert tested the per-instance
    // meLoadingStateStage, which the ctor freezes at START and nothing ever advances --
    // a vacuous check. The console reads the GLOBAL stage (dword_82FAE4B0), exactly as
    // ResumeLoadingWorld does.
    gBrnScriptedLoadPaused = false;
    CGS_ASSERT(gBrnScriptedLoadStage == E_LOADINGSTATESTAGE_START,
               "meLoadingStateStage == E_LOADINGSTATESTAGE_START");
    gBrnScriptedLoadPaused = true;

    // @0x823AAB00 -- the pending-GUI-FSM-stage slot (gm+0x9A0644) is stamped 4 here; the
    // PC models that slot as RequestGuiFsmStage (the same field Marketing/Start/MemoryCard
    // stamp with 1/2/4).
    BrnGame::GetMainGameModule()->RequestGuiFsmStage(4);

    // @0x823AAC84 / @0x823AAC8C -- mbSaveLoadState = 1, mbVideoState = 1 (the profile phase
    // is the sim-paused save-load window: update set 0xE1).
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
    {
        BrnGameMainFlowController::gpMainGameFlowController->SetSaveLoadState(true);
        BrnGameMainFlowController::gpMainGameFlowController->SetVideoState(true);
    }
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
    // @0x823AAC78 -- CLEAR ONLY (no raise): by the profile phase the world load is meant
    // to be running, so this state never re-parks it.
    gBrnScriptedLoadPaused = false;

    BrnGame::GetMainGameModule()->RequestGuiFsmStage(4);
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "MemoryCard: OnEnter -> GUI FSM stage 4 (BrnBFProFsm)\n";
}
// @ 0x823AACC8 -- clear the save-load + video bytes on the way out.
void MainGameFlowStateMemoryCard::OnLeave()
{
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
    {
        BrnGameMainFlowController::gpMainGameFlowController->SetSaveLoadState(false);   // @0x823AACE4
        BrnGameMainFlowController::gpMainGameFlowController->SetVideoState(false);      // @0x823AACEC
    }
}

// @ 0x823F2F98 -- gated on the world-load stage (X360 dword_82FAE4B0 == 8); advance when
// the GUI posts phase-complete (the profile task resolved).
// ⭐ CORRECTED 2026-08-14 (boot audit F-P4-4 / F-P6-21): the gate was keyed to
// gBrnInitialLoadingComplete -- the INITIAL-loading-done signal, which the loading screen
// raises long before the scripted world load starts. With the park chain restored that
// stand-in is true while the load still sits at stage 0, so the profile phase would
// advance with the world unloaded. The console condition is the scripted stage itself.
void MainGameFlowStateMemoryCard::Update()
{
    // X360 0x823F2F98 opens with the shared scripted-load spine.
    LoadingScriptedState::Update();

    // @0x823F2FB0 -- the world-loaded gate, tested FIRST.
    if (gBrnScriptedLoadStage != 8)
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
    // @0x823AABF0 -- CLEAR ONLY (see MemoryCard::OnEnter).
    gBrnScriptedLoadPaused = false;

    BrnGame::GetMainGameModule()->RequestGuiFsmStage(3);
    mbIsCollisionWorldPrepared = false;

    // @0x823AAC04 -- mbVideoState = 1 (the post-title intro is a video phase).
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SetVideoState(true);
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "CompleteLoading: OnEnter -> GUI FSM stage 3 (BrnCmpLdFsm)\n";
}
// @ 0x823AAC18 -- clear the video byte. (The console also clears an unmapped byte at
// gm+0x70A65D here; its consumer is not recovered, so it stays unmodelled -- boot audit
// F-P4-5 carries it as an unmapped-store TODO rather than a fabricated home.)
void MainGameFlowStateCompleteLoading::OnLeave()
{
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SetVideoState(false);      // @0x823AAC34
}

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

    // ⭐ 2026-08-16 (boot audit F-P4-6). The return-to-front-end poll at gm+0x9A0626 is read
    // in BOTH CompleteLoading::Update and InGame::Update on the console -- in states 0-5 it
    // is consumed AND DISCARDED. Only InGame carried it here, so a front-end request raised
    // during COMPLETE_LOADING (the console swallows it) survived on PC and would fire the
    // instant the flow reached IN_GAME. One clear-and-ignore restores parity.
    if (BrnGameMainFlowController::gBrnReturnToFrontEndRequested)
        BrnGameMainFlowController::gBrnReturnToFrontEndRequested = false;

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
        // ⭐ THE ANTI-WEDGE BOUND IS GONE, 2026-08-16 (boot audit F-P6-9 / F-P4-10). A PC-only
        // 1800-frame budget used to release the flow if the collision load had not finished,
        // "so a broken collision load costs the collision, not the session". There is no
        // console analogue -- @0x823F2E08 holds here indefinitely -- and its own note said to
        // delete it once the collision stream was proven. It is: the boot log shows stage 7
        // loading worldcol.bin (21,793,024 bytes, 792 resources into pool 2) and the player
        // car placed on track, with the bound never firing. A timeout whose only remaining
        // effect would be to hide a real streaming failure is worse than the hang it prevents.
        if (gBrnScriptedLoadStage == 8)
        {
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
        // (no else: while the stage is 7 the state HOLDS, exactly as @0x823F2E34-7C does,
        //  and LoadingScriptedState::Update at the top of this function drives the load.)
    }
}
// ⭐ 2026-08-16 (boot audit F-P6-11). CompleteLoading::Render IS DoDispatch on the console --
// the same render-dispatch spine MainGameFlowStateInGame::Render drives. Ours was empty, so
// for the whole compload window (which is now a real hold on stage 7 while the collision
// streams, not a couple of frames) nothing dispatched a render frame from the flow state and
// the screen was carried entirely by whatever the previous state left behind.
void MainGameFlowStateCompleteLoading::Render()
{
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    if (lpGameModule != 0)
        lpGameModule->DoDispatch();
}

MainGameFlowStateInGame::MainGameFlowStateInGame() {}
// OnEnter/OnLeave/Update/Render are homed in BrnGameMainFlowInGameState.cpp (the DWARF
// home for the in-game state's per-frame surface); only the ctor stays with this group.

// ---------------------------------------------------------------------------
// The in-game per-frame world tick, called by MainGameFlowStateInGame::Update.
// Requests go into the same GameData input the scripted spine uses; the frame-level
// resource tick (BrnGameModule::ResourceUpdateThread) drains it for every flow state,
// so no extra pump is needed here.
// ---------------------------------------------------------------------------
void DriveInGameWorldUpdate(BrnSound::Module::Io::RootPreUpdateOutputBuffer* lpSoundPreUpdateOutput)
{
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    if (lpGameModule == 0)
        return;

    // Console: BrnGameModule::DoUpdate @0x823F0AF8 (pseudocode line 302) and
    // ConstructUpdateSet @0x823DCB40 both derive the world's update set from the flow state
    // machine per frame; ConstructUpdateSetFromFsm @0x823BD420 is one call away from each.
    // This used to pass the frozen KU_INGAME_UPDATE_SET (0x88) -- see the deleted constant's
    // obituary in the header for why that made the sim-pause bit unreachable in game.
    //
    // ORDERING (checked, no extra plumbing needed): MainGameFlowInGameState.cpp:82-89 runs
    // DoUpdate() -- which runs the game-state pre-world leg and therefore CheckGameActions,
    // the writer of mbSimPaused -- BEFORE DriveInGameWorldUpdate() in the same frame. So the
    // set built here is already this frame's value.
    const BrnUpdateSet lUpdateSet = lpGameModule->ConstructUpdateSetFromFsm();

    // Change-only dev trace, mirroring UpdateWorldModule's at :327-338. Unpaused reads 0x88;
    // the pause adds bit 0x1 and it reads 0x89.
    {
        static u32 s_uLastInGameUpdateSet = 0xFFFFFFFFu;
        if ((u32)lUpdateSet != s_uLastInGameUpdateSet)
        {
            s_uLastInGameUpdateSet = (u32)lUpdateSet;
            if (CgsDev::Log::gpDebugPrint != 0)
                *CgsDev::Log::gpDebugPrint << "[flow] in-game update set -> "
                                           << CgsDev::E_PRINTMODE_HEXONCE
                                           << (u32)lUpdateSet << "\n";
        }
    }

    DriveWorldUpdateFrame(&s_GameDataInput, lUpdateSet, lpSoundPreUpdateOutput);
}
