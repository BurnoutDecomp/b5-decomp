#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_RIG_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_RIG_H

// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourRig.h
//
// BrnDirector::Camera::BehaviourRig -- the "rig" camera behaviour: drives the
// camera off an authored CameraRig with optional spring, orientation/position
// lag, DOF, and looker post-processes. Authoritative home for the full class
// definition and its Parameters block.
//
// PROVENANCE:
//   X360 asm + ARTIST pseudocode (BehaviourRig.cpp addresses above) is the spine;
//   DecFIGS DWARF for BehaviourRig.h fills the member set. Members accessed BY NAME.
//   Sizes differ from X360 (64-bit PC build); no raw-offset padding for embedded types
//   that have called methods -- instead each utility type is fully or stub-defined here.
//
// FLAG: Minimal stubs for Behaviour, VisibilityCollisionPolicy, BehaviourSharedInfo,
//   BehaviourSharedPrepareReleaseInfo, CameraRig, CameraShake, OrientationLag, and
//   Tweaker are defined inline. Replace each with its canonical home header when that
//   TU is reconstructed; the member/method NAMES are stable.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                   // Vector3/Matrix44Affine/VecFloat
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Numeric/CgsRandom.h"         // CgsNumeric::Random
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"  // CgsSceneManager::EntityId
#include "GameSource/BurnoutConstants.h"                      // EActiveRaceCarIndex
#include "GameSource/Director/Camera/Camera.h"                // BrnDirector::Camera::Camera
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"    // GeometryCollisionPredictor (embedded carve)
#include "GameSource/Director/Camera/Utils/BrnVehicleCollisionPredictor.h" // Utils::VehicleCollisionPredictor (embedded carve)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"     // AABBox, VersionNumber
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h" // Utils::Tweaker (the REAL home;
                                                              //   this header's old minimal
                                                              //   slice is retired -- see below)
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"  // Utils::CameraShake (+ Parameters)
                                                              //   -- THE home; this header's own
                                                              //   full definition is retired below
#include "GameSource/Director/Camera/Utils/BrnLooker.h"       // Looker + Random typedef
#include "GameSource/Director/Camera/Utils/BrnPositionLag.h"  // PositionLag
#include "GameSource/Director/Utils/BrnVehicleRef.h"          // BrnDirector::VehicleRef (base)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"  // THE canonical Behaviour base +
                                                              //   BehaviourSharedInfo /
                                                              //   BehaviourSharedPrepareReleaseInfo
                                                              //   (this header's old forks retired)
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"    // BrnDirector::Timestep (the REAL home;
                                                              //   the old local fork is retired)
#include "GameSource/Physics/PhysicsUtilities/Spring1D.h"     // BrnPhysics::Spring1D

namespace BrnDirector { class WorldMap; }   // GameSource/Director/Utils/BrnDirectorWorldMap.h (minimal slice)
// BrnPlayerInfo.h (VehicleInfo) is included by the .cpp; the header only embeds named members.

namespace BrnDirector
{

// RETIRED (2026-07-29): the minimal `struct Timestep` fork that used to sit here (with its
// own EType whose E_TIMESTEP_INVALID was 0) is gone. The real home is
// GameSource/Director/Utils/BrnDirectorTimestep.h, included above -- its EType is
// { E_TIMESTEP_INVALID = -1, E_WORLD, E_WORLD_NO_SLOMO, E_GAME, E_TIMESTEP_COUNT }. The fork
// was self-inconsistent: BehaviourRig::Construct set the type to its INVALID (0) while
// BehaviourRig::Update asserts the type is > E_TIMESTEP_INVALID. The console stores 0 there
// (== E_WORLD under the real enum), which the canonical enum makes consistent.

namespace Camera
{

// BehaviourSharedInfo / BehaviourSharedPrepareReleaseInfo now come from the canonical
// Behaviour.h (included above); the forward decls + the two forked definitions this header
// used to carry are retired.

// ============================================================================
// RETIRED (2026-07-30, the ICE-anim de-fork wave): `class CollisionPolicy` and
// `class VisibilityCollisionPolicy` used to be defined here. Their real home -- the one every
// one of their own tripwires names -- is ../BrnCollisionPolicy.h, included at the top of this
// file, and they MOVED there unchanged (VisibilityCollisionPolicy additionally carved out the
// three see-through bytes at +0x1A0..+0x1A2 that the retired BrnBehaviourIceAnim.h slice
// carried, from inside its own [+0xE8, +0x210) reserved span).
//
// WHY THEY HAD TO MOVE: BrnBehaviourIceAnim.h carried its OWN definitions of both, so any TU
// pulling the named-parameter bank (-> BehaviourPassengerCam.h -> this file) AND the ICE-anim
// behaviour was C2011 on both -- which is exactly the set of arbitrator states the retail game
// intro runs through (ArbStateCarSelect / ArbStateOnlineCarSelect / ArbStateRaceIntro ...).
// One home settles it, the same way CameraShake's and Tweaker's moves did.
// ============================================================================

namespace Utils
{

// ============================================================================
// CameraRig -- positions/orients the camera relative to an authored Params block.
//   FULL DEFINITION from DWARF BrnCameraRig.h. Sized from X360 asm member accesses.
// ============================================================================
class CameraRig
{
public:
    // Author-visible parameter block (DWARF CameraRig.h). Nominal 64-byte X360 body
    // (each Vector3 aligns to 16 bytes on the SIMD ISA; the bool is padded to 16).
    struct Params
    {
        // X360 visitor: `void Serialise<S>(S&)` (camera-tunings TextFile{Read,Write}Serialiser).
        // Per-instance body is a separate TU.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        Vector3    mOffsetFromTarget;            // +0x00
        Vector3    mOffsetFromRotationCentre;    // +0x10
        f32        mfFOV;                        // +0x20
        f32        mfRoll;                       // +0x24
        f32        mfPitch;                      // +0x28
        f32        mfYaw;                        // +0x2C
        bool       mbWidescreenOnly;             // +0x30
    };

