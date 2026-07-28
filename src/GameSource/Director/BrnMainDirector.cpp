// ============================================================================
// GameSource/Director/BrnMainDirector.cpp
//
// BrnDirector::MainDirector -- the top-level cinematic camera director. Compilation
// home for the 21-function MainDirector class TU. The ENGINE source path the X360
// asserts quote is "..\..\..\GameSource\Director/BrnMainDirector.cpp", so this file
// mirrors it.
//
// ⚠️ LAYOUT: this TU was rewritten by the BehaviourManager wave together with its header.
// MainDirector is NO LONGER a console-sized opaque buffer addressed at CONSOLE byte offsets
// through a char* view -- that model could not host x64-width sub-objects (see the header's
// LAYOUT MODEL banner) and is what forced the previous wave to hold ICEWrapper::Construct
// and Arbitrator::Construct back. Every field is now a NAMED member; the `+0xNNNNN` console
// offsets quoted throughout are PROVENANCE for the member's identity, never an index.
//
// BODIED here (faithfully reconstructed from the X360 asm):
//   * MainDirector()  -- ctor
//   * Construct()     -- the runtime build, incl. the BehaviourManager + Arbitrator that the
//                        previous wave had to hold back
//   * Prepare()       -- the staged PREPARE machine, incl. the BehaviourManager stage
//   * Release()       -- the staged RELEASE machine
//   * Destruct()      -- the four pool resets + the collision-generator / ICE teardown
//   * GetLivePlayerCarIndex() -- the shared player-car predicate both entry points inline
//   * PreSceneQueryUpdate()   -- the whole-body guard (its guarded steps stay gated)
//   * Update()        -- the prologue, the no-player path, the arbitrator leg of the
//                        gameplay middle, and the whole publish tail
//   * UpdateArbitrator() + BuildArbStateSharedInfo() -- the per-frame arbitrator context
//
// DECLARATION-ONLY + FLAGGED (in the header): UpdateICE / UpdateMoments / UpdateAttribSys /
// UpdateCameraBehaviours* / UpdateDebug* / ProcessInputQueue / ProcessNewVehicleEvents /
// HandlePrepareForModeAction / CalcTrafficLightSpace / DebugDisplayCurrentCamera /
// PostGuiUpdate. Each indexes a NOT-HOMED aggregate, paraphrases a VMX pipeline, or depends
// on un-dumped rodata.
// ============================================================================

#include "GameSource/Director/BrnMainDirector.h"

#include "GameSource/Director/DirectorModule/BrnDirectorInputOutput.h" // BrnDirector::DirectorInputOutput
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"    // DirectorIO::InputBuffer
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp" // DirectorIO::OutputBuffer
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h" // BrnDirector::DebugComponent
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h" // ArbStateSharedInfo
#include "GameSource/Director/BrnDirectorResourceManager.h"            // BrnDirector::DirectorResourceManager
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatus
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT

// FLAG: CgsSceneManager::CgsCollision::BaseCollisionGenerator has no reconstructed home
//   layout yet (the committed CgsSceneManagerModule.h forward-declares it only). Destruct /
//   Release / Prepare call its Destruct() on the embedded CgsGraphics::Camera / collision-
//   generator object; declared here as a minimal external with just that member so the call
//   compiles. Replace with the real home when the collision-generator TU lands.
namespace CgsSceneManager { namespace CgsCollision {
    struct BaseCollisionGenerator { void Destruct(); };
} }

namespace BrnDirector
{
    namespace
    {
        // The collision-generator view of an embedded aggregate. Takes a NAMED member's
        // address (never an offset into this class's storage) -- the X360 tears the embedded
        // CgsGraphics::Camera down through BaseCollisionGenerator::Destruct because on this
        // build they are one object.
        inline CgsSceneManager::CgsCollision::BaseCollisionGenerator* lpAsCollisionGenerator(void* lpObject)
        {
            return reinterpret_cast<CgsSceneManager::CgsCollision::BaseCollisionGenerator*>(lpObject);
        }
    }

    // ------------------------------------------------------------------------
    // MainDirector (ctor)  @ X360 0x827E4AB8  (EXECUTED in goal trace)
    //
    // The X360 sequence is: construct the owned sub-objects in place (ICEWrapper @+0x50,
    // ICETake @+0x124F0, ArbitratorStateContainer @+0x130D0 == arbitrator +0x310,
    // BehaviourManager @+0x1CB10, CarScoreData @+0x33B50), then seed the -1 sentinel index
    // fields (+0x122B8, +0x12384, +0x12450, +0x12DB4, +0x34974, +0x34DC0, a 16-entry
    // stride-0x2C table from +0x34DF8, +0x35090, +0x353A8).
    //
    // With the sub-objects now declared as NAMED MEMBERS, every one of those in-place builds
    // is performed by the members' own constructors -- the placement-new list the previous
    // (opaque-buffer) model needed is gone, and with it the host-size overrun risk.
    //
    // ⚠️ THE -1 SENTINEL SEEDS ARE A DOCUMENTED QUIET GATE. Each one lands inside a region
    // whose type is still un-homed (the ICE-wrapper tail / the shot-selector block / the
    // arbitrator-region head / the per-rival score table inside maModeActionAndDebugBlock),
    // so there is no named field to write. Under the previous model they were raw stores into
    // a console-addressed buffer; writing them now would mean re-introducing exactly the
    // offset arithmetic this rewrite removed, and into regions whose host contents differ.
    // CONSEQUENCE: those index fields start at 0 rather than -1. Every one is consumed by an
    // un-homed aggregate that is itself gated, so nothing reconstructed reads them today.
    // DELETE-WHEN: each owning type is homed -- then the sentinel becomes that type's own
    // constructor's business, which is where the console puts it too (the compiler inlined
    // the sub-object ctors into this one).
    // ------------------------------------------------------------------------
    MainDirector::MainDirector()
        : mpDebugComponent(0)
        , miMomentBucketFreeCount(0)
        , muMomentBucketOccupancy(0)
        , miForcedCameraCarIndex(-1)
        , miStageCounter(0)
        , miStage(0)
        , mfConstructTime(0.0)
    {
        // Owned sub-objects: mICEWrapper / mCameraFinaliser / mArbitrator /
        // mBehaviourManager / mLastCamera / mCgsCamera run their own constructors.
        // ⚠️ GATE: the -1 sentinel seeds (see the banner).
    }

