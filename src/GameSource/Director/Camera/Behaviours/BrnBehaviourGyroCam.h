#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GYRO_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GYRO_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert + the rig asserts)
#include "rw/math/vpu/types.h"                        // rw::math::vpu::Vector3 (SetWorldSpaceNormalizedVectorFromCar)
#include "rw/math/vpu/vector3_operation.h"            // MagnitudeSquared (the IsSimilar magnitude assert)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h
//
// BrnDirector::Camera::BehaviourGyroCam -- the "gyro" follow-camera behaviour: it holds the
// camera a fixed height/distance/pitch off a tracked car along a world-space vector from the
// car, smoothing the rig with a PositionLag and a CameraShake and optionally an attachment
// "truck" that eases the offset distance in. HOME for the BehaviourGyroCam slices this TU bodies:
//   - SetParameters                       @0x821F4068  (adopt a gyro-cam param block; type tag == 9)
//   - AttachToRaceCar                     @0x821F40D8  (bind the rig to a race car; VehicleRef @+0x510)
//   - GetCollisionPolicy                  @0x821FA1C0  (return one of two collision policies, +0x20/+0x260)
//   - SetWorldSpaceNormalizedVectorFromCar@0x822067D0  (seed the from-car vector members @+0x530/+0x540)
// The full behaviour (Construct/Prepare/Update and the rest of the rig) and its Behaviour base land
// with their own TUs; this header models only the members these functions touch, BY NAME, at their
// asm-attested offsets. Reserved byte spans place each field exactly.
//
// Class layout cross-checked against the DecFIGS DWARF (BrnBehaviourGyroCam.h:108): the private
// members run mpParameters, mVisibilityCollisionPolicy (the +0x20 collision policy GetCollisionPolicy
// returns when NOT using vehicle-attachment collision), mVehicleAttachmentCollisionPolicy (the +0x260
// policy returned when it IS), mTransform, mLooker, mAttachedTo (Behaviour::VehicleRef, the +0x510
// block AttachToRaceCar writes), mCurrentTargetPos, mWorldSpaceNormalizedVectorFromCar (+0x530),
// mDesiredWorldSpaceNormalizedVectorFromCar (+0x540), mOriginalPoint, mPositionLag, mRandom, mShake,
// mAttachmentTruck, the height/distance/pitch/blend scalars, and the mbIsPlanted/.../
// mbIsWorldSpaceVectorSet (+0x632)/mbUseVehicleAttachmentCollision (+0x633) flags.
//
// SIZE-STABLE PIN: the X360 is a 4-byte-pointer build; this PC reconstruction is 64-bit, so a real
// 8-byte pointer mid-struct would shift every later offset. The vtable + the adopted-parameter
// pointer are therefore the size-stable raw 32-bit slots the X360 stores via `stw`; the typed
// parameter pointer (mpParameters) is appended at the tail and reached by name, keeping by-name
// access type-correct while reproducing every pinned offset exactly.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. The behaviours each carry a type id in
//   the leading word of their Parameters block; SetParameters asserts the block's id is the
//   gyro-cam one. The console value for eBehaviourGyroCam is 9 (the asm at 0x821F4088 compares
//   the block's first word against 9). Replace with the real EBehaviourType enum when the
//   Behaviour base TU lands; the gyro-cam enumerator's VALUE (9) is pinned from the asm.
enum EBehaviourTypeGyroCam
{
    eBehaviourGyroCam = 9
};

// FLAG: the upper bound the AttachToRaceCar index assert enforces. The console value for
//   BrnPhysics::Vehicle::ku8MaxNumRaceCars is 8 (the asm at 0x821F40F0 compares the race-car
//   index against 8). Replace with the real BrnPhysics::Vehicle constant when that TU lands;
//   the VALUE (8) is asm.
// Guarded: BrnBehaviourBystanderCam.h / BrnBehaviourLooseAttachment.h each independently
// (re)declare this same identically-valued unnamed enum in this namespace (a latent ODR risk
// that only surfaces once a single TU includes more than one of them -- as BrnArbStateTakedown.cpp
// now does for GyroCam + LooseAttachment). The guard makes the second inclusion a no-op instead
// of a redefinition error; each sibling header carries the identical guard.
#ifndef BRNDIRECTOR_CAMERA_KU_MAX_NUM_RACE_CARS_DEFINED
#define BRNDIRECTOR_CAMERA_KU_MAX_NUM_RACE_CARS_DEFINED
enum { KU_MAX_NUM_RACE_CARS = 8 };
#endif

class BehaviourGyroCam
{
public:

    // The gyro-cam parameter block (a behaviour parameter block with the gyro-cam type tag).
    // GetType returns the tag SetParameters asserts on.
    class Parameters
    {
    public:
        EBehaviourTypeGyroCam GetType() const
        {
            return static_cast<EBehaviourTypeGyroCam>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word (cached by SetParameters)
    };

