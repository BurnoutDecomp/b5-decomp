// ============================================================================
// GameSource/Director/BrnMainDirector.cpp
//
// BrnDirector::MainDirector -- the top-level cinematic camera director. Compilation
// home for the 21-function MainDirector class TU. The ENGINE source path the X360
// asserts quote is "..\..\..\GameSource\Director/BrnMainDirector.cpp", so this file
// mirrors it.
//
// BODIED here (faithfully reconstructed from the X360 asm; they touch only the attested
// stage scalars / list heads and committed-destructor calls):
//   * MainDirector()  -- ctor: in-place build the owned sub-objects + seed the -1 sentinels
//   * Release()       -- the staged RELEASE state machine
//   * Destruct()      -- null the four bookkeeping list heads + destruct collision-gen + ICE
//   * GetICEWrapper() / GetArbitrator() -- the committed-sub-object accessors
//
// DECLARATION-ONLY + FLAGGED (in the header): the remaining 16 functions -- the Construct
// VMX/LCG seed pipeline, the Update spine and its sub-updates (UpdateArbitrator / UpdateICE /
// UpdateMoments / UpdateAttribSys / UpdateCameraBehaviours* / UpdateDebug* / ProcessInputQueue /
// ProcessNewVehicleEvents / HandlePrepareForModeAction / CalcTrafficLightSpace /
// DebugDisplayCurrentCamera / PreSceneQueryUpdate / PostGuiUpdate / Prepare). Each indexes the
// NOT-HOMED BehaviourManager / AllVehicleData / GameState-action aggregates, paraphrases a VMX
// pipeline, depends on un-dumped rodata, or calls a declaration-only sibling -- the rules forbid
// bodying any of those. They are documented + declared in BrnMainDirector.h and resolve to their
// real bodies when the dependent TUs land.
//
// LAYOUT NOTE: MainDirector is modelled as a sized opaque buffer (see BrnMainDirector.h). The
// bodied functions address their fields at the X360-attested CONSOLE byte offsets through a
// char*/typed view of `this` -- the established convention in the committed BrnDirectorModule.cpp.
// ============================================================================

#include "GameSource/Director/BrnMainDirector.h"

#include "GameSource/Director/BrnDirectorICEWrapper.h"   // BrnDirector::ICEWrapper (embedded sub-object: Destruct)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitrator.h" // BrnDirector::Arbitrator (embedded sub-object accessor)
#include "GameSource/Director/DirectorModule/BrnDirectorInputOutput.h" // BrnDirector::DirectorInputOutput (the per-frame bundle)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"    // DirectorIO::InputBuffer (GetPlayerCarIndex / GetUsedRaceCars)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp" // DirectorIO::OutputBuffer (SetCgsCamera / SetCameraOutput)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h" // BrnDirector::DebugComponent (UpdatePanoramaScreenshots)
#include "GameSource/Director/Camera/Camera.h"           // BrnDirector::Camera::Camera (the frame camera)
#include "GameSource/Director/Camera/BrnCameraFinaliser.h" // BrnDirector::CameraFinaliser (the embedded +0x12480 finaliser)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // External sub-object types this TU only references by call (their full homes are
    // separate TUs; the per-TU `cl /c` gate compiles against these minimal externals and the
    // real symbols resolve at link time -- the BrnDirectorModule.cpp convention).
    // ------------------------------------------------------------------------

    // FLAG: CgsSceneManager::CgsCollision::BaseCollisionGenerator has no reconstructed home
    //   layout yet (the committed CgsSceneManagerModule.h forward-declares it only). Destruct /
    //   Release call its Destruct() on the embedded CgsGraphics::Camera / collision-generator
    //   object; declared here as a minimal external with just that member so the call compiles.
    //   Replace with the real home when the collision-generator TU lands.
}

namespace CgsSceneManager { namespace CgsCollision {
    struct BaseCollisionGenerator { void Destruct(); };
} }

namespace BrnDirector
{
    // ICETake, ArbitratorStateContainer, BehaviourManager and CarScoreData are the owned
    // sub-objects the ctor builds in place. Their construction is performed at their attested
    // regions via these minimal external constructors (placement-new), exactly as
    // BrnDirectorModule.cpp builds its owned sub-objects. FLAG: minimal externals -- the real
    // constructors live in each type's own TU.
    namespace ICE_External { struct ICETake { ICETake(); }; }
    struct ICEWrapperBuild           { ICEWrapperBuild(); };          // -> ICEWrapper() at +0x50
    struct ArbitratorStateContainerB { ArbitratorStateContainerB(); }; // -> at +0x12DB4 region
    struct BehaviourManagerBuild     { BehaviourManagerBuild(); };    // -> at +0x1CB10 region
    struct CarScoreDataBuild         { CarScoreDataBuild(); };        // -> at +0x33760 region

