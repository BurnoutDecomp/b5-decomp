// ============================================================================
// GameSource/Director/DirectorLinkStubs.cpp
//
// PC LINK-MOUNT STUBS for the DirectorModule mount (2026-07-29, DJ fly-by campaign).
//
// The director spine is now in the game exe's source list. Some of its declaration-only
// callees have no owning TU in the link yet -- either because the TU does not exist, or
// because mounting it would drag a whole un-landed sub-system in behind it. Rather than
// fabricate bodies inside the real headers, each one gets a MARKED, QUIET stub here:
//
//   * every stub carries WHY it is a stub and a DELETE-WHEN note;
//   * no stub traps -- these sit on per-frame paths and a trap would make the exe unusable;
//   * a stub that must return a value returns the console's own "nothing happened" value;
//   * NO stub returns a reference to a fabricated object. Where a symbol returns a reference
//     the stub is omitted and the caller's TU is kept out of the link instead.
//
// This file is the Director's twin of GameSource/World/WorldLinkStubs.cpp. When a real TU
// lands for any symbol below, DELETE its stub here (a duplicate definition is a link error,
// so the removal is enforced by the build).
//
// ---------------------------------------------------------------------------
// GROUP A -- the nine arbitrator states that are NOT on the fly-by path.
//   All ten states are reconstructed under Arbitrator/States/, but each of the nine below
//   drags a different un-landed sub-system into the link (ICEMoviePlayer, MomentSelector,
//   BehaviourIceAnim, BehaviourInterpolate, the DirectorResourceManager shot-group getters,
//   the Attrib shot vault). Mounting all ten took the link from 47 to 137 unresolved, most of
//   them reference-returning accessors that cannot be honestly stubbed. Only
//   ArbStateAttractMode -- the DJ fly-by's own state -- is mounted for real.
//   CONSEQUENCE: the arbitrator can build its state container and can still refuse to enter
//   any of these states, but if one IS entered it produces no camera and reports "released".
//   That is the SAME observable state as before the director was mounted at all.
//   DELETE-WHEN: a state's sub-system lands -> mount the state's .cpp, delete its block here.
//
// GROUP B -- the two debug behaviours the BehaviourManager instantiates by template.
//   BehaviourManager::AllocateBehaviour<T> is instantiated for every behaviour type, which
//   forces each type's vtable. DebugFlyWorld and DebugOrbitPlayer are DEV-MENU behaviours
//   (their real TUs pull the Tweaker mapping API and the panorama screenshot callback); they
//   are never allocated on the fly-by path.
//
// GROUP C -- sub-systems with no landed TU at all (ICE/ICEWrapper, the director
//   DebugComponent, SharedPlaylists, the Attrib collection lookup, rw SLerp).
//
// GROUP D -- declared-only leaves of already-mounted TUs.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"

#include "GameSource/Director/BrnDirectorICEWrapper.h"
#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"
#include "GameSource/Director/Camera/BrnBehaviourManager.h"

#include "GameSource/Director/Arbitrator/States/BrnArbStateCarSelect.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateCrashMode.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateCrashNav.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateDriveThru.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateOnlineCarSelect.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateOnlineRaceIntro.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStatePostEvent.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRaceIntro.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRankUp.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRoaming.h"

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugOrbitPlayer.h"
#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"           // the DWARF home (NOT the stale BrnBehaviourPassengerCam.h slice)

#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"                       // group E
#include "GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.h" // group E

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include "SharedClasses/Traffic/BrnTrafficSection.h"
#include "SharedClasses/Trigger/BrnGenericRegion.h"
#include "SharedClasses/Trigger/BrnRegion.h"
#include "SharedClasses/Trigger/BrnTriggerData.h"

// ----------------------------------------------------------------------------
// GROUP A -- the nine off-path arbitrator states.
//
// Every stub below mirrors the BASE ArbitratorState default (see
// Arbitrator/BrnDirectorArbitratorState.cpp), which is what an un-entered state does anyway:
//   Construct() -- build the state's camera and clear the base flags;
//   Prepare()   -- "ready" (the arbitrator only calls it after CanRun said yes);
//   Update()    -- drive nothing;
//   Release()   -- "already released" (ReleaseAll asserts the result);
//   Destruct()  -- own nothing;
//   GetName()   -- the state's own console name literal (these ARE attested: the whole family
//                  sits in .rodata at 0x821F62E0..0x821F6740, one GetName symbol per state).
// ----------------------------------------------------------------------------
#define BRN_DIRECTOR_STUB_ARBSTATE(CLASS, NAME_LITERAL)                             \
    void CLASS::Construct()                                                         \
    {                                                                               \
        ArbitratorState::Construct();                                               \
    }                                                                               \
    bool CLASS::Prepare(ArbStateSharedInfo& lrSharedInfo)                           \
    {                                                                               \
        (void)lrSharedInfo;                                                         \
        return true;                                                                \
    }                                                                               \
    void CLASS::Update(ArbStateSharedInfo& lrSharedInfo)                            \
    {                                                                               \
        (void)lrSharedInfo;                                                         \
    }                                                                               \
    bool CLASS::Release(ArbStateSharedInfo& lrSharedInfo)                           \
    {                                                                               \
        (void)lrSharedInfo;                                                         \
        return true;                                                                \
    }                                                                               \
    const char* CLASS::GetName() const                                              \
    {                                                                               \
        return NAME_LITERAL;                                                        \
    }

namespace BrnDirector
{
    // The GetName literals are the ARTIST .rodata names (one symbol each, 0x821F62F0 /
    // 0x821F6300 / 0x821F6330 / 0x821F6730 / 0x821F6480 / 0x821F6310 / 0x821F6320 /
    // 0x821F6710 / 0x821F6238).
    // ArbStateCarSelect joined this group on 2026-07-30, when the ICE-anim header de-fork made
    // its .cpp compile and the container swapped its empty placeholder for the real layout. It
    // is stubbed rather than mounted for a DIFFERENT reason from its siblings: its .cpp compiles
    // fine, but linking it pulls BrnBehaviourIceAnim.cpp, whose camera comes out of
    // KeyAnimController (the ICE take evaluator -- 2 of ~8 functions bodied) and the
    // declaration-only IceAnimCameraOps naming layer. Stubbing THOSE would stub the code that
    // produces the camera. DELETE-WHEN: KeyAnimController::Update/Prepare land + IceAnimCameraOps
    // is bodied -> mount BrnArbStateCarSelect.cpp + BrnBehaviourIceAnim.cpp, delete this line.
    // (It declares no Release() of its own -- Release/Destruct are not in its exported X360
    //  function set, so the base declarations stand and only four slots are stubbed here.)
    // ⭐⭐ ArbStateCarSelect's FOUR STUBS ARE GONE (2026-08-01). They were
    //     void ArbStateCarSelect::Construct()                        { ArbitratorState::Construct(); }
    //     bool ArbStateCarSelect::Prepare(ArbStateSharedInfo&)       { return true; }
    //     void ArbStateCarSelect::Update(ArbStateSharedInfo&)        { (void)lrInfo; }
    //     const char* ArbStateCarSelect::GetName() const             { return "ArbStateCarSelect"; }
    // and they were four LNK2005s against the real TU. That TU
    // (Arbitrator/States/BrnArbStateCarSelect.cpp) is now mounted together with
    // Camera/Behaviours/BrnBehaviourIceAnim.cpp, Shots/ShotControllers/BrnKeyAnimController.cpp
    // and Camera/BrnCameraReference.cpp -- it is the state that owns the REAL cameras: the
    // junkyard shot-group setup, the three authored ICE intro shots off mGameIntroGroup
    // ("606002"), and the rotate-about-car orbit camera. The stubbed Update above is what made
    // ArbitratorStateContainer::UpdateAll drive nothing for that state every frame.
    // (It declares no Release()/Destruct() of its own -- those are not in its exported X360
    //  function set, so the base declarations stand.)

