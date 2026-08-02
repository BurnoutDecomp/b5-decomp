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
//   * ProcessInputQueue()     -- the game-action queue drain (junkyard/car-select arms)
//   * PostGuiUpdate()         -- every leg whose destination is a GameState field
//
// DECLARATION-ONLY + FLAGGED (in the header): UpdateICE / UpdateMoments / UpdateAttribSys /
// UpdateCameraBehaviours* / UpdateDebug* / ProcessNewVehicleEvents /
// HandlePrepareForModeAction / CalcTrafficLightSpace / DebugDisplayCurrentCamera.
// Each indexes a NOT-HOMED aggregate, paraphrases a VMX pipeline, or depends on un-dumped
// rodata.
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
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"                 // ICE::CameraSpaceHandler
                                                                       //   (UpdateCameraBehavioursPostScene
                                                                       //    stages one per frame)
// -- ProcessNewVehicleEvents / UpdateAttribSys: the car's authored camera attribs ----------
#include "GameSource/Director/SharedIO/BrnDirectorVehicleInputInterface.h"   // NewVehicleEvent queue
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"          // Attrib::Gen::burnoutcarasset
#include "GameSource/AttribSys/Generated/classes/camerabumperbehaviour.h"    // Attrib::Gen::camerabumperbehaviour
#include "GameSource/AttribSys/Generated/classes/cameraexternalbehaviour.h"  // Attrib::Gen::cameraexternalbehaviour

