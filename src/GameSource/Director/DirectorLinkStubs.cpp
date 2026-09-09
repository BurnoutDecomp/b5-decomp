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

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                          // ICEWrapper::Construct boot witness
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
    // ===========================================================================================
    // ⭐⭐⭐ ITEM 4 IS DONE. THE CRASH CAMERA IS ON SCREEN AND ITS SLOW MOTION IS FILMED.
    // (Rewritten 2026-08-29. The chain had SEVEN breaks, not four and not five; all seven are
    //  closed. Every line below is a measured run or a read of the asm, not an inference.)
    //
    //   [crashcam] mbCrashActive -> 1 (playerIdx=0 raceCars=1)
    //   [crashcam] roaming crash edge: crashActive=1 camCrashFlag=0 takedownBit=0 candidate=1
    //   [crashcam] Prepare -> 0 (framesActive=0 validMoments=0 deathcam=0 crashType=0)
    //   [crashcam] Update meState -> 1            PREPARING
    //   [crashcam] Prepare -> 1 (framesActive=2)  accepted on frame 2, per the moment rule
    //   [crashcam] Update meState -> 2            ACTIVE
    //   [crashcam] container current state -> 2 (ArbStateCrashing)
    //   [crashcam] slomo requested scale=0.285714 velMagSq=219.81 firstFrame=1
    //   [slomo] FILM LATCH raised on dilation episode 1 (scale 0.285714)
    //   [slomo] simScale=0.285714 gameScale=1.000000 simStep=0.004762 gameStep=0.016667
    //   [crashcam] Update meState -> 3 -> 5 -> 0  AFTERCRASH, INTERPOLATING, released
    // 260 frames captured at every present -- 107 dilated (present 4964..5070), then 153 at real
    // time -- each stamped with its own timestep in the frames.csv sidecar.
    //
    // ---- THE SEVEN, AND WHAT EACH ONE TURNED OUT TO BE -----------------------------------
    // (1) the director's time-dilation transport -- ICEElementDescription::GetDefaultFloat()
    //     type-punned an INTEGER default through the ICEValue union. Fixed 2026-08-28.
    // (2) mfSimTimestep published as a hard 0 by a stale gate. Fixed 2026-08-28.
    // (3) nothing wrote GameState::mbCrashActive -- the gate on roaming's crash edge, whose only
    //     writer in the image is MainDirector::ProcessInputQueue's tail. Landed 2026-08-29,
    //     atomically with (4).
    // (4) ArbStateCrashing did not exist: its container slot was an empty placeholder whose
    //     inherited Update never writes meState and which has no exit edge. All nine functions
    //     landed 2026-08-29.
    // (5) the two impact controllers were "a mount away" -- they were NOT. BehaviourBystanderCam
    //     .cpp reaches every foreign object through ~30 `namespace detail` shims that are
    //     declared and never defined, and the one carrying the whole feature
    //     (`Camera_SetTimeScale`) is the slow-motion controller's ONLY observable effect. Both
    //     Updates re-expressed against the real types in a mounted partfile.
    // (6) VehicleTracker::Update was GATED, and it is the slow motion's ONLY data source -- the
    //     linear-velocity journal ImpactSlomoController reads is written nowhere else. The gate
    //     said the type was "un-homed"; it was not, it was that Update reached a private ODR fork
    //     of DirectorIO::InputBuffer. Fork retired, body re-fitted, TU mounted, caller un-gated.
    // (7) ⛔⛔ THE ARBITRATOR NEVER LEFT ArbStateCarSelect. Traced:
    //       container current state -> 1 (ArbStateRoaming)
    //       container current state -> 8 (ArbStateCarSelect)   <- and never again
    //       roaming meState -> 0                               <- for the rest of the session
    //     ArbStateRoaming::ProcessPossibleStateChanges is the ONLY caller of the crash /
    //     takedown / race-intro / drive-thru / post-event / rank-up edges, so EVERY OTHER
    //     DIRECTOR STATE WAS UNREACHABLE. Invisible because car select's ACTIVE arm publishes
    //     GetSelectedGameplayCamera(), i.e. the ordinary chase camera. The gate that caused it
    //     read "Release() is NOT in this TU's X360 export set, so the hand-off cannot be written
    //     faithfully" -- ⚠️ absence from an export set means the class does not OVERRIDE it, and
    //     the DWARF does declare ArbStateCarSelect::Release. Un-gated 2026-08-29.
    //
    // ---- WHAT IS STILL OPEN, IN THE ORDER A NEXT WAVE SHOULD TAKE IT ---------------------
    //
    // ⛔ A. THE DETERMINISTIC CRASH TRIGGER CANNOT PRODUCE THE SLOW MOTION -- only a real
    //    high-speed collision can, and that is a PHYSICS fact, not a director one.
    //    ImpactSlomoController::Update needs |linVel|^2 > 179.86 (30 MPH in squared m/s).
    //    MEASURED three ways: `slomo requested scale=1.000000 velMagSq=0.478469` on a forced
    //    crash (the controller RAN and measured 0.48 against a 179.86 gate); the 1 Hz heartbeat
    //    read 0.004..0.09 for the whole ACTIVE window; and [crash-probe] showed the car moving
    //    7 mm per frame while mfTimeCrashing advanced normally. The SAME instrument read
    //    809..2241 while driving, so the zero is the car, not the probe.
    //    ⇒ ForceRaceCarCrash / SetRaceCarCrashing leave the car stationary on this build. Until
    //    the crashing-car rigid body carries its momentum, -CrashPlayer is useless for filming
    //    the slow motion and a natural collision at 60+ MPH is the only route (roughly one run
    //    in two produces one; two of five reach the y-apex the gate also needs).
    //
    // ⛔ B. ArbStateCrashMode -- the stub two lines below is the LAST piece of item 4. Its
    //    Prepare @0x82265F70 is misattributed to BrnBehaviourManager.h, where the header declares
    //    it with NO definition: a shadowing redeclaration only a LINK finds. ⚠⚠ POLARITY TRAP:
    //    AttractMode negates at the call site, CrashMode does not -- different symbols, identical
    //    `IsReadyToPrepare()` spelling. The remaining blocker is the parameters argument
    //    (`manager + 0x125AC` == BehaviourParameterBank + 0x7C); carve
    //    BehaviourAftertouchCrash::Parameters there, exactly as the deathcam block was carved
    //    into NamedParameters this wave.
    //
    // ⛔ C. THE CRASH-ENERGY CLASSIFIER (BrnDirectorVehicleTracker.cpp). meCrashType is pinned
    //    at E_CRASH_NOT_CRASHING because its two thresholds are .data floats with no recoverable
    //    value and the 0.0f placeholders would answer E_CRASH_LOW_ENERGY every time -- the ONE
    //    value that SUPPRESSES the slow motion. It also needs two InputBuffer accessors
    //    (+0x7900 player speed, +0x7904 the fast-top-down arm) that do not exist. Until it lands,
    //    the crash camera slows down crashes the console might have left at real time.
    //
    // ⛔ D. ArbStateCarSelect::Release. The DWARF declares it (BrnArbStateCarSelect.h:196); the
    //    body is not recovered, so the hand-back to roaming carries ONE FLAGged line
    //    (`meState = E_STATE_INACTIVE`) standing in for it. ⚠ That line is load-bearing, not
    //    tidiness: without it the state re-takes the frame every update -- measured 6,066
    //    SetCurrentState calls in one run -- and because UpdateAll visits car select (index 8)
    //    AFTER crashing (index 2), the crash camera was handed the frame and had it taken away
    //    again in the same update. Fold the line into the real Release when it lands.
    //
    // ⛔ E. THE MOMENT SUB-SYSTEM still answers "no valid moments" (GROUP F's
    //    MomentController::NewMoment allocates nothing), so SelectNormalCrashCamera always takes
    //    its failsafe arm and the crash camera is the chase camera rather than an authored shot.
    //    THE SLOW MOTION DOES NOT DEPEND ON IT -- it fires from the failsafe arm, as filmed --
    //    but the framing does. Two GROUP G traps (MomentTumbling::SignalIsGoodTimeToPlant,
    //    MomentBystanderSeesAction::SetPerceivedDistanceModificationFactor) go live the same day
    //    this does; both bodies are already written at their own homes.
    //
    // ⛔ F. PlayerCrashInfo's PRODUCER. Its LAYOUT is homed now (the DWARF had it all along, as
    //    BrnDirector::Camera::PlayerCrashInfo -- +0x26 mbWrecked, +0x27 mbHitWater, which also
    //    names the condition on Arbitrator::Update's gated "BlackFade_Water" branch), but step 13
    //    of BridgeWorldToDirector still does not fill it. Until it does, mbWrecked reads whatever
    //    the input buffer holds and the "Wrecked" exit is not trustworthy.
    //
    // ---- THE DIAGNOSTIC THAT FOUND ALL OF THIS -------------------------------------------
    // BRN_CRASHCAM_DIAG (off unless set; edge-triggered except one 1 Hz heartbeat) reports the
    // mbCrashActive edge, roaming's four-conjunct crash edge, both states' meState edges, the
    // CONTAINER'S CURRENT STATE -- whose single writer had no read-out at all -- and the
    // slow-motion controller's verdict together with the velocity it measured. Three runs of the
    // finished chain could not distinguish "the arbitrator never asked" from "we ran and the gate
    // said no" until it existed. Keep it until item 4's remainder is closed.
    // ===========================================================================================
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateCrashMode,       "ArbStateCrashMode")
    // ⭐⭐ ArbStateDriveThru's FIVE STUBS ARE GONE (2026-08-29, drive-thru camera wave).
    // GameSource/Director/Arbitrator/States/BrnArbStateDriveThru.cpp is on the exe source list
    // and owns Construct/Prepare/Update/Release/GetName for real. While these stubs stood,
    // Prepare's unconditional `true` and Update's empty body meant the drive-thru state took
    // the frame, published a camera nothing had ever written, and never handed back --
    // structurally identical to what ArbStateCrashing's empty shell did to the crash camera.
    // (Nothing noticed, because until item (7) above was fixed the arbitrator never left car
    // select and this state was unreachable anyway.)
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateOnlineCarSelect, "ArbStateOnlineCarSelect")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateOnlineRaceIntro, "ArbStateOnlineRaceIntro")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStatePostEvent,       "ArbStatePostEvent")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateRaceIntro,       "ArbStateRaceIntro")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateRankUp,          "ArbStateRankUp")

    // CrashNav and Roaming already own SOME of their virtuals in their mounted-elsewhere
    // headers, so only the missing slots are stubbed.
    // These five are silent-drop stubs for a state that is fully reconstructed but not mounted:
    // Arbitrator/States/BrnArbStateCrashNav.cpp owns the real Construct/Prepare/Update/
    // Release/GetName. While they stand, Update does nothing and Release always succeeds, so
    // un-gating the arbitrator's crash-nav trigger would enter a state that publishes a camera
    // nothing ever writes.
    // ⇒ ALL FIVE COME OUT TOGETHER WITH THE MOUNT, never one at a time (LNK2005 x5 otherwise).
    // That TU needs exactly twelve externals this link does not provide:
    //     8  ICEMoviePlayer::{Construct,Prepare,Update,Stop,Loop,GetCamera,InterpolateFrom,
    //        CutToInterpolateOut} -- written, in the unmounted Utils/BrnICEMoviePlayer.cpp.
    //     2  ICEWrapper::{GetCurrentMovie,PlayMovie} -- written, unmounted
    //        SDKs/Packages/ICE/ICEWrapper.cpp.
    //     1  SharedCameraContainer::GetGameplayCameraHelperIndex -- written, unmounted TU.
    //     1  Camera::Camera::SetRequestedBorderPostFX -- declared in Camera/Camera.h, defined
    //        nowhere in the tree; the only one needing recovery rather than a mount.
    void ArbStateCrashNav::Construct()                          { ArbitratorState::Construct(); }
    // Prepare's stub added 2026-08-29 ALONGSIDE the real body landing in the (still unmounted)
    // TU: declaring the override in BrnArbStateCrashNav.h puts a fifth slot in this class's
    // vtable, and MainDirector references that vtable -- so the header edit alone takes the
    // shared link RED (measured: LNK2001 from BrnMainDirector.obj). This forwards to the base,
    // which is byte-for-byte the behaviour the class had before the override existed.
    bool ArbStateCrashNav::Prepare(ArbStateSharedInfo& lrInfo)  { return ArbitratorState::Prepare(lrInfo); }
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

    // Concrete Behaviour vtables measure EIGHT slots, not the TEN Behaviour.h's banner lists:
    // its slots 6/7 (GetParameters/SetParameters) are very likely not virtual. Behaviour.h's lane.

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
    void ICEWrapper::Construct()
    {
        // PARTIAL BODY: every store the recorded Construct makes except the tail
        // Camera::Construct on the ICE camera's embedded director camera. That call is the
        // only missing declaration -- BrnDirector::Camera::Camera::Construct is bodied in
        // Camera/Camera.cpp, which is not on the exe source list. The full body is kept in
        // SDKs/Packages/ICE/ICEWrapper.cpp, unmounted.
        // miICELoadStateB (+0x120E8) is the stage word ICEWrapper::Prepare switches on.
        // DELETE-WHEN: GameSource/Director/Camera/Camera.cpp joins the link.
        mVehicleRef.Construct();

        // VehicleRef::Set(E_PLAYER_CAR, ..) inlined: the ref is bound, not merely zeroed.
        mVehicleRef.meType         = VehicleRef::E_PLAYER_CAR;
        mVehicleRef.mbSet          = true;
        mVehicleRef.muRef          = 0;
        mVehicleRef.miRaceCarIndex = -1;

        // Take the dev-tools action stack off the "unconstructed" sentinel the ctor seeds.
        mActionQueue.Clear();

        miICELoadStateB = 0;
        miICELoadStateA = 0;

        // MISSING HERE: mICECamera's embedded Camera::Construct (see above).

        mfTimeScale = 0.0f;

        // TEMPORARY boot witness, not console behaviour; remove once the wave is signed off.
        {
            static bool sbLoggedOnce = false;
            if (!sbLoggedOnce && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedOnce = true;
                *CgsDev::Log::gpDebugPrint
                    << "[g09-ice] ICEWrapper::Construct refType=" << static_cast<s32>(mVehicleRef.meType)
                    << " refSet=" << static_cast<s32>(mVehicleRef.mbSet)
                    << " raceCar=" << mVehicleRef.miRaceCarIndex
                    << " actionQueueLen=" << mActionQueue.miLength
                    << " loadStage=" << miICELoadStateB << "\n";
            }
        }
    }
    // Blocked on ICE::ICEController::DestructMenus, which has no body anywhere in the tree
    // (real body in the unmounted SDKs/Packages/ICE/ICEWrapper.cpp).
    // DELETE-WHEN: ICEController::DestructMenus is reconstructed and ICEManager.cpp mounts.
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

    // ---- SharedPlaylists::Construct: SCAFFOLD RETIRED 2026-09-08 (p0 wave) -- real TU
    //      GameSource/Director/Utils/BrnICEMoviePlayer_wP0_01.cpp is MOUNTED (the playlist
    //      half split out of BrnICEMoviePlayer.cpp) and really seeds the five playlists.

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
// ONE gate remains: MomentController::NewMoment, below.
//
// It is a camera blocker: NewMoment allocates nothing, so every MomentHandle stays
// !IsAllocated(), MomentSelector::Update's classification loop skips them all, muValidMoments
// is pinned at 0, and ArbStateRoaming::Update's DRIVING arm never calls SelectBestMoment.
//
// Mounting the closure (BrnMomentControllerNewMoment.cpp + BrnMoment.cpp + all 14
// Moments/*.cpp) costs 147 non-CRT unresolved externals, by family:
//     49  detail::MomentSharedInfo_* reach shims -- the record has no home in this tree; every
//         Moments/*.cpp declares its own declaration-only free-function reaches into it.
//     35  per-moment-class virtuals and privates (Destruct / GetInstanceType / SetParameters /
//         Prepare / Release, plus MomentTumbling::SetGyroCamParameters and
//         MomentPlayerJumping::UpdateCamera)
//     24  BehaviourCollection<T,P,N> methods -- six template methods x four instantiations
//     19  other subsystems (BehaviourRig's virtual set + Parameters::Construct, six
//         vector-deleting destructors, BehaviourPassengerCam::SetParameters,
//         Camera::SetRequestedBorderPostFX, ShotSelector::GetCrashShot,
//         DirectorResourceManager::GetKeyAnim)
//     13  other detail:: reach shims (ICETakeData_*, IceAnimShotData_*, Vehicle_*,
//         BehaviourRig_*, CameraState_AppendToDebugLog, ...)
//      7  BehaviourParameterBank / NamedParameters accessors
// Every `AllocateVoid<MomentXxx>()` arm placement-constructs a MomentXxx, which emits its
// vftable, which needs every virtual of that class defined at link -- so the twelve-arm switch
// drags all twelve subclasses in whole.
//
// AND THE CLOSURE ALONE STILL WOULD NOT MAKE A CUTAWAY PLAY: nothing in this tree ticks a
// moment. MomentController::UpdateAllMoments has no body anywhere, MainDirector::UpdateMoments
// is declaration-only, and the call is commented out in MainDirector::Update -- so a moment
// never leaves E_STATE_INVALID_INACTIVE and muValidMoments stays 0 even with every handle
// allocated. Retiring the gate without that tick is a silent no-op.
//
// DELETE-WHEN, in dependency order -- do NOT start at the bottom:
//   1. Give BehaviourParameterBank / NamedParameters the moment camera parameter blocks and
//      body their accessors (Camera lane). The four cheapest moment TUs are blocked on nothing
//      else, and every one of those accessors returns a const reference, so none can be stubbed.
//   2. Home the MomentSharedInfo record (its readers are the detail:: declarations at the head
//      of every Moments/*.cpp; it is Moment::Update's third argument, currently `const void*`).
//   3. Body MomentController::UpdateAllMoments and MainDirector::UpdateMoments, then un-gate
//      the commented-out call in MainDirector::Update. Those last two are NOT in this file set.
//   4. Body the six BehaviourCollection<> template methods and the per-class virtuals.
//   5. Mount the closure TUs and delete the gate below.
//
// The moment pool's console bucket (`AbstractPool<70,20,Vector4>` == 1120 B) is TOO SMALL on
// this x64 host: MomentPlayerJumping is 1296 B. Fixed and ratcheted with a static_assert per
// moment type -- see the HOST BUCKET WIDENING banner in BrnMomentController.h.
// ============================================================================
#include "GameSource/Director/MomentController/BrnMomentSelector.h"     // MomentSelector
#include "GameSource/Director/MomentController/BrnMomentController.h"   // MomentController
#include "GameSource/Director/Camera/BrnCameraValidityAccount.h"        // group G: ValidityAccount::Print x2
#include "GameSource/Director/MomentController/Moments/BrnMomentTumbling.h"  // group G: MomentTumbling
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h" // group G: DebugPrinter / DebugLog

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // MomentController::NewMoment @0x82255850
    //
    // Real body: GameSource/Director/MomentController/BrnMomentControllerNewMoment.cpp
    // (complete, twelve-arm, compile-green -- it is the MOUNT that is blocked, not the code).
    //
    // WHAT THIS DEGRADES: no establishing-shot or jump/stunt cutaway can exist. The console
    // allocates a moment of leMomentType out of mMomentPool, hands the slot to the handle and
    // pushes the bank's parameters onto it, then returns TRUE unconditionally. Here: allocate
    // nothing and return the same TRUE, leaving the handle !IsAllocated().
    //
    // TRUE IS LOAD-BEARING: a FALSE clears MomentSelector::Prepare's mbPrepared, which is
    // ArbStateRoaming::Prepare's return value, which is the gate that lets meState leave
    // E_STATE_PREPARING -- reporting failure would freeze the whole director state machine.
    // An unallocated handle is safe downstream: MomentSelector::Update's loop `continue`s on
    // !IsAllocated() and MomentHandle::Release is a no-op on one.
    //
    // DELETE-WHEN: the five-step plan in the GROUP F banner above, in that order.
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
    // ⭐ GROUP G (NEW 2026-08-29, crash-camera wave) -- THE THREE DEV-ONLY LEAVES
    // BrnArbStateCrashing.cpp REACHES. All three are TRAP stubs, not quiet ones, and that is
    // deliberate: every call site is inside `if (IsDebugDisplayActive())`, and
    // ArbStateCrashing::Construct seeds that flag FALSE (the console only raises it from a dev
    // tool), so retail never reaches any of them. A quiet body would make them look finished
    // for ever; a trap says "unfinished" and can only fire under the dev flag.
    //
    //   MomentSelector::ActualDebugRender  @0x8221C6D8  (~300 asm lines: it walks the handle +
    //       description arrays and prints one line per candidate through the DebugPrinter)
    //   ValidityAccount::Print(DebugPrinter&) / Print(DebugLog&)  -- the DWARF-declared pair
    //       (BrnCameraValidityAccount.h:85/:88). Both walk the raised reason bits and print the
    //       name of each; the reason-NAME table is what is missing, and inventing 31 strings is
    //       exactly the kind of fabrication the faithfulness gate exists to stop.
    //
    // DELETE-WHEN: ActualDebugRender is bodied, and the ValidityAccount reason-name table is
    // recovered (the enumerators themselves already are: BrnCameraValidityAccount.h names all
    // 31, EFailedFlag / ENoCutToFlag / ENoCutFromFlag).
    // ------------------------------------------------------------------------
    void MomentSelector::ActualDebugRender(DebugPrinter& lrDebugPrinter) const
    {
        (void)lrDebugPrinter;
        CGS_ASSERT(false, "MomentSelector::ActualDebugRender is not reconstructed");
        __debugbreak();
    }

    // ------------------------------------------------------------------------
    // ⭐ AND THE ONE CROSS-SUBSYSTEM TRAP: MomentBystanderSeesAction::
    // SetPerceivedDistanceModificationFactor @0x822197E0.
    //
    // ⚠️ THIS ONE IS *NOT* DEV-ONLY -- read before deleting or before relying on it.
    // ArbStateCrashing::Update calls it on a live path: when the selected crash moment is a
    // BYSTANDER_SEES_ACTION shot that has held the crash for longer than kfMomentTime (2.0 s),
    // it squashes the shot's perceived distance to 0.5 so a long crash stays readable.
    //
    // ⭐ THE BODY IS WRITTEN AND COMMITTED, at its real home
    // (MomentController/Moments/BrnMomentBystanderSeesAction.cpp), together with the two
    // BehaviourBystanderCam members it needs (mfPerceivedDistanceModificationFactor @+0x358 and
    // the re-frame latch @+0xCF, both carved this wave). It is the MOUNT that is blocked, not
    // the code: mounting that TU opens NINE unresolved externals of its own -- seven
    // `detail::MomentSharedInfo_*` free-function shims that are declared and never defined
    // anywhere, plus BehaviourParameterBank::GetBystanderCam{,Close}MomentParams. Measured this
    // wave, on this link.
    //
    // ⛔ IT IS A TRAP RATHER THAN A NO-OP ON PURPOSE. A quiet body here would swallow a
    // camera-framing change on a live path -- the exact silent-drop shape this file's own
    // GROUP F banner documents. If it ever fires, the fix is the mount, not the stub.
    // DELETE-WHEN: the seven MomentSharedInfo shims are replaced by the real accessors and
    // BrnMomentBystanderSeesAction.cpp joins the exe source list.
    // ------------------------------------------------------------------------
    void MomentBystanderSeesAction::SetPerceivedDistanceModificationFactor(f32 lfFactor)
    {
        (void)lfFactor;
        CGS_ASSERT(false, "MomentBystanderSeesAction TU is not mounted");
        __debugbreak();
    }

    // ------------------------------------------------------------------------
    // MomentTumbling::SignalIsGoodTimeToPlant @0x8220A078 -- the same shape as the bystander
    // trap above, and for the same reason: the body is REAL and committed at its own home
    // (MomentController/Moments/BrnMomentTumbling.cpp -- "on a LEAD-subtype tumble, raise the
    // gyro rig's plant-request pair"), it is the MOUNT that is missing. ArbStateCrashing::Update
    // calls it on the first frame of a slow-motion burst when the selected crash moment is a
    // TUMBLING shot.
    //
    // ⭐ IT CANNOT FIRE ON THIS BUILD, and the reason is checkable rather than hopeful: the call
    // site is guarded by mMomentSelector.HasSelectedMoment(), and NOTHING can select a moment
    // while MomentController::NewMoment (the GROUP F stub above) allocates nothing and leaves
    // every MomentHandle !IsAllocated() -- SnoopNumValidMoments counts only allocated handles,
    // so muValidMoments is pinned at 0 and SelectBestMoment has no candidate. The same argument
    // covers the bystander trap above. When the moment sub-system comes up, BOTH become live on
    // the same day, which is exactly why they are traps and not no-ops.
    // DELETE-WHEN: BrnMomentTumbling.cpp joins the exe source list.
    // ------------------------------------------------------------------------
    void MomentTumbling::SignalIsGoodTimeToPlant()
    {
        CGS_ASSERT(false, "MomentTumbling TU is not mounted");
        __debugbreak();
    }
}

namespace BrnDirector
{
namespace Camera
{
    // See the GROUP G banner above -- both overloads are dev-only read-outs.
    void ValidityAccount::Print(DebugPrinter& lrDebugPrinter) const
    {
        (void)lrDebugPrinter;
        CGS_ASSERT(false, "ValidityAccount::Print(DebugPrinter&) is not reconstructed");
        __debugbreak();
    }

    void ValidityAccount::Print(DebugLog& lrDebugLog) const
    {
        (void)lrDebugLog;
        CGS_ASSERT(false, "ValidityAccount::Print(DebugLog&) is not reconstructed");
        __debugbreak();
    }
}
}