    // ------------------------------------------------------------------------
    // small typed-view helpers (the BrnDirectorModule.cpp idiom: address the attested
    // CONSOLE offsets through a char* view of `this`; never a host-layout cast).
    // ------------------------------------------------------------------------
    static inline char* lpByteView(MainDirector* lpThis)
    {
        return reinterpret_cast<char*>(lpThis);
    }
    static inline s32& lrStageWord(MainDirector* lpThis)
    {
        return *reinterpret_cast<s32*>(lpByteView(lpThis) + MainDirector::KU_OFF_STAGE);
    }
    static inline s32& lrStageCounter(MainDirector* lpThis)
    {
        return *reinterpret_cast<s32*>(lpByteView(lpThis) + MainDirector::KU_OFF_STAGE_COUNTER);
    }

    // ------------------------------------------------------------------------
    // GetICEWrapper / GetArbitrator -- the embedded committed sub-objects, reached at their
    // attested CONSOLE regions (parity by name; the offset is provenance).
    // ------------------------------------------------------------------------
    ICEWrapper& MainDirector::GetICEWrapper()
    {
        return *reinterpret_cast<ICEWrapper*>(lpByteView(this) + KU_OFF_ICE_WRAPPER);
    }

    Arbitrator& MainDirector::GetArbitrator()
    {
        // The embedded Arbitrator region (CONSOLE +0x12DC0: the ctor builds the state container
        // at +0x12DC0+0x310 and seeds the arbitrator vtables there -- BrnDirectorArbitrator.h).
        return *reinterpret_cast<Arbitrator*>(lpByteView(this) + 0x12DC0);
    }

    // ------------------------------------------------------------------------
    // MainDirector (ctor)  @ X360 0x827E4AB8  (EXECUTED in goal trace)
    //
    // Build the owned sub-objects in place and seed the -1 sentinel index fields the asm
    // stores. The asm sequence (provenance):
    //   ICEWrapper()                    at +0x50
    //   word -1 -> +0x122B8/+0x12384/+0x12450   (three -1 sentinel indices, r11=+0x121F0)
    //   ICETake()                       at +0x124F0
    //   word -1 -> +0x12DB4                      (r10=+0x12DB4)
    //   ArbitratorStateContainer()      at +0x130D0   (arbitrator region +0x12DC0 + 0x310)
    //   ...embedded arbitrator special-state vtable installs at +0x3910/+0x41A0/+0x4340 of the
    //      arbitrator region (NOT reproduced here; they belong to the embedded Arbitrator)
    //   BehaviourManager()              at +0x1CB10   (r3 = +0x1CB10)
    //   word -1 -> +0x34974                      (r9, stwx)
    //   CarScoreData()                  at +0x33760
    //   word -1 -> +0x34DC0                      (0x250 off r11=+0x34B70)
    //   a 16-entry table of -1 from +0x34DF8 (stride 0x2C)   -- the per-rival index table
    //   word -1 -> +0x35090 (0x2C0 off r8=+0x34DD0) / +0x353A8 (0x838 off r11=+0x34B70)
    //
    // BODIED FAITHFULLY for the parts that touch only the attested sentinel fields + the owned
    // sub-object placement-builds. The owned sub-objects are constructed via minimal external
    // builders (placement-new at their attested regions) -- the BrnDirectorModule.cpp pattern.
    //
    // FLAG: the embedded BehaviourManager and the arbitrator special-state vtable installs are
    //   un-homed; the BehaviourManager build is performed by its external builder (its real ctor
    //   resolves at link), and the per-rival -1 index table is seeded by attested offset/stride.
    //   The vtable-pointer installs the asm makes into the arbitrator region are NOT reproduced
    //   here (they belong to the embedded Arbitrator's own construction, performed in its region).
    // ------------------------------------------------------------------------
    MainDirector::MainDirector()
    {
        char* lpBase = lpByteView(this);

        // Owned sub-objects, built in place at their attested regions.
        new (reinterpret_cast<void*>(lpBase + KU_OFF_ICE_WRAPPER)) ICEWrapperBuild();      // +0x50
        new (reinterpret_cast<void*>(lpBase + 0x124F0))           ICE_External::ICETake(); // +0x124F0
        new (reinterpret_cast<void*>(lpBase + 0x130D0))           ArbitratorStateContainerB(); // +0x130D0
        new (reinterpret_cast<void*>(lpBase + 0x1CB10))           BehaviourManagerBuild();  // +0x1CB10
        new (reinterpret_cast<void*>(lpBase + 0x33B50))           CarScoreDataBuild();      // +0x33B50 (asm: addis r3,r30,3 -> +0x30000, addi r3,r3,0x3B50)

        // -1 sentinel index fields (asm word stores, offsets read straight from the asm).
        *reinterpret_cast<s32*>(lpBase + 0x122B8) = -1;   // stw r31,0xC8(r11=+0x121F0)
        *reinterpret_cast<s32*>(lpBase + 0x12384) = -1;   // stw r31,0x194(r11)
        *reinterpret_cast<s32*>(lpBase + 0x12450) = -1;   // stw r31,0x260(r11)
        *reinterpret_cast<s32*>(lpBase + 0x12DB4) = -1;   // stwx r31,r30,(r10=+0x12DB4)
        *reinterpret_cast<s32*>(lpBase + 0x34974) = -1;   // stwx r31,r30,(r9=+0x34974)
        *reinterpret_cast<s32*>(lpBase + 0x34DC0) = -1;   // stw r31,0x250(r11=+0x34B70)

        // The 16-entry per-rival index table seeded to -1: first store at +0x34DF8 (r9 = r8+0x28,
        // r8 = +0x34DD0), stride 0x2C bytes, 16 iterations (the asm's r10 = 0xF..-1 down-count).
        char* lpTable = lpBase + 0x34DF8;
        for (s32 liEntry = 15; liEntry >= 0; --liEntry)
        {
            *reinterpret_cast<s32*>(lpTable) = -1;
            lpTable += 0x2C;
        }

        *reinterpret_cast<s32*>(lpBase + 0x35090) = -1;   // stw r31,0x2C0(r8=+0x34DD0)
        *reinterpret_cast<s32*>(lpBase + 0x353A8) = -1;   // stw r31,0x838(r11=+0x34B70)
    }

