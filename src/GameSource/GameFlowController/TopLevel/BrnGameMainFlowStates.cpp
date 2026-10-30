#include "GameShared/GameClasses/Gui/CgsGuiShared.h"
#include "GameSource/Graphics/BrnRendererModuleIO.h"
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"

#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h"   // embedded controller and SendEvent
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
#include "GameSource/Game/GameBridgeGameStateToX.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"  // GameStateModuleIO::OutputBuffer (its lock bracket)
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"  // CgsGuiModuleIO::Input/OutputBuffer (the C4 sound-leg GUI endpoints)
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"   // CgsModule::LockBuffersForIO (the partial spine's network -> GUI bracket)
#include "GameSource/Network/BrnNetworkModule.h"              // BrnNetwork::BrnNetworkModule::ProcessBeforeSimulation (the partial spine)
#include "GameSource/Network/BrnNetworkModuleIO.h"            // BrnNetworkModuleIO::PreSimulationInputBuffer / OutputBuffer

#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"

// The loading flow uses the actual GameData member and its persistent resource IO pair.
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
{
}
void LoadingScriptedState::OnEnter() {}
void LoadingScriptedState::OnLeave() {}
void LoadingScriptedState::Render()
{
    // ARTIST 823E7998 is shared by all six loading-state Render slots.
    if (gBrnScriptedLoadStage == 8)
        BrnGame::GetMainGameModule()->DoDispatch();
}