    // ⛔⛔⛔ READ THIS BEFORE MOUNTING ArbStateCrashMode -- IT IS NOT THE CRASH CAMERA.
    // (Mapped 2026-08-28, slow-motion transport wave. Every claim below is read off the asm or
    //  the committed tree, not inferred from the state's name.)
    //
    // THE CRASH CAMERA THE GAME ACTUALLY ENTERS IS ArbStateCrashing, AND IT DOES NOT EXIST.
    // ArbStateRoaming::ProcessPossibleStateChanges @0x82219C58 is the crash gate. It TESTS
    // crash mode and then ENTERS A DIFFERENT STATE -- BrnArbStateRoaming.cpp:912-919:
    //     lrContainer.GetState(E_STATE_CRASH_MODE)->CanRun(...)          // index 4, the TEST
    //     ArbUtils::ChangeToStateWithoutRelease(..., E_STATE_CRASHING,   // index 2, the ENTRY
    //                                           meState, E_STATE_CHANGING_TO_CRASHING, ...)
    // and E_STATE_CRASHING's container slot is `class ArbStateCrashing : public ArbitratorState {};`
    // -- an EMPTY placeholder in BrnDirectorArbitratorStateContainer.h:50 with no overrides at
    // all, so every crash drives a do-nothing base Update. That is the hollow-shell shape, and
    // it is why crashes today keep the ordinary chase camera and never slow down.
    // ArbStateCrashMode (index 4) is only ever REACHED from ArbStateCrashing::
    // ProcessPossibleStateChanges @0x8224F5B0 (its debug line is literally "Switching to crash
    // mode"), so mounting crash mode alone changes NOTHING on screen.
    //
    // ArbStateCrashing IS in the X360 ledger -- 8 functions, TU `blocked`, no file in the tree:
    //     Construct                   @0x82259EA0    88 asm
    //     Prepare                     @0x822655E8    62
    //     Update                      @0x8226BFB0   408
    //     Release                     @0x82234F70    33
    //     CanRun                      @0x821F6258     4
    //     GetName                     @0x821F6248     4
    //     SelectNormalCrashCamera     @0x82254FB0   218
    //     ProcessPossibleStateChanges @0x8224F5B0   201
    //     ApplySlomoAndShake          @0x8224F8D8   113   <- THE IMPACT SLOW MOTION
    // ⚠️ ApplySlomoAndShake is NOT in `work show`'s list for that TU: its DecFIGS primary_file
    // is CgsArray.h (the usual inlined-header misattribution), so a TU-scoped dossier misses it.
    // Its DWARF members (BrnArbStateCrashing.h:105-129) name the whole camera:
    //     BehaviourHandle<BehaviourSpirallingDeathcam> mDeathcam;
    //     BehaviourHandle<BehaviourIceAnim>            mTakenDownCam;
    //     ImpactSlomoController  mImpactSlomoController;   // BehaviourBystanderCam.h:102
    //     ImpactShakeController  mImpactShakeController;   // BehaviourBystanderCam.h:~135
    //     MomentSelector mMomentSelector; VehicleTracker::ECrashType meCrashType; ...
    //     EState { INACTIVE, PREPARING, ACTIVE, AFTERCRASH, AFTERCRASH_SLOW,
    //              INTERPOLATING_TO_ROAMING, CHANGING_TO_ROAMING, RELEASING }
    // Both controllers and BehaviourSpirallingDeathcam.cpp already exist in this tree.
    //
    // AND ArbStateCrashMode's OWN MOUNT IS NOT FREE EITHER. BrnArbStateCrashMode.h:58 declares
    // `bool Prepare(ArbStateSharedInfo&) override;` with NO definition -- a shadowing
    // redeclaration only a LINK can find. The body DOES exist (@0x82265F70; its primary_file is
    // misattributed to BrnBehaviourManager.h, which is why the TU dossier shows 5 functions and
    // not 6). It is byte-for-byte the shape of ArbStateAttractMode::Prepare @0x8225B220 --
    //     if (meState == ACTIVE || meState == CHANGING_TO_ROAMING) return true;
    //     meState = PREPARING;
    //     if (!mAftertouch.IsAllocated())
    //         manager->NewBehaviour<BehaviourAftertouchCrash>(mAftertouch, this, 0, 1);
    //         mAftertouch.GetBehaviour()->SetParameters(<bank + 0x7C>);
    //     return mAftertouch.IsReadyToPrepare();
    // -- with ONE polarity difference worth stating, because it is exactly the kind of thing a
    // copy-paste from the sibling gets wrong: AttractMode's callee is the NON-negating
    // BehaviourHandle<T>::IsWaitingToPrepare instantiation (`BehaviourRoadRun` @0x82212AC8) and
    // it negates at the call site (clrlwi/cntlzw/extrwi); CrashMode's callee is the ALREADY-
    // negating IsReadyToPrepare (`BehaviourAfterto` @0x8222D0E8, whose body ends
    // `IsBehaviourWaitingToPrepare(...) == 0`) and it returns the value RAW. Both spell
    // `return mHandle.IsReadyToPrepare();` in this tree -- through different symbols.
    // The remaining blocker on that Prepare is the parameters argument: `manager + 0x125AC` ==
    // BehaviourParameterBank + 0x7C, and BrnBehaviourParameterBank.h models the bank as
    // `u8 maReservedHead[0x2334]` plus ONE named block. Carve BehaviourAftertouchCrash::
    // Parameters at bank+0x7C, or land the Prepare with that one leg FLAG-gated.
    //
    // ═══════════════════════════════════════════════════════════════════════════════════════
    // ⭐⭐ REMAINDER, REWRITTEN 2026-08-28/29 (crash-camera wave). Everything below is read off
    // the ARTIST asm, the committed tree, or a measured run. THE PREVIOUS ORDER WAS INCOMPLETE:
    // it had four steps and the chain has FIVE breaks, one of which nothing in this note named.
    // Two of the five are now CLOSED.
    // ═══════════════════════════════════════════════════════════════════════════════════════
    //
    // ✅ CLOSED (1). THE DIRECTOR'S TIME-DILATION TRANSPORT. It was held behind
    //    BRN_DIRECTOR_SLOMO because every ICE camera published mfSimTimeScale == 0 and arming it
    //    froze the game at the console's 0.005 floor. The cause was NEITHER of the two candidates
    //    that note left open (the take's memclear seed / the asset port): it was
    //    ICEElementDescription::GetDefaultFloat() type-punning an INTEGER default through the
    //    ICEValue union, so TIME_SCALE's DEFAULT=(s32)100 read back as 1.401e-43 and rounded to 0.
    //    Fixed in ICEDataEnums.hpp against the console's own type-aware body @0x82530894; the
    //    flag is retired and MainDirector publishes unconditionally. Measured:
    //        [ice-timescale] raw TIME_SCALE=100.000000 -> mfSimTimeScale=1.000000
    //        [slomo] simScale=1.000000 gameScale=1.000000 simStep=0.016667
    //
    // ✅ CLOSED (2). mfSimTimestep WAS PUBLISHED AS A HARD 0 by a stale gate in
    //    MainDirector::UpdateArbitrator. ArbStateCrashMode's SEVEN intro timers (flash, borders,
    //    blur in/out, blur ramp, tilt) are all `-= lrSharedInfo.mfSimTimestep`, so they would
    //    have hung forever the moment that TU mounted. Now real. See that call site.
    //
    // ⛔ OPEN (3). NOTHING WRITES GameState::mbCrashActive, SO THE ARBITRATOR NEVER EVEN TRIES.
    //    This is the break the old note missed entirely, and it is UPSTREAM of everything else
    //    here. ArbStateRoaming::ProcessPossibleStateChanges gates its crash edge on
    //    lrGameState.mbCrashActive; the only writer in the image is the tail of
    //    MainDirector::ProcessInputQueue @0x822372F8 (pseudocode 854-873), which is GATED in our
    //    reconstruction. The console block is transcribed verbatim at that gate and its old
    //    blocker -- "+0x44A, a byte with no recovered name" -- is resolved: +0x44A == 1098 ==
    //    Camera::VehicleInfo::mRaceCarState.mbCrashing.
    //    MEASURED with BRN_CRASH_PLAYER: a real committed player crash (mbCrashing=1 for 300+
    //    frames, bridged to the director) produced NO state change and NO dilation -- one
    //    [slomo] line at simStep=0.016667, BRN_FRAME_DUMP_ARM=slomo wrote frames=0, and the
    //    physics' own mfTimeCrashing advanced 0.016667 s/frame throughout.
    //    ⛔⛔ IT MUST LAND IN THE SAME COMMIT AS (4). On its own it hands the first crash to the
    //    empty ArbStateCrashing shell, whose base Update never writes meState and which has no
    //    exit edge -- a camera frozen for the rest of the session, out of a green build.
    //
    // ⛔ OPEN (4). ArbStateCrashing ITSELF. Home BrnArbStateCrashing.h from the DWARF, de-fork
    //    the container placeholder, body the 9 functions, mount it.
    //
    // ⛔ OPEN (5). THEN mount BrnArbStateCrashMode.cpp with its Prepare, and delete both stub
    //    lines below.
    //
    // ───────────────────────────────────────────────────────────────────────────────────────
    // WHAT (4) NEEDS THAT IS NOT YET IN THE TREE -- checked, not assumed:
    //   * BehaviourBystanderCam.cpp IS NOT MOUNTED, and it owns BOTH controllers' Update bodies
    //     (ImpactSlomoController::Update @0x82227230, ImpactShakeController::Update @0x82243720).
    //     They are fully bodied there -- so ApplySlomoAndShake is a mount away, not a decompile
    //     away. ⭐ ImpactSlomoController::Update is literally the crash slow motion: it writes
    //     Camera_SetTimeScale(camera, 0.2857143f) for a 2.0 s burst, i.e. the finished chain
    //     should show simStep 0.016667 -> 0.004762 for two seconds. That is the number to gate on.
    //   * MomentSelector::SnoopNumValidMoments @0x8221BD28 (needed by Prepare) and
    //     ResetTimeActive (needed by Update case 0) are DECLARE-ONLY -- two new unresolved
    //     externals unless bodied. Its Update/Prepare/Release/AddMoment/SelectBestMomentWith-
    //     Exclusion ARE bodied and the TU IS mounted.
    //   * ArbStateSharedInfo::mpPlayerCar is the `BrnDirector::VehicleInfo` NAMESPACE FORK
    //     BrnDirectorAllVehicleData.h:38 already documents and fixed for its own surface: no such
    //     type exists, the real one is BrnDirector::Camera::VehicleInfo. CanRun cannot be written
    //     without fixing the forward declaration in BrnDirectorArbitratorState.h the same way.
    //   * PlayerCrashInfo has NO layout (forward-declared only), and step 13 of
    //     BridgeWorldToDirector ("build the 48-byte PlayerCrashInfo block") is itself dropped --
    //     see that TU's banner. Update reads mpPlayerCrashInfo+0x26 for the "Wrecked" vs "Crash"
    //     effect name and mbPlayerWasWreckedThisCrash. Both ends are missing; land them together.
    //
    // ───────────────────────────────────────────────────────────────────────────────────────
    // LAYOUT, pinned from Construct @0x82259EA0 + Update @0x8226BFB0 (console offsets; parity is
    // BY NAME as always -- these are provenance, and Hex-Rays prints them in DECIMAL):
    //     +0x180  BehaviourHandle<BehaviourSpirallingDeathcam> mDeathcam        (0x14 bytes)
    //     +0x194  BehaviourHandle<BehaviourIceAnim>            mTakenDownCam    (0x14)
    //     +0x1A8  ImpactSlomoController mImpactSlomoController                  (12)
    //     +0x1B4  ImpactShakeController mImpactShakeController                  (20 -- see below)
    //     +0x1C8  MomentSelector        mMomentSelector                         (0x1E4)
    //     +0x3AC  VehicleTracker::ECrashType meCrashType   (Prepare: = mpPlayerTracker->+0x298)
    //     +0x3B0  f32  mfTimeActive       (+= mfTimestep)     +0x3B4 f32 mfMomentTimer
    //     +0x3B8  f32  mfFailsafeTimer    +0x3BC bool mbAftertouchingSinceStart
    //     +0x3BD  bool mbAftertouchActivated                  +0x3BE bool mbShouldUseDeathcam
    //     +0x3BF  bool mbPlayerWasWreckedThisCrash            +0x3C0 bool mbPlayerWasTakenDown
    //     +0x3C4  EActiveRaceCarIndex mePlayerKillerIndex     +0x3C8 EState meState
    // Hex-Rays' `field_3A8/3A9/3AA` are NOT state members -- they are mMomentSelector's own
    // +0x1E0/+0x1E1/+0x1E2, i.e. mbHasSelectedMoment / mbPrepared / mbHasMaxLimit. Likewise
    // field_390 == miFramesActive, field_398 == muValidMoments, field_38C == mfTimeActive.
    // ⚠️ ImpactShakeController IS 20 BYTES, not the 68 BehaviourBystanderCam.h currently models
    // (`f32 + u8 maImpactEffect[0x40]`, itself FLAGged as a guess). ApplySlomoAndShake clears
    // exactly five floats at +0x1B4..+0x1C4 and mMomentSelector starts at +0x1C8; the DWARF says
    // the class holds one member, `CameraImpactEffect mImpactEffect`, and BrnCameraImpactEffect.h
    // already models that as f32 + u8[16] == 20. De-fork it to the real type when you home this.
    //
    // ⭐ Construct's two "clear" blocks and ApplySlomoAndShake's two "else" arms are the SAME
    // stores -- they are `mImpactSlomoController.Construct()` (mfTimeSinceLastSlomo = FLT_MAX,
    // mfTimeInSlomo = 0, mbFirstFrameOfSlomo = false) and `mImpactShakeController.Construct()`
    // (five zero floats). Both Constructs are inline-only (no X360 symbol); those are the bodies.
    //
    // ⛔⛔ HEX-RAYS GARBLES ApplySlomoAndShake's THREE PREDICATES -- read the asm. It emits
    // `v7 = v5; if (v7 || ...)`, aliasing the FIRST block into the test. The asm computes three
    // separate values and the test uses the SECOND (r28 = block1 @0x8224F930, r29 = block2
    // @0x8224F96C, then r10 = block2 and r29 = block3 @0x8224F9A4/A8):
    //     lbHardStop = mMomentSelector.HasSelectedMoment() &&
    //                  GetSelectedMoment()->GetType() == E_MOMENT_HARD_STOP;   // moment +0x170
    //     block1 = lbHardStop && meCrashType == E_CRASH_NORMAL;      // -> lbDontSetRealTime arg
    //     block2 = lbHardStop && meCrashType != E_CRASH_NORMAL;      // -> SUPPRESS the slomo
    //     block3 = lbHardStop && meCrashType == E_CRASH_HIGH_ENERGY; // -> SUPPRESS the shake
    //     slomo: if (block2 || meCrashType == E_CRASH_LOW_ENERGY || meEventType == 3 ||
    //                meEventType == 0 || mpGameState->mbImpactTimeActive)
    //                mImpactSlomoController.Construct();
    //            else mImpactSlomoController.Update(mCamera, mfTimestep, *mpAllVehicleData,
    //                                               *mpPlayerTracker, *mpDebugPrinter, block1);
    //     shake: if (block3) mImpactShakeController.Construct();
    //            else mImpactShakeController.Update(mCamera, mfTimestep, *mpAllVehicleData,
    //                     *mpPlayerTracker, *mpRandom, *mpDebugPrinter, <stack VehicleRef>);
    // The shake's 7th argument is a 16-byte stack record built at 0x8224FA78..0x8224FA88:
    // { s32 0, s32 -1, s32 0, bool true }. ⚠️ Neither call's argument list survives Hex-Rays:
    // f1 (the timestep) consumes the r5 GPR slot, so r5 is never assigned and the decompiler
    // drops four of the six pointer arguments. Take the signature from the asm.
    //
    // CONSTANTS (dumped from the ARTIST image this wave; the .data pool cross-checks the
    // CarSelect wave's calibration -- flt_82CDADA0 really is 2.0):
    //     kfMomentTime = kfFailsafeTime = 2.0f   (flt_82001D9C; both compares in Select/Process)
    //     the AFTERCRASH duration            = 0.5f   (flt_82CDAD90, Update case 3)
    //     the bystander distance factor      = 0.5f   (flt_82001DA0)
    //     FLT_MAX = flt_8200173C, 1.0 = flt_82001C98, 0.0 = flt_82001CC0
    // GameState fields Update/Process read, by name: mbGoToCrashModeAfterIntro (+0x11C),
    // mEventState.GetCurrent() (+0x124), mbCrashActive (+0xF9), mfCrashTimeRemaining (+0xFC,
    // `<= 1.0f` is the lbTooLateToSwitchCameras argument), mbImpactTimeActive (+0x105),
    // meEventType (+0x120), mbPlayerWasTakenDown (+0x1C3), mePlayerKillerIndex (+0x1C4).
    // Container hand-offs: Process's `+13744` is state table[4] == E_STATE_CRASH_MODE (that is
    // the "Switching to crash mode" edge) and Update's `+13732` is table[1] == E_STATE_ROAMING.
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateCrashMode,       "ArbStateCrashMode")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateDriveThru,       "ArbStateDriveThru")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateOnlineCarSelect, "ArbStateOnlineCarSelect")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateOnlineRaceIntro, "ArbStateOnlineRaceIntro")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStatePostEvent,       "ArbStatePostEvent")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateRaceIntro,       "ArbStateRaceIntro")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateRankUp,          "ArbStateRankUp")

    // CrashNav and Roaming already own SOME of their virtuals in their mounted-elsewhere
    // headers, so only the missing slots are stubbed.
    void ArbStateCrashNav::Construct()                          { ArbitratorState::Construct(); }
    void ArbStateCrashNav::Update(ArbStateSharedInfo& lrInfo)   { (void)lrInfo; }
    bool ArbStateCrashNav::Release(ArbStateSharedInfo& lrInfo)  { (void)lrInfo; return true; }
    const char* ArbStateCrashNav::GetName() const               { return "ArbStateCrashNav"; }

    // ⭐ ArbStateRoaming's FOUR STUBS ARE GONE (2026-08-01). They were
    //     void ArbStateRoaming::Construct()                          { ArbitratorState::Construct(); }
    //     bool ArbStateRoaming::Prepare(ArbStateSharedInfo&)         { return true; }
    //     bool ArbStateRoaming::Release(ArbStateSharedInfo&)         { return true; }
    //     const char* ArbStateRoaming::GetName() const               { return "ArbStateRoaming"; }
    // and they blocked the mount of GameSource/Director/Arbitrator/States/BrnArbStateRoaming.cpp
    // with four LNK2005s. That TU is now on the exe source list and owns all four for real,
    // together with the newly-written Update @0x822643A0 -- the function that actually drives
    // the roaming state machine and is the ONLY writer of E_STATE_CHANGING_TO_CAR_SELECT (via
    // ProcessPossibleStateChanges). While these stubs stood, Prepare's unconditional `true`
    // hid the fact that the real gate was never even reached: ArbStateRoaming had no Update
    // override at all, so vtable slot 2 fell through to ArbitratorState::Update's empty body
    // and meState never left E_STATE_PREPARING.
    // The moment sub-system BrnArbStateRoaming.cpp reaches through MomentSelector is stubbed
    // in GROUP F at the foot of this file -- down to two entries as of 2026-08-23, only one of
    // which is on a live path. Read that banner before planning a moment wave.

    // Two states declare an explicit Destruct() override that their .cpp bodies.
    void ArbStateOnlineRaceIntro::Destruct() {}
    void ArbStatePostEvent::Destruct()       {}
}