    // ------------------------------------------------------------------------
    // Release  @ X360 0x82236EB0
    //
    // The staged RELEASE state machine. The stage word (CONSOLE +0x35424) selects the case;
    // each case advances it and the cases fall through (1->2->3->5). On the first (stage 0)
    // it destructs the collision generator; on the last (stage 5) it clears the stage counter
    // (+0x35420 -> 0) and reports completion. An out-of-range stage asserts and reports failure.
    //
    // (The asm "case 4" is the default branch -- there is no case 4 in the jump table.)
    // Faithful to the asm: NO added control flow; the fall-through chain matches the jump table.
    // ------------------------------------------------------------------------
    bool MainDirector::Release()
    {
        s32& lrStage = lrStageWord(this);

        switch (lrStage)
        {
        case 0:
            // Release case-0 tears down the object at the ICE-wrapper region (CONSOLE +0x50):
            // asm 0x82236F0C `addi r3,r30,0x50` -> BaseCollisionGenerator::Destruct(this+0x50).
            // (The FULL Destruct() below tears down the collision generator at +0x349D0; these
            //  are different call sites and must not be conflated.)
            reinterpret_cast<CgsSceneManager::CgsCollision::BaseCollisionGenerator*>(
                lpByteView(this) + KU_OFF_ICE_WRAPPER)->Destruct();
            // fall through
        case 1:
            lrStage = 1;
            // fall through
        case 2:
            lrStage = 2;
            // fall through
        case 3:
            lrStage = 3;
            // fall through
        case 5:
            lrStage = 5;
            lrStageCounter(this) = 0;
            return true;

        default:
            CGS_ASSERT(false, "Invalid Stage\n");
            return false;
        }
    }

    // ------------------------------------------------------------------------
    // Destruct  @ X360 0x8224FCC0
    //
    // Null the four per-frame bookkeeping list heads (64-bit zero stores), destruct the embedded
    // CgsGraphics::Camera / collision-generator object, then destruct the embedded ICE wrapper.
    // Faithful to the asm store-for-store.
    // ------------------------------------------------------------------------
    void MainDirector::Destruct()
    {
        char* lpBase = lpByteView(this);

        // Four list/tree heads nulled (the asm stores a 64-bit zero into each).
        *reinterpret_cast<u64*>(lpBase + KU_OFF_LISTHEAD_A) = 0;   // +0x1CAF8
        *reinterpret_cast<u64*>(lpBase + KU_OFF_LISTHEAD_B) = 0;   // +0x24808
        *reinterpret_cast<u64*>(lpBase + KU_OFF_LISTHEAD_C) = 0;   // +0x2C638
        *reinterpret_cast<u64*>(lpBase + KU_OFF_LISTHEAD_D) = 0;   // +0x2F038

        // Destruct the embedded collision generator (CONSOLE +0x349D0).
        reinterpret_cast<CgsSceneManager::CgsCollision::BaseCollisionGenerator*>(
            lpBase + KU_OFF_COLLISION_GENERATOR)->Destruct();

        // Destruct the embedded ICE wrapper (CONSOLE +0x50).
        GetICEWrapper().Destruct();
    }

    // ========================================================================
    //  THE PER-FRAME DIRECTOR SPINE  (reconstructed in the director wave)
    //
    //  ADDITIONAL ATTESTED CONSOLE OFFSETS used below (all read straight off the X360
    //  bodies; provenance only -- every one is reached through a NAMED helper here, never
    //  poked inline):
    //    +0x00040 (64)      &DebugComponent  -- the module's debug component back-pointer
    //                       (DirectorModule::Construct @0x8225C590 stores `this+552` into
    //                        module+2880 == MainDirector+0x40)
    //    +0x12480 (74880)   CameraFinaliser  (CameraFinaliser::Update call site)
    //    +0x32F10 (208656)  mLastCamera      (Camera::operator= both ways: the no-player
    //                       early-out reads it, the tail writes it back)
    //    +0x337E0 (210912)  the camera-state block CameraFinaliser::Update takes
    //    +0x33140 (209152)  the "forced camera car" override index
    //    +0x349D0 (215504)  the published CgsGraphics::Camera (== GetCgsCamera)
    //    +0x35420/+0x35424  the staged-init counter / stage word (KU_OFF_STAGE*)
    // ========================================================================