// ARTIST 823CE098: original supplied-IO GUI command producer.
void LoadingScriptedState::RenderGUI(
    CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
    CgsGui::CgsGuiModuleIO::OutputBuffer* /*lpGuiOutput*/,
    CgsGui::ModelIO::OutputBuffer* /*lpGuiModelOutput*/,
    CgsGui::ViewIO::InputBuffer* lpGuiViewInput,
    bool lbSkipGui)
{
    BrnGame::BrnGameModule* const lpGameModule = BrnGame::GetMainGameModule();
    CgsModule::IOBufferStack* const lpInputStack = lpGameModule->GetUpdateInputBufferStack();
    CgsModule::IOBufferStack* const lpOutputStack = lpGameModule->GetUpdateOutputBufferStack();
    RendererIO::InputBuffer* lpRendererInput = 0;
    RendererIO::OutputBuffer* lpRendererOutput = 0;
    lpInputStack->CreateIOBuffer(&lpRendererInput, 0);
    lpOutputStack->CreateIOBuffer(&lpRendererOutput, 0);

    BrnGame::DispatchThreadInputBuffer* const lpDispatchInput =
        lpGameModule->GetDispatchThreadInputBufferManager().GetWriteBuffer();
    lpDispatchInput->LockForWrite();
    lpDispatchInput->SetIsDiskError(lpGameModule->GetIsDiskError());
    lpDispatchInput->UnlockForWrite();
    lpGameModule->GetRenderModule().Update(lpInputStack, lpOutputStack,
                                          lpRendererInput, lpRendererOutput);
    if (!lbSkipGui)
    {
        CgsModule::LockBuffersForIO(lpGuiInput, lpRendererOutput);
        lpGameModule->BridgeRendererToGui(lpGuiInput, lpRendererOutput);
        CgsModule::UnlockBuffersForIO(lpGuiInput, lpRendererOutput);
        lpGuiInput->LockForWrite();
        CgsGraphics::Camera lGuiCamera = CgsGui::GetGuiCamera();
        lpGuiInput->SetCamera(lGuiCamera);
        lpGuiInput->UnlockForWrite();
        lpRendererOutput->LockForRead();
        lpGameModule->GetGuiModule().Render(lpGuiViewInput, lpGuiInput, lpRendererOutput);
        lpRendererOutput->UnlockForRead();
    }

    lpInputStack->DestroyIOBuffer(&lpRendererInput);
    lpOutputStack->DestroyIOBuffer(&lpRendererOutput);
}
// @ 0x823C6AB0 -- the base FinishLoading is a tail-call into the flow controller's
// SendEvent(STATEEND); it is vtable-only (no static callers on either side). The PC body
// was empty, so a dispatch through it silently did nothing (boot audit F-P6-19).
void LoadingScriptedState::FinishLoading()
{
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SendEvent(
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

    if (!lbPrepared && lpGameDataInputBuffer != 0)
    {
        lpEffectsOutput->LockForRead();
        // The console reads BOTH interfaces through the CONST accessors under the read lock
        // (the pair guarded by "Not locked for reading"). Through the non-const pointer, C++
        // overload resolution picks the write-lock accessors instead, and their guard ("Not
        // locked for writing") fires on the first prepare frame of every boot -- the assert
        // screen was the whole game from then on. Same shape as LoadWorldModule below.
        const BrnEffects::EffectsIO::OutputBuffer* lpEffectsOutputRead = lpEffectsOutput;
        lpGameDataInputBuffer->GetAttribSysRequestInterface()->mRequestQueue.Append(
            lpEffectsOutputRead->GetVaultRequestInterface()->mRequestQueue);
        lpGameDataInputBuffer->AppendRequestInterface<4096>(
            *lpEffectsOutputRead->GetResourceRequestInterface());
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
        // The console's own position: the collision prepare has finished, so the world's
        // surfacelist collection exists and the effects module can point its instance at it
        // and push every surface's skid-mark colour pair into the trail system.
        //
        // ⭐ THE 0xC0000005 THAT TOOK THIS BACK OUT ON 2026-09-02 (runs 17/18,
        //   Attrib::Instance::GetClass + 0x11 <- PostWorldPreparePrepare + 0x22F/+0x298)
        // was NOT an AttribSys instance-binding problem and NOT an asset gap. It was a
        // one-argument transcription defect in Attrib::Gen::visualfxsurface's constructor:
        // the console's @0x8227FC00 takes an Attrib::RefSpec (its Instance ctor is
        // sub_8280A248, which opens with `bl Attrib__RefSpec__GetCollection`), and this tree
        // declared it taking an Attrib::Collection*. Every caller passes the surface layout
        // +0x10 -- an embedded 24-byte RefSpec -- so reading it as a Collection fetched
        // mpClass from +0x20, eight bytes past the record, and dereferenced the garbage.
        // `GetClass + 0x11` is precisely the `mov eax,[rax]` after `mov rax,[rax+0x20]`.
        // Fixed in GameSource/AttribSys/Generated/classes/visualfxsurface.h + the four call
        // sites (2026-09-03). The [skid-bind] line in PostWorldPreparePrepare prints both
        // sides of the resolve on the first pass.
        lpGameModule->GetEffectsModule().PostWorldPreparePrepare();
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
// ARTIST823EF4D8: a fresh GameState output on each second-prepare attempt.
bool LoadingScriptedState::LoadGameState2(
        BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
        const BrnResource::GameDataIO::OutputBuffer* /*lpGameDataOutput*/)
{
    BrnGame::BrnGameModule* const lpGameModule = BrnGame::GetMainGameModule();
    CgsModule::IOBufferStack* const lpOutputStack = lpGameModule->GetUpdateOutputBufferStack();
    BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput = 0;
    lpOutputStack->CreateIOBuffer(&lpGameStateOutput, "GameState");
    if (lpGameModule->GetGameStateModule().Prepare2(lpGameStateOutput))
    {
        lpOutputStack->DestroyIOBuffer(&lpGameStateOutput);
        return true;
    }
    lpGameStateOutput->LockForRead();
    const BrnGameState::GameStateModuleIO::OutputBuffer* const lpRead = lpGameStateOutput;
    lpGameDataInput->AppendRequestInterface<3072>(*lpRead->GetResourceRequestInterface());
    lpGameStateOutput->UnlockForRead();
    lpOutputStack->DestroyIOBuffer(&lpGameStateOutput);
    return false;
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

    // The console reads the allocator list out of the GameData OUTPUT buffer (read-locked by
    // the caller) and passes it as DirectorModule::Prepare's second argument (not an s32
    // replay token), which MainDirector::Prepare forwards straight to ICEWrapper::Prepare
    // (BrnDirectorICEWrapperPrepare.cpp).
    const BrnResource::GameDataIO::AllocatorList* lpAllocatorList =
        lpGameDataOutputBuffer ? lpGameDataOutputBuffer->GetAllocatorList() : 0;

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

// ARTIST823F22D8: the partial loading update owns sixteen temporary IO buffers.
// The persistent GameData pair is allocated by BrnGameModule::Prepare on the
// resource stacks (gm+996F10/14). Loading and GamePrepare share that pair.
namespace BrnGameMainFlowController
{
    BrnResource::GameDataIO::InputBuffer* GetScriptedLoadGameDataInput()
    {
        return BrnGame::GetMainGameModule()->GetGameDataModuleInputBuffer();
    }

    BrnResource::GameDataIO::OutputBuffer* GetScriptedLoadGameDataOutput()
    {
        return BrnGame::GetMainGameModule()->GetGameDataModuleOutputBuffer();
    }
}

void LoadingScriptedState::Update()
{
    BrnGame::BrnGameModule* const lpGameModule = BrnGame::GetMainGameModule();
    BrnResource::GameDataIO::InputBuffer* const lpGameDataInput =
        lpGameModule->GetGameDataModuleInputBuffer();
    BrnResource::GameDataIO::OutputBuffer* const lpGameDataOutput =
        lpGameModule->GetGameDataModuleOutputBuffer();
    // Capture the route before the stage ladder. Reaching8 during a partial call
    // does not run normal DoUpdate or tick its timers again in the same sub-step.
    if (gBrnScriptedLoadStage != 8)
    // ARTIST823F22D8: all sixteen temporary IO objects belong to this partial update.
    {
        CgsModule::IOBufferStack* const lpInputStack = lpGameModule->GetUpdateInputBufferStack();
        CgsModule::IOBufferStack* const lpOutputStack = lpGameModule->GetUpdateOutputBufferStack();
        CgsInput::InputIO::OutputBuffer* lInputOutput = 0;
        BrnGameState::GameStateModuleIO::OutputBuffer* lControllerGameStateOutput = 0;
        BrnSound::Module::Io::RootInputBuffer* lSoundInput = 0;
        BrnSound::Module::Io::RootOutputBuffer* lSoundOutput = 0;
        BrnSound::Module::Io::RootPreUpdateOutputBuffer* lSoundPreOutput = 0;
        BrnWorldIO::UpdateInputBuffer* lWorldInput = 0;
        BrnWorldIO::UpdateOutputBuffer* lpWorldOutput = 0;
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput = 0;
        CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput = 0;
        CgsGui::ViewIO::InputBuffer* lpViewInput = 0;
        CgsGui::ModelIO::OutputBuffer* lpModelOutput = 0;
        BrnGameState::GameStateModuleIO::PreWorldInputBuffer* lGameStatePreInput = 0;
        BrnGameState::GameStateModuleIO::PostWorldInputBuffer* lGameStatePostInput = 0;
        BrnGameState::GameStateModuleIO::OutputBuffer* lGameStateOutput = 0;
        BrnNetwork::BrnNetworkModuleIO::PreSimulationInputBuffer* lNetworkInput = 0;
        BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lNetworkOutput = 0;

        lpOutputStack->CreateIOBuffer(&lInputOutput, "Input");
        lpOutputStack->CreateIOBuffer(&lControllerGameStateOutput, "GameState");
        lpInputStack->CreateIOBuffer(&lSoundInput, "Sound");
        lpOutputStack->CreateIOBuffer(&lSoundOutput, "Sound");
        lpOutputStack->CreateIOBuffer(&lSoundPreOutput, "SoundRootPreUpdateOutput");
        lpInputStack->CreateIOBuffer(&lWorldInput, "World");
        lpOutputStack->CreateIOBuffer(&lpWorldOutput, "World");
        lpInputStack->CreateIOBuffer(&lpGuiInput, "GUI");
        lpOutputStack->CreateIOBuffer(&lpGuiOutput, "GUI");
        lpInputStack->CreateIOBuffer(&lpViewInput, "GUIView");
        lpOutputStack->CreateIOBuffer(&lpModelOutput, "GUIModel");
        lpInputStack->CreateIOBuffer(&lGameStatePreInput, "GameStatePreWorld");
        lpInputStack->CreateIOBuffer(&lGameStatePostInput, "GameStatePostWorld");
        lpOutputStack->CreateIOBuffer(&lGameStateOutput, "GameState");
        lpOutputStack->CreateIOBuffer(&lNetworkInput, "NetworkPreSim");
        lpOutputStack->CreateIOBuffer(&lNetworkOutput, "Network");

        lpGameDataInput->LockForWrite();
        lpGameDataOutput->LockForRead();

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
                if (!LoadSoundModuleAgain(lpGameDataInput, lpGameDataOutput))
                    break;
                // fall through
            case 2:
                gBrnScriptedLoadStage = 2;
                // ⭐ REAL since 2026-09-02 (tyre-mark wave). X360:
                // `if (!LoadEffectsModule(this, gameDataIn, gameDataOut)) break;`
                // Was `[deferred: effects module placeholder]` -- and it was, literally: the
                // module was a 1-byte ODR stub in BrnGameModule.hpp until this change.
                LogScriptedStageOnce(2, "LoadEffectsModule -- real");
                if (!LoadEffectsModule(lpGameDataInput, lpGameDataOutput))
                    break;
                // fall through
            case 3:
                gBrnScriptedLoadStage = 3;
                // ⭐ REAL since 2026-08-11 (was `[deferred]`, which is why PROGRESSION.DAT never
                // loaded and OnPlayerCarChange fired "lpProgressionData != NULL"). X360:
                // `if (!LoadGameState2(this, gameDataIn)) break;`
                LogScriptedStageOnce(3, "LoadGameState2 -- real");
                if (!LoadGameState2(lpGameDataInput, lpGameDataOutput))
                    break;
                // fall through
            case 4:
                gBrnScriptedLoadStage = 4;
                // ARTIST823F25E0..261C: the existing GameData write lock spans
                // this real GUI second-prepare call and its retry branch.
                if (!lpGameModule->GetGuiModule().Prepare2(lpGameDataInput))
                    break;
                // fall through
            case 5:
                gBrnScriptedLoadStage = 5;
                LogScriptedStageOnce(5, "LoadWorldModule -- real");
                if (!LoadWorldModule(lpGameDataInput, lpGameDataOutput))
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
                if (lSoundInput != 0)
                {
                    lSoundInput->LockForWrite();
                    u8 lu8CueRecord = 0;
                    lSoundInput->GetGameActionQueue().AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lu8CueRecord), 0x129, 1);
                    lSoundInput->UnlockForWrite();
                }
                gBrnScriptedLoadStage = 8;
                break;
            case 7:
                gBrnScriptedLoadStage = 7;
                // X360: LoadWorldCollision @0x823E73E0 -- WorldModule::PrepareWorldCollision,
                // then EffectsModule::PostWorldPreparePrepare on completion (that last hop
                // is still deferred; see LoadWorldCollision's own note).
                LogScriptedStageOnce(7, "LoadWorldCollision -- real");
                if (!LoadWorldCollision(lpGameDataInput, lpGameDataOutput))
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

        const BrnUpdateSet lUpdateSet = lpGameModule->ConstructUpdateSetFromFsm();
        const u32 luStartPressedPort = lpGameModule->DoUpdate_InputPreWorld(
            lpInputStack, lpOutputStack, lInputOutput);
        lpGameModule->DoPreUpdate_Sound(lpOutputStack, lSoundPreOutput, lpGuiInput);
        lpGameModule->GetNetworkModule().ProcessBeforeSimulation(
            lpInputStack, lpOutputStack, lNetworkInput, lNetworkOutput, lUpdateSet);
        {
            const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* const lpNetworkRead = lNetworkOutput;
            CgsModule::LockBuffersForIO(lpGuiInput, lpNetworkRead);
            lpGameModule->BridgeNetworkToGui(lpGuiInput, lpNetworkRead);
            CgsModule::UnlockBuffersForIO(lpGuiInput, lpNetworkRead);
        }
        {
            const CgsGui::CgsGuiModuleIO::OutputBuffer* const lpGuiRead = lpGuiOutput;
            BrnGameState::GameStateModuleIO::PostWorldInputBuffer* const lpGameStatePost = lGameStatePostInput;
            CgsModule::LockBuffersForIO(lpGameStatePost, lpGuiRead);
            lpGameModule->BridgeGuiToGameState(lpGameStatePost, lpGuiRead);
            CgsModule::UnlockBuffersForIO(lpGameStatePost, lpGuiRead);
        }

        // Native input is fresh and unstaged: zero timers/controller/network, not a
        // retained main-cascade snapshot. The separate PostInput is never consumed
        // through PostWorldUpdate on this branch and its requests die with this call.
        lpGameModule->GetGameStateModule().PreWorldUpdate(
            lpInputStack, lpOutputStack, lGameStatePreInput, lGameStateOutput, lUpdateSet);
        {
            const BrnGameState::GameStateModuleIO::OutputBuffer* const lpGameStateRead = lGameStateOutput;
            CgsModule::LockBuffersForIO(lpGuiInput, lpGameStateRead);
            lpGameModule->BridgeGameStateToGui(lpGuiInput, lpGameStateRead);
            CgsModule::UnlockBuffersForIO(lpGuiInput, lpGameStateRead);
        }

        if (gBrnScriptedLoadStage > 5)
        {
            const BrnSound::Module::Io::RootPreUpdateOutputBuffer* const lpSoundRead = lSoundPreOutput;
            BrnWorldIO::UpdateInputBuffer* const lpWorldWrite = lWorldInput;
            CgsModule::LockBuffersForIO(lpWorldWrite, lpSoundRead);
            lpGameModule->BridgeSoundToWorld(lWorldInput, lSoundPreOutput);
            CgsModule::UnlockBuffersForIO(lpWorldWrite, lpSoundRead);
            if ((lUpdateSet & 0x20) == 0 && (lUpdateSet & 0x40) == 0)
            {
                CgsMemory::LinearMalloc* const lpFrameAllocator =
                    lpGameModule->GetReusableLoadingScreenAllocator();
                lpFrameAllocator->FreeAll();
                lpGameModule->GetWorldModule().Update(lUpdateSet,
                    lpInputStack, lpOutputStack, lWorldInput, lpWorldOutput, lpFrameAllocator);
            }
        }

        if ((lUpdateSet & 0x20) != 0)
        {
            lpGameDataInput->UnlockForWrite();
            lpGameModule->DoUpdate_GuiPreWorld(lpInputStack, lpOutputStack,
                lpGuiOutput, lpModelOutput, lpGameDataInput);
            lpGameDataInput->LockForWrite();
        }

        // This is deliberately the untouched first GameState output, not the
        // PreWorld producer output that the GUI wrapper has just read.
        lpGameModule->DoUpdate_InputPostWorld(lpInputStack, lpOutputStack,
            lControllerGameStateOutput, lpWorldOutput, luStartPressedPort);
        lpGameModule->GetGameTimer().Update();
        const CgsSystem::Timer& lrGameTimer = lpGameModule->GetGameTimer();
        CgsGui::GuiEventTimeInfo lTimeInfo;
        lTimeInfo.Set(lrGameTimer.GetScaleCurrent() * lrGameTimer.GetRate(),
            static_cast<f32>(lrGameTimer.GetAccumTicks()) + lrGameTimer.GetAccumulator());
        lpGuiInput->LockForWrite();
        BrnGame::PushGuiEvent(lTimeInfo, lpGuiInput);
        lpGuiInput->UnlockForWrite();

        lpGuiOutput->Clear();
        lpModelOutput->Clear();
        lpGuiInput->LockForWrite();
        if (luStartPressedPort != CgsInput::KU_NUMBER_OF_PADS)
            lpGuiInput->GetGuiEvents()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&luStartPressedPort), 143, sizeof(luStartPressedPort));
        lpGameModule->BridgeGameToGui(lpGuiInput);
        lInputOutput->LockForRead();
        lpGameModule->BridgeControllerToGui(lpGuiInput, lInputOutput);
        lInputOutput->UnlockForRead();
        lpGuiInput->UnlockForWrite();

        lpGameModule->GetGuiModule().Update(lUpdateSet, lpInputStack, lpOutputStack,
            lpGuiInput, lpGuiOutput, lpModelOutput, lpViewInput,
            lpGameDataInput, lpGameDataOutput, false);

        lpGuiOutput->LockForRead();
        lSoundInput->LockForWrite();
        lpGameModule->BridgeGuiToSound(lSoundInput, lpGuiOutput);
        lSoundInput->UnlockForWrite();
        lpGuiOutput->UnlockForRead();
        if (gBrnScriptedLoadStage > 5)
        {
            lpWorldOutput->LockForRead();
            lSoundInput->LockForWrite();
            lpGameModule->BridgeWorldToSound(lSoundInput, lpWorldOutput, lUpdateSet);
            lSoundInput->UnlockForWrite();
            lpWorldOutput->UnlockForRead();
        }
        const CgsSystem::Timer& lrSimTimer = lpGameModule->GetSimTimer();
        lpGameModule->GetSoundModule().Update(
            lrGameTimer.GetScaleCurrent() * lrGameTimer.GetRate(),
            lrSimTimer.GetScaleCurrent() * lrSimTimer.GetRate(),
            lpInputStack, lpOutputStack, lSoundInput, lSoundOutput, lUpdateSet);

        lpModelOutput->LockForRead();
        lpGuiOutput->LockForRead();
        lpWorldOutput->LockForRead();
        lSoundOutput->LockForRead();
        lpGameModule->BridgeGuiToResource(lpGameDataInput, lpModelOutput, lpGuiOutput);
        if (gBrnScriptedLoadStage > 5)
            lpGameModule->BridgeWorldToResource(lpGameDataInput, lpWorldOutput);
        lpGameModule->BridgeGuiToGame(lpGuiOutput);
        const BrnSound::Module::Io::RootOutputBuffer* const lpSoundRead = lSoundOutput;
        lpGameDataInput->GetAttribSysRequestInterface()->mRequestQueue.Append(
            lpSoundRead->GetAttribSysRequestInterface()->mRequestQueue);
        lpGameDataInput->GetRequestInterface()->mRequestQueue.Append(
            lpSoundRead->GetResourceRequestInterface()->mRequestQueue);
        lSoundOutput->UnlockForRead();
        lpWorldOutput->UnlockForRead();
        lpGuiOutput->UnlockForRead();
        lpModelOutput->UnlockForRead();

        lInputOutput->LockForRead();
        lpGameModule->BridgeInputToGame(lInputOutput);
        lInputOutput->UnlockForRead();
        if (gBrnScriptedLoadStage != 8)
            RenderGUI(lpGuiInput, lpGuiOutput, lpModelOutput, lpViewInput, false);
        lpGameDataInput->UnlockForWrite();
        lpGameDataOutput->UnlockForRead();
        lpOutputStack->DestroyIOBuffer(&lNetworkOutput);
        lpOutputStack->DestroyIOBuffer(&lNetworkInput);
        lpOutputStack->DestroyIOBuffer(&lGameStateOutput);
        lpInputStack->DestroyIOBuffer(&lGameStatePostInput);
        lpInputStack->DestroyIOBuffer(&lGameStatePreInput);
        lpOutputStack->DestroyIOBuffer(&lpModelOutput);
        lpInputStack->DestroyIOBuffer(&lpViewInput);
        lpOutputStack->DestroyIOBuffer(&lpGuiOutput);
        lpInputStack->DestroyIOBuffer(&lpGuiInput);
        lpOutputStack->DestroyIOBuffer(&lpWorldOutput);
        lpInputStack->DestroyIOBuffer(&lWorldInput);
        lpOutputStack->DestroyIOBuffer(&lSoundPreOutput);
        lpOutputStack->DestroyIOBuffer(&lSoundOutput);
        lpInputStack->DestroyIOBuffer(&lSoundInput);
        lpOutputStack->DestroyIOBuffer(&lControllerGameStateOutput);
        lpOutputStack->DestroyIOBuffer(&lInputOutput);

    }

    else
    {
        // ARTIST enters the complete native update directly once stage8 is reached.
        lpGameModule->DoUpdate();
    }
}