    // FLAG: the two collision-policy sub-objects GetCollisionPolicy exposes are
    //   mVisibilityCollisionPolicy (+0x20, DWARF :163) and mVehicleAttachmentCollisionPolicy
    //   (+0x260, DWARF :164). Their concrete types land with the full behaviour TU; modelled here
    //   as opaque embedded sub-objects so the accessor returns a typed pointer at the asm-attested
    //   offsets (the returned policy's interior is not touched by this slice).
    class CollisionPolicy;

    // Adopt a gyro-cam parameter block: assert it carries the gyro-cam type tag, cache its first
    // word at +0x10, then store the pointer. @0x821F4068.
    void SetParameters(const Parameters* lpParameters);

    // Bind the gyro-cam rig to a race car: record the race-car index, mark valid / set, assert the
    // index is in range. @0x821F40D8 (VehicleRef block @+0x510). meRaceCarIndex is an
    // EActiveRaceCarIndex (modelled as s32 here).
    void AttachToRaceCar(s32 meRaceCarIndex);

    // Return this behaviour's active collision policy: the vehicle-attachment policy (+0x260) when
    // mbUseVehicleAttachmentCollision is set, otherwise the visibility policy (+0x20). @0x821FA1C0.
    CollisionPolicy* GetCollisionPolicy();

    // Seed the world-space normalized from-car vector: assert it has not already been set and that
    // it is unit length, store it into both the current and desired vector members, and mark it
    // set. @0x822067D0. lVectorFromCar arrives in the first vector register (v1).
    void SetWorldSpaceNormalizedVectorFromCar(rw::math::vpu::Vector3 lVectorFromCar);

private:

    // FLAG: only the members these functions touch are modelled at their asm-attested offsets; the
    //   rest of the gyro-cam rig lands with the full behaviour TU. Reserved byte spans place each
    //   field. All fields are public-of-layout (offsetof pins live in the .cpp verify the exact
    //   layout). The vtable + parameter pointer use size-stable 32-bit slots (see SIZE-STABLE PIN
    //   note above); the typed parameter pointer is appended at the tail and reached by name.
    u8    maHead000[0x10];                          // +0x000 .. +0x00F  vtable + rig head (X360 4B ptr slot)
    s32   mParamWord1;                              // +0x010  cached lpParameters->miParamWord1
    u32   muParametersSlot;                         // +0x014  adopted parameter block (X360 4B ptr slot)
    u8    maReserved018[0x20 - 0x18];               // +0x018 .. +0x01F (rig members not modelled here)

    u8    maVisibilityCollisionPolicy[0x260 - 0x20];        // +0x020  visibility collision policy (&-of)
    u8    maVehicleAttachmentCollisionPolicy[0x510 - 0x260]; // +0x260  vehicle-attachment collision policy (&-of)

    // --- mAttachedTo (Behaviour::VehicleRef) sub-block AttachToRaceCar writes, +0x510 .. +0x51F ---
    s32   miAttachedSet;                            // +0x510  attached-set flag (= 1)
    s32   meAttachedRaceCarIndex;                   // +0x514  the attached race car index
    s32   miAttachedField518;                       // +0x518  cleared to 0 by AttachToRaceCar
    u8    mbAttachedField51C;                        // +0x51C  flag set (= 1) by AttachToRaceCar
    u8    maReserved51D[0x530 - 0x51D];             // +0x51D .. +0x52F (rig + VehicleRef tail)

    rw::math::vpu::Vector3 mWorldSpaceNormalizedVectorFromCar;        // +0x530
    rw::math::vpu::Vector3 mDesiredWorldSpaceNormalizedVectorFromCar; // +0x540
    u8    maReserved550[0x632 - 0x550];             // +0x550 .. +0x631 (rig members not modelled here)

    bool  mbIsWorldSpaceVectorSet;                  // +0x632  set once SetWorldSpaceNormalizedVectorFromCar runs
    bool  mbUseVehicleAttachmentCollision;          // +0x633  selects which collision policy is active