    // ------------------------------------------------------------------------
    // GetCgsCamera -- the embedded CgsGraphics::Camera at CONSOLE +0x349D0.
    //
    // MainDirector::Update fills it (Camera::Camera::CopyToCgsCamera) and publishes it
    // (OutputBuffer::SetCgsCamera); DirectorModule::Update then copies it into the module's
    // own mCgsCamera. Exposed by name so no caller reaches into this class's storage.
    // ------------------------------------------------------------------------
    CgsGraphics::Camera& MainDirector::GetCgsCamera()
    {
        return *reinterpret_cast<CgsGraphics::Camera*>(lpByteView(this) + KU_OFF_COLLISION_GENERATOR);
    }

    const CgsGraphics::Camera& MainDirector::GetCgsCamera() const
    {
        return *reinterpret_cast<const CgsGraphics::Camera*>(
                   reinterpret_cast<const char*>(this) + KU_OFF_COLLISION_GENERATOR);
    }

    namespace
    {
        // Attested CONSOLE offsets of the three further sub-objects this TU's per-frame
        // bodies reach. Kept file-local (they are this TU's provenance, not API).
        enum : u32
        {
            KU_OFF_DEBUG_COMPONENT_PTR = 0x00040,   // MainDirector +0x40  -> DebugComponent*
            KU_OFF_CAMERA_FINALISER    = 0x12480,   // MainDirector +0x12480
            KU_OFF_LAST_CAMERA         = 0x32F10,   // MainDirector +0x32F10
            KU_OFF_FORCED_CAMERA_CAR   = 0x33140,   // MainDirector +0x33140
            KU_OFF_CAMERA_STATE_BLOCK  = 0x337E0,   // MainDirector +0x337E0
        };
    }

    // The frame camera the director carries over between frames (the arbitrator's result,
    // finalised). Reached by name; see the offset table above.
    static inline Camera::Camera& lrLastCamera(MainDirector* lpThis)
    {
        return *reinterpret_cast<Camera::Camera*>(
                   reinterpret_cast<char*>(lpThis) + KU_OFF_LAST_CAMERA);
    }

    static inline CameraFinaliser& lrCameraFinaliser(MainDirector* lpThis)
    {
        return *reinterpret_cast<CameraFinaliser*>(
                   reinterpret_cast<char*>(lpThis) + KU_OFF_CAMERA_FINALISER);
    }