#undef BRN_DIRECTOR_STUB_ARBSTATE

// ----------------------------------------------------------------------------
// GROUP B -- the two dev-menu behaviours the manager's AllocateBehaviour<T> forces vtables
// for. Their real TUs (BrnBehaviourDebugFlyWorld.cpp / BrnBehaviourDebugOrbitPlayer.cpp) pull
// the whole Tweaker mapping API + the panorama screenshot callback. Neither is allocated on
// the fly-by path -- only ArbStateAttractMode's BehaviourRoadRunner is.
// Update() returns FALSE = "I produced no camera this frame", the same answer the console's
// own behaviours give when they have nothing to say.
// DELETE-WHEN: Camera/Utils/BrnCameraTweaker.cpp lands -> mount both real TUs, delete this.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
namespace Camera
{
    void BehaviourDebugFlyWorld::Construct()
    {
        // NOTE: these two classes are still PRE-BASE forks -- they carry their own
        // `void* mpVTable` at +0x00 instead of deriving from Camera::Behaviour, so there is no
        // base Construct to chain to. (Retiring those two forks the way the road runner's was
        // retired is a separate job.)
    }

    bool BehaviourDebugFlyWorld::Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo)
    {
        (void)lrInfo;
        return true;
    }

    bool BehaviourDebugFlyWorld::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
    {
        (void)lrCamera;
        (void)lrInfo;
        return false;
    }

    void BehaviourDebugFlyWorld::SetupTweaker(Utils::Tweaker& lrTweaker)
    {
        (void)lrTweaker;
    }

    const char* BehaviourDebugFlyWorld::GetName() const
    {
        return "BehaviourDebugFlyWorld";
    }

    void BehaviourDebugFlyWorld::WarpToLookAt(Vector3 lEye, Vector3 lLookAt)
    {
        (void)lEye;
        (void)lLookAt;
    }

    // ------------------------------------------------------------------------
    // BehaviourPassengerCam's two DECLARATION-ONLY virtuals (added 2026-08-02, camera
    // parameter-chain wave). The class's other four virtuals are REAL, in its own TU
    // Behaviours/BehaviourPassengerCam.cpp, now mounted -- these two are the ones its header
    // already marks "own ledger fn (declared-only)" (DWARF cpp:65 / cpp:116; neither is
    // X360-exported under this class's name).
    //
    // WHY THEY ARE NEEDED NOW: BrnBehaviourManager.cpp:965 explicitly instantiates
    // AllocateBehaviour<BehaviourPassengerCam>(), which emits the vtable. That instantiation
    // used to bind to the STALE 0x18-byte BrnBehaviourPassengerCam.h SLICE -- a second,
    // non-derived definition of the same class that had never met the real one in a TU -- so
    // it booked a pool bucket from the wrong sizeof and referenced no virtuals at all. See
    // the repoint note at the top of BrnBehaviourManager.cpp.
    //
    // The values are the base Behaviour's own defaults for these two slots (Prepare == ready,
    // SetupTweaker == nothing to expose), so a passenger cam allocated today behaves exactly
    // as it did while the class was the slice: allocatable and inert. Nothing allocates one.
    // DELETE-WHEN the passenger cam's Prepare/SetupTweaker bodies land.
    // ------------------------------------------------------------------------
    bool BehaviourPassengerCam::Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo)
    {
        (void)lrInfo;
        return true;
    }

    void BehaviourPassengerCam::SetupTweaker(Utils::Tweaker& lrTweaker)
    {
        (void)lrTweaker;
    }

    void BehaviourDebugOrbitPlayer::Construct()
    {
        // (same pre-base fork note as BehaviourDebugFlyWorld::Construct above)
    }

    bool BehaviourDebugOrbitPlayer::Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo)
    {
        (void)lrInfo;
        return true;
    }

    bool BehaviourDebugOrbitPlayer::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
    {
        (void)lrCamera;
        (void)lrInfo;
        return false;
    }

    void BehaviourDebugOrbitPlayer::SetupTweaker(Utils::Tweaker& lrTweaker)
    {
        (void)lrTweaker;
    }

    const char* BehaviourDebugOrbitPlayer::GetName() const
    {
        return "BehaviourDebugOrbitPlayer";
    }
}
}