// ARTIST823E7700; DecFIGS3119A8 confirms this bool/two-buffer signature.
// The caller holds the persistent GameData write/read locks throughout this attempt.
bool LoadingScriptedState::LoadSoundModuleAgain(
        BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput)
{
    BrnGame::BrnGameModule* const lpGameModule = BrnGame::GetMainGameModule();
    CgsModule::IOBufferStack* const lpInputStack = lpGameModule->GetUpdateInputBufferStack();
    CgsModule::IOBufferStack* const lpOutputStack = lpGameModule->GetUpdateOutputBufferStack();
    BrnSound::Module::Io::RootInputBuffer* lpSoundInput = 0;
    BrnSound::Module::Io::RootOutputBuffer* lpSoundOutput = 0;
    lpInputStack->CreateIOBuffer(&lpSoundInput, "Sound");
    lpOutputStack->CreateIOBuffer(&lpSoundOutput, "Sound");

    const bool lbPrepared = lpGameModule->GetSoundModule().PrepareOnEnteringGameplay(
        lpGameDataOutput->GetAllocatorList(), lpInputStack, lpOutputStack,
        lpSoundInput, lpSoundOutput);
    if (!lbPrepared)
    {
        lpSoundOutput->LockForRead();
        const BrnSound::Module::Io::RootOutputBuffer* const lpRead = lpSoundOutput;
        lpGameDataInput->GetAttribSysRequestInterface()->mRequestQueue.Append(
            lpRead->GetAttribSysRequestInterface()->mRequestQueue);
        lpGameDataInput->AppendRequestInterface<4096>(*lpRead->GetResourceRequestInterface());
        lpSoundOutput->UnlockForRead();
    }
    lpOutputStack->DestroyIOBuffer(&lpSoundOutput);
    lpInputStack->DestroyIOBuffer(&lpSoundInput);
    return lbPrepared;
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

    meLoadingScreenStage = E_LOADINGSTAGE_START;
    mbGuiPreloadDone     = false;

    // @0x823AA9D8 -- mbSaveLoadState = 1. NB the console never EXECUTES this body (SendEvent
    // never targets state 0; Construct seeds it raw), so the initial-load update set stays
    // 0x80 there. The PC no longer calls OnEnter at boot either -- see the removed SetState
    // in BrnGameModule::Construct -- but the body is reproduced faithfully for re-entry.
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SetSaveLoadState(true);
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
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SetSaveLoadState(false);
}