    // ------------------------------------------------------------------------
    // Construct  @ 0x8225B448   (EXECUTED in goal trace)
    //
    // Build the director runtime. The X360 call/store sequence, in order:
    //
    //     mStage        = 5;                        // +0x35424
    //     mConstructTime = lfTime;                  // +0x35428 (double)
    //     mStageCounter = 0;                        // +0x35420
    //     DirectorDevTools::Construct( this, this, lpResourceManager );
    //     <CgsGraphics::Camera ctor>( this + 215504 );        // sub_827F94E8, +0x349D0
    //     CgsGraphics::Camera::SetFovHorizontal( this + 215504, ... );
    //     CgsGraphics::Camera::UpdatePerspectiveProjectionMatrix( this + 215504 );
    //     Camera::Camera::Construct( this + 208656 );         // mLastCamera, +0x32F10
    //     AllVehicleData::Construct( this + 76928 );
    //     <five flag bytes cleared around +215412..+215497>
    //     Camera::BehaviourManager::Construct( this + 117520 );
    //     CGS_ASSERT( lpResourceManager != NULL );            // BrnBehaviourManager.h:168
    //     *(this + 208588) = lpResourceManager;               // the manager back-pointer
    //     MomentParameterBank::Construct( this + 117440 );
    //     Arbitrator::Construct( this + 77248 );              // == GetArbitrator(), +0x12DC0
    //     <the VMX + 1284865837-multiplier LCG camera-shake seed pipeline>
    //     KeyAnimShakeController::Construct( this + 74960, lpResourceManager );
    //     ShotSelector::Construct( this + 74224, lpResourceManager );
    //     ICEWrapper::Construct( this + 80 );
    //     GameState::Clear( this + 210912 );
    //     DebugPrinter::Construct( this + 210864 / +210792 / +210828 );
    //
    // BODIED HERE: the three staged-init words and BOTH CAMERAS. That subset is deliberately
    // chosen and it is the one this wave depends on:
    //   * `Camera::Camera::Construct(this + 208656)` builds mLastCamera -- the camera Update's
    //     no-player path copies into the frame camera and then publishes. Without it the
    //     director would publish RAW UNINITIALISED STORAGE on frame 1 and
    //     ValidateTransformWithDebugInfo would assert on the NaNs (and a dev assert stops the
    //     sim). Leaving Construct declaration-only while bodying Update would have been a
    //     latent crash, not a gap.
    //   * the CgsGraphics::Camera at +0x349D0 is the one GetCgsCamera names and SetCgsCamera
    //     publishes. Its committed Construct() already sets the default FOV and calls
    //     SetFovHorizontal internally, so the explicit SetFovHorizontal the asm makes right
    //     after is redundant here -- and its FOV argument is an UNINITIALISED fp register in
    //     the decompilation (a Hex-Rays artefact of the PPC fp calling convention), i.e. NOT
    //     recovered, so calling it with a guessed value would be strictly worse than relying
    //     on Construct's own default. FLAG: that one call is not reproduced.
    //
    // ⚠️ EVERYTHING ELSE IS A DOCUMENTED QUIET GATE, for the reasons the header already
    // records: DirectorDevTools / AllVehicleData / Camera::BehaviourManager /
    // MomentParameterBank / KeyAnimShakeController / ShotSelector / GameState / DebugPrinter
    // have no homed layouts at their attested offsets, and the camera-shake seed is a VMX+LCG
    // pipeline the reconstruction rules forbid paraphrasing to scalar.
    //
    // ⚠️ TWO SUB-OBJECT BUILDS ARE HELD BACK FOR A DIFFERENT, SHARPER REASON -- read this
    // before un-gating them. MainDirector is an OPAQUE 0x35450-byte buffer and the sub-objects
    // are placed at their CONSOLE offsets, but the HOST types are WIDER (their embedded
    // pointers widen 4->8). For most of them that is harmless because the neighbouring space
    // is unused. It is NOT harmless for these two:
    //   * ICEWrapper::Construct( this + 0x50 ) -- the console ICEWrapper runs to ~+0x1210C,
    //     and this TU now places the CameraFinaliser at +0x12480. The host ICEWrapper is
    //     wider; if it grows past 0x12430 bytes it will run INTO the finaliser and silently
    //     corrupt the inertia controller. Console-safe, host-unproven.
    //   * Arbitrator::Construct( this + 0x12DC0 ) -- console span 0x12DC0..0x172C8; the host
    //     Arbitrator (embedded states, handles and Cameras all widen) has no proven size.
    // Both are held until those two host sizes are pinned. Consequence: no arbitrator, hence
    // no arbitrator STATE, hence no ArbStateAttractMode -- which is exactly the gate the DJ
    // flyby sits behind, together with the BehaviourManager one.
    //
    // DELETE-WHEN: per sub-object, as each type is homed; for the two above, additionally once
    // `sizeof` on the host is measured against the console placement window (a static_assert
    // in this TU is the natural way to hold that invariant once the types exist).
    // ------------------------------------------------------------------------
    void MainDirector::Construct(const DirectorResourceManager* lpResourceManager, f32 lfTime)
    {
        CGS_ASSERT(lpResourceManager != 0, "lpDirectorResourceManager != NULL");

        lrStageWord(this)    = 5;   // +0x35424
        lrStageCounter(this) = 0;   // +0x35420

        *reinterpret_cast<f64*>(lpByteView(this) + KU_OFF_CONSTRUCT_TIME) =
            static_cast<f64>(lfTime);   // +0x35428 (the asm stores the incoming double)

        // The published graphics camera (+0x349D0). Construct() seeds the default
        // FOV/aspect/clip planes and the identity view; then rebuild the projection exactly as
        // the asm does. (See the SetFovHorizontal FLAG in the banner.)
        GetCgsCamera().Construct();
        GetCgsCamera().UpdatePerspectiveProjectionMatrix();

        // mLastCamera (+0x32F10) -- the carried-over frame camera Update reads and writes.
        lrLastCamera(this).Construct();

        // Everything else: gated (see the banner).
    }

