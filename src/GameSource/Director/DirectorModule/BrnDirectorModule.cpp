// DirectorModule staged lifecycle and per-frame update spine.
// ARTIST Construct8225C590, Prepare822712D8, Release82239198,
// Destruct82250D98, PreSceneQueryUpdate8225C768, Update82275300,
// PostGuiUpdate82250DD0 and ProcessSceneQueryResults82239278.
// ReplayDirector and DirectorSerialiser are owned typed engine objects.
// Console displacements in historical evidence comments are provenance only.

#include "GameSource/Director/BrnDirectorModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::AddMonitor
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"          // DirectorIO::InputBuffer
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp" // DirectorIO::OutputBuffer
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"      // BrnDirector::SceneQueryInterface (the per-frame post office)

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// Shared helper: stage the per-frame scene-query post office.
//
// All three per-frame entry points build a BrnDirector::SceneQueryInterface on their own
// stack, point slot 0 at the SceneManager producer published in the scene-query OUTPUT
// buffer, fill (or deliberately NULL) the six post-office slots, and Clear() it before
// handing it to the director. The three differ ONLY in which slots they populate --
// reproduced exactly, per entry point, at each call site below.
//
// The cast: DirectorIO::SceneQueryOutputBuffer::GetSceneQueryInterface() returns the
// address of its published member typed as the deliberately-opaque
// BrnDirector::DirectorIO::SceneQueryInterface forward-decl (see
// BrnDirectorModuleIOSceneQuery.h); BrnDirector::SceneQueryInterface's slot 0 is typed at
// the real producer, CgsSceneManager::SceneManagerIO::SceneQueryInterface. Same address,
// two names for it -- the reinterpret is the seam that header documents.
// ----------------------------------------------------------------------------
static inline CgsSceneManager::SceneManagerIO::SceneQueryInterface* lpProducerOf(
        DirectorIO::SceneQueryOutputBuffer* lpSceneQueryOutputBuffer)
{
    return reinterpret_cast<CgsSceneManager::SceneManagerIO::SceneQueryInterface*>(
               lpSceneQueryOutputBuffer->GetSceneQueryInterface());
}