// ARTIST823EF310: preparation and its same-call GUI retry/resource forward.
bool LoadingScriptedState::LoadGUIModule(
    BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
    CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
    CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
    CgsGui::ModelIO::OutputBuffer* lpModelOutput,
    CgsGui::ViewIO::InputBuffer* lpViewInput,
    const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput)
{
    BrnGame::BrnGameModule* const lpGameModule = BrnGame::GetMainGameModule();
    BrnGui::GuiModule& lrGui = lpGameModule->GetGuiModule();
    if (lrGui.Prepare(lpGuiOutput, lpGameDataInput, lpGameDataOutput))
    {
        if (!lpGuiInput)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBuffer",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 238);
            CgsDev::Assert::EndAssert();
        }
        lpGuiInput->LockForWrite();
        BrnGui::GuiEventRunFsm lEvent;
        // ARTIST leaves meFsmToRun untouched in these explicit-ID records.
        lEvent.mInitialStateId = 0;
        lEvent.mFsmId = CgsIDCompress("BrnBFPreFsm");
        lEvent.meFlowToUse = BrnGui::E_GUIFLOW_HUD;
        lpGuiInput->GetGuiEvents()->AddEvent(&lEvent, 144, sizeof(lEvent));
        lEvent.mFsmId = CgsIDCompress("BrnOverlay");
        lEvent.meFlowToUse = BrnGui::E_GUIFLOW_OVERLAY;
        lpGuiInput->GetGuiEvents()->AddEvent(&lEvent, 144, sizeof(lEvent));
        if (!lpGuiInput)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpInputBuffer",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\module\\CgsModuleUtils.h", 248);
            CgsDev::Assert::EndAssert();
        }
        lpGuiInput->UnlockForWrite();
        return true;
    }
    lrGui.Update(static_cast<BrnUpdateSet>(0x40), lpGameModule->GetUpdateInputBufferStack(),
        lpGameModule->GetUpdateOutputBufferStack(), lpGuiInput, lpGuiOutput,
        lpModelOutput, lpViewInput, lpGameDataInput, lpGameDataOutput, false);
    lpModelOutput->LockForRead();
    lpGuiOutput->LockForRead();
    lpGameModule->BridgeGuiToResource(lpGameDataInput, lpModelOutput, lpGuiOutput);
    lpGuiOutput->UnlockForRead();
    lpModelOutput->UnlockForRead();
    return false;
}