    // ------------------------------------------------------------------------
    // Prepare  @ 0x8224FB38
    //
    // The staged PREPARE state machine DirectorModule::Prepare pumps at its own stage 4.
    // The stage word (KU_OFF_STAGE, CONSOLE +0x35424) selects the entry case; the cases fall
    // through, so one call advances as far as it can:
    //
    //   0 -> tear the collision generator down, then register the dev-tools GameTalk message
    //        handler under the name "Camera", and zero the head word
    //   1 -> zero the frame counter (+0x12454)
    //   2 -> ICEWrapper::Prepare
    //   4 -> Camera::BehaviourManager::Prepare
    //   5 -> seed the 20-entry behaviour-helper index table (19..0 descending) + set its
    //        count to 20, and clear the +0x1CAB8 list head
    //   6 -> (empty)
    //   7 -> done: clear the stage counter, report success
    // (there is deliberately NO case 3 in the X360 jump table -- reproduced.)
    //
    // Any sub-Prepare returning false reports false without advancing; the framework retries.
    // An out-of-range stage asserts (BrnMainDirector.cpp:224).
    //
    // ⚠️ TWO STAGES ARE DOCUMENTED QUIET GATES (each is skipped, not trapped, and the machine
    // still advances so the director comes up):
    //   * stage 0's GameTalk registration -- EA::GameTalk::GameTalkManager has no
    //     reconstructed home. Consequence: the "Camera" dev-tools commands
    //     (Start/StopRenderMetrics, the ICE editor hooks) are not reachable from GameTalk.
    //     Nothing in the game path uses them. DELETE-WHEN: the GameTalk manager is homed.
    //   * stage 4/5's Camera::BehaviourManager::Prepare + the 20-entry helper index table --
    //     BrnBehaviourManager.h is forward-decl-only, so the type has no layout and the table
    //     has no named member. This is THE blocker for the whole camera-behaviour system
    //     (see MainDirector::Update). Consequence: no camera behaviour can be allocated, so
    //     the director publishes its carried-over camera rather than a behaviour-driven one.
    //     DELETE-WHEN: BrnDirector::Camera::BehaviourManager gets a real layout + Prepare.
    //
    // The collision-generator teardown in stage 0 IS reproduced (its callee is committed) --
    // note it targets CONSOLE +0x349D0, the same region GetCgsCamera names, exactly as
    // Destruct() does.
    // ------------------------------------------------------------------------
    bool MainDirector::Prepare(DirectorIO::OutputBuffer* lpOutputBuffer, s32 liPrepareArg,
                               const DirectorResourceManager* lpResourceManager)
    {
        s32& lrStage = lrStageWord(this);

        switch (lrStage)
        {
        case 0:
            reinterpret_cast<CgsSceneManager::CgsCollision::BaseCollisionGenerator*>(
                lpByteView(this) + KU_OFF_COLLISION_GENERATOR)->Destruct();
            // ⚠️ GATE: EA::GameTalk::GameTalkManager::GetInstance()->RegisterMessageHandler(
            //              BrnDirector::DirectorDevTools::GameTalkMsgHandler, "Camera" );
            //   -- un-homed GameTalk API (see the banner).
            // fall through
        case 1:
            lrStage = 1;
            // fall through
        case 2:
            lrStage = 2;
            // asm: ICEWrapper::Prepare(this+80, a2, a3, a4) -- the wrapper pumps its own ICE
            // resource acquisition through the director output buffer, forwarding the
            // prepare arg and the module's resource manager.
            if (!GetICEWrapper().Prepare(lpOutputBuffer, liPrepareArg, lpResourceManager))
                return false;
            // fall through
        case 4:
            lrStage = 4;
            // ⚠️ GATE: Camera::BehaviourManager::Prepare(this + 117520) -- un-homed (banner).
            // fall through
        case 5:
            lrStage = 5;
            // ⚠️ GATE: seed the behaviour-helper index table at this+117344 with 19..0 and
            //   set this+117424 = 20; clear the list head at this+117432. All inside the
            //   un-homed BehaviourManager aggregate (banner).
            // fall through
        case 6:
            lrStage = 6;
            // fall through
        case 7:
            lrStage = 7;
            lrStageCounter(this) = 0;
            return true;

        default:
            CGS_ASSERT(false, "Invalid Stage\n");
            return false;
        }
    }

    // ------------------------------------------------------------------------
    // PreSceneQueryUpdate  @ 0x8225BA00
    //
    // The pre-scene-query pass. Its ENTIRE body is guarded by one condition, and that guard
    // is reproduced here faithfully: the director only does pre-query work when there is a
    // LIVE PLAYER CAR -- i.e. when the effective car index is not -1 AND its bit is set in
    // the input buffer's used-race-car mask. The effective index is the input buffer's
    // player-car index, overridden by the director's own "forced camera car" index
    // (CONSOLE +0x33140) whenever that override is >= 0 and itself a used race car.
    //
    // Inside that guard the X360 runs, in order:
    //     AllVehicleData::Update( ..., usedRaceCars, vehicleInfoArray, playerIndex, contacts, ... )
    //     MainDirector::ProcessInputQueue( this, lpIO )
    //     if ( !lpIO->mpInputBuffer[+31432] )   // sim not paused
    //     {
    //         <forced camera car> = playerIndex;
    //         VehicleTracker::Update( this+211424, this+210912, input, playerIndex, this+218167 );
    //     }
    //     CrashAnalyser::Update( this+74844, input, this+210912, playerIndex );
    //     Camera::BehaviourManager::ReleaseBehaviours( this+117520 );
    //     if ( this[+218160] )  this[+211173] = 0;
    //     MainDirector::UpdateCameraBehavioursPreScene( this, lpIO, playerIndex );
    // and it also clears the "ICE just finished" latch at the very top
    // (`if (this[+218173]) this[+218172] = 0;`).
    //
    // ⚠️ THE GUARDED BODY IS A DOCUMENTED QUIET GATE. Every one of those seven steps lands on
    // a not-yet-homed aggregate: AllVehicleData / VehicleTracker / CrashAnalyser are reached
    // at raw offsets inside the opaque MainDirector storage with no named members;
    // Camera::BehaviourManager has no layout; ProcessInputQueue and
    // UpdateCameraBehavioursPreScene are themselves declaration-only for the same reason.
    //
    // WHY BODY THE GUARD AT ALL: because the guard is the part that decides whether ANYTHING
    // happens, and on the current PC bring-up it is FALSE every frame (there is no player
    // vehicle, so the input buffer's player-car index is the -1 "none" sentinel and the
    // used-race-car mask is empty). So this reconstruction is behaviourally EXACT today --
    // the console does nothing here either -- and it becomes partial, not wrong, the moment a
    // player car exists.
    //
    // DELETE-WHEN: AllVehicleData / VehicleTracker / CrashAnalyser / BehaviourManager are
    // homed and ProcessInputQueue + UpdateCameraBehavioursPreScene are bodied.
    // ------------------------------------------------------------------------
    void MainDirector::PreSceneQueryUpdate(const DirectorInputOutput* lpIO)
    {
        // The "ICE sequence just finished" latch (asm: if (this[+218173]) this[+218172] = 0).
        // GATED with the rest of the un-named tail; see the banner.

        if (!IsPlayerCarLive(lpIO))
            return;

        // ⚠️ GATE -- the seven guarded steps above.
    }