// ----------------------------------------------------------------------------
// GROUP C -- sub-systems with no landed TU.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
    // -- ICEWrapper (the ICE/in-car-entertainment take player). MainDirector embeds one by
    //    value and Constructs/Prepares/Destructs it; nothing on the fly-by path drives a take.
    //    Prepare returns TRUE = "staged Prepare finished", so DirectorModule::Prepare's stage
    //    machine advances instead of spinning for ever.
    //    DELETE-WHEN: BrnDirectorICEWrapper.cpp's Construct/Prepare/Destruct land.
    ICEWrapper::ICEWrapper() {}
    void ICEWrapper::Construct()
    {
        // ⭐ PARTIAL REAL BODY (2026-08-01). The console's Construct zeroes both ICE load-state
        // scalars (+0x120E4 / +0x120E8 -- this header's own member comments record it), and
        // +0x120E8 == miICELoadStateB is the STAGE WORD ICEWrapper::Prepare @0x8253DD90
        // switches on. While Construct was a pure no-op that word held whatever the allocation
        // left, so a non-zero value would send the newly-real Prepare straight down its
        // "already prepared" arm and the ICE element-description system would never initialise
        // -- the same end state as the stub it replaces, but silent and intermittent.
        // The rest of Construct (the camera / mover / manager / action-queue sub-object builds)
        // remains un-landed: see BrnDirectorICEWrapper.cpp.
        miICELoadStateA = 0;
        miICELoadStateB = 0;
    }
    // (The two ICE sub-objects ICEWrapper embeds by value -- their ctors' TUs are un-landed;
    //  see the ICE::* block at the end of this file.)
    void ICEWrapper::Destruct() {}

    // ⛔⛔ RETIRED 2026-08-01 (ICE-anim transform wave) -- ICEWrapper::Prepare's `return true;`
    // was THE most expensive stub in this subsystem, and it was invisible in exactly the way
    // this project's top defect class always is.
    //
    // @0x8253DD90 runs `ICE::InitICEDescriptions()` at its stage 0, and that call is the ONLY
    // one in the entire image. InitICEDescriptions builds the PER-CHANNEL ELEMENT SCHEDULES
    // (gaICEElementChannels) that ICETake::SetParameter iterates to decide which elements to
    // evaluate. With the stub in place those schedules stayed at miNumKeyElements == 0, so the
    // take evaluator's element loops ran ZERO times, mValues[] was never written, and EVERY
    // authored ICE camera element -- eye XYZ, look XYZ, the reference SPACES, lens, focus --
    // read back as 0 for the whole session. A take could load, bind, seek and play its full
    // parametric timeline (measured: guid 610132 Intro_FlyCam_Loop, 40.02 s, timer advancing,
    // param 0 -> 1) and still produce a camera parked at (0,0,0) in car space with an identity
    // basis. Nothing about that looks like a missing initialiser.
    // The real (partial) body is now in GameSource/Director/BrnDirectorICEWrapperPrepare.cpp.

    // -- RETIRED 2026-08-01 (ICE take-runtime wave): ICEResourceMgr's two take-data lookups
    //    used to be `return 0` here. The ID overload @0x821F6A00 is now REAL in
    //    GameSource/Director/BrnDirectorResourceManager.cpp -- it is the one bridge from the
    //    ICE take runtime to the loaded take dictionaries, and mpICEDictionaryList is bound
    //    now, so a null answer would have been a legal-looking lie (every caller null-checks
    //    it, so a resident take would just never play). The index overload stays null there,
    //    with the reason.

    // -- DirectorResourceManager::Prepare RETIRED 2026-08-01. The real body @0x8225CA08 is
    //    bodied in its own TU (GameSource/Director/BrnDirectorResourceManager.cpp), which is
    //    now in the exe source list. It resolves the CameraVault and constructs all 65
    //    shot-group slots; this `return true` was what left them null-collection instances.

    // -- The director's own CgsDev::DebugComponent page ("Camera"). Its five recovered
    //    functions all index DirectorModule regions this reconstruction does not model yet.
    //    QUIET no-ops: the debug menu simply has an empty Camera page.
    void DebugComponent::Construct(DirectorModule* lpDirectorModule)
    {
        (void)lpDirectorModule;
    }

    void DebugComponent::UpdatePanoramaScreenshots(Camera::Camera* lpCamera)
    {
        (void)lpCamera;
    }

    void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        (void)lpRender;
    }

    const char* DebugComponent::GetName() const
    {
        return "Camera";   // the page name the console registers (DWARF + the header's note)
    }

    void DebugComponent::OnActivate() {}

    // -- SharedPlaylists::Construct. Home = Utils/BrnICEMoviePlayer.cpp (bodied there), which
    //    is not in the link because ICEMoviePlayer pulls the whole ICE take/movie chain.
    //    The container Constructs one by value; nothing reads it on this path.
    //    DELETE-WHEN: BrnICEMoviePlayer.cpp joins the link.
    void SharedPlaylists::Construct() {}

    // -- The two scene-query post-office free functions BrnSceneQueryInterface.h declares.
    //    OutEventVolumeTestDeepest mints the 16-bit query id for a staged volume test; 0 is
    //    the console's "no id" value and SceneQueryInterface treats it as a failed post.
    //    sub_8221CC98 resets slot 1's post office and returns a pointer into it.
    //    DELETE-WHEN: the two post-office TUs land.
    u32* sub_8221CC98(void* lpSlot)
    {
        return static_cast<u32*>(lpSlot);
    }
}

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    u32 OutEventVolumeTestDeepest(void* lpPostOffice, void* lpQueryParams)
    {
        (void)lpPostOffice;
        (void)lpQueryParams;
        return 0u;
    }
}