    // ------------------------------------------------------------------------
    // Construct  @ 0x8225B448   (EXECUTED in goal trace)
    //
    // The X360 call/store sequence, in order:
    //
    //     mStage         = 5;                                   // +0x35424
    //     mfConstructTime = lfTime;                             // +0x35428 (double)
    //     mStageCounter  = 0;                                   // +0x35420
    //     DirectorDevTools::Construct( this, this, lpResourceManager );
    //     <CgsGraphics::Camera ctor>( this + 0x349D0 );          // sub_827F94E8
    //     CgsGraphics::Camera::SetFovHorizontal( this + 0x349D0, ... );
    //     CgsGraphics::Camera::UpdatePerspectiveProjectionMatrix( this + 0x349D0 );
    //     Camera::Camera::Construct( &mLastCamera );             // +0x32F10
    //     AllVehicleData::Construct( this + 0x12C80 );
    //     <five flag bytes cleared around +0x34974 .. +0x349C9>
    //     Camera::BehaviourManager::Construct( &mBehaviourManager );          // +0x1CB10
    //     CGS_ASSERT( lpResourceManager != NULL );               // BrnBehaviourManager.h:168
    //     *(this + 208588) = lpResourceManager;                  // == manager +91068
    //     <the moment-bucket pool occupancy word (+0x1CAB8) cleared>
    //     MomentParameterBank::Construct( this + 0x1CAC0 );
    //     Arbitrator::Construct( &mArbitrator );                 // +0x12DC0
    //     <the VMX + 1284865837-multiplier LCG camera-shake seed pipeline into +0x32EE0..>
    //     KeyAnimShakeController::Construct( this + 0x124D0, lpResourceManager );
    //     ShotSelector::Construct( this + 0x121F0, lpResourceManager );
    //     ICEWrapper::Construct( &mICEWrapper );                 // +0x50
    //     GameState::Clear( this + 0x337E0 );
    //     DebugPrinter::Construct( this + 0x337B0 / +0x33768 / +0x3378C );
    //     <the flag/latch tail seeds +0x3542F .. +0x3543F, and +0x33100 = -1>
    //
    // ⭐ `*(this + 208588) = lpResourceManager` IS NOT A MainDirector FIELD. 117520 + 91068 ==
    // 208588: it is `mBehaviourManager.mpDirectorResourceManager`, which is exactly why the
    // guard immediately before it quotes **BrnBehaviourManager.h:168**. The previous wave had
    // it as an un-named raw store; it is now the named setter.
    //
    // ⭐ THE TWO HELD-BACK BUILDS ARE NOW REAL. `Camera::BehaviourManager::Construct` and
    // `Arbitrator::Construct` were held back by the previous wave for a HOST-SIZE reason (the
    // host types are wider than their console placement windows inside the old opaque
    // buffer). Both are named members now, so there is no window to overrun and both run.
    // `ICEWrapper::Construct` runs for the same reason.
    //
    // ⚠️ STILL DOCUMENTED QUIET GATES -- every one is an un-homed aggregate, not a size issue:
    //   * DirectorDevTools::Construct -- un-homed (maDirectorDevTools). CONSEQUENCE: the
    //     GameTalk "Camera" dev-tools commands have no handler state. Nothing in the game path
    //     uses them.
    //   * AllVehicleData::Construct + the five flag bytes -- un-homed (maAllVehicleData).
    //     CONSEQUENCE: the per-frame vehicle tracker starts zeroed; the arbitrator states that
    //     read it are the gameplay ones, not the attract/flyby path.
    //   * MomentParameterBank::Construct -- un-homed (maMomentParameterBank). CONSEQUENCE: no
    //     moment can be parameterised; UpdateMoments is itself declaration-only.
    //   * the camera-shake LCG seed pipeline (+0x32EE0..) -- a multi-stage VMX + 1284865837
    //     multiplier LCG the reconstruction rules forbid paraphrasing to scalar, writing into
    //     the un-homed maRandom region. CONSEQUENCE: the shake RNG table starts zeroed.
    //   * KeyAnimShakeController::Construct / ShotSelector::Construct -- un-homed.
    //   * GameState::Clear + the three DebugPrinter::Constructs + the flag/latch tail seeds --
    //     un-homed regions (maGameState / maDebugPrinter* / maStateFlagTail).
    //     ⚠️ CONSEQUENCE WORTH KNOWING: the GameState block is what tells the arbitrator to
    //     enter attract mode, and DebugPrinter is what the arbitrator states print through.
    //     Both are handed to ArbStateSharedInfo as zeroed storage today.
    //   DELETE-WHEN: per sub-object, as each type is homed -- then swap that member's opaque
    //   span in the header for the real type and un-gate its line here.
    //
    // The SetFovHorizontal call the asm makes right after the camera ctor is deliberately not
    // reproduced: its FOV argument is an UNINITIALISED fp register in the decompilation (a
    // Hex-Rays artefact of the PPC fp calling convention), i.e. NOT recovered, and
    // CgsGraphics::Camera::Construct already seeds the default FOV through the same setter.
    // ------------------------------------------------------------------------
    void MainDirector::Construct(const DirectorResourceManager* lpResourceManager, f32 lfTime)
    {
        miStage         = 5;
        mfConstructTime = static_cast<f64>(lfTime);
        miStageCounter  = 0;

        // ⚠️ GATE: DirectorDevTools::Construct( this, this, lpResourceManager );

        // The published graphics camera (+0x349D0).
        mCgsCamera.Construct();
        mCgsCamera.UpdatePerspectiveProjectionMatrix();

        // mLastCamera (+0x32F10) -- the carried-over frame camera Update reads and writes.
        // Without this the director would publish RAW UNINITIALISED STORAGE on frame 1 and
        // ValidateTransformWithDebugInfo would assert on the NaNs.
        mLastCamera.Construct();

        // ⚠️ GATE: AllVehicleData::Construct( maAllVehicleData ) + the five flag bytes.

        // ⭐ REAL (was held back for host size): the camera-behaviour manager.
        mBehaviourManager.Construct();

        CGS_ASSERT(lpResourceManager != 0, "lpDirectorResourceManager != NULL");
        mBehaviourManager.SetDirectorResourceManager(lpResourceManager);

        // The moment-bucket pool's occupancy word (+0x1CAB8), cleared before the bank build.
        muMomentBucketOccupancy = 0;

        // ⚠️ GATE: MomentParameterBank::Construct( maMomentParameterBank );

        // ⭐ REAL (was held back for host size): the camera arbitrator -- and with it the
        // 11-state container, the shared-camera container and the three special-cam states
        // (crash-nav / ATTRACT MODE / render-metrics).
        mArbitrator.Construct();

        // ⚠️ GATE: the VMX/LCG camera-shake seed pipeline into maRandom.
        // ⚠️ GATE: KeyAnimShakeController::Construct / ShotSelector::Construct.

        // ⭐ REAL (was held back for host size): the ICE wrapper.
        mICEWrapper.Construct();

        // ⚠️ GATE: GameState::Clear / the three DebugPrinter::Constructs.

        // The forced-camera-car override starts at the -1 "none" sentinel (+0x33100).
        miForcedCameraCarIndex = -1;

        // ⚠️ GATE: the flag/latch tail seeds (+0x3542F..+0x3543F) into maStateFlagTail.
    }

