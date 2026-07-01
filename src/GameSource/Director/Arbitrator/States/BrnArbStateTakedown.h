#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_TAKEDOWN_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_TAKEDOWN_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // BehaviourHandle<>, BehaviourManager, Camera::BehaviourInterpolate
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"     // Camera::BehaviourGyroCam
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h" // Camera::BehaviourLooseAttachment
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h" // Camera::BehaviourAftertouchCrash
#include "GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.h"   // Camera::ImpactShakeController
#include "GameSource/Director/MomentController/BrnMomentSelector.h"      // BrnDirector::MomentSelector
#include "GameSource/Director/Arbitrator/States/BrnSimpleIceTakedownPlayer.h" // TakedownPlayer, SimpleIceTakedownPlayer

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateTakedown.h
//
// BrnDirector::ArbStateTakedown -- the director arbitrator state that plays the takedown
// camera sequence when a race car is taken down. It owns one "takedown player" per takedown
// style (BrnDirector::TakedownPlayer subclasses -- see BrnSimpleIceTakedownPlayer.h for the
// shared interface -- the B3-classic replay-style takedown, the destruction-path takedown,
// the drive-by takedown, the shutdown/impact takedown, and the simple ICE-anim takedown) plus
// the shared impact-shake controller, the debug/gyro/interpolate camera behaviours the base
// takedown flow drives, and a MomentSelector used to pick an establishing "moment" shot while
// the takedown plays out.
//
// LAYOUT: member NAMES + DWARF declaration order come from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Director/Arbitrator/States/BrnArbStateTakedown.h),
// gated on the X360 ledger's 18-function set for this TU (ARTIST asm is the offset/behaviour
// authority; DWARF supplies names/types/shape only for functions the X360 build attests).
// Per-member X360 (4-byte-pointer) offsets are pinned from the ARTIST asm (Construct
// @0x8225A318, Prepare @0x8226D828, Update @0x8225A698, Release @0x822353B8) and quoted in
// comments as provenance; parity on the x64 compile-gate host is BY NAMED MEMBER (the
// project's x64-gate rule), not by byte offset.
//
// TRACTABILITY: this TU's 18 functions split into two tiers.
//   * BODIED here (BrnArbStateTakedown.cpp): the trivial one-liners (GetName, every player's
//     HasFinished(), inline in this header), ArbStateTakedown::Release, DriveByTakedownPlayer::
//     Update, and SimpleIceTakedownPlayer::Prepare/Update/Release -- the functions whose
//     per-frame work resolves entirely through NAMED accessors already homed elsewhere
//     (BehaviourHandle<T>::GetProducedCamera()/GetBehaviour()/IsBehaviourReadyToUse(),
//     BehaviourIceAnim::HasFinishedOrFailed(), Camera::RequestStartEffectHook(),
//     BehaviourManager::CheckNoBehavioursAreAllocatedByState()/UnSetBehaviourUsedByHandle()).
//   * DECLARATION-ONLY (honest FLAG comment at each): everything else -- ArbStateTakedown::
//     Construct/Prepare/Update/Destruct/PickNewTakedownType; B3ClassicTakedownPlayer's full
//     Construct/Prepare/Release/Update; DestructionPathTakedownPlayer's full Construct/Prepare/
//     Release/Update; DriveByTakedownPlayer's Construct/Prepare/Release (Update IS bodied);
//     ShutdownTakedownPlayer's full Construct/Prepare/Release/Update. Each keeps its X360
//     vtable slot with an inline comment naming exactly what blocks it; the recurring blockers
//     are:
//       (a) a VMX "world-space normalized vector from car" computation that reads the live
//           race car's world position out of AllVehicleData::GetRaceCar()'s return -- that
//           accessor is intentionally typed `const void*` (its own TU is not reconstructed
//           yet), so the position read would have to be a raw-offset poke into a real,
//           not-yet-typed C++ object -- exactly what the project rules forbid (this is NOT
//           the "external serialised blob" exception: it is a live game-object record with a
//           pending reconstructed home);
//       (b) the BehaviourInterpolate camera-blend setup helper (X360 sub_8224EE58) whose
//           parameter mapping (duration vs. the two camera-source handles) is not confidently
//           recoverable from the ARTIST asm alone without guessing an argument order -- every
//           B3Classic/DestructionPath/Shutdown Update case that hands off into an interpolate
//           blend hits this, even though their steady-state per-frame math is otherwise
//           tractable;
//       (c) (ArbStateTakedown::Update only) MomentSelector-driven moment selection combined
//           with an ImpactShakeController::Update call whose full argument list the pseudocode
//           collapses, plus a raw read into the opaque per-vehicle team/race-car snapshot; and
//       (d) (Construct-family only) MomentSelector::AddMoment call sites whose argument-register
//           pattern packs a second integer word alongside the type tag that does not match
//           AddMoment's currently-declared (Moment::EType, f32) signature (see the Construct
//           comment below) -- or (ShutdownTakedownPlayer::Construct) a
//           BehaviourLooseAttachment::Parameters block whose ~140-byte tail is not yet named
//           past its {meType, miParamWord1} head.
//     This is the same honest-declaration convention BrnArbStatePostEvent.h's PickAppropriateShot
//     uses for an unrecovered VMX pipeline.
// ----------------------------------------------------------------------------