// (BaseCollisionGenerator::Destruct @0x8284CB38 REMOVED 2026-08-10, cache-fill wave: its
//  owning TU CgsCollisionGenerator.cpp is now on the build list and carries the same
//  attested-empty body, so this PC-leaf stand-in became an LNK2005 -- which is exactly the
//  tripwire it was left behind to be.)
}

// ----------------------------------------------------------------------------
// GROUP D -- declared-only leaves of TUs that ARE in the link.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
namespace Camera
{
    // BehaviourManager::DebugDumpToTTY @0x82220750 -- walks every helper slot and prints
    // GetDebugFullName. It is called from the manager's allocation-failure path only.
    // QUIET no-op: the allocation failure still asserts through its own CGS_ASSERT.
    // DELETE-WHEN: BehaviourHelper::GetDebugFullName lands.
// FLAG PC-platform leaf: debug-TTY dump no-op on the PC link (director mount 2026-07-29) -- the real body walks BehaviourHelper::GetDebugFullName, which is declaration-only behind the un-homed helper interior.
    void BehaviourManager::DebugDumpToTTY() const {}
}
}

namespace BrnTraffic
{
    // CalcDirectionAtParameter is GONE FROM HERE (2026-07-29): transcribed for real into
    // SharedClasses/Traffic/BrnTrafficSection.cpp beside its landed siblings, from
    // @0x821F4DB8. It was on the fly-by's own data path -- the road runner's lane frame -- and
    // the stub's zeroed output is what made the first real lane seat report dir=(0,0,0).

    // CalcTransformAtParameter is GONE FROM HERE (2026-07-29): transcribed for real into
    // SharedClasses/Traffic/BrnTrafficSection.cpp from the console's two-function split
    // (sub_82219030 resolves the rung pair, sub_82207998 does the arithmetic). It is on the
    // fly-by's own data path -- MoveAlongTrafficLane{Forwards,Backwards} sample the reached
    // lane point through it -- and the stub's zeroed axes would have produced a look-at with
    // no forward at every step of the walk.

