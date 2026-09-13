#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_TAKEDOWN_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_TAKEDOWN_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // BehaviourHandle<>, BehaviourManager
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.h"  // Camera::BehaviourInterpolate (+ ::Parameters, held by value)
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"     // Camera::BehaviourGyroCam
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h" // Camera::BehaviourLooseAttachment
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h" // Camera::BehaviourAftertouchCrash
#include "GameSource/Director/Camera/Behaviours/BehaviourBystanderCamImpactControllers.h"   // Camera::ImpactShakeController
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
// LAYOUT: member NAMES + declaration order come from the declaration reference
// (references/DecFIGS/dwarfdump/GameSource/Director/Arbitrator/States/BrnArbStateTakedown.h),
// gated on the console ledger's 27-function set for this TU (the ARTIST asm is the offset /
// behaviour authority; declaration reference supplies names/types/shape only for functions the console build
// attests). Every per-member offset quoted below is pinned store-for-store from
// ArbStateTakedown::Construct -- which seeds every sub-player, every behaviour handle and every
// parameter block -- and cross-checked against each player's Release and against
// ArbStateTakedown::Update's member reads. Parity on the x64 compile-gate host is BY NAMED
// MEMBER; the quoted offsets are provenance.
//
// Every Prepare / Update below is bodied in BrnArbStateTakedown.cpp, and every store those
// bodies make now goes through a declared setter on the behaviour that owns it.
// ----------------------------------------------------------------------------