    // ------------------------------------------------------------------------
    // Prepare  @ 0x8224FB38
    //
    // The staged PREPARE state machine DirectorModule::Prepare pumps at its own stage 4. The
    // stage word selects the entry case; the cases fall through, so one call advances as far
    // as it can:
    //
    //   0 -> tear the collision generator down, then register the dev-tools GameTalk message
    //        handler under the name "Camera", and zero the dev-tools head word
    //   1 -> zero the frame counter (+0x121A0-ish, inside the shot block)
    //   2 -> ICEWrapper::Prepare
    //   4 -> Camera::BehaviourManager::Prepare
    //   5 -> seed the moment-bucket pool free queue (19..0 descending) + count 20, and clear
    //        its occupancy word (+0x1CAB8)
    //   6 -> (empty)
    //   7 -> done: clear the stage counter, report success
    // (there is deliberately NO case 3 in the X360 jump table -- reproduced.)
    //
    // Any sub-Prepare returning false reports false without advancing; the framework retries.
    // An out-of-range stage asserts (BrnMainDirector.cpp:224).
    //
    // ⭐ STAGE 4 IS NOW REAL. It was the wave-1 "THE blocker" gate on the (mistaken) grounds
    // that BehaviourManager had no layout; the layout has been committed since d5612215 and
    // BehaviourManager::Prepare @0x8223DBE0 is now bodied, so the three behaviour pools get
    // their free queues and the manager is genuinely ready to allocate behaviours.
    //
    // ⭐ STAGE 5 IS NOW REAL TOO, and it is NOT a behaviour-manager table. +117344/+117424/
    // +117432 sit BEFORE MomentParameterBank (+117440) and nowhere near the manager (+117520):
    // the 20-entry descending seed + count + head is a pool free-queue refill, i.e. the
    // moment-bucket pool's. Written through this class's three named fields.
    //
    // ⚠️ ONE DOCUMENTED QUIET GATE REMAINS: stage 0's GameTalk registration --
    //   EA::GameTalk::GameTalkManager has no reconstructed home. CONSEQUENCE: the "Camera"
    //   dev-tools commands (Start/StopRenderMetrics, the ICE editor hooks) are not reachable
    //   from GameTalk. Nothing in the game path uses them.
    //   DELETE-WHEN: the GameTalk manager is homed.
    // ------------------------------------------------------------------------
    bool MainDirector::Prepare(DirectorIO::OutputBuffer* lpOutputBuffer, s32 liPrepareArg,
                               const DirectorResourceManager* lpResourceManager)
    {
        switch (miStage)
        {
        case 0:
            lpAsCollisionGenerator(&mCgsCamera)->Destruct();
            // ⚠️ GATE: EA::GameTalk::GameTalkManager::GetInstance()->RegisterMessageHandler(
            //              BrnDirector::DirectorDevTools::GameTalkMsgHandler, "Camera" );
            //   -- un-homed GameTalk API (see the banner). The `*this = 0` dev-tools head
            //   store that follows it belongs to the same un-homed object.
            // fall through
        case 1:
            miStage = 1;
            // ⚠️ GATE: the frame counter inside maShotAndAnalysisBlock (console +0x121A0).
            // fall through
        case 2:
            miStage = 2;
            // The wrapper pumps its own ICE resource acquisition through the director output
            // buffer, forwarding the prepare arg and the module's resource manager.
            if (!mICEWrapper.Prepare(lpOutputBuffer, liPrepareArg, lpResourceManager))
                return false;
            // fall through
        case 4:
            miStage = 4;
            if (!mBehaviourManager.Prepare())
                return false;
            // fall through
        case 5:
        {
            miStage = 5;
            muMomentBucketOccupancy = 0;
            for (s32 liSlot = 19; liSlot >= 0; --liSlot)
            {
                maMomentBucketFreeQueue[19 - liSlot] = liSlot;
            }
            miMomentBucketFreeCount = 20;
            // fall through
        }
        case 6:
            miStage = 6;
            // fall through
        case 7:
            miStage = 7;
            miStageCounter = 0;
            return true;

        default:
            CGS_ASSERT(false, "Invalid Stage\n");
            return false;
        }
    }