// ----------------------------------------------------------------------------
// Construct  @ 0x8225C590  (EXECUTED in goal trace)
//
// Chains to the ModuleSingleBuffered base Construct, builds the debug component and the
// active camera, seeds the prepare/release stage words and a handful of scattered
// scratch fields the padding regions absorb, constructs the top-level MainDirector and
// the director replay serialiser, registers three CgsDev::PerfMonCpu monitors, and
// finally marks the module as a "new module" (the inherited CgsModule::Module base
// flag).
//
// FLAGS (see the header for the full writeup):
//   * mDirectorResourceManager.Construct() is currently a stub in the already-landed
//     BrnDirectorResourceManager.h -- the X360's inlined EventReceiverQueue<512,16>
//     init that this call site performs on the real binary is not reproduced by it.
//   * the X360 also stores &mDebugComponent into a word at this+0xB40 (inside
//     MainDirector's own opaque storage, +0x40 from MainDirector's own base) -- not
//     reproduced (no exposed field on MainDirector to write it through by name).
//   * the release-stage seed value is the literal 4 (asm-proven store), which is
//     EReleaseStage::E_RELEASESTAGE_MANAGER, not E_RELEASESTAGE_DONE -- see the Release
//     comment below for the same asm-literal-vs-DONE-enumerator observation.
// ----------------------------------------------------------------------------
void DirectorModule::Construct(f32 lfTime)
{
    CgsModule::ModuleSingleBuffered::Construct();

    mePrepareStage = E_PREPARESTAGE_START;                 // this+0x38B10 = 0
    meReleaseStage = static_cast<EReleaseStage>(4);         // this+0x38B14 = 4 (asm literal; see FLAG above)

    mDebugComponent.Construct(this);

    // The console inlines DirectorResourceManager::Construct here (its own
    // EventReceiverQueue<512,16> init, the prepare stage, the list pointers); the call is
    // that same body expressed by name.
    mDirectorResourceManager.Construct();

    // The X360 also inlines WorldMap::Construct here (module+2312/2328/2332 = the world
    // map's EventReceiverQueue<128,16>, module+2464 = meLoadingState). NOT optional: the
    // lane requests LoadData issues name &mReceiverQueue as their reply address, and
    // BaseEventReceiverQueue::AddEvent takes `(liSize + 8) % miAlignment` -- an
    // unconstructed queue has miAlignment == 0 and the first reply divides by zero.
    mWorldMap.Construct();

    // The six scene-query post offices (0x8225C638..0x8225C660): PostOffice::Construct inlined --
    // the fine office gets its Array's length word (`stw 0, 0x28`) and then its specialised Clear
    // out of line (`bl sub_8221CC98`); the other five fold Construct + Clear into the single
    // length-word store (+0xA70 / +0xA9C / +0xAC8 / +0xAD0 / +0xAFC).
    mLineTestFinePostOffice.Construct();
    mLineTestNearestPostOffice.Construct();
    mLineTestFastDoubleSidedPostOffice.Construct();
    mSphereTestFastPostOffice.Construct();
    mVolumeTestFinePostOffice.Construct();
    mVolumeTestDeepestPostOffice.Construct();

    mReplayDirector.SetFirstFrame(false);  // ARTIST 8225C664
    mReplayDirector.GetFinalCamera().Construct();

    mMainDirector.Construct(&mDirectorResourceManager, lfTime);

    mDirectorSerialiser.Construct();
    mbIsReplaying = false;
    mbReplayRestoring = false;

    // Budget literals are the X360 rodata floats flt_82004014 (Pre SQ / Post Gui) and
    // flt_82001C98 (Main) -- both cross-confirmed elsewhere in this codebase as 0.1f and
    // 1.0f respectively (BrnMathUtils.cpp / BrnDirectorModuleDebugPrinter.cpp for
    // flt_82004014==0.1f; GameShared/.../CgsCamera.cpp for flt_82001C98==1.0f), not
    // decompiler guesses.
    miPerfCount_PreSQUpdate  = CgsDev::PerfMonCpu::AddMonitor("Pre SQ Update",   CgsDev::E_PMP_13, false, 0.1f, true);
    miPerfCount_MainUpdate   = CgsDev::PerfMonCpu::AddMonitor("Main Update",     CgsDev::E_PMP_13, false, 1.0f, true);
    miPerfCount_PostGuiUpdate = CgsDev::PerfMonCpu::AddMonitor("Post Gui Update", CgsDev::E_PMP_13, false, 0.1f, true);

    mbIsNewModule = true;   // this+0x004 (inherited CgsModule::Module member)
}

// ----------------------------------------------------------------------------
// Release  @ 0x82239198  (recovered; not executed in goal trace)
//
// Staged RELEASE state machine, the same shape as MainDirector::Release: the stage word
// (meReleaseStage) selects the case; the cases fall through (0/1->2->3->4). On the
// first (stage 0 or 1) it releases the owned MainDirector; on the last (stage 3) it
// releases the ModuleSingleBuffered base and, on success, advances to stage 4 and
// clears mePrepareStage. An out-of-range stage asserts and reports failure. Faithful to
// the asm: NO added control flow; the fall-through chain matches the jump table.
//
// NOTE: the asm's final stage literal is 4, not EReleaseStage::E_RELEASESTAGE_DONE (6)
// -- kept faithful to the asm value rather than "corrected" to the DWARF DONE
// enumerator (this TU's Release only progresses local bookkeeping to stage 4; the
// WORLDMAP/DONE stages the DWARF enum lists are driven by other, not-yet-reconstructed
// code paths).
// ----------------------------------------------------------------------------
bool DirectorModule::Release()
{
    switch (meReleaseStage)
    {
    case 0:
    case 1:
        meReleaseStage = static_cast<EReleaseStage>(1);
        if (!mMainDirector.Release())
            return false;
        // fall through
    case 2:
        meReleaseStage = static_cast<EReleaseStage>(3);
        // fall through
    case 3:
        meReleaseStage = static_cast<EReleaseStage>(3);
        if (!CgsModule::ModuleSingleBuffered::Release())
            return false;
        meReleaseStage = static_cast<EReleaseStage>(4);
        mePrepareStage = E_PREPARESTAGE_START;
        return true;

    default:
        CGS_ASSERT(false, "Invalid Stage\n");
        return false;
    }
}