    // FindNeighbourForRung is GONE FROM HERE (2026-08-22): real body @0x82752B70 transcribed
    // into SharedClasses/Traffic/BrnTrafficSection.cpp. It is on the driving-traffic path
    // (UpdateParams_UpdateNeighbours / UpdateParams_UpdatePlan lane changes), where the
    // 0xFFFF sentinel meant no traffic car ever found a lane to change into.

    // CalcDistanceAlongSection is GONE FROM HERE (2026-08-22): the real body @0x82705900 was
    // already landed in SharedClasses/Traffic/BrnTrafficSection.cpp, so this stub was an
    // LNK2005 against it as well as a 0.0f on every UpdateParams lookahead and gap test.
}

namespace ICE
{
    // The two ICE sub-objects BrnDirector::ICEWrapper embeds BY VALUE, so its (stubbed) ctor
    // has to default-construct them. Their real ctors live in un-landed TUs
    // (SDKs/Packages/ICE/ICEManager + GameSource/Director/Camera/ICECameraMover.cpp).
    // Empty is the honest body here: every member of both is either default-init or set by
    // the matching Construct(), and neither Construct runs on this build (ICEWrapper::Construct
    // is itself stubbed above -- no ICE take is ever played).
    // DELETE-WHEN: ICEManager / ICECameraMover join the link.
    ICEManager::ICEManager() {}
    ICECameraMover::ICECameraMover() {}

    // ⭐ RETIRED 2026-08-01 (ICE take-runtime wave): `ICETake::ICETake() {}` used to sit
    // here. It was a SILENT-DROP stub of the exact species the RaceCarState::operator=
    // incident taught us to hunt: the real ctor (SDKs/Packages/ICE/ICEDataICETake.cpp,
    // X360 @0x822145E8) MemClears the 48-entry decoded value table mValues[], and this
    // empty body left all 192 bytes as stack/heap garbage. Its own comment justified it
    // as "pulled in by ICEManager's embedded ICEController" -- i.e. never actually
    // evaluated -- and that excuse expired the moment the take runtime joined the link.
    // The real body now owns the symbol (this file lost an LNK2005 to it, which is how
    // the stub was found).
}

// RETIRED (2026-07-31): the `Attrib::FindCollection` stub that used to sit here is GONE.
// The coordinated decl+consumer pass it was waiting on has landed -- the canonical
// declaration in GameSource/AttribSys/Generated/attrib_findcollection.h now carries the
// asm-verified two-key signature and the real body lives in its own TU,
// SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribsupport.cpp (already in
// the exe source list). The consumer count turned out to be 9 generated ctors + one
// hand-written caller, not the 57 the old comment estimated.

namespace rw
{
namespace math
{
namespace vpu
{
    // The vendor affine-matrix SLerp (declared in rw/math/vpu/matrix44affine_operation.h,
    // body owned by the SDK and not reconstructed). ONE caller reaches it:
    // InertiaController::Update @0x8221ECD0, and only on the branch where the camera has
    // requested LAG (`1 - CameraEffects::mfCameraLag < 1`, i.e. lag > 0). With no lag the
    // console returns before the call, which is the state on every frame of this build.
    // The stub therefore returns lrTo unchanged -- exactly the t == 1 endpoint, i.e. "adopt
    // the freshly-finalised transform", which is what the no-lag path already does. It is the
    // inert answer, not an approximation of the interpolation.
    // DELETE-WHEN: the vendor op is reconstructed (then camera lag starts working).
    Matrix44Affine SLerp(const Matrix44Affine& lrFrom, const Matrix44Affine& lrTo,
                         const float* lpafBlend)
    {
        (void)lrFrom;
        (void)lpafBlend;
        return lrTo;
    }
}
}
}

// ⛔⛔ ALL THREE TRIGGER-REGION STUBS ARE NOW RETIRED. The `namespace BrnTrigger { ... }` block
// that stood here is DELETED (2026-08-20, [gateui r4], verify_r3_fix3gsm N4).
//   * `GenericRegion::GetType()` and `TriggerData::GetGenericRegion(s32)` went on 2026-08-01 --
//     real bodies in BrnGenericRegion.h (the console inlines it too) and BrnTriggerData.cpp.
//     The first was the dangerous kind: `(Type)0` is E_TYPE_JUNK_YARD, so every one of the 4670
//     generic regions in TRIGGERS.DAT answered "I am a junkyard" to anyone who asked, and the
//     director survived it only because its partner stub handed back NULL.
//   * `BoxRegion::ComputeTransform()` was the last one, an IDENTITY matrix whose own banner
//     admitted "STILL A STUB". It now has its real body -- the three-SinCos basis rotation --
//     at `SharedClasses/Trigger/BrnRegion.cpp` (X360 0x821F2FD0), alongside
//     `BoxRegion::ComputeDirection` (0x821F2CA8).
// KEEPING the stub here would be an LNK2005 against BrnRegion.cpp the moment that TU mounts,
// which this wave's TriggerEntityModuleInputInterface / TriggerQueryManager mounts require.

