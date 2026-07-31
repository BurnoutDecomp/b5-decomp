// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorModule.cpp
//
// BrnDirector::DirectorModule -- the top-level director engine module. Of the 7
// functions this build's ARTIST export recovers for this TU, 3 are reconstructed here:
//   - Construct  @ 0x8225C590  [EXECUTED in goal trace]
//   - Destruct   @ 0x82250D98  [recovered; not executed in goal trace]
//   - Release    @ 0x82239198  [recovered; not executed in goal trace]
//
// Prepare (0x822712D8), Update (0x82275300), PreSceneQueryUpdate (0x8225C768) and the
// nested BrnDirector::CameraFinaliser::Update (0x82250440) remain declaration-only --
// every one of them reaches at least one un-landed callee (MainDirector::Prepare/
// Update/PreSceneQueryUpdate are themselves declaration-only in BrnMainDirector.h;
// BrnDirector::WorldMap has no homed type at all; BrnReplays::DirectorSerialiser /
// BrnDirector::ReplayDirector / BrnDirector::InertiaController /
// BrnDirector::KeyAnimShakeController are all [todo]) -- per AGENTS.md the
// reconstruction rules forbid bodying a function whose callee graph isn't resolved.
// See BrnDirectorModule.h for their per-function BLOCKED notes.
//
// LAYOUT: DirectorModule is modelled with NAMED members at asm-proven offsets (real
// committed sub-object types where they exist) + explicit padding for untouched spans
// -- see the header for the full rationale (this TU was previously reconstructed, and
// reverted after fresh-eyes review, as one opaque byte buffer accessed via raw
// reinterpret_cast offset pokes throughout; that pattern is not used here).
// ============================================================================

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
//   * the 6th AddMonitor argument (liParentHandle) is read from a register the asm
//     never explicitly sets at two of the three call sites (a decompiler-visible
//     uninitialised-local artifact on the X360 side); passed as 0 here (the
//     conventional "no parent" sentinel for this API family).
// ----------------------------------------------------------------------------
void DirectorModule::Construct(f32 lfTime)
{
    CgsModule::ModuleSingleBuffered::Construct();

    mePrepareStage = E_PREPARESTAGE_START;                 // this+0x38B10 = 0
    meReleaseStage = static_cast<EReleaseStage>(4);         // this+0x38B14 = 4 (asm literal; see FLAG above)

    mDebugComponent.Construct(this);

    // FLAG: DirectorResourceManager::Construct() is currently a stub (see header FLAG);
    // the X360's inlined event-queue init at this call site (module+584/600/604 = its own
    // EventReceiverQueue<512,16>) is not reproduced -- its Prepare is a DirectorLinkStubs
    // no-op that never touches the queue.
    mDirectorResourceManager.Construct();

    // The X360 also inlines WorldMap::Construct here (module+2312/2328/2332 = the world
    // map's EventReceiverQueue<128,16>, module+2464 = meLoadingState). NOT optional: the
    // lane requests LoadData issues name &mReceiverQueue as their reply address, and
    // BaseEventReceiverQueue::AddEvent takes `(liSize + 8) % miAlignment` -- an
    // unconstructed queue has miAlignment == 0 and the first reply divides by zero.
    mWorldMap.Construct();

    mCamera.Construct();

    mMainDirector.Construct(&mDirectorResourceManager, lfTime);

    mDirectorSerialiser.Construct(
        /* liId               */ 4,
        /* liMode             */ 0,
        /* liBufferSize       */ 1024,
        /* liStaticBufferSize */ 384,
        /* lpcName            */ "DirectorModule",
        /* liFlag             */ 0);

    // Budget literals are the X360 rodata floats flt_82004014 (Pre SQ / Post Gui) and
    // flt_82001C98 (Main) -- both cross-confirmed elsewhere in this codebase as 0.1f and
    // 1.0f respectively (BrnMathUtils.cpp / BrnDirectorModuleDebugPrinter.cpp for
    // flt_82004014==0.1f; GameShared/.../CgsCamera.cpp for flt_82001C98==1.0f), not
    // decompiler guesses.
    miPerfCount_PreSQUpdate  = CgsDev::PerfMonCpu::AddMonitor("Pre SQ Update",   13, 0, 0.1, 0, 1);
    miPerfCount_MainUpdate   = CgsDev::PerfMonCpu::AddMonitor("Main Update",     13, 0, 1.0, 0, 1);
    miPerfCount_PostGuiUpdate = CgsDev::PerfMonCpu::AddMonitor("Post Gui Update", 13, 0, 0.1, 0, 1);

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
bool DirectorModule::Prepare(DirectorIO::OutputBuffer* lpOutputBuffer, s32 liPrepareArg)
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
        if (!mMainDirector.Prepare(lpOutputBuffer, liPrepareArg, &mDirectorResourceManager))
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
// ProcessSceneQueryResults  @ 0x82239278   -- ⚠️ DOCUMENTED QUIET GATE
//
// Drain the scene-query RESULTS queue the SceneManager published back into the director's
// scene-query INPUT buffer, and hand each result to the post office that minted its query
// id. The X360 body is a plain VariableEventQueue<4032,16> walk
// (GetFirstEvent / GetNextEvent) with a 6-way switch on the RESULT TYPE:
//
//   type 1 -> post office @this+2468 (+0x9A4)   (delivery helper IDA-truncated to `__`)
//   type 2 -> post office @this+2512 (+0x9D0)   OutEventLineTestNearestResult<40>
//   type 3 -> post office @this+2676 (+0xA74)   OutEventLineTestFastDoubleSidedResult<..>
//   type 4 -> post office @this+2720 (+0xAA0)   OutEventSphereTestFastResult<10>
//   type 5 -> post office @this+2772 (+0xAD4)   OutEventVolumeTestDeepestResult<10>
//   type 6 -> post office @this+2764 (+0xACC)   OutEventVolumeTestFineResult<1>
//   default -> assert "Unhandled result type" (BrnDirectorModule.cpp:537)
// (note types 5 and 6 land on the post offices in the OPPOSITE order to their declaration
//  -- see the header; that crossover is the binary's, not a transcription slip.)
//
// WHY GATED: every one of the six delivery calls is an IDA-TRUNCATED symbol
// (`CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult_40_::` and siblings) --
// the member NAME is cut off mid-token, so neither the function nor its signature is
// recovered, and type 1's helper has no recovered name at all. Guessing six cross-module
// entry points would be fabrication. The post offices themselves are correctly sized and
// staged (see the header), and BrnDirector::SceneQueryInterface::Clear -- which resets them
// each frame -- is already real, so nothing goes stale while this is gated.
//
// CONSEQUENCE WHILE GATED: results the SceneManager returns are not delivered, so any
// director query (camera collision line tests, the visibility/geometry predictors) reports
// "no result" rather than a wrong one. The director's camera path does not depend on a
// query answer to produce a camera -- it degrades, it does not break -- and no query is
// issued at all until the MainDirector middle is un-gated.
//
// DELETE-WHEN: delete this gate and transcribe the 6-way switch once the six
// SceneManagerIO OutEvent*Result post-office delivery functions have recovered names +
// signatures (headless IDA 9.3 on artist_copy.i64 will de-truncate them; they are members
// of the OutEvent*Result queue templates already homed under
// GameShared/GameClasses/SceneManager/).
// ----------------------------------------------------------------------------
void DirectorModule::ProcessSceneQueryResults(
        const DirectorIO::SceneQueryInputBuffer* lpSceneQueryInputBuffer)
{
    (void)lpSceneQueryInputBuffer;
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
        mbReplayStartedThisFrame = 1;       // this+231716
    }
    mbIsReplaying = lbIsReplaying;          // this+232192

    // ---- 2. ⚠️ QUIET GATE: the director-serialiser mode bookkeeping ------------------
    // The X360 then reads the director serialiser's mode word and stamps two of its flags:
    //     if ( mDirectorSerialiser.GetMode() == E_MODE_RESTORING /*7*/ )
    //     {
    //         mbReplayRestoring                  = true;   // this+232193
    //         mDirectorSerialiser.mbDataRestored = true;   // this+231817 == serialiser +0x59
    //     }
    //     else if ( mode == E_MODE_RECORDING_PREPARING /*1*/ || mode == E_MODE_PLAYING_PREPARING /*4*/ )
    //     {
    //         mDirectorSerialiser.mbDataReady    = true;   // this+231816 == serialiser +0x58
    //     }
    // (The two byte offsets land EXACTLY on BrnReplays::BaseSerialiser's committed
    //  mbDataReady @+0x58 / mbDataRestored @+0x59 -- that coincidence is the attestation
    //  that mDirectorSerialiser is at this+0x38930 and that these are its fields.)
    //
    // GATED because both fields are `protected` on BrnReplays::BaseSerialiser and there is no
    // public setter; adding one means editing GameSource/Replays/BrnReplayBaseSerialiser.h,
    // which is outside this wave's ownership. The exact additive change is written into the
    // wave log for the conductor. Reaching around the class to poke +0x58/+0x59 by offset is
    // precisely the raw-offset-into-another-class's-storage pattern this TU was rewritten to
    // remove, so it is not done here.
    //
    // CONSEQUENCE WHILE GATED: only the replay-record/restore handshake is affected, and the
    // whole replay leg is gated anyway (step 4). The live camera path is untouched.
    //
    // DELETE-WHEN: delete this gate once BrnReplays::BaseSerialiser exposes
    // SetDataReady(bool) / SetDataRestored(bool) (or DirectorSerialiser is homed).

    // ---- 3. the per-frame scene-query post office ----------------------------------
    // This pass populates ALL SIX post offices: it is the one that may MINT query ids.
    SceneQueryInterface lSceneQuery;
    lSceneQuery.mpSceneQueryInterface          = lpProducerOf(lpSceneQueryOutputBuffer);
    lSceneQuery.mpPostOffice04                 = mSceneQueryPostBoxA;                 // +0x9A4
    lSceneQuery.mpPostOffice08                 = mPostBoxLineTestNearest;             // +0x9D0
    lSceneQuery.mpPostOffice0C                 = mPostBoxLineTestFastDoubleSided;     // +0xA74
    lSceneQuery.mpPostOffice10                 = mPostBoxSphereTestFast;              // +0xAA0
    lSceneQuery.mpPostOffice14                 = mPostBoxVolumeTestFine;              // +0xACC
    lSceneQuery.mpVolumeTestDeepestPostOffice  = mPostBoxVolumeTestDeepest;           // +0xAD4
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
        // ⚠️ QUIET GATE -- the REPLAY leg. The X360 runs, when the serialiser mode is
        // E_MODE_PLAYING_PREPARING (4), E_MODE_PLAYING (5) or E_MODE_PLAYING_STALLED (6):
        //     mDirectorSerialiser.mbDataReady = true;
        //     BrnReplays::DirectorSerialiser::Read(&mDirectorSerialiser);            // @0x82650340
        //     ReplayDirector::PreSceneQueryUpdate(                                   // @0x8225BD28
        //         this + 221008 /* +0x35F50 */,
        //         BrnReplays::DirectorSerialiser::GetStaticLayout(&mDirectorSerialiser), // @0x821F5C58
        //         &lIO );
        // GATED because BrnReplays::DirectorSerialiser is un-homed (only the base
        // BaseSerialiser is committed -- GetStaticLayout/Read/Write are the derived type's)
        // and BrnDirector::ReplayDirector::PreSceneQueryUpdate is itself declaration-only
        // (BrnReplayDirector.h documents why: an unmodelled layout plus a VMX pipeline).
        // CONSEQUENCE: replay playback drives no camera. Free drive / all live gameplay is
        // unaffected -- mbIsReplaying is false for every one of those frames.
        // DELETE-WHEN: BrnReplays::DirectorSerialiser gets a home AND
        // ReplayDirector::PreSceneQueryUpdate is bodied.
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
//   3. run MainDirector::Update (live) or ReplayDirector::Update (replay -- GATED). BOTH
//      legs publish their camera into the output buffer themselves (SetCameraOutput +
//      SetCgsCamera are called from exactly those two functions and nowhere else);
//   4. copy whichever director ran into the module's own published graphics camera
//      (mCgsCamera = <that director's CgsGraphics::Camera>);
//   5. the replay RECORD leg + serialiser registration (GATED).
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

    // ---- 1. deliver last frame's scene-query answers -------------------------------
    ProcessSceneQueryResults(lpSceneQueryInputBuffer);

    // ---- 2. the post office, PRODUCER SLOT ONLY ------------------------------------
    // asm: v29[0] = SceneQueryOutputB(a7); memset(&v29[1], 0, 24);  -- the six post-office
    // slots are explicitly zeroed on this pass (unlike PreSceneQueryUpdate), so the
    // subsequent Clear() is a no-op. Reproduced verbatim: this pass consumes answers, it
    // does not issue queries.
    SceneQueryInterface lSceneQuery;
    lSceneQuery.mpSceneQueryInterface         = lpProducerOf(lpSceneQueryOutputBuffer);
    lSceneQuery.mpPostOffice04                = 0;
    lSceneQuery.mpPostOffice08                = 0;
    lSceneQuery.mpPostOffice0C                = 0;
    lSceneQuery.mpPostOffice10                = 0;
    lSceneQuery.mpPostOffice14                = 0;
    lSceneQuery.mpVolumeTestDeepestPostOffice = 0;
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
        // ⚠️ QUIET GATE -- the REPLAY leg. The X360 runs, for serialiser modes 4/5/6:
        //     ReplayDirector::Update( this + 221008,                                  // @0x8225C298
        //                             DirectorSerialiser::GetStaticLayout(&mDirectorSerialiser),
        //                             &lIO );
        //     mCgsCamera = *(CgsGraphics::Camera*)(this + 221712);   // ReplayDirector +0x240
        // Same blockers as the PreSceneQueryUpdate replay gate (un-homed DirectorSerialiser;
        // ReplayDirector::Update declaration-only). Note the module's graphics camera is left
        // UNCHANGED here rather than being fed a fabricated one -- so a replay frame simply
        // re-presents the previous camera instead of a wrong one.
        // DELETE-WHEN: as the PreSceneQueryUpdate replay gate.
    }

    // ---- 5. ⚠️ QUIET GATE: the replay RECORD leg + serialiser registration ----------
    // For serialiser modes E_MODE_RECORDING_PREPARING (1) / E_MODE_RECORDING (2) /
    // E_MODE_RECORDING_STALLED (3) the X360 snapshots the frame into the serialiser's static
    // layout and writes the record:
    //     Camera::Camera::Construct( DirectorSerialiser::GetStaticLayout(&mDirectorSerialiser) );
    //     Camera::Camera::operator=( GetStaticLayout(...), this + 211472 );
    //     *(u64*)(GetStaticLayout(...) + 352) = *(u64*)(this + 204736);
    //     *(u32*)(GetStaticLayout(...) + 360) = *(u32*)(this + 208508);   // + 364/368 from
    //     *(u32*)(GetStaticLayout(...) + 372) = *(f32*)(this + 211408);   //   208512/208516
    //     DirectorSerialiser::Write( &mDirectorSerialiser );              // @0x82650438
    // and then, UNCONDITIONALLY:
    //     ReplayIO::RequestInterface::RegisterSerialiser(                 // @0x821F34A0
    //         lpOutputBuffer->GetReplayRequestI(), &mDirectorSerialiser );
    // and finally a dev tripwire:
    //     CGS_ASSERT( !lpSceneQueryOutputBuffer->GetSceneQueryInterface()->HasData(),
    //                 "!lpSceneQueryOutput->GetSceneQueryInterface()->HasData()" );  // :387
    //
    // GATED for three separate reasons, none of them fixable inside this wave:
    //   * DirectorSerialiser::GetStaticLayout / ::Write are the UN-HOMED derived serialiser's;
    //   * every source offset above (this+204736 / +208508.. / +211408 / +211472) lands inside
    //     the un-modelled +0x36380..+0x38920 tail (CameraDebugInfo / GameState / VehicleTracker),
    //     so there is no named member to read them through;
    //   * CgsSceneManager::SceneManagerIO::SceneQueryInterface::HasData @0x82204E48 has no
    //     declaration on the committed SceneManager header (its own header says the body lives
    //     in CgsSceneManagerModuleIO.cpp), and that header is outside this wave's ownership.
    // CONSEQUENCE: no replay recording, and the director's serialiser is not registered with
    // the replay subsystem. Neither affects the live camera. The dropped assert is a dev
    // tripwire only.
    // DELETE-WHEN: DirectorSerialiser is homed, the +0x36380 tail members are recovered, and
    // SceneQueryInterface::HasData is declared on the SceneManager header.

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
        lSceneQuery.mpSceneQueryInterface         = 0;   // even slot 0 -- see the note above
        lSceneQuery.mpPostOffice04                = 0;
        lSceneQuery.mpPostOffice08                = 0;
        lSceneQuery.mpPostOffice0C                = 0;
        lSceneQuery.mpPostOffice10                = 0;
        lSceneQuery.mpPostOffice14                = 0;
        lSceneQuery.mpVolumeTestDeepestPostOffice = 0;
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
            miPostGuiCarIndexLatch = liCameraType;
    }

    lpOutputBuffer->UnlockForWrite();
    lpInputBuffer->UnlockForRead();

    CgsDev::PerfMonCpu::StopMonitor(miPerfCount_PostGuiUpdate);
    return 0;   // see the return-value FLAG on PreSceneQueryUpdate
}

}