    // ------------------------------------------------------------------------
    // Release  @ X360 0x82236EB0
    //
    // The staged RELEASE state machine. The stage word selects the case; each case advances
    // it and the cases fall through (1->2->3->5). On the first (stage 0) it destructs the
    // object at the ICE-wrapper region; on the last (stage 5) it clears the stage counter and
    // reports completion. An out-of-range stage asserts and reports failure.
    // (The asm "case 4" is the default branch -- there is no case 4 in the jump table.)
    // ------------------------------------------------------------------------
    bool MainDirector::Release()
    {
        switch (miStage)
        {
        case 0:
            // asm 0x82236F0C `addi r3,r30,0x50` -> BaseCollisionGenerator::Destruct(this+0x50),
            // i.e. the ICE-wrapper region. (Destruct() below targets the +0x349D0 graphics
            // camera instead; these are different call sites and must not be conflated.)
            lpAsCollisionGenerator(&mICEWrapper)->Destruct();
            // fall through
        case 1:
            miStage = 1;
            // fall through
        case 2:
            miStage = 2;
            // fall through
        case 3:
            miStage = 3;
            // fall through
        case 5:
            miStage = 5;
            miStageCounter = 0;
            return true;

        default:
            CGS_ASSERT(false, "Invalid Stage\n");
            return false;
        }
    }

    // ------------------------------------------------------------------------
    // Destruct  @ X360 0x8224FCC0
    //
    // The X360 body stores a 64-bit zero into four words, destructs the embedded
    // CgsGraphics::Camera / collision generator, then destructs the embedded ICE wrapper.
    //
    // ⭐ THE FOUR WORDS ARE POOL OCCUPANCY WORDS, not "list heads" as the previous wave
    // recorded them. Cross-checked against BehaviourManager::Prepare @0x8223DBE0's per-pool
    // free-queue layout:
    //     +0x1CAB8 (117432)  the moment-bucket pool's occupancy word
    //     +0x24848 (149576) == manager +32056  mLargeBehaviourPool's occupancy
    //     +0x2C5B8 (181688) == manager +64168  mSmallBehaviourPool's occupancy
    //     +0x2F038 (192568) == manager +75048  mBehaviourHelperPool's occupancy
    // So Destruct is "reset the four pools", and all four are now named operations.
    // ------------------------------------------------------------------------
    void MainDirector::Destruct()
    {
        muMomentBucketOccupancy = 0;      // the moment-bucket pool
        mBehaviourManager.Destruct();     // the manager's three pools

        lpAsCollisionGenerator(&mCgsCamera)->Destruct();
        mICEWrapper.Destruct();
    }

    // ------------------------------------------------------------------------
    // GetLivePlayerCarIndex -- the shared "which player car is live this frame" resolution.
    //
    // Both Update @0x82274070 and PreSceneQueryUpdate @0x8225BA00 open with the SAME sequence
    // (the X360 emits it twice, inlined):
    //
    //     index  = lpInputBuffer->GetPlayerCarIndex();
    //     forced = miForcedCameraCarIndex;                       // +0x33100
    //     if ( forced > -1 && lpInputBuffer->GetUsedRaceCars()->IsBitSet(forced) )
    //         index = forced;                                    // the override wins
    //     if ( index == -1 )  return -1;
    //     return lpInputBuffer->GetUsedRaceCars()->IsBitSet(index) ? index : -1;
    //
    // (the CgsBitArray.h:203 "invalid index : N < 8" assert both call sites bake is the
    //  BitArray bounds check inlined -- CGS_ASSERT carries it here.)
    //
    // ⚠️ NOTE ON THE BIT TEST. The X360 emits it as
    //     ((1 << (index & 0x3F)) & ((1 << (index & 0x3F)) >> 32)) != 0
    // which is a Hex-Rays artefact of the 64-bit `rldicl`/`and` pair the PPC uses to test one
    // bit of the 64-bit BitArray<8> word -- NOT a literal shift-by-32 of a shifted 1.
    // Reproduced as the committed BitArray query it actually is, which is what the DWARF
    // member type (CgsContainers::BitArray<8u> mUsedRaceCars) says it must be.
    //
    // The X360 keeps this index and threads it into UpdateCameraBehavioursPostScene /
    // UpdateMoments / UpdateICE / UpdateArbitrator, so the de-inlined helper returns it.
    // ------------------------------------------------------------------------
    s32 MainDirector::GetLivePlayerCarIndex(const DirectorInputOutput* lpIO) const
    {
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;

        s32 liIndex = static_cast<s32>(lpInput->GetPlayerCarIndex());

        const CgsContainers::BitArray<8u>* lpUsedRaceCars = lpInput->GetUsedRaceCars();

        if (miForcedCameraCarIndex > -1)
        {
            CGS_ASSERT(miForcedCameraCarIndex < 8, "invalid index");
            if (miForcedCameraCarIndex < 8 &&
                lpUsedRaceCars->IsBitSet(static_cast<u32>(miForcedCameraCarIndex)))
            {
                liIndex = miForcedCameraCarIndex;
            }
        }

        if (liIndex == -1)
            return -1;

        CGS_ASSERT(liIndex < 8, "invalid index");
        if (liIndex < 8 && lpUsedRaceCars->IsBitSet(static_cast<u32>(liIndex)))
            return liIndex;

        return -1;
    }