// ============================================================================
// GROUP E (NEW 2026-07-29, with the two shared gameplay cameras' RE-BASE)
//
// ⛔ ONE OF THE TWO IS RETIRED (2026-08-01, orbit-camera wave).
//   `CameraSphericalRotationController::Construct` was an EMPTY body here, and BOTH clauses
//   of its justification had expired:
//     * "the console INLINES it into each owner (so there is no standalone body to read, and
//        its tail lands inside an un-mapped SmoothMover)" -- the first half is true and is
//        not a reason to leave it empty (the inlining IS the body, and three owners emit the
//        identical ten stores); the second half is stale, because SmoothMover has been homed
//        since, so the tail lands on named members.
//     * "Nothing reads either one ... MainDirector::UpdateCameraBehavioursPostScene (the only
//        path that would dispatch it) is gated" -- the post-scene behaviour pass was un-gated
//        on 2026-08-01, and BehaviourRotateAboutVehicle::BecomeSimilarTo calls this on the
//        LIVE car-select path to discard accumulated stick state. With the empty stub the
//        stale yaw/pitch survived every re-seat.
//   It now has its real body in its own home, Camera/Utils/BrnCameraSphericalRotationController.cpp.
//   THAT IS THE THIRD TIME THIS CAMPAIGN A "nothing on the live path reads this" GATE HAS
//   GONE STALE WITHOUT ANYTHING IN THE BUILD, THE LINKER OR A BOOT TEST NOTICING.
//
// ⛔⛔ BOTH OF GROUP E'S STUBS ARE NOW RETIRED. `CameraShakeICEController::Construct` went
//   the same way as its neighbour on 2026-08-02 (ICE-shake wave) -- real body in
//   Camera/Utils/BrnCameraShakeICEController.cpp -- and ITS justification had expired too. It
//   read:
//     "unlike its neighbour it is a real (non-inlined) console call whose body was never
//      dumped, so there is nothing to transcribe. It stays safe for the same reason as before
//      -- the manager's pools construct every behaviour with `new (slot) T()`
//      (BrnAbstractPool.h:148), i.e. value-initialisation, so the sub-object starts zeroed
//      regardless"
//   ⚠️⚠️ THE FIRST CLAUSE WAS SIMPLY FALSE: 0x8223EBF0 is a fully exported 186-line function
//     in .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8223EBF0.json. "Never dumped" is a claim about
//     the export set, and the export set is one directory listing away. FOURTEENTH stale gate.
//   ⚠️⚠️ AND THE SECOND CLAUSE -- the safety argument -- WAS THE DANGER, not the mitigation.
//     Construct's real job is to set mMatrix to the IDENTITY. Zero-initialised, mMatrix is the
//     ALL-ZERO matrix, and BehaviourGameplayExternal::Update inlines GetMatrix() as four
//     `lvx128` off mBoostShake (0x82241C70..0x82241C8C) and multiplies the result into the
//     camera transform. An all-zero matrix does not leave a post-multiply alone -- it
//     ANNIHILATES it, collapsing the chase camera onto the origin with an empty basis.
//     BehaviourGameplayExternal::Prepare has been calling mBoostShake.Construct() since
//     2026-08-01 (BrnBehaviourGameplayExternal.cpp:260), so the zeroed matrix was already
//     sitting in the object waiting for its one reader to land.
//   ⇒ THAT IS THE THIRD TIME THIS CAMPAIGN A "safe because nothing reads it yet" GATE HAS
//     BEEN ONE COMMIT AWAY FROM BEING WRONG, and the second time in this very namespace.
// ============================================================================
namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    // ------------------------------------------------------------------------
    // ✅ CameraShake::Update @0x82221310 -- THE STUB IS GONE (retired 2026-08-02,
    //    rotate-helper wave). It used to be an EMPTY `{}` right here.
    //
    // It was the textbook silent-drop stub: it compiled, linked, ran, and discarded every
    // camera shake in the game, with nothing in the build, the linker or a boot test able to
    // say so. Its own FLAG spelled out that two independent accidents were hiding it and that
    // the second would die the moment a non-zero shake blend arrived.
    //
    // ALL THREE of its named blockers are now closed, and TWO OF THE THREE REASONS TURNED OUT
    // TO BE SOFTER THAN THEY READ -- which is the part worth keeping:
    //   * `Utils::RotateMatrix44AffineByEulerAnglesZXY` was described here as "almost entirely
    //     an inlined XMVectorSinCos minimax polynomial whose coefficient table has not been
    //     dumped". Both halves were true and NEITHER was a reason: the coefficients are an
    //     implementation detail OF sin and cos, and de-optimising a console minimax to the
    //     exact libm form is the standing convention of the very file it lives in. BODIED in
    //     Camera/Utils/CameraUtils.cpp.
    //   * `CgsNumeric::Random::RandomFloat(f32,f32)` / `::RandomVector(Vector3,Vector3)` were
    //     described as having "no X360 symbol for either (both inlined / ICF-folded away)".
    //     True, and again not a reason: an inline expansion IS a body, and this very function
    //     inlines the scalar draw three times over. BODIED in
    //     GameShared/GameClasses/Numeric/CgsRandom.cpp.
    //   * the Serialise<S> drag WAS real, and was solved the way this comment itself
    //     suggested: `Update` is file-split into Camera/Utils/BrnCameraShakeUpdate.cpp, which
    //     is what the build now mounts. BrnCameraShake.cpp (the serialiser slice) stays off.
    //
    // ⭐ AND IT WAS RETIRED BEFORE ITS SECOND CALLER LANDED, ON PURPOSE.
    //   BehaviourGameplayExternal::ApplyJumpEffects -- bodied the same day -- ends on a call
    //   to this function, and BehaviourGameplayExternal::Update .cpp:505 will make another.
    //   Had the stub still been standing, both of those shakes would have silently done
    //   nothing the moment they linked.
    // ------------------------------------------------------------------------
}
}
}