// ----------------------------------------------------------------------------
// Destruct  @ 0x82250D98  (recovered; not executed in goal trace)
//
// Destructs the owned MainDirector, then the ModuleSingleBuffered base. Faithful to the
// asm (two calls, in order, no other work).
// ----------------------------------------------------------------------------
void DirectorModule::Destruct()
{
    mMainDirector.Destruct();
    CgsModule::ModuleSingleBuffered::Destruct();
}

// ============================================================================
//  THE PER-FRAME SPINE  (reconstructed in the director wave)
// ============================================================================

// ----------------------------------------------------------------------------
// Prepare  @ 0x822712D8
//
// The staged PREPARE state machine. The output buffer is write-locked for the WHOLE call
// (both the success and the failure exits unlock it). The stage word (mePrepareStage)
// selects the entry case and the cases fall through, so one call can advance several
// stages when each sub-Prepare completes immediately:
//
//   0 -> register the module's debug component ("Camera" in the debug menu)
//   1 -> CgsModule::ModuleSingleBuffered::Prepare (the base's own staged init)
//   2 -> DirectorResourceManager::Prepare (its ICE/attrib vault + shot-group banks)
//   3 -> WorldMap::LoadData              (trigger + traffic-lane + AI-lane resources)
//   4 -> MainDirector::Prepare           (dev tools, ICE wrapper, behaviour manager)
//   5 -> done: clear the release stage, unlock, report success
//
// Any sub-Prepare returning false leaves the stage word where it is and reports false --
// the module framework simply calls Prepare again next tick and it resumes from there.
// An out-of-range stage asserts (BrnDirectorModule.cpp:140) and reports failure.
//
// NOTE ON THE STAGE VALUES. The asm stores the literals 1..5; the DWARF EPrepareStage
// enumerators (START/RESOURCES/ICE/WORLDMAP/MANAGER/...) only line up with this build's
// use in the middle of the range (stage 3 really is the WORLDMAP step, stage 4 really is
// the MANAGER step), and this build's TERMINAL stage is 5, not E_PREPARESTAGE_DONE (8).
// The asm literals are kept, exactly as the committed Release does for its own terminal
// value -- faithful to the binary rather than "corrected" to the enumerator.
//
// The two extra arguments the X360 threads through (a2 = the output buffer, a3 = a plain
// s32 forwarded untouched into MainDirector::Prepare) are named for what they are.
// ----------------------------------------------------------------------------
bool DirectorModule::Prepare(DirectorIO::OutputBuffer* lpOutputBuffer,
                             const BrnResource::GameDataIO::AllocatorList* lpAllocatorList)
{
    lpOutputBuffer->LockForWrite();

    switch (mePrepareStage)
    {
    case 0:
        // this+552 == &mDebugComponent.
        mDebugComponent.Register();
        // fall through
    case 1:
        mePrepareStage = static_cast<EPrepareStage>(1);
        if (!CgsModule::ModuleSingleBuffered::Prepare())
            break;
        // fall through
    case 2:
        mePrepareStage = static_cast<EPrepareStage>(2);
        // asm: DirectorResourceManager::Prepare(this+584, a2, this+2896). this+2896 ==
        // this+0xB50 == mMainDirector's own +0x50 -- the embedded ICE wrapper (the
        // "HACK ice wrapper" the manager's own declared signature already names).
        if (!mDirectorResourceManager.Prepare(lpOutputBuffer, &mMainDirector.GetICEWrapper()))
            break;
        // fall through
    case 3:
        mePrepareStage = static_cast<EPrepareStage>(3);
        // asm: WorldMap::LoadData(this+2216, OutputBuffer::GetResour(a2)) -- the world map
        // pumps its resource requests through the GameData request interface published in
        // the director's output buffer. (LoadData is currently a documented quiet gate --
        // see BrnDirectorWorldMap.cpp.)
        if (!mWorldMap.LoadData(lpOutputBuffer->GetResour()))
            break;
        // fall through
    case 4:
        mePrepareStage = static_cast<EPrepareStage>(4);
        if (!mMainDirector.Prepare(lpOutputBuffer, lpAllocatorList, &mDirectorResourceManager))
            break;
        // fall through
    case 5:
        mePrepareStage = static_cast<EPrepareStage>(5);
        meReleaseStage = static_cast<EReleaseStage>(0);
        lpOutputBuffer->UnlockForWrite();
        return true;

    default:
        CGS_ASSERT(false, "Invalid Stage\n");
        break;
    }

    lpOutputBuffer->UnlockForWrite();
    return false;
}