    // ------------------------------------------------------------------------
    // IsPlayerCarLive -- the shared "is there a live player car this frame" predicate.
    //
    // Both Update @0x82274070 and PreSceneQueryUpdate @0x8225BA00 open with the SAME
    // sequence (the X360 emits it twice, inlined):
    //
    //     index = lpInputBuffer->GetPlayerCarIndex();
    //     forced = *(s32*)(this + 0x33140);
    //     if ( forced > -1 && lpInputBuffer->GetUsedRaceCars()->IsBitSet(forced) )
    //         index = forced;                         // the director's camera-car override wins
    //     if ( index == -1 )  return false;
    //     return lpInputBuffer->GetUsedRaceCars()->IsBitSet(index);
    //
    // (the CgsBitArray.h:203 "invalid index : N < 8" assert both call sites bake is the
    //  BitArray bounds check inlined -- CGS_ASSERT carries it here.)
    //
    // ⚠️ NOTE ON THE BIT TEST. The X360 emits it as
    //     ((1 << (index & 0x3F)) & ((1 << (index & 0x3F)) >> 32)) != 0
    // which is a Hex-Rays artefact of the 64-bit `rldicl`/`and` pair the PPC uses to test one
    // bit of the 64-bit BitArray<8> word -- it is NOT a literal shift-by-32 of a shifted 1.
    // Reproduced as the committed BitArray query it actually is, which is what the DWARF
    // member type (CgsContainers::BitArray<8u> mUsedRaceCars) says it must be.
    // ------------------------------------------------------------------------
    bool MainDirector::IsPlayerCarLive(const DirectorInputOutput* lpIO) const
    {
        const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;

        s32 liIndex = static_cast<s32>(lpInput->GetPlayerCarIndex());

        const s32 liForced = *reinterpret_cast<const s32*>(
            reinterpret_cast<const char*>(this) + KU_OFF_FORCED_CAMERA_CAR);

        const CgsContainers::BitArray<8u>* lpUsedRaceCars = lpInput->GetUsedRaceCars();

        if (liForced > -1)
        {
            CGS_ASSERT(liForced < 8, "invalid index");
            if (liForced < 8 && lpUsedRaceCars->IsBitSet(static_cast<u32>(liForced)))
                liIndex = liForced;
        }

        if (liIndex == -1)
            return false;

        CGS_ASSERT(liIndex < 8, "invalid index");
        return liIndex < 8 && lpUsedRaceCars->IsBitSet(static_cast<u32>(liIndex));
    }