// ARTIST823EF688: the four GUI IO objects belong to this loading update.
void MainGameFlowStateInitialLoadingScreen::Update()
{
    BrnGame::BrnGameModule* const lpGameModule = BrnGame::GetMainGameModule();
    BrnResource::GameDataIO::InputBuffer* const lpGameDataInput =
        lpGameModule->GetGameDataModuleInputBuffer();
    BrnResource::GameDataIO::OutputBuffer* const lpGameDataOutput =
        lpGameModule->GetGameDataModuleOutputBuffer();
    CgsModule::IOBufferStack* const lpInputStack = lpGameModule->GetUpdateInputBufferStack();
    CgsModule::IOBufferStack* const lpOutputStack = lpGameModule->GetUpdateOutputBufferStack();
    CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput = 0;
    CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput = 0;
    CgsGui::ViewIO::InputBuffer* lpViewInput = 0;
    CgsGui::ModelIO::OutputBuffer* lpModelOutput = 0;
    lpInputStack->CreateIOBuffer(&lpGuiInput, "GUI");
    lpOutputStack->CreateIOBuffer(&lpGuiOutput, "GUI");
    lpInputStack->CreateIOBuffer(&lpViewInput, "GUIView");
    lpOutputStack->CreateIOBuffer(&lpModelOutput, "GUIModel");
    lpGameDataInput->LockForWrite();
    lpGameDataOutput->LockForRead();
    if (meLoadingScreenStage > E_LOADINGSTAGE_CONTROLLERMODULE)
        lpGameModule->GetDebugManager().Update(1.0f / 60.0f);
    switch (meLoadingScreenStage)
    {
    case E_LOADINGSTAGE_START:
    case E_LOADINGSTAGE_CONTROLLERMODULE:
        meLoadingScreenStage = E_LOADINGSTAGE_CONTROLLERMODULE;
        if (!LoadControllerModule(lpGameDataInput, lpGameDataOutput, &mpInputModuleAllocator))
            break;
    case E_LOADINGSTAGE_GUIMODULE:
        meLoadingScreenStage = E_LOADINGSTAGE_GUIMODULE;
        if (!LoadGUIModule(lpGameDataInput, lpGuiInput, lpGuiOutput,
                lpModelOutput, lpViewInput, lpGameDataOutput))
            break;
    case E_LOADINGSTAGE_DIRECTORMODULE:
        meLoadingScreenStage = E_LOADINGSTAGE_DIRECTORMODULE;
        if (!LoadDirectorModule(lpGameDataInput, lpGameDataOutput))
            break;
    case E_LOADINGSTAGE_SOUND_MODULE:
        meLoadingScreenStage = E_LOADINGSTAGE_SOUND_MODULE;
        if (!LoadSoundModule(lpGameDataInput, lpGameDataOutput))
            break;
    case E_LOADINGSTAGE_NETWORK:
        meLoadingScreenStage = E_LOADINGSTAGE_NETWORK;
        if (!LoadNetworkModule(lpGameDataInput, lpGameDataOutput))
            break;
    case E_LOADINGSTAGE_JUICE:
        meLoadingScreenStage = E_LOADINGSTAGE_JUICE;
    case E_LOADINGSTAGE_MASSIVE:
        meLoadingScreenStage = E_LOADINGSTAGE_MASSIVE;
        lpGameModule->GetSoftRebootData()->mbEngineModulesLoaded = true;
    case E_LOADINGSTAGE_REPLAYS:
        meLoadingScreenStage = E_LOADINGSTAGE_REPLAYS;
        if (!lpGameModule->GetReplayModule().Prepare(lpGameDataOutput->GetAllocatorList()))
            break;
    case E_LOADINGSTAGE_DONE:
        meLoadingScreenStage = E_LOADINGSTAGE_DONE;
        FinishLoading();
        break;
    default:
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("0",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameFlowController/TopLevel/BrnGameMainFlowInitialLoadingScreenState.cpp", 268);
        CgsDev::Assert::EndAssert();
        break;
    }
    if (meLoadingScreenStage <= E_LOADINGSTAGE_GUIMODULE)
    {
        CgsModule::IOHelper<RendererIO::InputBuffer> lRendererInput(lpInputStack, "Render");
        CgsModule::IOHelper<RendererIO::OutputBuffer> lRendererOutput(lpOutputStack, "Render");
        lpGameModule->GetRenderModule().Update(lpInputStack, lpOutputStack,
            lRendererInput, lRendererOutput);
        GetDispatchWriteBuffer()->ShowLoadingScreen();
    }
    else
    {
        lpGameModule->GetGameTimer().Update();
        const CgsSystem::Timer& lrGameTimer = lpGameModule->GetGameTimer();
        CgsGui::GuiEventTimeInfo lTimeInfo;
        lTimeInfo.Set(lrGameTimer.GetScaleCurrent() * lrGameTimer.GetRate(),
            static_cast<f32>(lrGameTimer.GetAccumTicks()) + lrGameTimer.GetAccumulator());
        lpGuiInput->LockForWrite();
        BrnGame::PushGuiEvent(lTimeInfo, lpGuiInput);
        lpGuiInput->UnlockForWrite();
        lpGuiInput->LockForWrite();
        lpGameModule->BridgeGameToGui(lpGuiInput);
        lpGuiInput->UnlockForWrite();
        lpGameModule->GetGuiModule().Update(static_cast<BrnUpdateSet>(0x40),
            lpInputStack, lpOutputStack, lpGuiInput, lpGuiOutput,
            lpModelOutput, lpViewInput, lpGameDataInput, lpGameDataOutput, false);
        lpModelOutput->LockForRead();
        lpGuiOutput->LockForRead();
        lpGameModule->BridgeGuiToResource(lpGameDataInput, lpModelOutput, lpGuiOutput);
        lpGameModule->BridgeGuiToGame(lpGuiOutput);
        if (!mbGuiPreloadDone && lpGameModule->IsGuiPhaseComplete())
            mbGuiPreloadDone = true;
        lpGuiOutput->UnlockForRead();
        lpModelOutput->UnlockForRead();
        RenderGUI(lpGuiInput, lpGuiOutput, lpModelOutput, lpViewInput, false);
    }
    lpGameDataInput->UnlockForWrite();
    lpGameDataOutput->UnlockForRead();
    lpOutputStack->DestroyIOBuffer(&lpModelOutput);
    lpInputStack->DestroyIOBuffer(&lpViewInput);
    lpOutputStack->DestroyIOBuffer(&lpGuiOutput);
    lpInputStack->DestroyIOBuffer(&lpGuiInput);
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

    BrnGame::GetMainGameModule()->GetThreadLayout().SetResourcePercentageOfFrame(5);

        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SendEvent(BrnGameMainFlowController::E_MGE_STATEEND);
}

// --- MainGameFlowStateStartScreen (the legal/title screen phase) -------------------------
MainGameFlowStateStartScreen::MainGameFlowStateStartScreen() {}

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
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SetVideoState(true);
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "StartScreen: OnEnter -> GUI FSM stage 2 (BrnLegalFsm)\n";
}
void MainGameFlowStateStartScreen::OnLeave() {}