// ----------------------------------------------------------------------------
// ProcessSceneQueryResults  @ 0x82239278
//
// ⭐ BODIED 2026-09-25 (FX-DIRECTOR2, the camera scene-query closure). It was a quiet gate because
// the six delivery helpers were IDA-truncated; they are PostOffice<T,N>::Deliver (BrnPostOffice.h),
// one instantiation per result record, and all six are read.
//
// Drain the results the SceneManager answered this frame -- DoUpdate_Director appended them into
// the scene-query INPUT buffer's queue (VariableEventQueue<4032,16>::Append<32768,16> @0x823DA090)
// -- and hand each to the post office that minted its id. The walk is GetFirstEvent /
// GetNextEvent, stopping on a null event (`lwz r31, var_40 ; cmplwi r31, 0`); the switch is on the
// RESULT TYPE (`addi r11, r3, -1 ; cmplwi r11, 5`), and the id is always the low half of the
// record's query id (`clrlwi r4, r11, 16` -- SceneQueryId::GetIndex()):
//   type 1  +0x9A4  LineTestFine       id at +0x00; the package is the RECORD'S ADDRESS
//                                      (`stw r31, var_3C ; addi r5, r1, var_3C`)
//   type 2  +0x9D0  LineTestNearest    id at +0x28 (the nearest record's mQueryId)
//   type 3  +0xA74  FastDoubleSided    id at +0x00
//   type 4  +0xAA0  SphereTestFast     id at +0x00
//   type 5  +0xAD4  VolumeTestDeepest  id at +0x00
//   type 6  +0xACC  VolumeTestFine     id at +0x00
//   default         assert "Unhandled result type" (BrnDirectorModule.cpp:537, non-gating)
// Its one caller is Update @0x82275300. The records are the queue's byte images (the sanctioned
// external-byte-stream case), read through their SceneManagerIO types.
//
// ----------------------------------------------------------------------------
void DirectorModule::ProcessSceneQueryResults(
        const DirectorIO::SceneQueryInputBuffer* lpSceneQueryInputBuffer)
{
    typedef CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult            FineResult;
    typedef CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult         NearestResult;
    typedef CgsSceneManager::SceneManagerIO::OutEventLineTestFastDoubleSidedResult FastDoubleSidedResult;
    typedef CgsSceneManager::SceneManagerIO::OutEventSphereTestFastResult          SphereResult;
    typedef CgsSceneManager::SceneManagerIO::OutEventVolumeTestDeepestResult       DeepestResult;
    typedef CgsSceneManager::SceneManagerIO::OutEventVolumeTestFineResult          VolumeFineResult;

    const CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueue<4032>* lpResults =
        lpSceneQueryInputBuffer->GetResultsQueue();                                // 0x82206BA8

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    for (s32 liType = lpResults->GetFirstEvent(&lpEvent, &liSize);
         lpEvent != 0;
         liType = lpResults->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        switch (liType)
        {
        case 1:
        {
            const FineResult* lpResult = reinterpret_cast<const FineResult*>(lpEvent);
            mLineTestFinePostOffice.Deliver(lpResult->mQueryId.GetIndex(), lpResult);
            break;
        }
        case 2:
        {
            const NearestResult* lpResult = reinterpret_cast<const NearestResult*>(lpEvent);
            mLineTestNearestPostOffice.Deliver(lpResult->mQueryId.GetIndex(), *lpResult);
            break;
        }
        case 3:
        {
            const FastDoubleSidedResult* lpResult = reinterpret_cast<const FastDoubleSidedResult*>(lpEvent);
            mLineTestFastDoubleSidedPostOffice.Deliver(lpResult->mQueryId.GetIndex(), *lpResult);
            break;
        }
        case 4:
        {
            const SphereResult* lpResult = reinterpret_cast<const SphereResult*>(lpEvent);
            mSphereTestFastPostOffice.Deliver(lpResult->mQueryId.GetIndex(), *lpResult);
            break;
        }
        case 5:
        {
            const DeepestResult* lpResult = reinterpret_cast<const DeepestResult*>(lpEvent);
            mVolumeTestDeepestPostOffice.Deliver(lpResult->mQueryId.GetIndex(), *lpResult);
            break;
        }
        case 6:
        {
            const VolumeFineResult* lpResult = reinterpret_cast<const VolumeFineResult*>(lpEvent);
            mVolumeTestFinePostOffice.Deliver(lpResult->mQueryId.GetIndex(), *lpResult);
            break;
        }
        default:
            CGS_ASSERT(false, "Unhandled result type");                           // .cpp:537
            break;
        }
    }
}

