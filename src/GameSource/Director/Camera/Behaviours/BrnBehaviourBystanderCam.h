#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_BYSTANDER_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_BYSTANDER_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert + SetTarget index assert)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCam.h
//
// BrnDirector::Camera::BehaviourBystanderCam -- the "bystander cam" camera behaviour (the
// dramatic third-party camera that frames a chosen race car from a roadside vantage; the
// "bystander sees action" moment + the testbed arbitrator state install it). HOME for the
// three BehaviourBystanderCam class slices this TU bodies:
//   - GetCol        @0x821F9B28  (return &mCollisionPolicy at +0xD0; one-liner addi)
//   - SetParameters @0x821F3F10  (adopt a bystander-cam param block; type tag == 5)
//   - SetTarget     @0x821F3F80  (point the camera at a race car index)
// The full behaviour (Construct/Prepare/Update/the virtual GetCollisionPolicy and the rest of
// the rig) and the Behaviour base land with their own TUs; this header models only the members
// these three functions touch, BY NAME, at their asm-attested offsets. Reserved byte spans
// place them exactly.
//
// Class layout cross-checked against the DecFIGS DWARF (BehaviourBystanderCam.h:107): the
// private members run mTransform, mPositionFinder, mShake, mLooker, mCollisionPolicy
// (VisibilityCollisionPolicy, the +0xD0 sub-object GetCol returns), mRandom, mTarget
// (Behaviour::VehicleRef, the block SetTarget writes), mpParameters, then the two distance
// scalars and the mbSetup/mbGotPosition flags.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. Each behaviour carries a type id in the
//   leading word of its Parameters block; SetParameters asserts the block's id is the
//   bystander-cam one. The console value for eBehaviourBystanderCam is 5 (the asm at 0x821F3F30
//   compares the block's first word against 5). Replace with the real EBehaviourType enum when
//   the Behaviour base TU lands; the enumerator's VALUE (5) is asm.
enum EBehaviourTypeBystanderCam
{
    eBehaviourBystanderCam = 5
};

// FLAG: the upper bound the SetTarget index assert enforces. The console value for
//   BrnPhysics::Vehicle::ku8MaxNumRaceCars is 8 (the asm at 0x821F3FA0 compares meRaceCarIndex
//   against 8). Replace with the real BrnPhysics::Vehicle constant when that TU lands; the
//   VALUE (8) is asm.
// Guarded: see the identical guard note in BrnBehaviourGyroCam.h / BrnBehaviourLooseAttachment.h --
// this same unnamed enum is independently (re)declared in each; the guard makes a second
// inclusion in one TU a no-op instead of a redefinition error.
#ifndef BRNDIRECTOR_CAMERA_KU_MAX_NUM_RACE_CARS_DEFINED
#define BRNDIRECTOR_CAMERA_KU_MAX_NUM_RACE_CARS_DEFINED
enum { KU_MAX_NUM_RACE_CARS = 8 };
#endif

class BehaviourBystanderCam
{
public:

    // The bystander-cam parameter block: a type tag in its leading word plus behaviour-specific
    // data. GetType returns the tag SetParameters asserts on.
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block's fields into the camera-tunings
        // serialiser S (TextFile{Read,Write}Serialiser); the per-instance body is a separate TU.
        // Declared so the serialiser's Serialise<Parameters> can drive it by name.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeBystanderCam GetType() const
        {
            return static_cast<EBehaviourTypeBystanderCam>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word (cached by SetParameters)
    };

    // FLAG: the +0xD0 sub-object GetCol exposes is mCollisionPolicy
    //   (BrnDirector::Camera::VisibilityCollisionPolicy, DWARF :162). Its concrete type lands
    //   with the full behaviour TU; modelled here as an opaque embedded sub-object so the
    //   accessor returns a typed pointer at the asm-attested offset.
    class CollisionPolicy;

    // Return the address of the embedded collision policy at +0xD0. @0x821F9B28 (a single
    // `addi r3, r3, 0xD0; blr`).
    CollisionPolicy* GetCol();

    // Adopt a bystander-cam parameter block: assert it carries the bystander-cam type tag, then
    // cache its first word at +0x10 and store the pointer at +0x350. @0x821F3F10.
    void SetParameters(const Parameters* lpParameters);

    // Point the bystander camera at a race car: record the target index, mark the target set /
    // setup active, assert the index is in range, and flag the camera for re-acquisition.
    // @0x821F3F80. meRaceCarIndex is an EActiveRaceCarIndex (modelled as s32 here).
    void SetTarget(s32 meRaceCarIndex);

    // GROWN for MomentBystanderSeesAction::Update @0x82266730 (the moment polls its
    // candidate through the Behaviour-base head bytes +0x09 / +0x0B).
    bool HasFailed() const        { return mbFailed; }
    bool CanSwitchToMeNow() const { return mbCanSwitchToMeNow; }

    // ⭐ ADDED 2026-08-29 (crash-camera wave). The perceived-distance squash and its change
    // latch, reached by MomentBystanderSeesAction::SetPerceivedDistanceModificationFactor
    // @0x822197E0 -- which only writes when the value actually differs, then raises the latch.
    f32  GetPerceivedDistanceModificationFactor() const { return mfPerceivedDistanceModificationFactor; }
    void SetPerceivedDistanceModificationFactor(f32 lfFactor)
    {
        mfPerceivedDistanceModificationFactor = lfFactor;   // stfs 0x358(this)
        mbPerceivedDistanceChanged            = true;       // stb  1, 0xCF(this)
    }

private:

    // FLAG: only the members these three functions touch are modelled at their asm-attested
    //   offsets; the rest of the bystander-cam rig lands with the full behaviour TU. Reserved
    //   byte spans place them exactly.
    void*             mpVTable;                       // +0x000  behaviour vtable (opaque base head)
    u8                maReserved004[0x09 - 0x04];     // +0x004 .. +0x008 (rig members not modelled here)
    bool              mbFailed;                       // +0x009  the Behaviour-base failed flag (MomentBystanderSeesAction polls it)
    u8                mReserved00A;                   // +0x00A
    bool              mbCanSwitchToMeNow;             // +0x00B  the Behaviour-base switch-to gate (DWARF base name)
    u8                maReserved00C[0x10 - 0x0C];     // +0x00C .. +0x00F
    s32               mParamWord1;                    // +0x010  cached lpParameters->miParamWord1
    u8                maReserved014[0xCF - 0x14];     // +0x014 .. +0x0CE (rig members not modelled here)
    // ⭐ CARVED 2026-08-29 (crash-camera wave). MomentBystanderSeesAction::
    // SetPerceivedDistanceModificationFactor @0x822197E0 raises this byte whenever it CHANGES
    // the factor below (`stb 1, 0xCF(behaviour)` guarded by the `fcmpu` inequality test), i.e.
    // it is the "the framing distance moved, re-solve the vantage" latch.
    // FLAG: the byte's ROLE name is inferred from that one writer; the WRITE and its guard are
    //   asm-attested. RENAME-WHEN: the full bystander rig TU lands with the DWARF member list.
    bool              mbPerceivedDistanceChanged;     // +0x0CF
    u8                maCollisionPolicy[0x340 - 0xD0]; // +0x0D0  mCollisionPolicy (opaque, &-of by GetCol)

    // --- mTarget (Behaviour::VehicleRef) sub-block SetTarget writes, +0x340 .. +0x34F ---
    s32               miTargetSet;                    // +0x340  target-set / setup flag (= 1)
    s32               meTargetRaceCarIndex;           // +0x344  the target race car index
    s32               miTargetField348;               // +0x348  cleared to 0 by SetTarget
    u8                mbTargetField34C;               // +0x34C  flag set (= 1) by SetTarget
    u8                maReserved34D[0x350 - 0x34D];   // +0x34D .. +0x34F (VehicleRef tail not modelled)

    const Parameters* mpParameters;                   // +0x350  the adopted parameter block
    u8                maReserved354[0x358 - 0x354];   // +0x354 (rig member not modelled here)
    // ⭐ CARVED 2026-08-29 (crash-camera wave): the framing squash the crash camera drives
    // through MomentBystanderSeesAction. The sibling reconstruction of this class
    // (Behaviours/BehaviourBystanderCam.h) already names the same +0x358 float
    // mfPerceivedDistanceModificationFactor and seeds it to 1.0 in Construct, so the two agree.
    f32               mfPerceivedDistanceModificationFactor;   // +0x358
    u8                mbField35C;                      // +0x35C  flag set (= 1) by SetTarget (re-acquire)
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourBystanderCam::GetCol @0x821F9B28
//   addi r3, r3, 0xD0        ; &this->mCollisionPolicy
//   blr
// ----------------------------------------------------------------------------
inline BehaviourBystanderCam::CollisionPolicy*
BehaviourBystanderCam::GetCol()
{
    return reinterpret_cast<CollisionPolicy*>(maCollisionPolicy);   // this + 0xD0
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourBystanderCam::SetParameters @0x821F3F10
//   lwz  r11, 0(r4)          ; lpParameters->meType
//   cmplwi r11, 5            ; == eBehaviourBystanderCam
//   ... assert on mismatch (BehaviourBystanderCam.h:215) ...
//   lwz  r11, 4(r4)          ; lpParameters->miParamWord1
//   stw  r4,  0x350(r3)      ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)       ; mParamWord1  = lpParameters->miParamWord1
// ----------------------------------------------------------------------------
inline void
BehaviourBystanderCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourBystanderCam,
               "lpParameters->GetType() == eBehaviourBystanderCam");
    mpParameters = lpParameters;                  // stw r4,  0x350(this)
    mParamWord1  = lpParameters->miParamWord1;     // lwz r11,4(lp); stw r11, 0x10(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourBystanderCam::SetTarget @0x821F3F80
//   stw  r4,  0x344(r3)      ; meTargetRaceCarIndex = meRaceCarIndex
//   stb  1,   0x34C(r3)      ; mbTargetField34C     = 1
//   stw  1,   0x340(r3)      ; miTargetSet          = 1
//   stw  0,   0x348(r3)      ; miTargetField348     = 0
//   cmpwi r4, 8 ; blt skip   ; assert meRaceCarIndex < ku8MaxNumRaceCars (BrnVehicleRef.h:222)
//   stb  1,   0x35C(r3)      ; mbField35C           = 1  (issued AFTER the assert)
// ----------------------------------------------------------------------------
inline void
BehaviourBystanderCam::SetTarget(s32 meRaceCarIndex)
{
    meTargetRaceCarIndex = meRaceCarIndex;        // stw r4,  0x344(this)
    mbTargetField34C     = 1;                     // stb r30(=1), 0x34C(this)
    miTargetSet          = 1;                     // stw r30(=1), 0x340(this)
    miTargetField348     = 0;                     // stw r11(=0), 0x348(this)
    CGS_ASSERT(meRaceCarIndex < KU_MAX_NUM_RACE_CARS,
               "meRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars");
    mbField35C           = 1;                     // stb r30(=1), 0x35C(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_BYSTANDER_CAM_H