    // ------------------------------------------------------------------------
    // BuildArbStateSharedInfo -- the per-frame arbitrator context.
    //
    // De-inlined out of UpdateArbitrator @0x82271120, which builds this record directly onto
    // the stack frame it then hands to Arbitrator::Update. Every slot is asm-attested; the
    // console offsets in the trailing comments are the X360 stack offsets of the record
    // (sp+0x50 is the record base), matched against the committed ArbStateSharedInfo layout
    // recovered from the DecFIGS DWARF.
    //
    // TWO SLOTS ARE DELIBERATELY LEFT NULL because the X360 leaves them for the callee:
    // Arbitrator::Update's own prologue writes `mpSharedCameraContainer` (+0x00) and
    // `mpStateContainer` (+0x14) before dispatching. Zeroing them here is what the console's
    // uninitialised stack slots amount to, and the callee overwrites both unconditionally.
    //
    // ⚠️ ONE SLOT IS A DOCUMENTED QUIET GATE: `mpNamedParameters` (+0x1C) comes from
    // MainDirector +192592 == mBehaviourManager +75072, i.e. 16 bytes INTO the manager's
    // `mBehaviourParameterBank`, which is a FLAGGED opaque sub-object with no homed layout.
    // Passing a pointer into un-modelled storage would be a fabrication, so it is null.
    // CONSEQUENCE: an arbitrator state that reads named camera parameters gets null. None of
    // the states on the attract/flyby path does (Arbitrator::Update never touches +0x1C).
    // DELETE-WHEN: BehaviourParameterBank is homed.
    //
    // ⚠️ THE UN-HOMED REGION POINTERS. mpDebugPrinter / mpDebugLog / mpMomentController /
    // mpGameState / mpRandom / mpEffectInterface / mpAllVehicleData / mpPlayerTracker and the
    // two rotation controllers are handed on as their declared pointer types over this
    // class's NAMED opaque regions. That is honest storage of the right size in the right
    // order -- not an offset poke -- and each becomes a plain `&mMember` when its type lands.
    // ------------------------------------------------------------------------
    void MainDirector::BuildArbStateSharedInfo(const DirectorInputOutput* lpIO,
                                               s32 liPlayerCarIndex,
                                               ArbStateSharedInfo& lrSharedInfo) const
    {
        const DirectorIO::InputBuffer* lpInput  = lpIO->mpInputBuffer;
        DirectorIO::OutputBuffer*      lpOutput = lpIO->mpOutputBuffer;

        // The two timesteps. X360: `timer[+8] * timer[+4]` and `timer[+32] * timer[+28]`.
        // The accessor hands back the CgsSystem::TimerStatusInterface, whose committed layout
        // is `TimerStatus mGameTimerStatus @+0; TimerStatus mSimTimerStatus @+24;` and whose
        // TimerStatus is `miFrameCount@+0, mfBaseTimeStep@+4, mfTimeStepMultiplier@+8`. So
        // +8*+4 IS the GAME timer's GetCurrentTimeStep() (the same reach the committed
        // CameraFinaliser::Update makes) and +32*+28 is the SIM timer's.
        const CgsSystem::TimerStatus* lpGameTimerStatus =
            reinterpret_cast<const CgsSystem::TimerStatus*>(lpInput->GetTimerStatusInterface());

        f32 lfTimestep = lpGameTimerStatus->GetCurrentTimeStep();

        // ⚠️ QUIET GATE: mfSimTimestep. The sim timer sits at TimerStatusInterface +24, and
        //   `CgsSystem::TimerStatusInterface::GetSimTimerStatus()` is DECLARATION-ONLY in the
        //   committed CgsTimerStatusInterface.h -- a GameShared header this wave does not own.
        //   Reaching +24 by hand would be a raw offset into a foreign type (bug class (c)), so
        //   the slot is published as 0 instead of a fabricated value.
        //   CONSEQUENCE: an arbitrator state that scales by the SIM timestep would see 0. None
        //   on the live path does -- Arbitrator::Update reads only mfTimestep (+0x5C), and no
        //   committed state reads +0x60.
        //   DELETE-WHEN: the one-line body `{ return &mSimTimerStatus; }` is given to
        //   TimerStatusInterface::GetSimTimerStatus (recipe in the wave log's PART 4), then
        //   this becomes `…GetSimTimerStatus()->GetCurrentTimeStep()`.
        f32 lfSimTimestep = 0.0f;

        // ⚠️ GATE: `if ( maStateFlagTail[+0x3543C] ) { lfTimestep = 0; lfSimTimestep = 0; }`
        //   -- the ICE-owns-the-frame latch lives in the un-homed flag tail. CONSEQUENCE: the
        //   arbitrator keeps advancing its timers during an ICE take instead of freezing them.
        //   DELETE-WHEN: the MainDirector flag tail is named.

        const BrnDirector::Camera::VehicleInfo* lpRaceCars = lpInput->GetRaceCarInfo();
        const BrnDirector::Camera::VehicleInfo* lpPlayerCar =
            (liPlayerCarIndex >= 0) ? (lpRaceCars + liPlayerCarIndex) : 0;

        lrSharedInfo.mpSharedCameraContainer = 0;                                   // +0x00 (callee-filled)
        lrSharedInfo.mpDebugPrinter          = reinterpret_cast<DebugPrinter*>(
                                                  const_cast<u8*>(maDebugPrinterMain));         // +0x04
        lrSharedInfo.mpDebugLog              = reinterpret_cast<DebugLog*>(
                                                  const_cast<u8*>(maDebugLog));                 // +0x08
        lrSharedInfo.mpICEWrapper            = const_cast<ICEWrapper*>(&mICEWrapper);           // +0x0C
        lrSharedInfo.mpOutputInterface       = reinterpret_cast<DirectorOutputInterface*>(
                                                  lpOutput->GetDirectorOutputIn());             // +0x10
        lrSharedInfo.mpStateContainer        = 0;                                   // +0x14 (callee-filled)
        lrSharedInfo.mpBehaviourManager      = const_cast<Camera::BehaviourManager*>(
                                                  &mBehaviourManager);                          // +0x18
        lrSharedInfo.mpNamedParameters       = 0;                                   // +0x1C ⚠️ GATE
        lrSharedInfo.mpMomentController      = reinterpret_cast<MomentController*>(
                                                  const_cast<u8*>(maMomentController));         // +0x20
        lrSharedInfo.mpGameState             = reinterpret_cast<GameState*>(
                                                  const_cast<u8*>(maGameState));                // +0x24
        lrSharedInfo.mpRandom                = reinterpret_cast<Random*>(
                                                  const_cast<u8*>(maRandom));                   // +0x28
        lrSharedInfo.mpDirectorResourceManager = lpIO->mpResourceManager;                       // +0x2C
        lrSharedInfo.mpEffectInterface       = reinterpret_cast<const EffectInterface*>(
                                                  maEffectInterface);                           // +0x30
        lrSharedInfo.mpPlayerCrashInfo       = reinterpret_cast<const PlayerCrashInfo*>(
                                                  lpInput->GetPlayerCrashInfo());               // +0x34
        lrSharedInfo.mpAllVehicleData        = reinterpret_cast<const AllVehicleData*>(
                                                  maAllVehicleData);                            // +0x38
        lrSharedInfo.mpPlayerTracker         = reinterpret_cast<const VehicleTracker*>(
                                                  maVehicleTracker);                            // +0x3C
        lrSharedInfo.mpControllerInfo        = reinterpret_cast<const ControllerInfo*>(
                                                  lpInput->GetControll());                      // +0x40
        lrSharedInfo.mpRaceCars              = reinterpret_cast<const VehicleInfo*>(lpRaceCars); // +0x44
        lrSharedInfo.mpPlayerCar             = reinterpret_cast<const VehicleInfo*>(lpPlayerCar);// +0x48
        // X360: mpPlayerCar + 496 -- RaceCarState::mTransform, reached by name.
        lrSharedInfo.mpPlayerCarTransform    = lpPlayerCar ? &lpPlayerCar->mRaceCarState.mTransform
                                                           : 0;                                 // +0x4C
        lrSharedInfo.mePlayerActiveRaceCarIndex = liPlayerCarIndex;                             // +0x50
        lrSharedInfo.mpRotationController    = reinterpret_cast<const Camera2DRotationController*>(
                                                  maRotationController);                        // +0x54
        lrSharedInfo.mpSphericalRotationController =
            reinterpret_cast<const CameraSphericalRotationController*>(
                maSphericalRotationController);                                                 // +0x58
        lrSharedInfo.mfTimestep              = lfTimestep;                                      // +0x5C
        lrSharedInfo.mfSimTimestep           = lfSimTimestep;                                   // +0x60
    }