namespace Attrib { namespace Gen { class iceanim; } }

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // BrnDirector::B3ClassicTakedownPlayer -- the "Burnout 3 classic" replay-style takedown: an
    // aftertouch-crash flyback blended into a gyro-cam-anchored gameplay-camera hand-off via an
    // interpolate behaviour. declaration reference home BrnArbStateTakedown.h. console +0x180 (0x58 bytes).
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

        // Construct (declaration reference BrnArbStateTakedown.cpp) -- the console INLINES it into
        // ArbStateTakedown::Construct (the this+0x180 store block); de-inlined back
        // into its own body in the .cpp.
        void Construct();

        // Prepare -- allocate the gyro rig on the victim's car (takedown parameter
        // block, record +2112) and the blend that carries the gameplay camera into it.
        bool Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // Update -- the four-beat flyback; the FLYBACK hand-off allocates the second
        // blend (gyro cam -> gameplay camera) and advances to INTERPOLATING_TO_GAMEPLAY.
        Camera::Camera Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // Release -- reset the state machine, then drop the three behaviour holds.
        void Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // HasFinished @0x821F6268: return meState == E_STATE_FINISHED.
        bool HasFinished() const override { return meState == E_STATE_FINISHED; }

    private:
        Camera::BehaviourHandle<Camera::BehaviourGyroCam>     mGyroCam;           // console +0x04 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolaterA;     // console +0x18 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolaterB;     // console +0x2C (0x14)
        Camera::BehaviourInterpolate::Parameters              mInterpolateParams; // console +0x40 (0x10)

        f32    mfActiveTime;   // X360 +0x50  (asm: *(a2+80))
        EState meState;        // X360 +0x54  (asm: *(a2+84))
    };

    // ------------------------------------------------------------------------
    // BrnDirector::DestructionPathTakedownPlayer -- the destruction-path takedown: two flyback
    // beats blended by an interpolate behaviour. DWARF home BrnArbStateTakedown.h:114.
    // console +0x1D8 (0x6C bytes).
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

        // Construct (declaration reference BrnArbStateTakedown.cpp) -- inlined by the console into
        // ArbStateTakedown::Construct (the this+0x1D8 store block); de-inlined in the .cpp.
        void Construct();

        // Prepare -- two gyro rigs (default block on the player's car, takedown block
        // on the victim's), both seeded from the player tracker's implicit velocity, plus the
        // blend from a synthesised look-at camera into the first rig.
        bool Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // Update -- two flyback beats; the FLYBACK1 hand-off allocates the rig-to-rig
        // blend and advances to FLYBACK2.
        Camera::Camera Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // Release -- reset the state machine, then drop the four behaviour holds.
        void Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // HasFinished @0x821F6280: return meState == E_STATE_FINISHED.
        bool HasFinished() const override { return meState == E_STATE_FINISHED; }

    private:
        Camera::BehaviourHandle<Camera::BehaviourGyroCam>     mGyroCamA;          // console +0x04 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourGyroCam>     mGyroCamB;          // console +0x18 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolaterA;     // console +0x2C (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolaterB;     // console +0x40 (0x14)
        Camera::BehaviourInterpolate::Parameters              mInterpolateParams; // console +0x54 (0x10)

        f32    mfActiveTime;   // X360 +0x64  (asm: *(a2+100))
        EState meState;        // X360 +0x68  (asm: *(a2+104))
    };

    // ------------------------------------------------------------------------
    // BrnDirector::DriveByTakedownPlayer -- the drive-by takedown: a gyro cam on either the
    // shooter's or the victim's car, selected by which car's produced-camera dirty-behaviour
    // flag is set. declaration reference home BrnArbStateTakedown.h. console +0x398 (0x34 bytes).
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

        // Construct (declaration reference BrnArbStateTakedown.cpp) -- inlined by the console into
        // ArbStateTakedown::Construct (the this+0x398 store block); de-inlined in the .cpp.
        void Construct();

        // Prepare -- allocate BOTH gyro rigs on the victim's car; both adopt the same
        // drive-by parameter block (record +2928) and neither is given a from-car vector seed.
        bool Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // Update @0x8225A000 -- tractable: picks between the two gyro-cam handles' produced
        // cameras by their dirty-behaviour flag, no interpolate-setup / VMX dependency.
        Camera::Camera Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // Release -- reset the state machine, then drop the two gyro-cam holds.
        void Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // HasFinished @0x821F6298: return meState == E_STATE_FINISHED.
        bool HasFinished() const override { return meState == E_STATE_FINISHED; }

    private:
        Camera::BehaviourHandle<Camera::BehaviourGyroCam> mGyroCamDriveByL;  // console +0x04 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourGyroCam> mGyroCamDriveByR;  // console +0x18 (0x14)

        f32    mfActiveTime;   // X360 +0x2C  (asm: *(a2+44))
        EState meState;        // X360 +0x30  (asm: *(a2+48))
    };

    // ------------------------------------------------------------------------
    // BrnDirector::ShutdownTakedownPlayer -- the "shutdown"/impact takedown: the most elaborate
    // player, a 9-state sequence blending a gyro cam into an interpolate hand-off, then three
    // sequential loose-attachment "zoom" beats each registering a camera-impact effect on the
    // wrecked car. declaration reference home BrnArbStateTakedown.h. console +0x268 (0x130 bytes).
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

        // Construct -- the one sub-player Construct the console does NOT inline
        // (ArbStateTakedown::Construct calls it out of line).
        void Construct();

        // Prepare -- allocate the loose-attachment rig (hung off the player's car,
        // aimed at the victim's, adopting this player's OWN parameter block) plus the lookback
        // blend into it.
        bool Prepare(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // Update -- the nine-state sequence. LOOKBACK seeds the gyro rig's from-car
        // vector from the live cars' world positions and sets up the flyback blend; the three
        // zoom beats then run in sequence, each its own loose-attachment behaviour.
        Camera::Camera Update(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // Release -- reset the state machine, then drop all seven behaviour holds.
        void Release(const ArbitratorState* lpCallingState, ArbStateSharedInfo& lrSharedInfo) override;

        // HasFinished @0x821F62C8: return meState == E_STATE_FINISHED.
        bool HasFinished() const override { return meState == E_STATE_FINISHED; }

    private:
        Camera::BehaviourHandle<Camera::BehaviourGyroCam>       mGyroCam;         // console +0x04 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate>   mInterpolaterA;   // console +0x18 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate>   mInterpolaterB;   // console +0x2C (0x14)

        // ---- the loose-attachment hold plus three sequential "zoom" beats ----
        Camera::BehaviourHandle<Camera::BehaviourLooseAttachment> mLooseAttachment; // X360 +0x40 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourLooseAttachment> mZoom1;           // X360 +0x54 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourLooseAttachment> mZoom2;           // X360 +0x68 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourLooseAttachment> mZoom3;           // X360 +0x7C (0x14)

        Camera::BehaviourInterpolate::Parameters     mInterpolateParamsA; // console +0x90 (0x10)
        Camera::BehaviourInterpolate::Parameters     mInterpolateParamsB; // console +0xA0 (0x10)
        Camera::BehaviourInterpolate::Parameters     mInterpolateParamsC; // console +0xB0 (0x10)
        Camera::BehaviourLooseAttachment::Parameters mLooseAttachmentParameters; // console +0xC0 (0x64)

        f32    mfActiveTime;          // console +0x124  (asm: *(a2+292)) -- NOT seeded by Construct
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
        // declaration reference ETakedownType (BrnArbStateTakedown.h). Construct seeds meTakedownType with
        // the console's literal 1, i.e. the past-the-end E_NUM_TYPES sentinel ("no style picked
        // yet"); PickNewTakedownType is what selects a real style.
        enum ETakedownType
        {
            E_TYPE_B3CLASSIC = 0,

            E_NUM_TYPES      = 1
        };

        // DWARF EState (BrnArbStateTakedown.h:364). Update's dispatch switch is indexed by this
        // value (0..5, case 4 falls through to case 3's hand-off tail).
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
        // Construct.
        void        Construct() override;

        // Prepare -- pick the owning player off the game state on the first call,
        // then bring up this state's own three behaviours, the moment selector, and the player.
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;

        // Update -- the state's own six-state machine, the moment-selector cutting
        // policy for road rage, and the exit edge back to roaming.
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;

        bool        Release(ArbStateSharedInfo& lrSharedInfo) override; // @0x822353B8
        const char* GetName() const override;                          // @0x821F62E0

        // Destruct() is NOT in this TU's recovered function set -- no symbol, and nothing
        // references it -- so the base declaration is kept and no override is declared here
        // (the same call the eight sibling states made).

    private:
        // FLAG: no asm body recovered for PickNewTakedownType in this TU's ledger set, and no
        // recovered code calls it -- Prepare picks the player straight off the game state.
        // Declaration-only (declaration reference home BrnArbStateTakedown.cpp).
        void PickNewTakedownType(ArbStateSharedInfo& lrSharedInfo);

        // ---- members, DWARF order; X360 (4-byte-pointer) offsets in comments --------------
        // (the base's own members end at +0x172; the sub-player run starts at +0x180.)
        B3ClassicTakedownPlayer          mClassicTakedown;          // console +0x180  (0x58)
        DestructionPathTakedownPlayer    mDestructionPathTakedown;  // console +0x1D8  (0x6C)
        SimpleIceTakedownPlayer          mSimpleIceTakedown;        // X360 +0x244  (0x24)
        ShutdownTakedownPlayer           mShutdownTakedown;         // X360 +0x268  (0x130)
        DriveByTakedownPlayer            mDriveByTakedown;          // X360 +0x398  (0x34)

        Camera::ImpactShakeController                             mImpactShakeController; // console +0x3CC (five f32)
        Camera::BehaviourHandle<Camera::BehaviourAftertouchCrash>  mTakedownDebugCam;     // console +0x3E0 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourGyroCam>          mGyroCam;              // console +0x3F4 (0x14)
        Camera::BehaviourHandle<Camera::BehaviourInterpolate>      mInterpolator;         // console +0x408 (0x14)
        Camera::BehaviourInterpolate::Parameters                   mInterpolatorParams;   // console +0x41C (0x10)

        MomentSelector mMomentSelector;   // console +0x42C (0x1E4)

        TakedownPlayer*  mpCurrentTakedown;    // console +0x610 (asm: *(a1+1552))
        ETakedownType    meTakedownType;       // console +0x614
        s32              miIceMovieIndex;      // console +0x618 (Construct seeds -1)
        s32              miRoadRageRDCutCount; // console +0x61C (asm: *(a1+1564); Update compares it
                                               //              against KI_MAX_ROADRAGE_CUTS == 2)

        f32    mfFailsafeTimer;   // console +0x620 (asm: *(a1+1568))
        f32    mfActiveTime;      // console +0x624 (asm: *(a1+1572))
        EState meState;           // console +0x628 (asm: *(a1+1576))

        bool   mbAlwaysUseShutdownCam;   // console +0x62C (Construct clears)
        bool   mbUseTakedownDebugCam;    // console +0x62D (Construct clears; Update's debug-cam arm
                                         //              gates on it AND mTakedownDebugCam)
        bool   mbHasTriggeredFlash;      // console +0x62E (one-shot "Car_Reset" effect gate in
                                         //              Update; NOT seeded by Construct)

        static const s32 KI_MAX_ROADRAGE_CUTS;   // BrnArbStateTakedown.cpp (declaration reference value 2)
        static const f32 KF_TRANSITION_TIME;     // BrnArbStateTakedown.cpp:26
        static const f32 KF_MIN_FAILSAFE_TIME;   // BrnArbStateTakedown.cpp:27
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_TAKEDOWN_H