// ============================================================================
// GROUP F -- THE MOMENT SUB-SYSTEM (the establishing-shot / jump-cutaway camera).
//
// REWORKED 2026-08-23 (jump/stunt cutaway wave). It held FOUR entries when it was written on
// 2026-08-01; two were already real by that evening; TODAY one more became real and one new
// one was discovered. Net: it is DOWN TO TWO, and only ONE of those two is on a live path.
//
//   ✅ MomentSelector::SelectBestMomentWithExclusion @0x82250FC8 -- WAS a `return false`
//      stub sitting directly on the cutaway path. Now REAL in BrnMomentSelector.cpp,
//      together with its LRU arm SelectBestLRUMomentWithExclusion @0x8221BE50 (the only arm
//      the shipped path can take: MomentSelector::Construct writes meSelectionMode =
//      E_MODE_LRU_BEST and nothing calls SetSelectionMode).
//   ⛔ MomentController::NewMoment @0x82255850 -- still stubbed. See below.
//   ⛔ MomentSelector::SelectBestRandomMomentWithExclusion @0x8223A668 -- NEWLY stubbed,
//      deliberately, and UNREACHABLE. See its own banner.
//
// ============================================================================
// ⭐⭐ READ THIS BEFORE PLANNING ANOTHER "MOUNT THE MOMENTS" WAVE.
//
// The 2026-08-01 DELETE-WHEN here said mounting the NewMoment TU "costs +9 unresolved" and
// that the moment closure "is a wave of its own; it is NOT a camera blocker." BOTH HALVES OF
// THAT ARE WRONG, and they were wrong in opposite directions.
//
// (1) IT IS A CAMERA BLOCKER. This is the second half of the user-reported bug -- "super
//     jumps do not get counted at all. camera is also not firing". NewMoment stubbed means
//     it allocates nothing, so every MomentHandle stays !IsAllocated(); MomentSelector::
//     Update's classification loop `continue`s on all of them; muValidMoments is pinned at
//     the 0 that Construct writes; ArbStateRoaming::Update's DRIVING arm therefore never
//     calls SelectBestMoment and always falls through to GetSelectedGameplayCamera().
//
// (2) "+9" IS STALE BY AN ORDER OF MAGNITUDE. MEASURED 2026-08-23 against the object list of
//     the current shipping build (build/game/obj -- 1,524 objects archived into one .lib and
//     symbol-dumped, so this is the real defined set, not an estimate). Mounting
//     BrnMomentControllerNewMoment.cpp + BrnMoment.cpp + all 14 Moments/*.cpp costs
//     **142 NON-CRT unresolved externals**, attributed per TU:
//         NewMoment 33 | PlayerJumping 33 | HardStop 26 | TakedownLookback 18 |
//         PlayerStunt 13 | Tumbling 10 | BystanderSeesAction 9 | StationaryCrash 9 |
//         PassengerSeesAction 5 | NewCarJoined 5 | HitTraffic 2 | StaticCamImpact 1
//     (Trimming NewMoment's switch to only the two cutaway types still measured ~50.)
//     Every `AllocateVoid<MomentXxx>()` arm placement-constructs a MomentXxx, which emits its
//     vftable, which needs EVERY virtual of that class defined at link -- so the twelve-arm
//     switch drags all twelve subclasses in whole. Three families dominate:
//       (a) ~44 `detail::MomentSharedInfo_*` reach shims. The MomentSharedInfo record has NO
//           home anywhere in this tree; every Moments/*.cpp declares its own decl-only
//           free-function reaches into it. ⭐ HOMING MomentSharedInfo IS THE KEYSTONE -- it
//           collapses ~44 of the 142 in one move and unblocks every moment TU at once.
//       (b) 24 symbols from the four `BehaviourCollection<T,P,N>` instantiations
//           MomentPlayerJumping holds. That is only SIX template methods (Construct/Prepare/
//           Release/AddShot/CanSwitchToMeNow/HasFailed); write the template bodies once (X360
//           addresses are listed in BrnMomentPlayerJumping.h) and all four instantiations
//           resolve together.
//       (c) ~38 declaration-only per-class virtuals (Destruct / GetInstanceType /
//           SetParameters / Prepare / Release), most one-liners straight from the asm.
//     The rest belong to other lanes (BehaviourRig's own TU, BehaviourParameterBank's
//     GetPlayerJumping*ShotParams accessors, DirectorResourceManager::GetKeyAnim).
//
// (3) ⛔⛔ AND THE CLOSURE ALONE STILL WOULD NOT MAKE A CUTAWAY PLAY. NOTHING IN THIS TREE
//     TICKS A MOMENT. The console chain is
//         MainDirector::Update  ->  MainDirector::UpdateMoments @0x82250268
//                               ->  MomentController::UpdateAllMoments @0x82239DE8
//                               ->  each allocated moment's vtable slot 2 (Update)
//     and all three links are missing here: UpdateAllMoments has no body anywhere,
//     UpdateMoments is declaration-only (BrnMainDirector.h:161), and the call itself is
//     COMMENTED OUT in MainDirector::Update (BrnMainDirector.cpp:1617 --
//     `⚠️ GATE: UpdateMoments( lpIO, liPlayerCarIndex );`). A moment that is never Updated
//     never leaves E_STATE_INVALID_INACTIVE, so it is never IsValid(), so muValidMoments
//     stays 0 even with every handle allocated. Retiring this stub without that tick is a
//     silent no-op.
//
// DELETE-WHEN (revised, in dependency order -- do NOT start at the bottom):
//   1. Home the MomentSharedInfo record (its readers are the `detail::MomentSharedInfo_*`
//      declarations at the head of every Moments/*.cpp, each with its X360 offset in a
//      comment; the record is Moment::Update's third argument, currently `const void*`).
//   2. Body MomentController::UpdateAllMoments @0x82239DE8 (this file set) and
//      MainDirector::UpdateMoments @0x82250268, then un-gate BrnMainDirector.cpp:1617.
//      ⚠️ Those last two are NOT in this lane's file set.
//   3. Body the six BehaviourCollection<> template methods + the ~38 one-liner virtuals.
//   4. Mount GameSource/Director/MomentController/BrnMomentControllerNewMoment.cpp (which
//      already holds the real, full twelve-arm NewMoment) + BrnMoment.cpp + Moments/*.cpp,
//      and delete the stub below.
//
// ⭐ WHAT DID LAND TODAY, so the next wave does not re-discover it:
//   * BrnMomentSubclasses.h was an ELEVEN-CLASS ODR FORK -- layout-stubbed MomentXxx classes
//     whose Update() bodies were `{}`. NewMoment allocated THOSE. It is now a pure umbrella
//     over the real homes.
//   * BrnMomentParameterBank.cpp was a SIX-TYPE fork that re-declared Moment::Parameters as
//     `struct` (the class-key ODR fork the old DELETE-WHEN named) with two of its bool pairs
//     name-swapped. De-forked; every byte it writes is unchanged.
//   * BrnMomentPlayerJumping.cpp and BrnMomentTumbling.cpp now COMPILE (they did not).
//   * ⚠️ The moment pool's console bucket (`AbstractPool<70,20,Vector4>` == 1120 B) is TOO
//     SMALL on this x64 host: MomentPlayerJumping is 1296 B. Fixed + ratcheted with a
//     static_assert per moment type -- see the HOST BUCKET WIDENING banner in
//     BrnMomentController.h. Whoever mounts this would otherwise have got silent heap
//     corruption behind a non-fatal assert.
// ============================================================================
#include "GameSource/Director/MomentController/BrnMomentSelector.h"     // MomentSelector
#include "GameSource/Director/MomentController/BrnMomentController.h"   // MomentController

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // MomentController::NewMoment @0x82255850
    //
    // Real body: GameSource/Director/MomentController/BrnMomentControllerNewMoment.cpp
    // (complete, twelve-arm, compile-green -- it is the MOUNT that is blocked, not the code).
    //
    // ⛔ WHAT THIS DEGRADES, PRECISELY: no establishing-shot or jump/stunt cutaway can exist.
    // The console allocates a moment of leMomentType out of mMomentPool, hands the slot to the
    // handle, and pushes the bank's parameters onto it, then returns TRUE unconditionally
    // (`li r3, 1`). Here: allocate nothing and return the same TRUE, leaving the handle
    // !IsAllocated().
    //
    // ⭐ WHY IT RETURNS TRUE AND MUST KEEP RETURNING TRUE. This is not "the stub's convenient
    // value" -- it is load-bearing. A FALSE here clears MomentSelector::Prepare's mbPrepared,
    // which IS ArbStateRoaming::Prepare's return value, which IS the gate that lets meState
    // leave E_STATE_PREPARING. A stub that reported failure would freeze the entire director
    // state machine, not just the cutaways. The console's own success path returns TRUE
    // unconditionally, so this stub cannot report a failure the console would not have.
    //
    // ⭐ AND AN UNALLOCATED HANDLE IS SAFE DOWNSTREAM, BY CONSTRUCTION: MomentSelector::
    // Update's loop `continue`s on !IsAllocated() before it touches the moment, and
    // MomentHandle::Release is a no-op on one. That is exactly why this stub has been
    // invisible for three weeks -- nothing crashes, nothing asserts, the cutaways just never
    // happen.
    //
    // DELETE-WHEN: see the four-step revised plan in the GROUP F banner above. Steps 1 and 2
    // (home MomentSharedInfo; body + un-gate the per-frame moment tick) come FIRST -- deleting
    // this stub before them buys a link, not a camera.
    // ------------------------------------------------------------------------
    bool MomentController::NewMoment(Moment::EType leMomentType,
                                     MomentParameterBank::EMomentParamID leMomentParamID,
                                     MomentHandle& lrMomentHandleInOut,
                                     Camera::BehaviourManager& lrBehaviourManager)
    {
        (void)leMomentType;
        (void)leMomentParamID;
        (void)lrMomentHandleInOut;   // deliberately left !IsAllocated()
        (void)lrBehaviourManager;
        return true;
    }

    // ------------------------------------------------------------------------
    // MomentSelector::SelectBestRandomMomentWithExclusion @0x8223A668
    // (DWARF BrnMomentSelector.h:213 -> BrnMomentSelector.cpp:406)   NEW STUB 2026-08-23
    //
    // ⛔ WHAT THIS DEGRADES, PRECISELY: nothing that runs. It is the E_MODE_RANDOM_BEST
    // (meSelectionMode == 1) arm of the SelectBestMomentWithExclusion that was bodied today.
    // meSelectionMode is written in exactly two places in the whole program: MomentSelector::
    // Construct sets it to E_MODE_LRU_BEST (0) -- attested at ArbStateRoaming::Construct
    // @0x82259CD0, `stw 0, +0x1DC` -- and MomentSelector::SetSelectionMode, which is
    // DECLARATION-ONLY and which NOTHING in this tree calls (grep is clean; the three
    // arbitrator states that embed a selector all go Construct -> AddMoment -> Prepare). So
    // the dispatch can only ever take the LRU arm, which IS real. FALSE is also what the
    // console returns from this function when no candidate qualifies, so even if the mode were
    // somehow raised, this cannot report a selection the console would not have reported.
    //
    // ⭐ WHY NOT JUST LEAVE IT UNRESOLVED: SelectBestMomentWithExclusion's switch names it, and
    // /OPT:REF does not excuse an unresolved external in a COMDAT that IS referenced -- and
    // this one is, from a function on the live per-frame path.
    //
    // WHY IT IS NOT BODIED: the console body (132 asm lines) builds a weighted-random selector
    // on the stack -- `int_10_::AddElement(&selector, weight, &index)` per candidate then
    // `int_10_::GetSelection(&selector, random)` -- i.e. an Array<int,10>-backed
    // WeightedSelection<> whose two methods are their own X360 symbols and whose type has no
    // home anywhere in this tree. Writing it means recovering that container first, and the
    // result would be dead code. Faking a "pick the first valid one" body would be inventing a
    // selection policy the binary does not have.
    //
    // DELETE-WHEN: the weighted-selection container (AddElement / GetSelection) is recovered,
    // then body this from @0x8223A668 -- the surrounding candidate loop is IDENTICAL to the LRU
    // one already written in BrnMomentSelector.cpp (same ordered tests, same
    // (1 - recency) * weighting score), only the "keep the best" step differs.
    // ------------------------------------------------------------------------
    bool MomentSelector::SelectBestRandomMomentWithExclusion(CgsNumeric::Random& lRandom,
                                                            s32 liExclusion)
    {
        (void)lRandom;
        (void)liExclusion;
        return false;
    }
}