namespace Attrib { namespace Gen { class iceanim; } }

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // BrnDirector::B3ClassicTakedownPlayer -- the "Burnout 3 classic" replay-style takedown: an
    // aftertouch-crash flyback blended into a gyro-cam-anchored gameplay-camera hand-off via an
    // interpolate behaviour. DWARF home BrnArbStateTakedown.h:65.
    // ------------------------------------------------------------------------
    class B3ClassicTakedownPlayer : public TakedownPlayer
    {
    public:
        // DWARF EState (BrnArbStateTakedown.h:96). Construct seeds INACTIVE; Update's dispatch
        // switch is indexed by this value (0..4); HasFinished() is `meState == E_STATE_FINISHED`.
        enum EState
        {
            E_STATE_INACTIVE                    = 0,
            E_STATE_PREPARING                   = 1,
            E_STATE_FLYBACK                     = 2,
            E_STATE_INTERPOLATING_TO_GAMEPLAY   = 3,
            E_STATE_FINISHED                    = 4,

            E_NUM_STATES                        = 5
        };

        // Construct/Prepare/Release are NOT in this TU's recovered 18-function set (no asm body
        // available for this class). DECLARATION-ONLY: the overrides keep the X360 vtable slots.
        void Construct();                                                                          // @0x???????? (no asm recovered)
        bool Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;  // @0x???????? (no asm recovered)

        // Update @0x822656E0 -- DECLARATION-ONLY. The FLYBACK state's hand-off branch (when the
        // gyro-cam behaviour reports finished AND mfActiveTime has run past its threshold) runs
        // the same unrecovered BehaviourInterpolate camera-blend setup helper as file banner
        // FLAG (b) (X360 sub_8224EE58, allocating+configuring mInterpolaterB), so this function
        // shares that blocker even though its steady-state per-frame math (the sin-based
        // motion-blur amounts, the "Takedown" start-hook request) IS tractable through
        // Camera::RequestMotionBlur / Camera::RequestStartEffectHook. Left declaration-only
        // rather than body only the steady-state branch and drop the state-advance side effect.
        Camera::Camera Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        void Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;  // @0x???????? (no asm recovered)

        // HasFinished @0x821F6268: return meState == E_STATE_FINISHED.
        bool HasFinished() const override { return meState == E_STATE_FINISHED; }

    private:
        Camera::BehaviourHandle<Camera::BehaviourGyroCam>     mGyroCam;         // X360 +0x00 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolaterA;   // X360 +0x14 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolaterB;   // X360 +0x28 (0x14)
        Camera::BehaviourInterpolate::Parameters               mInterpolateParams; // X360 +0x3C

        f32    mfActiveTime;   // X360 +0x50  (asm: *(a2+80))
        EState meState;        // X360 +0x54  (asm: *(a2+84))
    };

    // ------------------------------------------------------------------------
    // BrnDirector::DestructionPathTakedownPlayer -- the destruction-path takedown: two flyback
    // beats blended by an interpolate behaviour. DWARF home BrnArbStateTakedown.h:114.
    // ------------------------------------------------------------------------
    class DestructionPathTakedownPlayer : public TakedownPlayer
    {
    public:
        // DWARF EState (BrnArbStateTakedown.h:146).
        enum EState
        {
            E_STATE_INACTIVE  = 0,
            E_STATE_PREPARING = 1,
            E_STATE_FLYBACK1  = 2,
            E_STATE_FLYBACK2  = 3,
            E_STATE_FINISHED  = 4,

            E_NUM_STATES      = 5
        };

        void Construct();                                                                          // @0x???????? (no asm recovered)
        bool Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;  // @0x???????? (no asm recovered)

        // Update @0x82265A58 -- DECLARATION-ONLY. Same FLAG (b) blocker as
        // B3ClassicTakedownPlayer::Update: the FLYBACK1 hand-off branch runs the unrecovered
        // BehaviourInterpolate camera-blend setup helper (sub_8224EE58).
        Camera::Camera Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        void Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;  // @0x???????? (no asm recovered)

        // HasFinished @0x821F6280: return meState == E_STATE_FINISHED.
        bool HasFinished() const override { return meState == E_STATE_FINISHED; }

    private:
        Camera::BehaviourHandle<Camera::BehaviourGyroCam>     mGyroCamA;       // X360 +0x00 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourGyroCam>     mGyroCamB;       // X360 +0x14 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolaterA;  // X360 +0x28 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolaterB;  // X360 +0x3C (0x14)
        Camera::BehaviourInterpolate::Parameters               mInterpolateParams; // X360 +0x50

        f32    mfActiveTime;   // X360 +0x64  (asm: *(a2+100))
        EState meState;        // X360 +0x68  (asm: *(a2+104))
    };

    // ------------------------------------------------------------------------
    // BrnDirector::DriveByTakedownPlayer -- the drive-by takedown: a gyro cam on either the
    // shooter's or the victim's car, selected by which car's produced-camera dirty-behaviour
    // flag is set. DWARF home BrnArbStateTakedown.h:164.
    // ------------------------------------------------------------------------
    class DriveByTakedownPlayer : public TakedownPlayer
    {
    public:
        // DWARF EState (BrnArbStateTakedown.h:193).
        enum EState
        {
            E_STATE_INACTIVE  = 0,
            E_STATE_PREPARING = 1,
            E_STATE_DRIVEBY   = 2,
            E_STATE_FINISHED  = 3,

            E_NUM_STATES      = 4
        };

        void Construct();                                                                          // @0x???????? (no asm recovered)
        bool Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;  // @0x???????? (no asm recovered)

        // Update @0x8225A000 -- tractable: picks between the two gyro-cam handles' produced
        // cameras by their dirty-behaviour flag, no interpolate-setup / VMX dependency.
        Camera::Camera Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        void Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;  // @0x???????? (no asm recovered)

        // HasFinished @0x821F6298: return meState == E_STATE_FINISHED.
        bool HasFinished() const override { return meState == E_STATE_FINISHED; }

    private:
        Camera::BehaviourHandle<Camera::BehaviourGyroCam> mGyroCamDriveByL;  // X360 +0x00 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourGyroCam> mGyroCamDriveByR;  // X360 +0x14 (0x14)

        f32    mfActiveTime;   // X360 +0x2C  (asm: *(a2+44))
        EState meState;        // X360 +0x30  (asm: *(a2+48))
    };

    // ------------------------------------------------------------------------
    // BrnDirector::ShutdownTakedownPlayer -- the "shutdown"/impact takedown: the most elaborate
    // player, a 9-state sequence blending a gyro cam into an interpolate hand-off, then three
    // sequential loose-attachment "zoom" beats each registering a camera-impact effect on the
    // wrecked car. DWARF home BrnArbStateTakedown.h:260.
    // ------------------------------------------------------------------------
    class ShutdownTakedownPlayer : public TakedownPlayer
    {
    public:
        // DWARF EState (BrnArbStateTakedown.h:300). State 7 (RELEASING) has no asm case in the
        // recovered Update dispatch (the jump table's case 7 falls to the default/assert case);
        // state 8 (FINISHED) is the terminal hold.
        enum EState
        {
            E_STATE_INACTIVE  = 0,
            E_STATE_PREPARING = 1,
            E_STATE_LOOKBACK  = 2,
            E_STATE_FLYBACK   = 3,
            E_STATE_ZOOM1     = 4,
            E_STATE_ZOOM2     = 5,
            E_STATE_ZOOM3     = 6,
            E_STATE_RELEASING = 7,
            E_STATE_FINISHED  = 8,

            E_NUM_STATES      = 9
        };

        // Construct @0x82208D80 -- DECLARATION-ONLY. The recovered body's tail calls
        // BrnDirector::Camera::BehaviourLooseAttachment::Parameters::Construct(this+0xC0) and
        // then stores several more field/constant writes into that Parameters block (offsets up
        // to ~+140 past its head, including three float magic constants). That Parameters type
        // (BrnBehaviourLooseAttachment.h) currently only models the {meType, miParamWord1} head
        // this TU's OTHER functions touch (SetParameters/AttachTo/SetTarget/Get) -- growing it to
        // the full ~140-byte block with named fields for every remaining zeroed/constant slot is
        // that type's own TU's work, not something to fabricate here as anonymous padding dressed
        // up as a body. Left declaration-only; the vtable slot is kept.
        void Construct();

        bool Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;  // @0x???????? (no asm recovered)

        // Update @0x8226D0B0 -- DECLARATION-ONLY. See the file banner FLAGs (a)/(b): state
        // LOOKBACK seeds the gyro-cam's world-space normalized from-car vector by reading the
        // live race car's world position out of AllVehicleData::GetRaceCar()'s intentionally-
        // opaque `const void*` return, and also runs the BehaviourInterpolate camera-blend setup
        // helper. Both feed every later state (FLYBACK..FINISHED), so the function is left
        // declaration-only rather than bodying only its zoom-beat tail (states FLYBACK..FINISHED,
        // which BY THEMSELVES are tractable through BehaviourLooseAttachment /
        // CameraImpactEffect::RegisterImpact) and silently dropping the vector-seed/interpolate-
        // blend side effects the earlier state depends on.
        Camera::Camera Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        void Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;  // @0x???????? (no asm recovered)

        // HasFinished @0x821F62C8: return meState == E_STATE_FINISHED.
        bool HasFinished() const override { return meState == E_STATE_FINISHED; }

    private:
        Camera::BehaviourHandle<Camera::BehaviourGyroCam>       mGyroCam;         // X360 +0x00 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate>   mInterpolaterA;   // X360 +0x14 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate>   mInterpolaterB;   // X360 +0x28 (0x14)

        // ---- three sequential "zoom" loose-attachment behaviours (X360 +0x40/+0x54/+0x68) ----
        Camera::BehaviourHandle<Camera::BehaviourLooseAttachment> mLooseAttachment; // X360 +0x40 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourLooseAttachment> mZoom1;           // X360 +0x54 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourLooseAttachment> mZoom2;           // X360 +0x68 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourLooseAttachment> mZoom3;           // X360 +0x7C (0x14)

        Camera::BehaviourInterpolate::Parameters               mInterpolateParamsA; // X360 +0x90
        Camera::BehaviourInterpolate::Parameters               mInterpolateParamsB; // X360 +0x94
        Camera::BehaviourInterpolate::Parameters               mInterpolateParamsC; // X360 +0x98
        Camera::BehaviourLooseAttachment::Parameters           mLooseAttachmentParameters; // X360 +0xC0

        f32    mfActiveTime;          // X360 +0x124  (asm: *(a2+292))
        EState meState;               // X360 +0x128  (asm: *(a2+296))
        bool   mbUsedFinalShotImpact; // X360 +0x12C  (asm: *(a2+300), cleared in Update's LOOKBACK case)
    };

    // ------------------------------------------------------------------------
    // BrnDirector::ArbStateTakedown -- the director arbitrator state itself. DWARF home
    // BrnArbStateTakedown.h:323.
    // ------------------------------------------------------------------------
    class ArbStateTakedown : public ArbitratorState
    {
    public:
        // DWARF ETakedownType (BrnArbStateTakedown.h:353). The X360 build only ever selects the
        // B3-classic style (PickNewTakedownType is not in this TU's recovered function set).
        enum ETakedownType
        {
            E_TYPE_B3CLASSIC = 0,

            E_NUM_TYPES      = 1
        };

        // DWARF EState (BrnArbStateTakedown.h:364). Update's dispatch switch is indexed by this
        // value (0..5, case 4 falls through to case 3's blocked-hand-off tail).
        enum EState
        {
            E_STATE_INACTIVE                     = 0,
            E_STATE_PREPARING                    = 1,
            E_STATE_TAKEDOWN_PLAYING             = 2,
            E_STATE_ROAD_RAGE_TAKEDOWN_PLAYING   = 3,
            E_STATE_CHANGING_TO_ROAMING          = 4,
            E_STATE_RELEASING                    = 5,

            E_NUM_STATES                         = 6
        };

        // ---- ArbitratorState virtual overrides (X360 vtable order; see base) -------------
        // Construct @0x8225A318 -- DECLARATION-ONLY. Most of the body (zero every sub-player's
        // state, seed the interpolate-parameters blocks) is mechanical, but its tail calls
        // MomentSelector::AddMoment three times with an argument-register pattern that does NOT
        // match that method's currently-declared (Moment::EType, f32) signature: the asm packs a
        // SECOND integer word (5, then 9) alongside the type tag into the same 64-bit GPR the
        // type occupies (e.g. `stw r27(=2), var_50; stw r11(=5), var_50+4; ld r4, var_50`), on
        // top of a separately-packed float register for the weight. Reconciling this would mean
        // either fabricating a 3-argument AddMoment overload or guessing which of {5, 9} is real
        // vs. incidental -- both cross into BrnMomentSelector's own (already-`done`) TU rather
        // than this one. Left declaration-only rather than guess the call.
        void        Construct() override;

        // Prepare @0x8226D828 -- DECLARATION-ONLY. See the file banner FLAGs (a)/(b): this
        // function seeds the debug-cam/gyro-cam's world-space normalized from-car vector from
        // the live race car's opaque position record and runs the interpolate camera-blend
        // setup helper, neither of which is recoverable by named member without fabricating a
        // layout/argument order.
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;

        // Update @0x8225A698 -- DECLARATION-ONLY. See the file banner FLAG (c): the per-frame
        // moment-selection + ImpactShakeController::Update call chain and a raw read into the
        // opaque per-vehicle snapshot are not recoverable by named member without fabrication.
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;

        bool        Release(ArbStateSharedInfo& lrSharedInfo) override; // @0x822353B8
        const char* GetName() const override;                          // @0x821F62E0

        // Destruct() IS in the base vtable but NO asm body was recovered for it in this TU's
        // 18-function ledger set. DECLARATION-ONLY: the override keeps the X360 vtable slot; the
        // body is left to the base (the per-TU cl /c gate does not link it). FLAG: body not
        // recovered.
        void        Destruct() override;

    private:
        // PickNewTakedownType @0x???????? -- choose which takedown style to play next. NOT in
        // this TU's recovered 18-function set (no asm body available). DECLARATION-ONLY.
        void PickNewTakedownType(ArbStateSharedInfo& lrSharedInfo);

        // ---- members, DWARF order; X360 (4-byte-pointer) offsets in comments --------------
        B3ClassicTakedownPlayer          mClassicTakedown;          // X360 +0x10   (0x58)
        DestructionPathTakedownPlayer    mDestructionPathTakedown;  // X360 +0x180  (0x6C)
        SimpleIceTakedownPlayer          mSimpleIceTakedown;        // X360 +0x244  (0x24)
        ShutdownTakedownPlayer           mShutdownTakedown;         // X360 +0x268  (0x130)
        DriveByTakedownPlayer            mDriveByTakedown;          // X360 +0x398  (0x34)

        Camera::ImpactShakeController                              mImpactShakeController; // X360 +0x3CC (+972 dec)
        Camera::BehaviourHandle<Camera::BehaviourAftertouchCrash>  mTakedownDebugCam;      // X360 +0x3E0 (+992 dec)
        Camera::BehaviourHandle<Camera::BehaviourGyroCam>          mGyroCam;               // X360 +0x3F4 (+1012 dec)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate>      mInterpolator;          // X360 +0x408 (+1032 dec)
        Camera::BehaviourInterpolate::Parameters                   mInterpolatorParams;    // X360 +0x41C (+1052 dec)

        MomentSelector mMomentSelector;   // X360 +0x42C (+1068 dec)

        TakedownPlayer* mpCurrentTakedown;    // X360 +0x610 (+1552 dec)
        ETakedownType    meTakedownType;      // X360 +0x618 (+1560 dec)
        s32              miIceMovieIndex;     // X360 +0x61C (+1564 dec)
        s32              miRoadRageRDCutCount;// X360 +0x??? -- FLAG: X360 store target not confidently pinned; see Construct/Release .cpp notes

        f32    mfFailsafeTimer;   // X360 +0x620 (+1568 dec)
        f32    mfActiveTime;      // X360 +0x624 (+1572 dec)
        EState meState;           // X360 +0x628 (+1576 dec)

        bool   mbAlwaysUseShutdownCam;   // X360 +0x62C (+1580 dec)
        bool   mbUseTakedownDebugCam;    // X360 +0x60C (+1548 dec)
        bool   mbHasTriggeredFlash;      // X360 +0x62D (+1581 dec)
        bool   mbUsingMomentSelector;    // X360 +0x60D (+1549 dec)  MomentSelector::Update gate (Update; DECLARATION-ONLY there)
        bool   mbPlayedRoadRageEffect;   // X360 +0x62E (+1582 dec)  road-rage "Car_Reset" one-shot gate (Update; DECLARATION-ONLY there)

        static const s32 KI_MAX_ROADRAGE_CUTS;   // BrnArbStateTakedown.cpp:28
        static const f32 KF_TRANSITION_TIME;     // BrnArbStateTakedown.cpp:26
        static const f32 KF_MIN_FAILSAFE_TIME;   // BrnArbStateTakedown.cpp:27
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_TAKEDOWN_H