// ----------------------------------------------------------------------------
// PreSceneQueryUpdate  @ 0x8225C768
//
// The first of the director's two per-frame passes: it runs BEFORE the scene query, so the
// director's cameras can ISSUE the queries whose answers Update will consume. Perf-monitored
// as "Pre SQ Update".
//
// Locks: the input buffer for read, the output buffer and the scene-query output buffer for
// write; all three released in reverse order at the end.
//
// Body:
//   1. latch the incoming "is replaying" flag into mbIsReplaying, taking the
//      false->true EDGE to clear mbReplayRestoring and raise mbReplayStartedThisFrame;
//   2. (GATED) the director-serialiser mode bookkeeping -- see the gate note below;
//   3. build the per-frame scene-query post office with ALL SIX post offices live (this is
//      the pass that may issue queries) and Clear() it;
//   4. bundle the DirectorInputOutput and run the live director
//      (MainDirector::PreSceneQueryUpdate) or, while replaying, the replay director (GATED).
//
// The 7th X360 argument arrives as a bool in a register (`extrwi r11,r9,8,16; clrlwi r10,
// r11,31` -- the decompiler widens it to __int16 and takes HIBYTE; the value is masked to
// one bit either way, so it is a bool).
// ----------------------------------------------------------------------------
s32 DirectorModule::PreSceneQueryUpdate(s32 liUnusedA, s32 liUnusedB,
                                        const DirectorIO::InputBuffer* lpInputBuffer,
                                        DirectorIO::OutputBuffer* lpOutputBuffer,
                                        DirectorIO::SceneQueryOutputBuffer* lpSceneQueryOutputBuffer,
                                        bool lbIsReplaying)
{
    // FLAG: the X360 takes these two arguments and never reads them (the module framework
    // passes the same six-argument shape to every module entry point).
    (void)liUnusedA;
    (void)liUnusedB;

    CgsDev::PerfMonCpu::StartMonitor(miPerfCount_PreSQUpdate);

    lpInputBuffer->LockForRead();
    lpOutputBuffer->LockForWrite();
    lpSceneQueryOutputBuffer->LockForWrite();

    // ---- 1. the replay-flag latch + its rising edge --------------------------------
    if (lbIsReplaying != mbIsReplaying && lbIsReplaying)
    {
        mbReplayRestoring        = false;   // this+232193
        mReplayDirector.SetFirstFrame(true);       // this+231716
    }
    mbIsReplaying = lbIsReplaying;          // this+232192

    // ARTIST 8225C7EC..8225C834: restore acknowledgement takes precedence
    // over the prepare/readiness handshake.
    const BrnReplays::BaseSerialiser::EMode leMode = mDirectorSerialiser.GetMode();
    if (leMode == BrnReplays::BaseSerialiser::E_MODE_RESTORING)
    {
        mbReplayRestoring = true;
        mDirectorSerialiser.SetDataRestored(true);
    }
    else if (leMode == BrnReplays::BaseSerialiser::E_MODE_RECORDING_PREPARING ||
             leMode == BrnReplays::BaseSerialiser::E_MODE_PLAYING_PREPARING)
    {
        mDirectorSerialiser.SetDataReady(true);
    }

    // ---- 3. the per-frame scene-query post office ----------------------------------
    // This pass populates ALL SIX post offices: it is the one that may MINT query ids.
    SceneQueryInterface lSceneQuery;
    lSceneQuery.Construct(lpProducerOf(lpSceneQueryOutputBuffer),
                          &mLineTestFinePostOffice,               // +0x9A4
                          &mLineTestNearestPostOffice,            // +0x9D0
                          &mLineTestFastDoubleSidedPostOffice,    // +0xA74
                          &mSphereTestFastPostOffice,             // +0xAA0
                          &mVolumeTestFinePostOffice,             // +0xACC
                          &mVolumeTestDeepestPostOffice);         // +0xAD4
    lSceneQuery.Clear();

    // ---- 4. hand off to whichever director is driving ------------------------------
    DirectorInputOutput lIO;
    lIO.mpInputBuffer         = lpInputBuffer;
    lIO.mpOutputBuffer        = lpOutputBuffer;
    lIO.mpResourceManager     = &mDirectorResourceManager;   // this+584
    lIO.mpWorldMap            = &mWorldMap;                  // this+2216
    lIO.mpSceneQueryInterface = &lSceneQuery;

    if (mbIsReplaying)
    {
        if (mDirectorSerialiser.IsPlaying())
        {
            mDirectorSerialiser.SetDataReady(true);
            mDirectorSerialiser.Read();
            mReplayDirector.PreSceneQueryUpdate(mDirectorSerialiser.GetStaticLayout(), &lIO);
        }
    }
    else
    {
        mMainDirector.PreSceneQueryUpdate(&lIO);   // this+2816
    }

    lpSceneQueryOutputBuffer->UnlockForWrite();
    lpOutputBuffer->UnlockForWrite();
    lpInputBuffer->UnlockForRead();

    CgsDev::PerfMonCpu::StopMonitor(miPerfCount_PreSQUpdate);

    // The X360 returns StopMonitor's r3; the committed CgsDev::PerfMonCpu::StopMonitor is
    // void, and no caller reads this return, so 0 is returned. FLAG: return value not
    // reproduced (the callee's own signature differs).
    return 0;
}