// @ 0x823F2D78 -- the title screen's flow decisions: command 71 (pre-accept) resumes the
// world load while the accept-dwell animation plays; command 70 (accepted) advances the
// main flow (-> MEMORY_CARD / BF_PROFILE). The per-frame GUI drive itself runs in
// BrnGameModule::GameMain (the console's LoadingScriptedState::Update shared spine).
// ARTIST823F2D78: invite bypass precedes pre-accept and phase completion.
void MainGameFlowStateStartScreen::Update()
{
    LoadingScriptedState::Update();
    BrnGame::BrnGameModule* const lpGameModule = BrnGame::GetMainGameModule();
    if (lpGameModule->GetGuiModule().IsPerformingInvite())
        lpGameModule->GetMainFlowStateMachine().SendEvent(BrnGameMainFlowController::E_MGE_STATEEND);
    else if (lpGameModule->IsGuiPreAcceptRequested())
    {
        ResumeLoadingWorld();
        BrnGame::GetMainGameModule()->ClearGuiPreAccept();
    }
    else if (lpGameModule->IsGuiPhaseComplete())
        lpGameModule->GetMainFlowStateMachine().SendEvent(BrnGameMainFlowController::E_MGE_STATEEND);
}

void MainGameFlowStateStartScreen::Render() { LoadingScriptedState::Render(); }

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
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SetVideoState(true);
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "MarketingScreens: OnEnter -> GUI FSM stage 1 (BrnVideoFsm)\n";
}