#include <cstring>   // std::memcpy (the game actions' packed CgsID / word payloads)

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
        , miPrepareStage(0)
        , miReleaseStage(0)
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
        // ASM (@0x8225B448): `stwx r31(=0), r30, r11` with r11 = 0x35420 and
        // `li r9,5; stwx r9, r30, r10` with r10 = 0x35424 -- the PREPARE stage starts at 0
        // (so Prepare runs its whole machine, including case 4's
        // BehaviourManager::Prepare) and the RELEASE stage starts at 5 == already released.
        miPrepareStage  = 0;
        miReleaseStage  = 5;
        mfConstructTime = static_cast<f64>(lfTime);

        // ⚠️ GATE: DirectorDevTools::Construct( this, this, lpResourceManager );

        // The published graphics camera (+0x349D0).
        mCgsCamera.Construct();
        mCgsCamera.UpdatePerspectiveProjectionMatrix();

        // mLastCamera (+0x32F10) -- the carried-over frame camera Update reads and writes.
        // Without this the director would publish RAW UNINITIALISED STORAGE on frame 1 and
        // ValidateTransformWithDebugInfo would assert on the NaNs.
        mLastCamera.Construct();

        // ⭐ REAL (2026-08-01): the director's per-frame vehicle snapshot. Its three player
        // spaces go to identity and its race-car pointer/index/bitset are cleared, so a frame
        // that reaches it before PreSceneQueryUpdate has published sees a defined object rather
        // than pool garbage. ⚠️ GATE (unchanged): the five flag bytes that follow it.
        mAllVehicleData.Construct();

        // ⭐ REAL (was held back for host size): the camera-behaviour manager.
        mBehaviourManager.Construct();

        // The named-camera-parameter bank. On the console this is
        // BehaviourParameterBank::Construct, which BehaviourManager::Construct calls (that call
        // is marked as a GATE in its body); here the one modelled block is seeded so
        // BuildArbStateSharedInfo can publish a REAL pointer instead of the null it used to.
        mNamedParameters.Construct();

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

        // ⭐ REAL (2026-08-01): `BrnDirector::GameState::Clear(this + 210912)` @0x8225B448.
        // This is the ONLY thing that seeds meJunkyardState to E_JY_INACTIVE, meEventType to
        // E_MODE_NONE, the three director-space transforms to identity, and the -1 sentinels.
        // While the region was an opaque byte span this never ran, so the whole snapshot --
        // including the junkyard sub-state the car-select ladder tests -- started as whatever
        // the allocator's memory happened to hold.
        maGameState.Clear();

        // ⚠️ GATE: the three DebugPrinter::Constructs.

        // [FLAG PC bring-up] The ICE scene-space transform (+0x12170). The console does NOT
        // seed it -- nothing in the whole image writes it except the in-game ICE editor's
        // preview leg in Update -- so on a console it starts at the module allocation's zero.
        // The PC allocation carries no such guarantee, and this matrix is staged into every
        // frame's ICE::CameraSpaceHandler, so it is zeroed explicitly rather than left to luck.
        // A scene-space authored take projects through a zero matrix either way; that is the
        // console's behaviour, not a deviation.
        // DELETE-WHEN: the ICE editor's scene-space producer is reconstructed.
        mICESceneSpace.xAxis.SetZero();
        mICESceneSpace.yAxis.SetZero();
        mICESceneSpace.zAxis.SetZero();
        mICESceneSpace.wAxis.SetZero();

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
        switch (miPrepareStage)
        {
        case 0:
            lpAsCollisionGenerator(&mCgsCamera)->Destruct();
            // ⚠️ GATE: EA::GameTalk::GameTalkManager::GetInstance()->RegisterMessageHandler(
            //              BrnDirector::DirectorDevTools::GameTalkMsgHandler, "Camera" );
            //   -- un-homed GameTalk API (see the banner). The `*this = 0` dev-tools head
            //   store that follows it belongs to the same un-homed object.
            // fall through
        case 1:
            miPrepareStage = 1;
            // ⚠️ GATE: the frame counter inside maShotAndAnalysisBlock (console +0x121A0).
            // fall through
        case 2:
            miPrepareStage = 2;
            // The wrapper pumps its own ICE resource acquisition through the director output
            // buffer, forwarding the prepare arg and the module's resource manager.
            if (!mICEWrapper.Prepare(lpOutputBuffer, liPrepareArg, lpResourceManager))
                return false;
            // fall through
        case 4:
            miPrepareStage = 4;
            if (!mBehaviourManager.Prepare())
                return false;
            // fall through
        case 5:
        {
            miPrepareStage = 5;
            muMomentBucketOccupancy = 0;
            for (s32 liSlot = 19; liSlot >= 0; --liSlot)
            {
                maMomentBucketFreeQueue[19 - liSlot] = liSlot;
            }
            miMomentBucketFreeCount = 20;
            // fall through
        }
        case 6:
            miPrepareStage = 6;
            // fall through
        case 7:
            miPrepareStage = 7;
            miReleaseStage = 0;   // asm: *(this+218148) = 0
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
        switch (miReleaseStage)
        {
        case 0:
            // asm 0x82236F0C `addi r3,r30,0x50` -> BaseCollisionGenerator::Destruct(this+0x50),
            // i.e. the ICE-wrapper region. (Destruct() below targets the +0x349D0 graphics
            // camera instead; these are different call sites and must not be conflated.)
            lpAsCollisionGenerator(&mICEWrapper)->Destruct();
            // fall through
        case 1:
            miReleaseStage = 1;
            // fall through
        case 2:
            miReleaseStage = 2;
            // fall through
        case 3:
            miReleaseStage = 3;
            // fall through
        case 5:
            miReleaseStage = 5;
            miPrepareStage = 0;   // asm: *(this+218144) = 0
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
    // ⛔⛔ THE `mpNamedParameters` GATE IS CLOSED (2026-08-01, junkyard-fire wave) -- AND ITS
    // OWN JUSTIFICATION IS WHY IT HAD TO BE. The gate used to publish null with the note
    // "CONSEQUENCE: an arbitrator state that reads named camera parameters gets null. None of
    // the states on the attract/flyby path does". That was true when it was written and stopped
    // being true the moment the junkyard fired: ArbStateCarSelect::Prepare does
    //     mLookAroundCarCam.GetBehaviour()->SetParameters(
    //         &lrSharedInfo.mpNamedParameters->GetLookAroundCarCamParameters());
    // which is a null dereference, and it crashed on the first frame the car-select state was
    // ever entered. (ArbStateOnlineCarSelect::Prepare has the identical line.) A stub's
    // "not on the live path" reasoning expires silently -- nothing in the build, the linker or
    // any boot test can tell you it has.
    // The slot now points at MainDirector::mNamedParameters, real named storage seeded by
    // NamedParameters::Construct.
    // ⭐ UPDATED 2026-08-02 (framing wave): the block now carries the console's TYPE TAG *AND*
    // its authored tunings. Both halves are transcribed --
    // BehaviourRotateAboutVehicle::Parameters::Construct @0x821FB300's thirteen re-tunes, plus
    // the FOUR the BANK's own Construct @0x8223DC90 applies to this block afterwards (the
    // Looker subject size 0.75/0.75 and screen offset +0.125/-0.125, transcribed in
    // NamedParameters::Construct with their asm and .rdata provenance). There is no data file
    // to load: BehaviourParameterBank::LoadParameters reads "d:\\camera.txt" and has no callers.
    // DELETE-WHEN: BehaviourParameterBank is homed (the other ~40 blocks are still unmodelled).
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
        lrSharedInfo.mpNamedParameters       = &mNamedParameters;                   // +0x1C
        lrSharedInfo.mpMomentController      = reinterpret_cast<MomentController*>(
                                                  const_cast<u8*>(maMomentController));         // +0x20
        lrSharedInfo.mpGameState             = const_cast<GameState*>(&maGameState);            // +0x24
        lrSharedInfo.mpRandom                = reinterpret_cast<Random*>(
                                                  const_cast<u8*>(maRandom));                   // +0x28
        lrSharedInfo.mpDirectorResourceManager = lpIO->mpResourceManager;                       // +0x2C
        lrSharedInfo.mpEffectInterface       = reinterpret_cast<const EffectInterface*>(
                                                  maEffectInterface);                           // +0x30
        lrSharedInfo.mpPlayerCrashInfo       = reinterpret_cast<const PlayerCrashInfo*>(
                                                  lpInput->GetPlayerCrashInfo());               // +0x34
        lrSharedInfo.mpAllVehicleData        = &mAllVehicleData;                                // +0x38
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
    // ⭐ STEP 2 IS NOW REAL. ProcessInputQueue is the ONLY consumer of the input buffer's
    // GAME-ACTION QUEUE and therefore the only writer of GameState::meJunkyardState in the
    // whole director. Until it ran, the junkyard sub-state could never leave E_JY_INACTIVE and
    // ArbStateRoaming could never hand the arbitrator to E_STATE_CAR_SELECT -- i.e. neither
    // the junkyard nor the retail intro camera was reachable, at ANY link-closure count.
    //
    // ⚠️ THE REST OF THE GUARDED BODY IS STILL A DOCUMENTED QUIET GATE. AllVehicleData /
    // VehicleTracker / CrashAnalyser are named opaque regions with no interiors, and
    // UpdateCameraBehavioursPreScene / BehaviourManager::ReleaseBehaviours are themselves
    // declaration-only (the last because its per-slot work drives the un-homed Behaviour
    // vtable).
    //
    // ⚠️ THE CONSOLE'S SECOND TEST is reproduced: `usedRaceCars.IsBitSet(playerCarIndex)`.
    // GetLivePlayerCarIndex already folds it in (see the header), so the guard below IS both
    // halves -- the X360 re-tests the bit because it inlines the index fetch, not because
    // there is a second condition.
    //
    // DELETE-WHEN: AllVehicleData / VehicleTracker / CrashAnalyser are homed and
    // UpdateCameraBehavioursPreScene is bodied.
    // ------------------------------------------------------------------------
    void MainDirector::PreSceneQueryUpdate(const DirectorInputOutput* lpIO)
    {
        // ⚠️ GATE: the "ICE sequence just finished" latch clear (maStateFlagTail).

        const s32 liPlayerCarIndex = GetLivePlayerCarIndex(lpIO);
        if (liPlayerCarIndex == -1)
            return;

        // ⭐⭐ X360 line 1 of the guarded body: AllVehicleData::Update @0x8221D938. This is the
        // ONLY producer of the snapshot every camera behaviour resolves its VehicleRefs against,
        // and it had never run on PC -- see the extracted-leg FLAG on UpdateRaceCarsBringUp for
        // exactly which two parts of the console's Update are deferred (the traffic array and
        // the nearest-car rebuild) and why.
        {
            const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;
            const CgsContainers::BitArray<8u>* lpUsedRaceCars = lpInput->GetUsedRaceCars();
            const BrnDirector::Camera::VehicleInfo* lpRaceCars = lpInput->GetRaceCarInfo();
            if (lpUsedRaceCars != 0 && lpRaceCars != 0)
            {
                mAllVehicleData.UpdateRaceCarsBringUp(
                    *lpUsedRaceCars, lpRaceCars,
                    static_cast<EActiveRaceCarIndex>(liPlayerCarIndex));
            }
        }

        // ⭐ X360 line 2 of the guarded body.
        ProcessInputQueue(lpIO);

        // ⚠️ GATE -- VehicleTracker::Update / CrashAnalyser::Update and the GameState latch
        //   clear (see the banner).

        // ⭐⭐ X360 LINE 6 (@0x8225BCF0). BODIED SINCE THE BANNER ABOVE WAS WRITTEN AND STILL
        // NOT CALLED -- BehaviourManager::ReleaseBehaviours has a full body in
        // BrnBehaviourManager.cpp and had NO caller anywhere in the image, so a released
        // BehaviourHandle only ever set its needs-releasing bit and NOTHING ever handed the
        // pool slot back. MEASURED consequence (2026-08-01): ArbStateCarSelect cycling
        // ROTATE_ABOUT_CAR <-> ACTIVE allocates one BehaviourInterpolate per cycle, and after
        // 20 cycles the 20-slot behaviour pool is exhausted -- "Array index out of bounds"
        // (CgsObjectPool.h:139) out of NewBehaviour<BehaviourInterpolate>, plus a rising tide
        // of "the set limit (N) has been reached" interpolation-lock asserts from the source
        // helpers that were never unlocked either (an interpolate behaviour releases its two
        // CameraReference locks in its Release virtual, i.e. from HERE).
        // It has to run BEFORE the pre-scene behaviour pass below, which is exactly where the
        // console puts it.
        mBehaviourManager.ReleaseBehaviours();

        // ⭐⭐ X360 LINE 7, THE LAST STEP OF THE GUARDED BODY (@0x8225BD18). This is where the
        // console runs Behaviour::Update for every live behaviour -- NOT inside MainDirector::
        // Update, which runs the COLLISION pass instead. Until this call existed, the PostScene
        // entry stood in for both and vtable slot 3 was never dispatched at all; see the banner
        // on UpdateCameraBehavioursPreScene.
        UpdateCameraBehavioursPreScene(lpIO, liPlayerCarIndex);
    }

    // ========================================================================================
    // THE GAMEPLAY-CAMERA ATTRIBUTE SEED  (camera parameter-chain wave, 2026-08-02)
    //
    // ⭐⭐ These two functions are the ONLY writers of BehaviourGameplayExternal::Parameters::
    // mbIsValid and BehaviourGameplayBumper::Parameters::mbIsValid anywhere on this build
    // (the third console writer, ReplayDirector::PreSceneQueryUpdate @0x8225BD28, is
    // declaration-only and the PC has no replay path). Both camera behaviours' whole Update
    // body sits inside `if (mpParameters->mbIsValid)`, so until one of these runs, the chase
    // and bumper cameras are structurally incapable of doing anything -- which is why
    // `sBringUpCamera` still draws the world after car select.
    //
    // Both seed the SAME two blocks, which live in the behaviour manager's parameter bank:
    //     bank + 0x2488  the external ("chase") block   == director + 0x314C8
    //     bank + 0x2538  the bumper ("in car") block    == director + 0x31578
    //     bank + 0x2480  the latched car attribs key    == director + 0x314C0
    // (the director-relative displacements are the ones these two functions inline; the
    // bank-relative ones are SharedCameraContainer::Prepare's. The bridge between them is
    // BehaviourManager == director + 0x1CB10, read off UpdateCameraBehavioursPreScene's own
    // `addis r26,r31,2 / addi r26,r26,-0x34F0`. See BrnBehaviourParameterBank.h.)
    //
    // The source of the tuning is the car's `burnoutcarasset` collection -- resolved by the
    // 64-bit key the world publishes -- and, inside it, the two RefSpecs at data +0x1A0
    // (cameraexternalbehaviour) and +0x1B8 (camerabumperbehaviour). Each resolved instance's
    // attribute data area IS Parameters::Source: Set re-loads `lwz r11, 4(source)` before
    // every field copy, and +0x04 of an Attrib::Instance is mpAttributeData.
    // ========================================================================================

    namespace
    {
        // Stage a Parameters::Source over a resolved generated instance's attribute data
        // area. The console passes the stack Attrib::Gen::* object straight to Set and lets
        // Set read its +0x04 slot; on x64 that slot is at +0x08, so the source block is
        // staged BY NAMED MEMBER here instead of reinterpret_cast'ing the instance.
        template <class TParameters, class TInstance>
        void SeedGameplayCameraParameters(TParameters& lrParameters, const TInstance& lrInstance)
        {
            typename TParameters::Source lSource;
            lSource.mpfValues = static_cast<const f32*>(lrInstance.GetLayoutPointer());
            lrParameters.Set(&lSource);
        }

        // The two Boost-FOV tripwire operands, as byte offsets into each camera's attribute
        // data area (`lfs f0, 0x18(r11)` / `lfs f0, 0x40(r11)`). They are the same two source
        // slots Parameters::Set copies mfBoostFOV from.
        const u32 KU_BUMPER_SOURCE_BOOST_FOV_OFFSET   = 0x18;
        const u32 KU_EXTERNAL_SOURCE_BOOST_FOV_OFFSET = 0x40;

        f32 ReadCameraSourceFloat(const void* lpAttributeData, u32 luByteOffset)
        {
            if (lpAttributeData == 0)
            {
                return 0.0f;
            }
            return *reinterpret_cast<const f32*>(
                static_cast<const u8*>(lpAttributeData) + luByteOffset);
        }
    }

    // ------------------------------------------------------------------------
    // ProcessNewVehicleEvents  @ 0x8221A6B0   -- ⭐⭐ THE ONLY PRIMARY SEED
    //
    // Console body, per event in lpInput->GetVehicleInputInterface()'s NewVehicleEvent queue:
    //     assert lEvent.mAttribsKey != 0                                         // .cpp:1783
    //     burnoutcarasset lCar( lEvent.mAttribsKey, 0 )    // FindCollection(class, key)
    //     assert lCar.IsValid()                                                  // .cpp:1787
    //     camerabumperbehaviour   lBumper  ( RefSpec(carData + 0x1B8).GetCollection(), 0 )
    //     cameraexternalbehaviour lExternal( RefSpec(carData + 0x1A0).GetCollection(), 0 )
    //     <lpcName = *(carData + 0x1E8), assert text only>
    //     assert lBumper.IsValid()                                               // .cpp:1795
    //     assert bumperData[0x18]   > 0.0f                                       // .cpp:1796
    //     assert lExternal.IsValid()                                             // .cpp:1797
    //     assert externalData[0x40] > 0.0f                                       // .cpp:1798
    //     BehaviourGameplayBumper  ::Parameters::Set( director + 0x31578, &lBumper   )
    //     BehaviourGameplayExternal::Parameters::Set( director + 0x314C8, &lExternal )
    //     std  lEvent.mAttribsKey, 0(director + 0x314C0)          <- latch, for UpdateAttribSys
    //
    // ⚠️ THE FIVE ASSERTS ARE NON-GATING, AND SO ARE THE TWO Set CALLS -- the console runs
    // both unconditionally, even when the collections did not resolve (in which case each
    // generated ctor has substituted Attrib::DefaultDataArea, i.e. zeros). That is
    // reproduced verbatim: an invalid car asset produces a VALID-but-zero parameter block on
    // the console too. UpdateAttribSys below is the one that gates its Sets on IsValid().
    // ------------------------------------------------------------------------
    void MainDirector::ProcessNewVehicleEvents(const DirectorIO::InputBuffer* lpInput)
    {
        if (lpInput == 0)
            return;

        const BrnDirectorVehicleInputInterface* lpVehicleInput = lpInput->GetVehicleInputInterface();
        if (lpVehicleInput == 0)
            return;

        const BrnDirectorVehicleInputInterface::NewVehicleEventQueue* lpQueue =
            lpVehicleInput->GetNewVehicleEventQueue();

        Camera::BehaviourParameterBank& lrBank = mBehaviourManager.GetBehaviourParameterBank();

        for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
        {
            const NewVehicleEvent& lrEvent = lpQueue->GetEvent(liEvent);

            CGS_ASSERT(lrEvent.mAttribsKey != 0, "lEvent.mAttribsKey!=0");            // :1783

            Attrib::Gen::burnoutcarasset lCarAsset(lrEvent.mAttribsKey, 0);
            CGS_ASSERT(lCarAsset.IsValid(), "Invalid car asset, key:");               // :1787

            Attrib::RefSpec* lpBumperRef   = lCarAsset.GetBumperCamRefSpec();
            Attrib::RefSpec* lpExternalRef = lCarAsset.GetExternalCamRefSpec();

            Attrib::Gen::camerabumperbehaviour lBumperCam(
                (lpBumperRef != 0)
                    ? const_cast<Attrib::Collection*>(lpBumperRef->GetCollection()) : 0, 0);
            Attrib::Gen::cameraexternalbehaviour lExternalCam(
                (lpExternalRef != 0)
                    ? const_cast<Attrib::Collection*>(lpExternalRef->GetCollection()) : 0, 0);

            // The console loads the asset name here; it only feeds the assert messages.
            (void)lCarAsset.GetAssetName();

            CGS_ASSERT(lBumperCam.IsValid(), "Invalid bumpercam asset, key:");        // :1795
            CGS_ASSERT(ReadCameraSourceFloat(lBumperCam.GetLayoutPointer(),
                                             KU_BUMPER_SOURCE_BOOST_FOV_OFFSET) > 0.0f,
                       "Invalid bumpercam Boost FOV, key:");                          // :1796
            CGS_ASSERT(lExternalCam.IsValid(), "Invalid externalcam asset, key:");    // :1797
            CGS_ASSERT(ReadCameraSourceFloat(lExternalCam.GetLayoutPointer(),
                                             KU_EXTERNAL_SOURCE_BOOST_FOV_OFFSET) > 0.0f,
                       "Invalid externalcam Boost FOV, key:");                        // :1798

            SeedGameplayCameraParameters(lrBank.GetGameplayBumperCameraParamsForCar(), lBumperCam);
            SeedGameplayCameraParameters(lrBank.GetGameplayExternalCameraParamsForCar(), lExternalCam);

            lrBank.SetGameplayCameraCarAttribsKey(lrEvent.mAttribsKey);

            // [diag, one-shot -- NOT console code] the far end of the chain the world's
            // new-vehicle publish starts. It reports the two bytes that decide whether the
            // console gameplay cameras can run at all. Remove when the chase camera's own
            // Update lands and the camera itself is the evidence.
            {
                static bool sbReported = false;
                if (!sbReported && (CgsDev::Message::gxMessageFilterFlags & 1) &&
                    CgsDev::Log::gpDebugPrint != 0)
                {
                    sbReported = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[newveh] MainDirector::ProcessNewVehicleEvents: seeded from key hi "
                        << static_cast<s32>(lrEvent.mAttribsKey >> 32) << " lo "
                        << static_cast<s32>(lrEvent.mAttribsKey & 0xFFFFFFFFu)
                        << " -- externalValid "
                        << (lrBank.GetGameplayExternalCameraParamsForCar().mbIsValid ? 1 : 0)
                        << " FOV " << lrBank.GetGameplayExternalCameraParamsForCar().mrFOV
                        << " boostFOV " << lrBank.GetGameplayExternalCameraParamsForCar().mfBoostFOV
                        << " | bumperValid "
                        << (lrBank.GetGameplayBumperCameraParamsForCar().mbIsValid ? 1 : 0)
                        << "\n";
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // UpdateAttribSys  @ 0x8221AFD0   -- the GameTalk live-tuning re-read (54 asm lines)
    //
    //     if ( !lpInput->GetControll()->mbGameTalkRefreshRequest )   return;   // controller +1
    //     burnoutcarasset lCar( bank.mxGameplayCameraCarAttribsKey, 0 );       // director+0x314C0
    //     camerabumperbehaviour   lBumper  ( RefSpec(carData + 0x1B8).GetCollection(), 0 );
    //     cameraexternalbehaviour lExternal( RefSpec(carData + 0x1A0).GetCollection(), 0 );
    //     if ( lBumper.IsValid()   )  bumperParams  .Set( &lBumper   );        // director+0x31578
    //     if ( lExternal.IsValid() )  externalParams.Set( &lExternal );        // director+0x314C8
    //
    // ⚠️⚠️ THIS IS NOT A PER-FRAME RE-SEED, and the camera-chain map carried into this wave
    // said it was. Its ONE gate is ControllerInfo +0x01, which the DecFIGS DWARF names
    // mbGameTalkRefreshRequest (BrnDirectorControllerInfo.h:49) -- the authoring tool's
    // "I changed a value, re-read it" pulse. Nothing on this build ever sets it, so this body
    // is inert here BY DESIGN, not by omission. It is transcribed anyway because it is cheap,
    // it is the console's, and it is the consumer that explains why the latched key exists at
    // all; but ProcessNewVehicleEvents above is the function that does the work.
    //
    // Unlike ProcessNewVehicleEvents this one GATES each Set on the resolve, and it carries
    // no asserts.
    // ------------------------------------------------------------------------
    void MainDirector::UpdateAttribSys(const DirectorIO::InputBuffer* lpInput)
    {
        if (lpInput == 0)
            return;

        const DirectorIO::ControlInput* lpControl = lpInput->GetControll();
        if (lpControl == 0 || !lpControl->IsGameTalkRefreshRequested())
            return;

        Camera::BehaviourParameterBank& lrBank = mBehaviourManager.GetBehaviourParameterBank();

        Attrib::Gen::burnoutcarasset lCarAsset(lrBank.GetGameplayCameraCarAttribsKey(), 0);

        Attrib::RefSpec* lpBumperRef   = lCarAsset.GetBumperCamRefSpec();
        Attrib::RefSpec* lpExternalRef = lCarAsset.GetExternalCamRefSpec();

        Attrib::Gen::camerabumperbehaviour lBumperCam(
            (lpBumperRef != 0)
                ? const_cast<Attrib::Collection*>(lpBumperRef->GetCollection()) : 0, 0);
        Attrib::Gen::cameraexternalbehaviour lExternalCam(
            (lpExternalRef != 0)
                ? const_cast<Attrib::Collection*>(lpExternalRef->GetCollection()) : 0, 0);

        if (lBumperCam.IsValid())
        {
            SeedGameplayCameraParameters(lrBank.GetGameplayBumperCameraParamsForCar(), lBumperCam);
        }
        if (lExternalCam.IsValid())
        {
            SeedGameplayCameraParameters(lrBank.GetGameplayExternalCameraParamsForCar(), lExternalCam);
        }
    }

    // ------------------------------------------------------------------------
    // ProcessInputQueue  @ 0x822372F8   -- ⭐⭐ THE GAME-ACTION -> GAMESTATE SEAM
    //
    // Drain the input buffer's game-action queue (DirectorIO::InputBuffer::GetGameActionQueue
    // @0x82206C50 == the buffer's embedded CgsModule::VariableEventQueue<13312,16>) and fold
    // each action into the director's GameState snapshot. The console's shape is:
    //
    //     lpQueue = lpIO->mpInputBuffer->GetGameActionQueue();
    //     mGameState.ResetPerFrameData();                       // inlined; see the GameState TU
    //     ProcessNewVehicleEvents( lpIO->mpInputBuffer );
    //     <the takedown pair + three straight copies out of the input buffer>
    //     for ( action = queue.GetFirstEvent(); action; action = queue.GetNextEvent(action) )
    //         switch ( action.type ) { ... 45 handled cases out of a 225-entry table ... }
    //     <the three waiting -> receivedThisFrame handshakes>
    //     <the slomo / crash / debug-render tail>
    //
    // ⚠️ WHAT IS BODIED HERE: the prologue, the walk, and the NINE junkyard / car-select cases
    // (62, 63, 64, 65, 73, 75, 76, 77, 85) plus the three handshakes. Those are the complete
    // set of arms that touch GameState +0x180..+0x1AC, i.e. the whole junkyard sub-machine.
    //
    // ⚠️ WHAT IS GATED, and why (each is a NO-OP here, never a wrong value):
    //   * the other 36 handled cases (0, 6, 12, 23, 24, 29, 30, 33, 34, 37, 39, 42, 43, 47,
    //     53, 54, 56, 58, 97, 98, 100, 102, 107, 113, 120, 132, 140, 144, 145, 146, 150, 151,
    //     205, 215, 216, 218, 223, 224) -- every one of them writes into a part of the
    //     GameState or the MainDirector flag tail that is still opaque, or calls an un-homed
    //     aggregate (AllVehicleData, the VMX drive-thru transform pipeline, DebugRender).
    //   * ProcessNewVehicleEvents -- declaration-only (AllVehicleData un-homed).
    //   * the post-loop slomo / crash-active tail and the whole debug-render block.
    // The console's own default arm is a no-op `b def_...`, so an unhandled id costs nothing.
    //
    // ⚠️ ACTION IDs ARE X360 ids, which run +5 above the PS3 DecFIGS DWARF's E_ACTION_* enum
    // across this range. The shift is pinned at BOTH ends: case 77's own assert string is
    // "lpExitAction" (BrnMainDirector.cpp:1410) and DWARF 72 == E_ACTION_CAR_SELECT_EXIT.
    // ------------------------------------------------------------------------
    void MainDirector::ProcessInputQueue(const DirectorInputOutput* lpIO)
    {
        // 0x82237314 `lwz r30, 0(r4)` -- the console's FIRST act is to take the input buffer
        // out of the DirectorInputOutput. r4 being live is the whole reason this function's
        // declaration needed the argument back.
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;
        if (lpInput == 0)
            return;

        const CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpInput->GetGameActionQueue();

        // 0x82237350..0x822373AC -- the inlined GameState::ResetPerFrameData().
        maGameState.ResetPerFrameData();

        // ⭐⭐ REAL as of 2026-08-02 (camera parameter-chain wave), at the console's own
        // position (immediately after ResetPerFrameData). The gate this replaces read
        // "declaration-only (AllVehicleData un-homed)" -- both halves were wrong; see the
        // declaration in BrnMainDirector.h. This is the ONLY writer of the two shared gameplay
        // cameras' Parameters::mbIsValid on this build.
        ProcessNewVehicleEvents(lpInput);

        // 0x822373B0 -- the takedown pair. The console re-reads the flag to assert on it
        // ("mbPlayerTakenDown", BrnMainDirector.cpp:232) between the two stores.
        if (lpInput->GetPlayerTakenDown())
        {
            maGameState.mbPlayerWasTakenDown = true;                        // +0x1C3
            CGS_ASSERT(lpInput->GetPlayerTakenDown(), "mbPlayerTakenDown");
            maGameState.mePlayerKillerIndex = lpInput->GetPlayerKillerCarIndex();  // +0x1C4
        }

        // 0x82237408..0x82237440 -- three straight copies out of the input buffer into the
        // GameState's RankUpInfo sub-object. The sub-object's DWARF layout is unreliable, so
        // the head word goes through its named 4-byte field and the two tail bytes through its
        // own opaque storage.
        maGameState.mRankUpInfo.miRivalTeamSelector = lpInput->GetRankUpRivalInfo();  // +0x1CC
        // ⚠️ GATE: GameState +0x1D0 / +0x1D1 <- input @0x7AD5 / @0x7AD6. Both ends are opaque
        //    bytes (InputBuffer::maFlagTail, RankUpInfo::maOpaque) with no recovered role on
        //    either side, so copying them would move bytes nobody can name. Recorded, not run.

        const CgsModule::Event* lpAction = 0;
        s32 liActionSize = 0;
        s32 liActionType = lpQueue->GetFirstEvent(&lpAction, &liActionSize);

        while (lpAction != 0)
        {
            // The console reads every payload as raw bytes off the record pointer (r30); the
            // per-action structs live in BrnGameActions.h, which is a GameState-side header
            // this TU does not own. Same access, named per case.
            const u8* lpacPayload = reinterpret_cast<const u8*>(lpAction);

            switch (liActionType)
            {
            // ---- 62  E_ACTION_NEW_CAR_UNLOCKED (16 bytes) ----------------------------
            case 62:
                maGameState.meJunkyardState          = GameState::E_JY_CAR_UNLOCK;  // = 3
                maGameState.mbIsRivalUnlock          = true;
                maGameState.mbNewCarUnlockedThisFrame = true;
                maGameState.miJunkyardPosIndex       = 0;
                // `ld r10, 0(r30); stdx r10, r31, 0x33968` -- the unlocked vehicle's CgsID.
                std::memcpy(&maGameState.mUnlockedVehicleType, lpacPayload + 0, sizeof(CgsID));
                break;

            // ---- 63  E_ACTION_CAR_UNLOCK_END (1 byte) --------------------------------
            case 63:
                if (maGameState.meJunkyardState == GameState::E_JY_CAR_UNLOCK)
                    maGameState.meJunkyardState = GameState::E_JY_CAR_SELECT;
                break;

            // ---- 64  E_ACTION_CAR_SELECTION_CHANGED (0x40 bytes) ---------------------
            case 64:
                if (maGameState.meJunkyardState == GameState::E_JY_INACTIVE)
                    maGameState.meJunkyardState = GameState::E_JY_CAR_SELECT;
                maGameState.mbNewCarUnlockedThisFrame = false;
                maGameState.miJunkyardPosIndex        = 0;
                maGameState.mbJunkyardPosJustChanged  = true;
                // ⭐ the ONLY writer of mJunkyardId in the image (see the GameState header).
                std::memcpy(&maGameState.mJunkyardId, lpacPayload + 0, sizeof(CgsID));
                maGameState.mbJunkyardPosIsLeft = (lpacPayload[0x30] != 0);
                break;

            // ---- 65  E_ACTION_CAR_SELECTION_CHANGED_DROPIN (16 bytes) ----------------
            case 65:
                maGameState.mbJunkyardPlayerRespawnedThisFrame = true;
                maGameState.mbJunkyardPosIsLeft = (lpacPayload[0x08] != 0);
                break;

            // ---- 73  E_ACTION_CAR_SELECT_TRANSITION_IN (2 bytes) ---------------------
            // ⭐⭐ THE ENTRY ACTION. payload[0] is "this is the transition IN"; payload[1] is
            // "there are cars to unlock". A brand-new profile has one unlocked car, so
            // CarSelectManager::StartTransitionInState @0x823929D0 posts {1,0} == INTRO_NO_CARS
            // and its EndTransitionInState @0x82392B30 posts {0,x} == CAR_SELECT.
            case 73:
                if (lpacPayload[0] != 0)
                {
                    maGameState.meJunkyardState = (lpacPayload[1] != 0)
                        ? GameState::E_JY_INTRO_UNLOCKING_CARS     // = 1
                        : GameState::E_JY_INTRO_NO_CARS;           // = 2
                }
                else
                {
                    maGameState.meJunkyardState = GameState::E_JY_CAR_SELECT;   // = 4
                }
                maGameState.mbJunkyardCarModActive = false;
                break;

            // ---- 75  E_ACTION_CAR_SELECT_READY (4 bytes) -----------------------------
            case 75:
            {
                s32 liReadyMode = 0;
                std::memcpy(&liReadyMode, lpacPayload + 0, sizeof(s32));
                if (liReadyMode == 2)
                {
                    maGameState.mbIsOnlineCarSelectActive          = true;
                    maGameState.mbHasOnlineCarSelectBeenAborted    = false;
                    maGameState.mbOnlineCarSelectCanStartRaceIntro = false;
                    maGameState.mbOnlineCarSelectMustClampToCar    = false;
                }
                break;
            }

            // ---- 76  E_ACTION_CAR_SELECT_MODIFICATION_SCREEN (8 bytes) ---------------
            case 76:
                maGameState.mbJunkyardCarModActive = (lpacPayload[0x04] != 0);
                break;

            // ---- 77  E_ACTION_CAR_SELECT_EXIT (0x20 bytes) ---------------------------
            case 77:
                CGS_ASSERT(lpAction != 0, "lpExitAction");   // BrnMainDirector.cpp:1410
                if (lpacPayload[0x10] != 0)
                {
                    maGameState.mbIsOnlineCarSelectActive = false;
                }
                else
                {
                    maGameState.meJunkyardState        = GameState::E_JY_INACTIVE;
                    maGameState.mbJunkyardCarModActive = false;
                }
                break;

            // ---- 85  (1 byte) --------------------------------------------------------
            case 85:
                maGameState.mbOnlineCarSelectCarIsShowable = (lpacPayload[0] != 0);
                break;

            default:
                // The console's own default arm, plus the 36 GATED cases listed in the banner.
                break;
            }

            liActionType = lpQueue->GetNextEvent(lpAction, &lpAction, &liActionSize);
        }

        // 0x822387FC..0x82238868 -- THE THREE waiting -> receivedThisFrame HANDSHAKES.
        // PostGuiUpdate raises the *waiting* bit on the frame the GUI event lands; the NEXT
        // ProcessInputQueue clears it and raises the *receivedThisFrame* bit, which
        // ResetPerFrameData drops again one frame later. That one-frame delivery contract is
        // the shape consumers depend on, so it is reproduced exactly.
        if (maGameState.mbJunkyardSelectionChangedMessageWaiting)
        {
            maGameState.mbJunkyardSelectionChangedMessageWaiting = false;
            maGameState.mbJunkyardSelectionChangedMessageReceivedThisFrame = true;
        }
        if (maGameState.mbJunkyardCarUnlockTickedClosedThisFrameMessageWaiting)
        {
            maGameState.mbJunkyardCarUnlockTickedClosedThisFrameMessageWaiting = false;
            maGameState.mbJunkyardCarUnlockTickedClosedThisFrame = true;
        }
        if (maGameState.mbRankUpMessageWaiting)
        {
            maGameState.mbRankUpMessageWaiting           = false;
            maGameState.mbRankUpMessageReceivedThisFrame = true;
        }

        // ⚠️ GATE: `if (<flag tail +0x35431>) mbCanUseSlomo = false;`, the crash-active leg
        //   (it indexes the published VehicleInfo at element +0x44A, a byte with no recovered
        //   name), and the whole debug-render tail.
    }

    // ------------------------------------------------------------------------
    // UpdateCameraBehavioursPostScene  @ 0x8224FD30
    //
    // ⭐ THE PASS THAT RUNS EVERY LIVE CAMERA BEHAVIOUR. It builds the ~1540-byte
    // Camera::BehaviourSharedInfo on its own stack and ends in
    //     BehaviourManager::PostCollisionUpdateAllBehaviours(&mBehaviourManager, <paused>,
    //                                                        lSharedInfo, <controller>, 1,
    //                                                        <debug printer>)
    // (its sibling entry point UpdateCameraBehavioursPreScene @0x82255318 ends in
    // UpdateAllBehaviours over the same block). Without this pass nothing ever dispatches a
    // Behaviour::Update, so every behaviour-produced camera stays at whatever
    // BehaviourHelper::Prepare constructed -- which is exactly why the fly-by camera could not
    // move no matter what the arbitrator did.
    //
    // The X360 prologue, in order (all of it reproduced below except where FLAGged):
    //   assert(input->GetTimerStatusInterface()->GetGameTimerStatus()->IsRunning())  // .cpp:1935
    //   lfGame    = gameTimer[+8] * gameTimer[+4];
    //   if (simTimer.mbRunning /*[+36]*/) { lfWorld = simTimer[+32] * simTimer[+28];
    //                                       lfWorldNoSlomo = simTimer[+28]; }
    //   else                              { lfWorld = 0; lfWorldNoSlomo = 0; }
    //   if (<ICE owns the frame>) lfGame = lfWorld = lfWorldNoSlomo = 0;
    //   <broadcast all three into the Timestep's VecFloat lanes>
    //   mUsedRaceCars = *input->GetUsedRaceCars(); mpRaceCars = input->GetRaceCarInfo();
    //   mePlayerCarIndex = liPlayerCarIndex; mpAllVehicleData = &mAllVehicleData; ...
    //   BehaviourManager::ProcessSceneQueryResults(...);
    //   <the ICE::CameraSpaceHandler build from the player + nearest race car>
    //   Camera2DRotationController::Update / CameraSphericalRotationController::Update
    //   <the ~90-line VMX copy of the two vehicle transforms into the shared info>
    //
    // ⭐⭐ THE SIX-SLOT BLACK HOLE IS CLOSED (2026-08-01, ICE-anim transform wave).
    // The banner that used to sit here said mPlayerInfo / mpAllVehicleData / mpPlayerTracker /
    // mpEffectInterface / mpDebugLog / mpDebugPrinter / mpCameraSpaceHandler were "left null
    // (the fly-by path reads none of them)". THAT JUSTIFICATION HAD ALREADY EXPIRED: three of
    // them are on BehaviourIceAnim::Update's straight-line path --
    //     mpAllVehicleData     -> every VehicleRef::IsValid / ::Get resolves against it
    //     mpCameraSpaceHandler -> Update COPY-CONSTRUCTS its own handler off it, frame 1
    //     mpDebugPrinter       -> the "Can/Can't see player" readout at the end of Update
    // -- and each was a null dereference waiting for the frame the ICE behaviour actually ran.
    // Nothing could observe that while BehaviourIceAnim::Construct's missing VehicleRef seeds
    // made Update fail out on its first line (see that file). Same lesson as the junkyard
    // wave's mpNamedParameters: a "not on the live path" note expires SILENTLY.
    //
    // Every one of them now points at storage this class already owns and already seeds; none
    // needed a new type. mPlayerInfo is a real VehicleInfo by value (Behaviour.h's ODR blocker
    // was retired when the tree collapsed to one SuspensionSpring), so it is filled with the
    // console's own `VehicleInfo::operator=` copy of the player's race-car record.
    //
    // ⚠️ WHAT IS STILL FLAGGED HERE:
    //   * mCarModifier / mCameraModifier / mfSpeedRatio / mfCrashTimeRemaining /
    //     mfTempFOVBoostAmount -- products of the ~500-line VMX prologue. Left zeroed, which is
    //     also what UpdateAllBehaviours' own (gated) responder prologue would leave them at.
    //   * the two rotation-controller Updates and ProcessSceneQueryResults -- VMX pipelines /
    //     un-homed controller interiors.
    //   * the ControllerInfo and DebugPrinter arguments -- both are INCOMPLETE types in this
    //     tree (declared, never defined), and the reconstructed UpdateAllBehaviours body casts
    //     both to void without dereferencing. They are passed as references over a static
    //     zeroed byte block rather than fabricating either layout.
    // DELETE-WHEN: per item, as each aggregate is homed.
    // ------------------------------------------------------------------------
    // ⭐⭐ SPLIT 2026-08-01 (car-select hand-off wave). This staging used to sit inside
    // UpdateCameraBehavioursPostScene; it is now shared, because the console has TWO entry
    // points over it and this build only ever ran one of them. See the banner on
    // UpdateCameraBehavioursPreScene below.
    void MainDirector::BuildBehaviourSharedInfo(const DirectorInputOutput* lpIO,
                                                s32 liPlayerCarIndex,
                                                Camera::BehaviourSharedInfo& lSharedInfo,
                                                ICE::CameraSpaceHandler& lCameraSpaces)
    {
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;

        const CgsSystem::TimerStatusInterface* lpTimerStatus = lpInput->GetTimerStatusInterface();

        const CgsSystem::TimerStatus* lpGameTimer = lpTimerStatus->GetGameTimerStatus();
        const CgsSystem::TimerStatus* lpSimTimer  = lpTimerStatus->GetSimTimerStatus();

        // .cpp:1935 -- the console asserts the game timer is running. On this bring-up the
        // director input buffer is not staged (DoUpdate_Director zeroes it), so it is not:
        // firing a dev assert every frame would block the sim, so the condition is evaluated
        // and reported, not trapped.
        // DELETE-WHEN: the per-frame director input staging lands (BridgeTimers -> the
        // director input's timer status interface).
        const bool lbGameTimerRunning = lpGameTimer->IsRunning();
        (void)lbGameTimerRunning;

        const f32 lfGameTimestep = lpGameTimer->GetCurrentTimeStep();   // [+8] * [+4]

        f32 lfWorldTimestep       = 0.0f;
        f32 lfWorldNoSlomoTimestep = 0.0f;
        if (lpSimTimer->IsRunning())                                    // [+36]
        {
            lfWorldTimestep        = lpSimTimer->GetCurrentTimeStep();  // [+32] * [+28]
            lfWorldNoSlomoTimestep = lpSimTimer->GetBaseTimeStep();     // [+28]
        }

        // ⚠️ GATE: `if (maStateFlagTail[+0x35400]) { all three = 0; }` -- the ICE-owns-the-frame
        // latch lives in the un-homed flag tail (same gate BuildArbStateSharedInfo documents).

        lSharedInfo.mTimestep.Set(BrnDirector::VecFloat(lfGameTimestep),
                                  BrnDirector::VecFloat(lfWorldTimestep),
                                  BrnDirector::VecFloat(lfWorldNoSlomoTimestep),
                                  lfGameTimestep, lfWorldTimestep, lfWorldNoSlomoTimestep);

        lSharedInfo.mUsedRaceCars             = *lpInput->GetUsedRaceCars();
        lSharedInfo.mpRaceCars                = lpInput->GetRaceCarInfo();
        lSharedInfo.mePlayerCarIndex          = static_cast<EActiveRaceCarIndex>(liPlayerCarIndex);
        lSharedInfo.mpDirectorResourceManager = lpIO->mpResourceManager;
        lSharedInfo.mpBehaviourManager        = &mBehaviourManager;
        lSharedInfo.mpWorldMap                = lpIO->mpWorldMap;
        lSharedInfo.mpSceneQueryInterface     = lpIO->mpSceneQueryInterface;
        lSharedInfo.mpRandom                  = reinterpret_cast<CgsNumeric::Random*>(
                                                    const_cast<u8*>(maRandom));

        // ---- the six slots that used to be published as null (see the banner) --------------
        // Each is the console's own `this + <offset>`, reached through the named member.
        lSharedInfo.mpAllVehicleData          = &mAllVehicleData;
        lSharedInfo.mpPlayerTracker           = reinterpret_cast<const VehicleTracker*>(
                                                    maVehicleTracker);
        lSharedInfo.mpEffectInterface         = reinterpret_cast<const EffectInterface*>(
                                                    maEffectInterface);
        lSharedInfo.mpDebugLog                = reinterpret_cast<DebugLog*>(maDebugLog);
        lSharedInfo.mpDebugPrinter            = reinterpret_cast<DebugPrinter*>(maDebugPrinterMain);

        // The player's own vehicle record, BY VALUE (console: VehicleInfo::operator= @0x821F49C8
        // into the shared info's mPlayerInfo, from `raceCars + 1264 * playerIndex` -- 1264 is the
        // X360 sizeof(VehicleInfo); the indexed member read below is the same element).
        // IsLookingAtTarget reads its mRaceCarState.mTransform and mAABB at the end of every
        // ICE-anim Update, so a zeroed record made every take report "can't see player".
        if (lSharedInfo.mpRaceCars != 0 && liPlayerCarIndex >= 0)
        {
            lSharedInfo.mPlayerInfo = lSharedInfo.mpRaceCars[liPlayerCarIndex];
        }

        // ---- the per-frame ICE reference-space cache ---------------------------------------
        // X360 @0x82250074. The eight matrices, in the console's own argument order (recovered
        // from the asm register/home-slot map, NOT from the 8-argument Hex-Rays rendering --
        // which drops the two stack arguments, as it always does):
        //     mCarToWorld          = the PLAYER's world transform
        //     mCar2ToWorld         = GetRaceCar(GetNearestRaceCarIndexToPlayer(1))'s transform
        //     mTrafficLightToWorld = GameState::mTrafficLightSpace          (maGameState + 0x10)
        //     mSceneToWorld        = the ICE editor's scene space           (this + 0x12170)
        //     mImpactToWorld       = AllVehicleData::GetPlayerImpactSpace()
        //     mHeadingToWorld      = AllVehicleData::GetPlayerHeadingSpace()
        //     mLooseHeadingToWorld = AllVehicleData::GetPlayerLooseHeadingSpace()
        //     mHeading2ToWorld     = the nearest race car's transform AGAIN (the console
        //                            recomputes the same index and re-reads the same +0x1F0;
        //                            reproduced, not "cleaned up")
        //     mpGamePlayCam        = &mSharedCameraContainer.mGameplayExternal (this + 0x166A4
        //                            == mArbitrator + 0x38E4 == container + 0x04)
        // The handler is a STACK object here exactly as it is on the console (its frame slot
        // var_2C0); every behaviour that needs it copy-constructs its own inside the
        // UpdateAllBehaviours call below, so its lifetime is this function.
        //
        // ⚠️ GUARD (not console code): the two vehicle reads go through AllVehicleData, which
        // asserts and then indexes mpRaceCars. On the frames before PreSceneQueryUpdate has
        // published one, that pointer is null. The console cannot reach this function in that
        // state (its caller is inside the live-player-car guard); this build can, so the
        // handler is staged only when the snapshot is populated and is otherwise left as
        // Construct's default. DELETE-WHEN: MainDirector::Update's own live-player-car
        // prologue is real.
        if (mAllVehicleData.GetRaceCars() != 0)
        {
            const EActiveRaceCarIndex leNearest =
                mAllVehicleData.GetNearestRaceCarIndexToPlayer(1u);

            const Matrix44Affine& lrPlayerToWorld =
                mAllVehicleData.GetPlayer().mRaceCarState.mTransform;
            const Matrix44Affine& lrNearestToWorld =
                mAllVehicleData.GetRaceCar(leNearest).mRaceCarState.mTransform;

            lCameraSpaces.Construct(lrPlayerToWorld,
                                    lrNearestToWorld,
                                    maGameState.mTrafficLightSpace,
                                    mICESceneSpace,
                                    mAllVehicleData.GetPlayerImpactSpace(),
                                    mAllVehicleData.GetPlayerHeadingSpace(),
                                    mAllVehicleData.GetPlayerLooseHeadingSpace(),
                                    lrNearestToWorld,
                                    &mArbitrator.GetSharedCameras().mGameplayExternal);
        }
        lSharedInfo.mpCameraSpaceHandler = &lCameraSpaces;
    }

    // ------------------------------------------------------------------------
    // UpdateCameraBehavioursPreScene  @ 0x82255318
    //
    // ⭐⭐ ADDED 2026-08-01 (car-select hand-off wave). THE CONSOLE HAS TWO PASSES OVER THE
    // BEHAVIOUR SET AND THIS BUILD ONLY EVER RAN ONE OF THEM -- and, worse, ran it from the
    // wrong entry point. Verified in the ARTIST asm, not inferred:
    //     MainDirector::PreSceneQueryUpdate @0x8225BA00
    //         -> @0x8225BD18  bl UpdateCameraBehavioursPreScene   @0x82255318
    //                             -> @0x822557B0 bl BehaviourManager::UpdateAllBehaviours
    //     MainDirector::Update @0x82274070
    //         -> @0x82274338  bl UpdateCameraBehavioursPostScene  @0x8224FD30
    //                             -> @0x8225024C bl BehaviourManager::PostCollisionUpdateAllBehaviours
    // The PC's PostScene entry called UpdateAllBehaviours -- i.e. it stood in for the PreScene
    // pass -- so vtable slot 3 (Behaviour::PostCollisionUpdate) was NEVER DISPATCHED anywhere
    // in the image. Every behaviour whose per-frame work lives in slot 3 was inert, silently:
    // BehaviourInterpolate does its ENTIRE blend, its parametric-time advance and its
    // mbHasFinished latch there, which is why ArbStateCarSelect's hand-off out of
    // GAME_INTRO_PART_THREE published an unwritten camera and then never finished.
    //
    // The two entries share BuildBehaviourSharedInfo above (the console builds the same
    // ~1540-byte block on its own stack in both; its per-entry prologue differences are inside
    // the same GATE list that banner already carries).
    //
    // ⚠️ ORDERING: PreSceneQueryUpdate runs before Update in the same sub-step, and both gate
    // on the SAME live-player-car predicate, so this pass runs on exactly the frames the old
    // single pass did -- and the arbitrator now reads THIS frame's behaviour output instead of
    // last frame's (the one-frame staleness the Update banner used to record is gone).
    // ------------------------------------------------------------------------
    void MainDirector::UpdateCameraBehavioursPreScene(const DirectorInputOutput* lpIO,
                                                      s32 liPlayerCarIndex)
    {
        Camera::BehaviourSharedInfo lSharedInfo = Camera::BehaviourSharedInfo();
        ICE::CameraSpaceHandler     lCameraSpaces;
        BuildBehaviourSharedInfo(lpIO, liPlayerCarIndex, lSharedInfo, lCameraSpaces);

        // ⚠️ FLAG (see the BuildBehaviourSharedInfo banner): the two incomplete-type
        // reference arguments.
        static u8 saOpaqueControllerInfo[64] = { 0 };
        static u8 saOpaqueDebugPrinter[64]   = { 0 };

        mBehaviourManager.UpdateAllBehaviours(
            false,                                                        // lbPaused
            lSharedInfo,
            *reinterpret_cast<const ControllerInfo*>(saOpaqueControllerInfo),
            true,                                                         // the console's `1`
            *reinterpret_cast<DebugPrinter*>(saOpaqueDebugPrinter));
    }

    // ------------------------------------------------------------------------
    // UpdateCameraBehavioursPostScene  @ 0x8224FD30 -- the COLLISION-pass twin (see above).
    // ------------------------------------------------------------------------
    void MainDirector::UpdateCameraBehavioursPostScene(const DirectorInputOutput* lpIO,
                                                       s32 liPlayerCarIndex)
    {
        Camera::BehaviourSharedInfo lSharedInfo = Camera::BehaviourSharedInfo();
        ICE::CameraSpaceHandler     lCameraSpaces;
        BuildBehaviourSharedInfo(lpIO, liPlayerCarIndex, lSharedInfo, lCameraSpaces);

        static u8 saOpaqueControllerInfo[64] = { 0 };
        static u8 saOpaqueDebugPrinter[64]   = { 0 };

        // ⭐ @0x8225024C -- the console's own call here, and the one this build was missing.
        mBehaviourManager.PostCollisionUpdateAllBehaviours(
            false,                                                        // lbPaused
            lSharedInfo,
            *reinterpret_cast<const ControllerInfo*>(saOpaqueControllerInfo),
            true,                                                         // the console's `1`
            *reinterpret_cast<DebugPrinter*>(saOpaqueDebugPrinter));
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

            // ⭐ @0x8224FD30 -- the pass that dispatches every live behaviour's Update. It is
            // REAL now (partially: see the body's FLAG list), which is what lets
            // BehaviourRoadRunner::Update run at all.
            UpdateCameraBehavioursPostScene(lpIO, liPlayerCarIndex);

            // ⚠️ GATE: UpdateMoments( lpIO, liPlayerCarIndex );
            // ⚠️ GATE: if ( !<ICE-owns-frame latch> ) UpdateICE( lpIO, liPlayerCarIndex );

            // ⭐ The arbitrator picks and runs the state that owns this frame's camera.
            UpdateArbitrator(lpIO, lCamera, liPlayerCarIndex);

            // ⚠️ GATE: the ~550-line VMX / effect-hook / world-map remainder (lines 271-823).
        }

        // Finalise: camera inertia + shake (the CameraFinaliser owns the InertiaController).
        mCameraFinaliser.Update(lpIO->mpInputBuffer, &maGameState, lpIO->mpResourceManager,
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

        // ⭐ X360 line 879 -- UpdateAttribSys( lpIO->mpInputBuffer ). REAL as of 2026-08-02,
        // in the console's own position (immediately after PrepareBehaviours). It is INERT on
        // this build by design: its only gate is the controller block's
        // mbGameTalkRefreshRequest byte, which only the live-tuning tool sets. See its body.
        UpdateAttribSys(lpIO->mpInputBuffer);

        // ⚠️ GATE: the rest of the post-publish tail (lines 871-877 / 880-924) -- see the banner.
    }

    // ------------------------------------------------------------------------
    // PostGuiUpdate  @ 0x82236F88   -- PARTIALLY LIVE (the two intro latches), rest still gated
    //
    // The post-GUI pass DirectorModule::PostGuiUpdate runs when not replaying. The X360 body
    // folds this frame's GUI events -- the ones BrnGameModule::BridgeGuiToDirector @0x823CBF70
    // has just published into the director INPUT buffer -- into the director's GameState, then
    // runs the effect interface + the "prepare for mode" action:
    //
    //     if ( input->GetCarSelectionChangedThisFrame() )     <GameState +211328 = 1>
    //     if ( input->HasGotCrashNavShownEvent() )            <set the crash-nav shown latch>
    //     if ( input->HasGotCrashNavHiddenEvent() )           <clear it>
    //     if ( input->HasGotColourCalibrationShownEvent() )   <set the colour-cal latch>
    //     if ( input->HasGotColourCalibrationHiddenEvent() )  <clear it>
    //     if ( input->HasGotShortcutMenuEvent() )
    //         <GameState shortcut-menu state> = input->GetShortcutMenuState();
    //     if ( input->GetEndOfCarSelect() && <mode> )         <mode = 5; clear a flag>
    //     if ( input->HasGotHookEnumeration() )
    //         EffectInterface::Update( maEffectInterface, *input->GetHookEnumeration(), ... )
    //     if ( input->GetRankUpThisFrame() )                  <rank latch + new rank>
    // ⭐  if ( input->GetStartNewProfileIntro() )    mbNewProfileIntroActive = true;
    // ⭐  if ( input->GetStartGameIntroFlyby()  )    mbGameIntroFlybyActive  = true;
    // ⭐  if ( input->GetStopGameIntroFlyby()   )  { mbGameIntroFlybyActive  = false;
    //                                               mbNewProfileIntroActive = false; }
    //     if ( input->HasNewDirectorProfileData() )           <profile data + a derived bool>
    //     ... ( the online post-event / 100%-sequence / mode-action tail )
    //     MainDirector::HandlePrepareForModeAction( this, maModeActionAndDebugBlock, lpIO )
    //
    // ⭐ NEARLY ALL OF IT IS LIVE NOW (2026-08-01). Naming maGameState turned the "un-homed
    // remainder" into ordinary members, so every leg above whose destination is a GameState
    // field is bodied below -- including the two that FEED the junkyard machine:
    //     GetCarSelectionChangedThisFrame()  -> mbJunkyardSelectionChangedMessageWaiting
    //     GetCarSelectTickerClosedThisFrame()-> mbJunkyardCarUnlockTickedClosedThisFrameMessageWaiting
    // (ProcessInputQueue converts both into their one-frame *receivedThisFrame* bits), and
    //     GetEndOfCarSelect() && meJunkyardState != E_JY_INACTIVE -> E_JY_WAITING_FOR_AUDIO.
    // ⚠️ The `<mode>` in the banner's `if ( input->GetEndOfCarSelect() && <mode> ) <mode = 5>`
    //    line is meJunkyardState, and 5 is E_JY_WAITING_FOR_AUDIO -- CORRECTED, it is not a
    //    game mode.
    //
    // ⚠️ STILL GATED (each names a destination this class cannot honestly reach yet):
    //   * the hook-enumeration leg -- BrnDirector::EffectInterface has no reconstructed home;
    //   * the online post-event / mode-action legs -- they write the MainDirector flag tail
    //     (+0x35479/+0x3547A) and call HandlePrepareForModeAction, itself declaration-only;
    //   * HasNewDirectorProfileData's SECOND store, `*(this + 91808) = (data == 1)`, which
    //     lands inside mArbitrator, not the GameState. (Its FIRST store, into
    //     DirectorProfileData +0x08, is an opaque sub-object byte-word -- also left alone.)
    //
    // PostGuiUpdate runs AFTER Update, so every latch here takes effect on the FOLLOWING
    // frame. That is the console's own one-frame delay, not a shortfall.
    // ------------------------------------------------------------------------
    void MainDirector::PostGuiUpdate(const DirectorInputOutput* lpIO)
    {
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;
        if (lpInput == 0)
            return;

        // GUI command 415 -> the junkyard selection-changed handshake's WAITING bit (+0x1A0).
        if (lpInput->GetCarSelectionChangedThisFrame())
            maGameState.mbJunkyardSelectionChangedMessageWaiting = true;

        if (lpInput->HasGotCrashNavShownEvent())
            maGameState.mbCrashNavShown = true;                       // +0x101
        if (lpInput->HasGotCrashNavHiddenEvent())
            maGameState.mbCrashNavShown = false;

        if (lpInput->HasGotColourCalibrationShownEvent())
            maGameState.mbColourCalibrationShown = true;              // +0x102
        if (lpInput->HasGotColourCalibrationHiddenEvent())
            maGameState.mbColourCalibrationShown = false;

        if (lpInput->HasGotShortcutMenuEvent())
            maGameState.mbShortCutMenuShown = lpInput->GetShortcutMenuState();   // +0x103

        // ⭐ GUI command 192 -- the car-select audio hold. Note the SECOND half of the test:
        // the console only takes this arm while the junkyard is already active, so an
        // end-of-car-select that arrives outside the junkyard is ignored, not a state forge.
        if (lpInput->GetEndOfCarSelect() &&
            maGameState.meJunkyardState != GameState::E_JY_INACTIVE)
        {
            maGameState.meJunkyardState        = GameState::E_JY_WAITING_FOR_AUDIO;   // = 5
            maGameState.mbJunkyardCarModActive = false;
        }

        // ⚠️ GATE: the hook-enumeration leg -> EffectInterface::Update (un-homed), and its
        //    else-arm's flag-tail store.

        // GUI command 303 -- the rank-up handshake's WAITING bit + the new rank.
        if (lpInput->GetRankUpThisFrame())
        {
            maGameState.mbRankUpMessageWaiting = true;                // +0x1AA
            maGameState.miRankUpNewRank        = lpInput->GetRankUpNewRank();   // +0x1AC
        }

        if (lpInput->GetStartNewProfileIntro())
            maGameState.mbNewProfileIntroActive = true;               // +0xD8 (GameState +216)

        if (lpInput->GetStartGameIntroFlyby())
            maGameState.mbGameIntroFlybyActive = true;                // +0xD9 (GameState +217)

        if (lpInput->GetStopGameIntroFlyby())
        {
            maGameState.mbGameIntroFlybyActive  = false;
            maGameState.mbNewProfileIntroActive = false;
        }

        // ⚠️ GATE: HasNewDirectorProfileData (opaque sub-object word + an arbitrator field),
        //    and the online post-event / mode-action pair.

        // GUI command 77 -> the car-unlock-ticker handshake's WAITING bit (+0x1A4).
        if (lpInput->GetCarSelectTickerClosedThisFrame())
            maGameState.mbJunkyardCarUnlockTickedClosedThisFrameMessageWaiting = true;

        // GUI commands 480 / 479 -- the online race-intro bar/clamp trio.
        if (lpInput->GetFinishedOnlineEventLoading())
        {
            maGameState.mbOnlineRaceIntroCanUseBars        = true;     // +0x1A9
            maGameState.mbOnlineCarSelectCanStartRaceIntro = true;     // +0x1A7
        }
        if (lpInput->GetStartedOnlineEventLoading())
        {
            maGameState.mbOnlineRaceIntroCanUseBars     = false;
            maGameState.mbOnlineCarSelectMustClampToCar = true;        // +0x1A8
        }

        // GUI commands 469 / 470 -- the 100%-completion sequence latch.
        if (lpInput->GetStarting100PercentSequence())
            maGameState.mbDoing100PercentSequence = true;              // +0x1B0
        if (lpInput->GetFinished100PercentSequence())
            maGameState.mbDoing100PercentSequence = false;
    }
}