// ----------------------------------------------------------------------------
// Update  @ 0x82275300
//
// The director's main per-frame pass, run AFTER the scene query. Perf-monitored as
// "Main Update". This is the function that ends with a camera in the output buffer.
//
// Locks: input + scene-query input for read, output + scene-query output for write.
//
// Body:
//   1. drain the scene-query results into the post offices (ProcessSceneQueryResults);
//   2. build the per-frame post office with slot 0 (the producer) ONLY -- this pass does
//      NOT mint queries, so the six post-office slots are deliberately NULLED (the X360
//      memsets them) and Clear() therefore does nothing;
//   3. run MainDirector::Update (live) or ReplayDirector::Update (replay). BOTH
//      legs publish their camera into the output buffer themselves (SetCameraOutput +
//      SetCgsCamera are called from exactly those two functions and nowhere else);
//   4. copy whichever director ran into the module's own published graphics camera
//      (mCgsCamera = <that director's CgsGraphics::Camera>);
//   5. the replay RECORD leg + unconditional serialiser registration.
// ----------------------------------------------------------------------------
s32 DirectorModule::Update(s32 liUnusedA, s32 liUnusedB,
                           const DirectorIO::InputBuffer* lpInputBuffer,
                           DirectorIO::OutputBuffer* lpOutputBuffer,
                           const DirectorIO::SceneQueryInputBuffer* lpSceneQueryInputBuffer,
                           DirectorIO::SceneQueryOutputBuffer* lpSceneQueryOutputBuffer)
{
    // FLAG: taken and never read by the X360 body (see PreSceneQueryUpdate).
    (void)liUnusedA;
    (void)liUnusedB;

    CgsDev::PerfMonCpu::StartMonitor(miPerfCount_MainUpdate);

    lpInputBuffer->LockForRead();
    lpSceneQueryInputBuffer->LockForRead();
    lpOutputBuffer->LockForWrite();
    lpSceneQueryOutputBuffer->LockForWrite();

    // ---- 1. deliver this frame's scene-query answers ------------------------------------
    // (DoUpdate_Director ran the queries PreSceneQueryUpdate issued between the two passes.)
    ProcessSceneQueryResults(lpSceneQueryInputBuffer);

    // ---- 2. the post office, PRODUCER SLOT ONLY ------------------------------------
    // asm: v29[0] = SceneQueryOutputB(a7); memset(&v29[1], 0, 24);  -- the six post-office
    // slots are explicitly zeroed on this pass (unlike PreSceneQueryUpdate), so the
    // subsequent Clear() is a no-op. Reproduced verbatim: this pass consumes answers, it
    // does not issue queries.
    SceneQueryInterface lSceneQuery;
    lSceneQuery.Construct(lpProducerOf(lpSceneQueryOutputBuffer), 0, 0, 0, 0, 0, 0);
    lSceneQuery.Clear();

    DirectorInputOutput lIO;
    lIO.mpInputBuffer         = lpInputBuffer;
    lIO.mpOutputBuffer        = lpOutputBuffer;
    lIO.mpResourceManager     = &mDirectorResourceManager;
    lIO.mpWorldMap            = &mWorldMap;
    lIO.mpSceneQueryInterface = &lSceneQuery;

    // ---- 3./4. run the driving director, then latch its graphics camera -------------
    if (!mbIsReplaying)
    {
        mMainDirector.Update(&lIO);

        // asm: CgsGraphics::Camera::operator=(this + 231824, this + 218320). this+218320 ==
        // mMainDirector's own +0x349D0 graphics camera -- the one MainDirector::Update just
        // filled from the finalised director camera via Camera::CopyToCgsCamera.
        mCgsCamera = mMainDirector.GetCgsCamera();
    }
    else
    {
        if (mDirectorSerialiser.IsPlaying())
        {
            mReplayDirector.Update(mDirectorSerialiser.GetStaticLayout(), &lIO);
            mCgsCamera = mReplayDirector.GetCgsCamera();
        }
    }

    // ARTIST 82275448..8227551C: snapshot in every recording mode,
    // then let Write select whether the stream can advance this frame.
    if (mDirectorSerialiser.IsRecording())
    {
        BrnReplays::DirectorSerialiserStaticLayout* lpFrame = mDirectorSerialiser.GetStaticLayout();
        lpFrame->mCamera.Construct();
        lpFrame->mCamera = mMainDirector.GetLastCamera();
        const Camera::BehaviourManager& lrManager = mMainDirector.GetBehaviourManager();
        lpFrame->mCarBaseAssetAttribsKey =
            lrManager.GetBehaviourParameterBank().GetGameplayCameraCarAttribsKey();
        lpFrame->mTempCameraBoostResponder = lrManager.GetTempCameraBoostResponder();
        lpFrame->mSpeedResponder = lrManager.GetSpeedResponder();
        lpFrame->mfLastHandbrakeTime = lrManager.GetLastHandbrakeTime();
        mDirectorSerialiser.Write();
    }

    // ARTIST 82275520..8227552C: every mode, including idle and restoring.
    lpOutputBuffer->GetReplayRequestI()->RegisterSerialiser(&mDirectorSerialiser);
    CGS_ASSERT(!lpProducerOf(lpSceneQueryOutputBuffer)->HasData(),
               "!lpSceneQueryOutput->GetSceneQueryInterface()->HasData()");

    lpInputBuffer->UnlockForRead();
    lpSceneQueryInputBuffer->UnlockForRead();
    lpOutputBuffer->UnlockForWrite();
    lpSceneQueryOutputBuffer->UnlockForWrite();

    CgsDev::PerfMonCpu::StopMonitor(miPerfCount_MainUpdate);
    return 0;   // see the return-value FLAG on PreSceneQueryUpdate
}