void MainGameFlowStateMarketingScreens::OnLeave() {
    // @0x823AABC4 -- mbVideoState = 0.
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SetVideoState(false);
}

// @ 0x823F2CD8 -- advance when the GUI posts phase-complete (command 70: the Criterion
// logo finished).
void MainGameFlowStateMarketingScreens::Update()
{
    // X360 0x823F2CD8 opens with the shared scripted-load spine.
    LoadingScriptedState::Update();

    if (BrnGame::GetMainGameModule()->IsGuiPhaseComplete())
    {
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SendEvent(
            BrnGameMainFlowController::E_MGE_STATEEND);
    }
}

void MainGameFlowStateMarketingScreens::Render() { LoadingScriptedState::Render(); }

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
    {
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SetSaveLoadState(true);
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SetVideoState(true);
    }
}
void MainGameFlowStateCheckDiskSpace::OnLeave() {}
// Update @ 0x823F2D28 lives in its DWARF home TU, BrnGameMainFlowCheckDiskSpace.cpp.
void MainGameFlowStateCheckDiskSpace::Render() { LoadingScriptedState::Render(); }

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
    {
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SetSaveLoadState(false);   // @0x823AACE4
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SetVideoState(false);      // @0x823AACEC
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
    if (BrnGame::GetMainGameModule()->IsGuiPhaseComplete())
    {
        // X360 @0x823F2FD0..D8: `stb r11(=1), 0x9A02F5(gm)` -- gm+10094117, the streaming-wait
        // request DoUpdate_GameStatePreWorld turns into game event 8 (-> WaitForStreaming ->
        // the boot pause), stored right before the STATEEND SendEvent.
        BrnGame::GetMainGameModule()->RequestStreamingWait();
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SendEvent(
            BrnGameMainFlowController::E_MGE_STATEEND);
    }
}
void MainGameFlowStateMemoryCard::Render() { LoadingScriptedState::Render(); }