    // Build the rig transform from Params + target AABBox + mbReverse flag.
    void Construct(const Params& lrParams, const AABBox& lrBounds, bool lbReverse);

    // Advance the rig per frame.
    void Update(Camera& lrCamera, Matrix44Affine lTarget);

    const Matrix44Affine& GetRigTransform() const { return mRigTransform; }
    f32                   GetFOV()          const { return mfFOV; }

private:
    Matrix44Affine mRigTransform;   // +0x00 (64 bytes)
    f32            mfFOV;           // +0x40
};

// ============================================================================
// RETIRED (2026-07-29): CameraShake used to be defined here in full ("FULL DEFINITION from
// DWARF BrnCameraShake.h" -- which is exactly the point: its home is BrnCameraShake.h, and
// that home now EXISTS at Camera/Utils/BrnCameraShake.h, included at the top of this file).
// The definition moved there byte-identically (same four f32, same Parameters block).
//
// WHY IT HAD TO MOVE: the two shared gameplay camera behaviours each embed a
// Utils::CameraShakeICEController, whose own DWARF home is BrnCameraShake.h too and which had
// no definition anywhere -- that is what kept BehaviourGameplayBumper /
// BehaviourGameplayExternal as raw-offset `void* mpVTable` forks instead of real
// Camera::Behaviour subclasses. Re-basing them means their headers need CameraShake, and
// BrnBehaviourManager.cpp pulls BOTH those headers and this one (via
// BrnBehaviourAftertouchCam.h) in a single TU -- two definitions of Camera::Utils::CameraShake
// in one TU is C2011. One home settles it.
// (The THIRD fork, the 16-byte reserved slice in BrnBehaviourIceAnim.h, is untouched: nothing
// outside its own .cpp includes that header, so it collides with nothing. Retire it with the
// rest of the IceAnim fork family -- Step 0 #3.)
// ============================================================================

// ============================================================================
// OrientationLag -- smoothly lag the camera orientation from frame to frame.
//   FULL DEFINITION from DWARF BrnOrientationLag.h.
// ============================================================================
class OrientationLag
{
public:
    struct Parameters
    {
        // X360 visitor: `void Serialise<S>(S&)` (camera-tunings TextFile{Read,Write}Serialiser).
        // Per-instance body is a separate TU.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        VersionNumber muVersion;       // +0x00
        f32  mfPitchSpring;            // +0x04
        f32  mfYawSpring;              // +0x08
        f32  mfRollSpring;             // +0x0C
        f32  mfSlerpSpring;            // +0x10
        bool mbUseSlerpSpring;         // +0x14
        void Construct();
    };