// ----------------------------------------------------------------------------
// PostGuiUpdate  @ 0x82250DD0
//
// The director's third and last per-frame pass, run after the GUI has updated (so camera
// work that must observe this frame's GUI state -- the shortcut menu, the crash-nav and
// colour-calibration overlays -- happens here). Perf-monitored as "Post Gui Update".
//
// Locks the input buffer for read and the output buffer for write. Runs
// MainDirector::PostGuiUpdate only when NOT replaying, then latches the input buffer's
// post-GUI car index.
//
// NOTE: on this pass the X360 builds the scene-query post office with EVERY slot null --
// including slot 0, the producer (the asm reuses the already-zero replay-flag register for
// all seven stores). So the whole post office is inert here and Clear() is a no-op; this
// pass cannot issue or receive queries. Reproduced verbatim.
// ----------------------------------------------------------------------------
s32 DirectorModule::PostGuiUpdate(s32 liUnusedA, s32 liUnusedB,
                                  const DirectorIO::InputBuffer* lpInputBuffer,
                                  DirectorIO::OutputBuffer* lpOutputBuffer)
{
    // FLAG: taken and never read by the X360 body (see PreSceneQueryUpdate).
    (void)liUnusedA;
    (void)liUnusedB;

    CgsDev::PerfMonCpu::StartMonitor(miPerfCount_PostGuiUpdate);

    lpInputBuffer->LockForRead();
    lpOutputBuffer->LockForWrite();

    if (!mbIsReplaying)
    {
        SceneQueryInterface lSceneQuery;
        lSceneQuery.Construct(0, 0, 0, 0, 0, 0, 0);   // even slot 0 -- see the note above
        lSceneQuery.Clear();

        DirectorInputOutput lIO;
        lIO.mpInputBuffer         = lpInputBuffer;
        lIO.mpOutputBuffer        = lpOutputBuffer;
        lIO.mpResourceManager     = &mDirectorResourceManager;
        lIO.mpWorldMap            = &mWorldMap;
        lIO.mpSceneQueryInterface = &lSceneQuery;

        mMainDirector.PostGuiUpdate(&lIO);
    }

    // ✅ GATE CLOSED (fly-by wave). The X360 finishes with
    //     s32 liIndex = *(s32*)((u8*)lpInputBuffer + 0x7AB8);   // a4[7854]
    //     if ( liIndex > -1 )  miPostGuiCarIndexLatch = liIndex;
    // -- a sticky latch that is only updated when the published word is not the -1 "none"
    // sentinel. The source word is now the NAMED DirectorIO::InputBuffer::miCameraType (the
    // slot BridgeGuiToDirector's command-591 arm writes, whose own assert calls it a "camera
    // type"), so this reads it through the accessor instead of poking the buffer.
    {
        const s32 liCameraType = lpInputBuffer->GetCameraType();
        if (liCameraType > -1)
            mReplayDirector.SetCameraSelection(liCameraType);
    }

    lpOutputBuffer->UnlockForWrite();
    lpInputBuffer->UnlockForRead();

    CgsDev::PerfMonCpu::StopMonitor(miPerfCount_PostGuiUpdate);
    return 0;   // see the return-value FLAG on PreSceneQueryUpdate
}

}