    // ------------------------------------------------------------------------
    // UpdateArbitrator  @ 0x82271120
    //
    // Build this frame's ArbStateSharedInfo (above) and drive the arbitrator with it. The
    // X360 tail is:
    //
    //     Arbitrator::Update( this + 77248,                       // == &mArbitrator
    //                         lpInputBuffer[+0x7AC8],             // mbSimPaused
    //                         lrCameraInOut,                      // the frame camera
    //                         lSharedInfo,
    //                         controller[+3],                     // cycle-camera
    //                         controller[+2] );                   // cycle-camera-held
    //
    // The two control bytes are read straight off the committed ControlInput block the input
    // buffer publishes (GetControll()); the X360 fetches the block twice because it re-issues
    // the accessor, not because they come from different places.
    // ------------------------------------------------------------------------
    void MainDirector::UpdateArbitrator(const DirectorInputOutput* lpIO,
                                        Camera::Camera& lrCameraInOut,
                                        s32 liPlayerCarIndex)
    {
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;

        ArbStateSharedInfo lSharedInfo;
        BuildArbStateSharedInfo(lpIO, liPlayerCarIndex, lSharedInfo);

        const DirectorIO::ControlInput* lpControl = lpInput->GetControll();

        mArbitrator.Update(lpInput->IsSimPaused(), lrCameraInOut, lSharedInfo,
                           lpControl->IsCycleCameraPressed(),
                           lpControl->IsCycleCameraHeld());
    }

    // ------------------------------------------------------------------------
    // PreSceneQueryUpdate  @ 0x8225BA00
    //
    // The pre-scene-query pass. Its ENTIRE body is guarded by one condition, reproduced here
    // faithfully: the director only does pre-query work when there is a LIVE PLAYER CAR.
    //
    // Inside that guard the X360 runs, in order:
    //     AllVehicleData::Update( ..., usedRaceCars, vehicleInfoArray, playerIndex, contacts )
    //     MainDirector::ProcessInputQueue( this, lpIO )
    //     if ( !lpInputBuffer->IsSimPaused() )
    //     {
    //         miForcedCameraCarIndex = playerIndex;
    //         VehicleTracker::Update( maVehicleTracker, maGameState, input, playerIndex, ... )
    //     }
    //     CrashAnalyser::Update( maShotAndAnalysisBlock+..., input, maGameState, playerIndex )
    //     Camera::BehaviourManager::ReleaseBehaviours( &mBehaviourManager )
    //     if ( <flag> )  <clear a GameState latch>
    //     MainDirector::UpdateCameraBehavioursPreScene( this, lpIO, playerIndex )
    // and it also clears the "ICE just finished" latch at the very top.
    //
    // ⚠️ THE GUARDED BODY IS A DOCUMENTED QUIET GATE. AllVehicleData / VehicleTracker /
    // CrashAnalyser / GameState are named opaque regions with no interiors; ProcessInputQueue,
    // UpdateCameraBehavioursPreScene and BehaviourManager::ReleaseBehaviours are themselves
    // declaration-only (the last because its per-slot work drives the un-homed Behaviour
    // vtable).
    //
    // WHY BODY THE GUARD AT ALL: it is the part that decides whether ANYTHING happens, and on
    // the current PC bring-up it is FALSE every frame (no player vehicle, so the input
    // buffer's player-car index is the -1 sentinel and the used-race-car mask is empty). So
    // this reconstruction is behaviourally EXACT today and becomes partial, never wrong, the
    // moment a player car exists.
    //
    // DELETE-WHEN: AllVehicleData / VehicleTracker / CrashAnalyser / GameState are homed and
    // ProcessInputQueue + UpdateCameraBehavioursPreScene are bodied.
    // ------------------------------------------------------------------------
    void MainDirector::PreSceneQueryUpdate(const DirectorInputOutput* lpIO)
    {
        // ⚠️ GATE: the "ICE sequence just finished" latch clear (maStateFlagTail).

        if (GetLivePlayerCarIndex(lpIO) == -1)
            return;

        // ⚠️ GATE -- the seven guarded steps above.
    }