    void Construct();
    void SetParameters(const Parameters* lpParameters);
    void Update(f32 lfTimestep, Matrix44Affine lTransform);
    const Matrix44Affine& GetTransform() const;

private:
    Matrix44Affine       mLastTransform;  // +0x00 (64 bytes)
    const Parameters*    mpParameters;   // +0x40
    bool                 mbFirstFrame;   // +0x44
};

// ============================================================================
// The camera dev-tools tweaker: DE-FORKED (BehaviourManager wave).
//   This header used to carry a minimal `class Tweaker { static Tweaker* Construct(Tweaker&);
//   u8 maReserved[0x800]; }` slice with the note "the real home is BrnCameraTweaker.h (not
//   yet reconstructed)". That home EXISTS now -- Utils/BrnCameraTweaker.h, with the real
//   DWARF layout (the 3x9 AxisMapping table, the pressed/released mapping tables and
//   mbHideInstructions @+0xA5C) and the same X360 Construct @0x821F8588 as a MEMBER
//   (`void Construct()` -- the console's `Tweaker::Construct(a2)` is that member with
//   this == a2). Keeping both definitions made every TU that pulled BehaviourRig.h AND
//   BrnCameraTweaker.h (e.g. BrnBehaviourGyroCam.h, and through it BrnBehaviourManager.cpp)
//   fail with C2011 on BrnDirector::Camera::Utils::Tweaker.
//   The slice is retired; the real home is included at the top of this file instead.
// ============================================================================

} // namespace Utils

// ============================================================================
// RETIRED (2026-07-29): this header used to carry PRIVATE forks of
//   * BehaviourSharedPrepareReleaseInfo (an empty struct),
//   * BehaviourSharedInfo (3 declaration-only accessors), and
//   * class Behaviour (the base slice, with the DWARF member names but no real home).
// All three now live in GameSource/Director/Camera/Behaviours/Behaviour.h, which is included
// at the top of this file. The accessor names BehaviourRig.cpp calls (GetWorld / GetTimestep
// / GetWorldMap) are carried forward verbatim by the canonical BehaviourSharedInfo, each
// resolving to the DWARF member the fork's own offset comment pinned. Behaviour::VehicleRig's
// nested VehicleRef and Behaviour::Parameters moved with it.
// ============================================================================

// ============================================================================
// BehaviourRig -- the "rig" camera behaviour.
//   Inherits Behaviour; all private members are by name (DWARF BehaviourRig.h:185-205).
// ============================================================================
class BehaviourRig : public Behaviour
{
public:
    // -------------------------------------------------------------------------
    // Parameters -- the authored parameter block for this behaviour.
    //   Inherits Behaviour::Parameters; the type tag for rig is 2 (eBehaviourRig).
    //   DWARF BehaviourRig.h:211..241.
    // -------------------------------------------------------------------------
    class Parameters : public Behaviour::Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks the rig block's fields (recursing into
        // mRigParams/mShakeParams/mLookerParams/mOrientationLagParams/mPositionLagParams) into the
        // camera-tunings serialiser S. Per-instance body is a separate TU.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        void Construct();

        Utils::VersionNumber            muVersion;           // version tag
        Utils::CameraRig::Params        mRigParams;          // authored rig
        Utils::CameraShake::Parameters  mShakeParams;        // shake post-process
        Utils::Looker::Parameters       mLookerParams;       // looker post-process
        Utils::OrientationLag::Parameters mOrientationLagParams;
        Utils::PositionLag::Parameters    mPositionLagParams;
        f32   mfSpringAccelFactor;
        f32   mfSpringMass;
        f32   mfSpringStiffness;
        f32   mfSpringDampening;
        f32   mfSpringMinStretch;
        f32   mfSpringMaxStretch;
        f32   mfDOFNear;
        f32   mfDOFFar;
        f32   mfDOFBlurDepth;
        f32   mfDOFIntensity;
        bool  mbUseAccelSpring;
        bool  mbUseShake;
        bool  mbReverse;
        bool  mbUseOrientationLag;
        bool  mbUsePositionLag;
    };

    // ---- Public interface (DWARF BehaviourRig.h) ----------------------------
    void SetParameters(const Parameters* lpParameters);

    virtual void          Construct()                                              override;
    virtual bool          Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;
    virtual bool          Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo) override;
    virtual CollisionPolicy* GetCollisionPolicy()                                  override;
    virtual void          SetupTweaker(Utils::Tweaker& lrTweaker)                 override;
    virtual const char*   GetName() const                                          override;

    void  StartLookingAtRaceCar(EActiveRaceCarIndex leIndex, bool lbLooking);
    void  AttachToRaceCar(EActiveRaceCarIndex leIndex);
    void  SetDetached(bool lbDetached);
    f32   GetAccelSpringOffsetRatio() const;

private:
    // Members in DWARF order (BehaviourRig.h:185-205). By name; sizes differ on PC.
    VisibilityCollisionPolicy      mCollisionPolicy;         // +0x14 (after Behaviour base)
    Utils::CameraRig               mRig;                    // after mCollisionPolicy
    Utils::CameraShake             mShake;
    Utils::OrientationLag          mOrientationLag;
    Utils::PositionLag             mPositionLag;
    Matrix44Affine                 mLastAttachedToTransform;
    Matrix44Affine                 mLastRigTransform;
    BrnPhysics::Spring1D           mAccelSpring;            // X360 +0x3C0
    Utils::Looker                  mLooker;                 // X360 +0x410
    Utils::Random                  mRandom;                 // (typedef CgsNumeric::Random)
    Behaviour::VehicleRef          mAttachedToRef;          // X360 +0x440
    Behaviour::VehicleRef          mLookingAtRef;           // X360 +0x450
    const Parameters*              mpParameters;            // X360 +0x460
    f32                            mfLastMPH;               // X360 +0x464
    bool                           mbDetached;              // X360 +0x468
    bool                           mbLookingLast;           // X360 +0x469
    bool                           mbLooking;               // X360 +0x46A
    bool                           mbSnap;                  // X360 +0x46B
};

// ============================================================================
// BehaviourRig::SetParameters inline @0x821F3B10
//   Asserts type tag == 2 (eBehaviourRig), stores block pointer + cached word + clears flag.
// ============================================================================
inline void
BehaviourRig::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == 2u, "lpParameters->GetType() == eBehaviourRig");
    mpParameters = lpParameters;
    SetNotPrepared();
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_RIG_H