    // ------------------------------------------------------------------------
    // Update  @ 0x82274070   -- THE FUNCTION THAT PUBLISHES THE CAMERA
    //
    // Shape of the X360 body (935 lines of pseudocode; the structure is what matters):
    //
    //     Camera::Camera lCamera;  lCamera.Construct();          // line 191, a STACK camera
    //     <the IsPlayerCarLive prologue>                         // lines 190-247
    //     if ( !live )   lCamera = mLastCamera;                  // LABEL_100, line 250
    //     else           <the ~570-line gameplay middle>         // lines 252-823
    //     <two small bookkeeping stores>                         // lines 824-834
    //     CameraFinaliser::Update( &mCameraFinaliser, input,
    //                              this+0x337E0, resourceMgr, &lCamera );   // line 835
    //     <slomo clamp + assert>                                 // lines 836-848
    //     mLastCamera = lCamera;                                 // line 851  <-- carry over
    //     <flag bookkeeping>                                     // lines 852-865
    //     DebugComponent::UpdatePanoramaScreenshots( *(this+64), &lCamera );  // line 866
    //     lCamera.ValidateTransformWithDebugInfo();              // line 867
    //     lCamera.CopyToCgsCamera( &GetCgsCamera() );            // line 868
    //     output->SetCgsCamera( GetCgsCamera() );                // line 869  <-- PUBLISH
    //     output->SetCameraOutput( lCamera );                    // line 870  <-- PUBLISH
    //     TimerRequests::SetTimestepMultiplier( ... );           // lines 871-875
    //     UpdateDebugInfo / DebugDisplayCurrentCamera /
    //     BehaviourManager::PrepareBehaviours / UpdateAttribSys  // lines 876-879
    //     <debug flag printers + latches>                        // lines 880-924
    //
    // EVERYTHING OUTSIDE THE `else` BRANCH IS RECONSTRUCTED HERE. That is deliberate and it
    // is the whole point of this wave: SetCameraOutput / SetCgsCamera have exactly TWO
    // callers in the entire binary -- this function and ReplayDirector::Update -- so this
    // tail IS the director's camera publish, and with it bodied the module produces a real
    // camera into the output buffer every frame.
    //
    // ⚠️ THE `else` BRANCH (lines 252-823) IS A DOCUMENTED QUIET GATE. It is the gameplay
    // middle: UpdateDebugPrinters, UpdateCameraBehavioursPostScene, UpdateMoments, UpdateICE,
    // UpdateArbitrator (@0x82271120 -> Arbitrator::Update @0x8226ADA0 -> the 12 arbitrator
    // states, including ArbStateAttractMode and ArbStateDriveThru), then ~550 lines of
    // VMX-dominated AllVehicleData / behaviour-parameter work. Every one of those routes
    // through BrnDirector::Camera::BehaviourManager, whose header is forward-decl-only, so
    // there is no layout to index and no behaviour to allocate; Arbitrator::Update is itself
    // declaration-only for exactly the same reason (see BrnDirectorArbitrator.h, which lists
    // the ~15 un-attested camera/effect entry points its body would have to fabricate).
    // Paraphrasing 550 lines of VMX to scalar is forbidden by the reconstruction rules
    // regardless.
    //
    // WHAT THAT MEANS BEHAVIOURALLY -- and why the gate is honest rather than a stub:
    //   * TODAY (PC bring-up, no player vehicle) the guard is FALSE every frame, so the
    //     console itself takes the `lCamera = mLastCamera` path. This reconstruction is
    //     therefore EXACT right now: a stable, finalised, inertia-smoothed camera is
    //     published into the output buffer each frame, which is precisely what the console
    //     does with no live player car.
    //   * ONCE A PLAYER CAR EXISTS the gate makes the camera hold its last value instead of
    //     tracking the car -- degraded, but still a valid camera on a valid path. It never
    //     traps and never publishes garbage.
    //
    // DELETE-WHEN: delete this gate and transcribe lines 252-823 once
    // BrnDirector::Camera::BehaviourManager has a real layout (that single type unblocks
    // UpdateArbitrator -> Arbitrator::Update -> ArbStateAttractMode/ArbStateDriveThru, which
    // is the path the DJ flyby intro runs on).
    // ------------------------------------------------------------------------
    void MainDirector::Update(const DirectorInputOutput* lpIO)
    {
        // The frame camera is a STACK local on the console too (v211) -- it is built fresh
        // every frame and only reaches the director's storage through mLastCamera below.
        Camera::Camera lCamera;
        lCamera.Construct();

        if (!IsPlayerCarLive(lpIO))
        {
            // LABEL_100 -- carry last frame's finalised camera forward.
            lCamera = lrLastCamera(this);
        }
        else
        {
            // ⚠️ GATE -- the gameplay middle (lines 252-823). See the banner.
            lCamera = lrLastCamera(this);
        }

        // Finalise: camera inertia + shake (the CameraFinaliser owns the InertiaController).
        lrCameraFinaliser(this).Update(lpIO->mpInputBuffer,
                                       reinterpret_cast<char*>(this) + KU_OFF_CAMERA_STATE_BLOCK,
                                       lpIO->mpResourceManager,
                                       &lCamera);

        // Carry the finalised camera into the next frame.
        lrLastCamera(this) = lCamera;

        // The debug component's panorama-screenshot pass gets the finished camera. The
        // pointer is the back-reference DirectorModule::Construct planted at this+0x40.
        DebugComponent* lpDebugComponent = *reinterpret_cast<DebugComponent**>(
            lpByteView(this) + KU_OFF_DEBUG_COMPONENT_PTR);
        if (lpDebugComponent != 0)
            lpDebugComponent->UpdatePanoramaScreenshots(&lCamera);

        // Validate (asserts on NaN / unreasonable position), then convert to the graphics
        // camera and publish BOTH forms into the director output buffer.
        lCamera.ValidateTransformWithDebugInfo();
        lCamera.CopyToCgsCamera(&GetCgsCamera());

        lpIO->mpOutputBuffer->SetCgsCamera(GetCgsCamera());
        lpIO->mpOutputBuffer->SetCameraOutput(lCamera);

        // ⚠️ GATE (the tail after the publish, lines 871-924): the requested time-step
        //   multiplier (CgsSystem::TimerRequests::SetTimestepMultiplier on the output
        //   buffer's timer-request interface, fed by the slomo factor computed inside the
        //   gated middle), UpdateDebugInfo / DebugDisplayCurrentCamera (both
        //   declaration-only, and UpdateDebugInfo additionally depends on the un-recovered
        //   near-clip rodata Camera.h already FLAGS), BehaviourManager::PrepareBehaviours +
        //   UpdateAttribSys (BehaviourManager, as above), and the camera-state debug flag
        //   printers. None of them alters the published camera -- the two publish calls
        //   above are already done. DELETE-WHEN: as the middle gate.
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
    //         this[+211171] = input->GetShortcutMenuState();
    //     if ( input->HasGotHookEnumeration() )
    //         EffectInterface::Update( this+212112, *input->GetHookEnumeration(),
    //                                  input->GetHookEnumeration()+4, this+218166 );
    //     ... ( the mode/action tail )
    //     MainDirector::HandlePrepareForModeAction( this, this+215872, lpIO );
    //
    // Note every INPUT-side accessor it needs is already committed on
    // DirectorIO::InputBuffer -- the blockers are all on the MainDirector side:
    //   * the four GUI latches and the shortcut-menu state land at raw offsets inside the
    //     un-modelled GameState region (this+0x338xx) with no named members;
    //   * BrnDirector::EffectInterface has no reconstructed home;
    //   * MainDirector::HandlePrepareForModeAction is itself declaration-only (it dispatches
    //     over the same un-homed GameState action region).
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