    // ------------------------------------------------------------------------
    // Update  @ 0x82274070   -- THE FUNCTION THAT PUBLISHES THE CAMERA
    //
    // Shape of the X360 body (935 lines of pseudocode; the structure is what matters):
    //
    //     Camera::Camera lCamera;  lCamera.Construct();          // line 191, a STACK camera
    //     <the live-player-car prologue>                          // lines 190-247
    //     if ( !live )   lCamera = mLastCamera;                    // LABEL_100, line 250
    //     else
    //     {
    //         UpdateDebugPrinters();                              // line 258
    //         <one debug byte -> the output buffer>               // line 259
    //         DebugLog::Print / DebugLog::Update                  // lines 260-265
    //         UpdateCameraBehavioursPostScene( lpIO, playerIdx );  // line 266
    //         UpdateMoments( lpIO, playerIdx );                    // line 267
    //         if ( !<ICE-owns-frame latch> ) UpdateICE( ... );     // lines 268-269
    //         ⭐ UpdateArbitrator( lpIO, lCamera, playerIdx );      // line 270
    //         <~550 lines of VMX AllVehicleData debug-render, the camera-interpolation
    //          controller, the effect-hook registration and the world-map safe-position
    //          work>                                              // lines 271-823
    //     }
    //     <two small bookkeeping stores>                          // lines 824-834
    //     CameraFinaliser::Update( &mCameraFinaliser, input, maGameState, resourceMgr,
    //                              &lCamera );                    // line 835
    //     <slomo clamp + assert>                                  // lines 836-848
    //     mLastCamera = lCamera;                                  // line 851  <-- carry over
    //     DebugComponent::UpdatePanoramaScreenshots( mpDebugComponent, &lCamera );  // line 866
    //     lCamera.ValidateTransformWithDebugInfo();               // line 867
    //     lCamera.CopyToCgsCamera( &mCgsCamera );                 // line 868
    //     output->SetCgsCamera( mCgsCamera );                     // line 869  <-- PUBLISH
    //     output->SetCameraOutput( lCamera );                     // line 870  <-- PUBLISH
    //     TimerRequests::SetTimestepMultiplier( ... );            // lines 871-875
    //     UpdateDebugInfo / DebugDisplayCurrentCamera /
    //     BehaviourManager::PrepareBehaviours / UpdateAttribSys   // lines 876-879
    //     <debug flag printers + latches>                         // lines 880-924
    //
    // ⭐ THE ARBITRATOR LEG OF THE MIDDLE IS NOW REAL. `UpdateArbitrator` at line 270 is the
    // single call that lets a director camera MOVE: it runs the arbitrator's outer state
    // machine, which runs whichever arbitrator state owns the frame (including
    // ArbStateAttractMode -- the DJ fly-by's path) and copies that state's camera into the
    // frame camera. The previous wave gated the whole middle on the belief that
    // BehaviourManager had no layout; it does, so the leg is transcribed.
    //
    // ⚠️ WHAT IS STILL GATED INSIDE THE MIDDLE (each a documented quiet gate, none of which
    // touches the camera the arbitrator just produced):
    //   * UpdateDebugPrinters / DebugLog::Print / DebugLog::Update -- DebugPrinter and
    //     DebugLog are un-homed named regions.
    //   * UpdateCameraBehavioursPostScene / UpdateMoments / UpdateICE -- all three are
    //     declaration-only (BehaviourManager::UpdateAllBehaviours @0x82251960 is a VMX
    //     attitude-band pipeline the rules forbid paraphrasing; the moment controller and the
    //     ICE take are un-homed).
    //     ⚠️ ORDERING NOTE: the console runs those three BEFORE UpdateArbitrator, so the
    //     arbitrator sees last frame's behaviour output rather than this frame's. That is a
    //     one-frame staleness in the behaviour-driven camera, not a wrong camera.
    //   * lines 271-823 -- ~550 lines of VMX AllVehicleData debug-render work, the camera
    //     interpolation controller, the effect-hook registration cascade and the world-map
    //     safe-position search. All reach un-homed aggregates and/or VMX pipelines.
    //   * the tail after the publish (lines 871-924): the requested time-step multiplier, the
    //     debug-info/overlay passes, BehaviourManager::PrepareBehaviours and UpdateAttribSys.
    //     None of them alters the published camera -- the two publish calls are already done.
    //
    // DELETE-WHEN: per item above, as each aggregate is homed.
    // ------------------------------------------------------------------------
    void MainDirector::Update(const DirectorInputOutput* lpIO)
    {
        // The frame camera is a STACK local on the console too (v211) -- built fresh every
        // frame, reaching the director's storage only through mLastCamera below.
        Camera::Camera lCamera;
        lCamera.Construct();

        const s32 liPlayerCarIndex = GetLivePlayerCarIndex(lpIO);

        if (liPlayerCarIndex == -1)
        {
            // LABEL_100 -- carry last frame's finalised camera forward.
            lCamera = mLastCamera;
        }
        else
        {
            // ⚠️ GATE: UpdateDebugPrinters / the debug byte / DebugLog::Print+Update.
            // ⚠️ GATE: UpdateCameraBehavioursPostScene( lpIO, liPlayerCarIndex );
            //   @0x8224FD30. THIS IS THE ONE REMAINING CALL BETWEEN A PREPARED BEHAVIOUR AND A
            //   MOVING CAMERA: it builds the ~1540-byte Camera::BehaviourSharedInfo on its own
            //   stack and hands it to BehaviourManager::UpdateAllBehaviours (which IS bodied now
            //   -- see BrnBehaviourManager.cpp). It stays gated because building that block
            //   means filling members this reconstruction cannot yet source honestly:
            //     * mPlayerInfo (a whole VehicleInfo copied out of mAllVehicleData, itself an
            //       un-homed named region of MainDirector) -- and handing behaviours a
            //       zero-filled one would be worse than not calling them at all;
            //     * mpEffectInterface / mpDebugLog / mpDebugPrinter / mpSceneQueryInterface /
            //       mpPlayerTracker / mpCameraSpaceHandler -- all pointers into MainDirector
            //       sub-objects that are still opaque regions;
            //     * mCarModifier / mCameraModifier / mfSpeedRatio / mfCrashTimeRemaining, which
            //       come out of the ~500-line VMX prologue of that function.
            //   The MEMBER MAP is now recovered though (see Camera/Behaviours/Behaviour.h: every
            //   console offset the two consumers touch -- +1360 mTimestep, +1424 mCarModifier,
            //   +1440 mUsedRaceCars, +1448 mpDirectorResourceManager ... +1520 mCameraModifier,
            //   +1536/+1537 the two trailing bools -- lands on the DWARF member its order
            //   predicts), so this is now a filling-in job, not a decoding one.
            //   CONSEQUENCE: no behaviour's Update runs, so every behaviour-produced camera
            //   holds whatever BehaviourHelper::Prepare constructed.
            //   DELETE-WHEN: AllVehicleData / VehicleTracker / DebugPrinter / DebugLog /
            //   EffectInterface are real members of MainDirector rather than opaque regions.
            // ⚠️ GATE: UpdateMoments( lpIO, liPlayerCarIndex );
            // ⚠️ GATE: if ( !<ICE-owns-frame latch> ) UpdateICE( lpIO, liPlayerCarIndex );

            // ⭐ The arbitrator picks and runs the state that owns this frame's camera.
            UpdateArbitrator(lpIO, lCamera, liPlayerCarIndex);

            // ⚠️ GATE: the ~550-line VMX / effect-hook / world-map remainder (lines 271-823).
        }

        // Finalise: camera inertia + shake (the CameraFinaliser owns the InertiaController).
        mCameraFinaliser.Update(lpIO->mpInputBuffer, maGameState, lpIO->mpResourceManager,
                                &lCamera);

        // Carry the finalised camera into the next frame.
        mLastCamera = lCamera;

        // The debug component's panorama-screenshot pass gets the finished camera.
        if (mpDebugComponent != 0)
            mpDebugComponent->UpdatePanoramaScreenshots(&lCamera);

        // Validate (asserts on NaN / unreasonable position), then convert to the graphics
        // camera and publish BOTH forms into the director output buffer.
        lCamera.ValidateTransformWithDebugInfo();
        lCamera.CopyToCgsCamera(&mCgsCamera);

        lpIO->mpOutputBuffer->SetCgsCamera(mCgsCamera);
        lpIO->mpOutputBuffer->SetCameraOutput(lCamera);

        // ⭐ X360 line 878 -- BehaviourManager::PrepareBehaviours(&mBehaviourManager,
        // lpIO->mpResourceManager). UNCONDITIONAL (outside the live-player-car branch above),
        // and it runs AFTER the publish, so a behaviour allocated during this frame's
        // arbitrator pass gets its first Prepare before the next frame reads it.
        //
        // This one call is what makes the attract state's poll terminate: NewBehaviour<> raises
        // the manager's "needs preparing" bit, ArbStateAttractMode::Prepare returns false while
        // it is set, and THIS is the only thing that clears it (by dispatching the behaviour's
        // own Prepare). Without it the fly-by state would poll for ever.
        mBehaviourManager.PrepareBehaviours(lpIO->mpResourceManager);

        // ⚠️ GATE: the rest of the post-publish tail (lines 871-877 / 879-924) -- see the banner.
    }