// --- MainGameFlowStateCompleteLoading (the post-title / compound-load phase) -------------
MainGameFlowStateCompleteLoading::MainGameFlowStateCompleteLoading() {}

// @ 0x823AABD0 -- request the BF_COMPLOAD GUI FSM stage (BrnCmpLdFsm; pending-stage
// byte = 3) and reset the settle latch (X360 this+4 = 0).
void MainGameFlowStateCompleteLoading::OnEnter()
{
    // @0x823AABF0 -- CLEAR ONLY (see MemoryCard::OnEnter).
    gBrnScriptedLoadPaused = false;

    BrnGame::GetMainGameModule()->RequestGuiFsmStage(3);
    mbIsCollisionWorldPrepared = false;

    // @0x823AAC04 -- mbVideoState = 1 (the post-title intro is a video phase).
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SetVideoState(true);
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "CompleteLoading: OnEnter -> GUI FSM stage 3 (BrnCmpLdFsm)\n";
}
// ARTIST823AAC18: clear the flow's video byte, then leave GUI boot-movie mode.
// game+0x70A65D == GUI base0x6EAA20 + mbBootMovieMode0x1FC3D; the same byte
// is initialized at82518B88 and read by both original resource validators.
void MainGameFlowStateCompleteLoading::OnLeave()
{
        BrnGame::GetMainGameModule()->GetMainFlowStateMachine().SetVideoState(false);      // @0x823AAC34
    BrnGame::GetMainGameModule()->GetGuiModule().SetMovieBootMode(false);                 // @0x823AAC3C
}

// @ 0x823F2E08 -- when the GUI posts phase-complete (the post-title intro finished) and
// the world is loaded: first pass stamps the load-state + latches (X360 sets the global
// load stage to 7), second pass advances to IN_GAME. (The quit-to-dash branch and the
// X360 load-state global are platform/world follow-ons.)
// ARTIST823F2E08: advance the collision stage, then consume game-over.
void MainGameFlowStateCompleteLoading::Update()
{
    LoadingScriptedState::Update();
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    if (lpGameModule->IsGuiPhaseComplete() && gBrnScriptedLoadStage == 8)
    {
        if (mbIsCollisionWorldPrepared)
            lpGameModule->GetMainFlowStateMachine().SendEvent(BrnGameMainFlowController::E_MGE_STATEEND);
        else
        {
            gBrnScriptedLoadStage = 7;
            mbIsCollisionWorldPrepared = true;
        }
        lpGameModule = BrnGame::GetMainGameModule();
    }
    if (lpGameModule->IsGameOverRequested())
    {
        lpGameModule->ClearGameOver();
        lpGameModule->GetMainFlowStateMachine().SendEvent(BrnGameMainFlowController::E_MGE_GAMEOVER);
    }
}
// ⭐ 2026-08-16 (boot audit F-P6-11). CompleteLoading::Render IS DoDispatch on the console --
// the same render-dispatch spine MainGameFlowStateInGame::Render drives. Ours was empty, so
// for the whole compload window (which is now a real hold on stage 7 while the collision
// streams, not a couple of frames) nothing dispatched a render frame from the flow state and
// the screen was carried entirely by whatever the previous state left behind.
void MainGameFlowStateCompleteLoading::Render()
{
    LoadingScriptedState::Render();
}

MainGameFlowStateInGame::MainGameFlowStateInGame() {}
// OnEnter/OnLeave/Update/Render are homed in BrnGameMainFlowInGameState.cpp (the DWARF
// home for the in-game state's per-frame surface); only the ctor stays with this group.