    // x64 typed view of the adopted parameter pointer (the by-name, type-correct store target).
    // Appended at the tail so it never disturbs the pinned offsets above; the X360 packs the same
    // pointer into the 4-byte slot at +0x014.
    const Parameters* mpParameters;
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGyroCam::SetParameters @0x821F4068
//   lwz  r11, 0(r4)          ; lpParameters->meType
//   cmplwi r11, 9            ; == eBehaviourGyroCam
//   ... assert on mismatch ...
//   lwz  r11, 4(r4)          ; lpParameters->miParamWord1
//   stw  r4,  0x14(r3)       ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)       ; mParamWord1  = lpParameters->miParamWord1
// ----------------------------------------------------------------------------
inline void
BehaviourGyroCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourGyroCam,
               "lpParameters->GetType() == eBehaviourGyroCam");
    mpParameters = lpParameters;                 // stw r31, 0x14(this)
    mParamWord1  = lpParameters->miParamWord1;   // lwz r11,4(lpParameters); stw r11, 0x10(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGyroCam::AttachToRaceCar @0x821F40D8
//   stw  r4,  0x514(r3)      ; meAttachedRaceCarIndex = meRaceCarIndex
//   stb  1,   0x51C(r3)      ; mbAttachedField51C     = 1
//   stw  1,   0x510(r3)      ; miAttachedSet          = 1
//   stw  0,   0x518(r3)      ; miAttachedField518     = 0
//   cmpwi r4, 8 ; blt skip   ; assert meRaceCarIndex < ku8MaxNumRaceCars (BrnVehicleRef.h:222)
// (all four stores precede the assert).
// ----------------------------------------------------------------------------
inline void
BehaviourGyroCam::AttachToRaceCar(s32 meRaceCarIndex)
{
    meAttachedRaceCarIndex = meRaceCarIndex;     // stw r4,  0x514(this)
    mbAttachedField51C     = 1;                  // stb r11(=1), 0x51C(this)
    miAttachedSet          = 1;                  // stw r11(=1), 0x510(this)
    miAttachedField518     = 0;                  // stw r10(=0), 0x518(this)
    CGS_ASSERT(meRaceCarIndex < KU_MAX_NUM_RACE_CARS,
               "meRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars");
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGyroCam::GetCollisionPolicy @0x821FA1C0
//   lbz  r11, 0x633(r3)      ; mbUseVehicleAttachmentCollision
//   cmplwi r11, 0 ; beq      ; if (set) ...
//   addi r3, r3, 0x260       ;   return &mVehicleAttachmentCollisionPolicy
//   ... else ...
//   addi r3, r3, 0x20        ;   return &mVisibilityCollisionPolicy
// ----------------------------------------------------------------------------
inline BehaviourGyroCam::CollisionPolicy*
BehaviourGyroCam::GetCollisionPolicy()
{
    if (mbUseVehicleAttachmentCollision)
    {
        return reinterpret_cast<CollisionPolicy*>(maVehicleAttachmentCollisionPolicy); // this + 0x260
    }
    return reinterpret_cast<CollisionPolicy*>(maVisibilityCollisionPolicy);            // this + 0x20
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGyroCam::SetWorldSpaceNormalizedVectorFromCar @0x822067D0
//   lbz  r11, 0x632(r3)            ; mbIsWorldSpaceVectorSet
//   ... assert !mbIsWorldSpaceVectorSet ("Doesn't make sense to set ... twice") ...
//   vmsum3fp128 ...                ; MagnitudeSquared(lVectorFromCar) (the IsSimilar(., 1.0f) assert)
//   ... assert IsSimilar(MagnitudeSquared(lVectorFromCar), 1.0f) ...
//   stvx128 v127, r3, 0x530        ; mWorldSpaceNormalizedVectorFromCar        = lVectorFromCar
//   stvx128 v127, r3, 0x540        ; mDesiredWorldSpaceNormalizedVectorFromCar = lVectorFromCar
//   stb  1,  0x632(r3)             ; mbIsWorldSpaceVectorSet = 1
//
// The console computes MagnitudeSquared as a VMX dot product (vmsum3fp128) and the IsSimilar
// tolerance compare with a vcmpgtfp/vperm pair; the reconstruction folds that to the scalar
// rw::math::vpu::MagnitudeSquared and an absolute-difference tolerance compare (the rw "IsSimilar"
// spelling is not yet homed, so the equivalent |x - 1| <= eps form is inlined into the assert).
// ----------------------------------------------------------------------------
inline void
BehaviourGyroCam::SetWorldSpaceNormalizedVectorFromCar(rw::math::vpu::Vector3 lVectorFromCar)
{
    CGS_ASSERT(!mbIsWorldSpaceVectorSet,
               "Doesn't make sense to set WorldSpaceNormalizedVectorFromCar twice");
    CGS_ASSERT(std::fabs(rw::math::vpu::MagnitudeSquared(lVectorFromCar) - 1.0f) <= 1.0e-3f,
               "rw::math::IsSimilar(MagnitudeSquared(lVectorFromCar), 1.0f)");

    mWorldSpaceNormalizedVectorFromCar        = lVectorFromCar;   // stvx128 v127, r30, 0x530
    mDesiredWorldSpaceNormalizedVectorFromCar = lVectorFromCar;   // stvx128 v127, r30, 0x540
    mbIsWorldSpaceVectorSet                   = true;             // stb r9(=1), 0x632(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GYRO_CAM_H