    // ------------------------------------------------------------------------
    // PostGuiUpdate  @ 0x82236F88   -- ⚠️ DOCUMENTED QUIET GATE
    //
    // The post-GUI pass DirectorModule::PostGuiUpdate runs when not replaying. The X360 body
    // folds this frame's GUI events into the director's game state and then runs the effect
    // interface + the "prepare for mode" action:
    //
    //     if ( input->HasGotCrashNavShownEvent() )            <set the crash-nav shown latch>
    //     if ( input->HasGotCrashNavHiddenEvent() )           <clear it>
    //     if ( input->HasGotColourCalibrationShownEvent() )   <set the colour-cal latch>
    //     if ( input->HasGotColourCalibrationHiddenEvent() )  <clear it>
    //     if ( input->HasGotShortcutMenuEvent() )
    //         <GameState shortcut-menu state> = input->GetShortcutMenuState();
    //     if ( input->HasGotHookEnumeration() )
    //         EffectInterface::Update( maEffectInterface, *input->GetHookEnumeration(), ... )
    //     ... ( the mode/action tail )
    //     MainDirector::HandlePrepareForModeAction( this, maModeActionAndDebugBlock, lpIO )
    //
    // Every INPUT-side accessor it needs is already committed on DirectorIO::InputBuffer --
    // the blockers are all on the MainDirector side: the four GUI latches and the
    // shortcut-menu state land inside the un-homed maGameState region, BrnDirector::
    // EffectInterface has no reconstructed home, and HandlePrepareForModeAction is itself
    // declaration-only (it dispatches over the same un-homed action region).
    //
    // CONSEQUENCE WHILE GATED: the director does not learn that the crash-nav / colour-
    // calibration / shortcut-menu overlays opened or closed, and fires no GUI-driven camera
    // effects. It does not affect the published camera (PostGuiUpdate runs AFTER Update).
    //
    // DELETE-WHEN: the MainDirector GameState region is named and BrnDirector::EffectInterface
    // is homed (HandlePrepareForModeAction unblocks with it).
    // ------------------------------------------------------------------------
    void MainDirector::PostGuiUpdate(const DirectorInputOutput* lpIO)
    {
        (void)lpIO;
    }
}
