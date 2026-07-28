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
// FLAG: minimal slice of the abstract collision-policy interface.
//   The real layout/method set lands with the CollisionPolicy TU.
// ============================================================================
class CollisionPolicy
{
public:
    virtual ~CollisionPolicy() {}
    virtual void GenerateSceneQueries(const void*, Camera&) {}
    virtual void ProcessSceneQueryResults(const void*, Camera&) {}

    // @0x82206450 (class TU; body in ../BrnCameraCollisionPolicy.cpp) -- give up:
    // record the failure reason in the shared info's validity account (+0x138),
    // drop the info's follow-request bit (the +0x140 u64, bit 1), raise mbFailed.
    void Fail(BehaviourSharedInfo* lpInfo, s32 liReason);

private:
    // ADDITIVE GROW (Fail @0x82206450 `stb 1,4(this)`): the policy's failed latch.
    bool mbFailed;   // +0x04 (X360; right after the vptr)
};

// ============================================================================
// FLAG: minimal slice of the visibility collision policy used by BehaviourRig.
//   The opaque blob is now CARVED around the members the class-TU bodies touch
//   (X360 offsets in comments; ORDER preserved, PC offsets differ -- all access
//   is BY NAME). The remaining reserved spans still need the unrecovered types
//   (LineTestNearestPostBox, VolumeTestDeepestPostBox, GroundConstraint etc.).
//   Nominal X360 size 0x240 bytes.
// ============================================================================
class VisibilityCollisionPolicy : public CollisionPolicy
{
public:
    void Construct();
    void SetCanFail(bool lbCanFail);
    void SetTarget(Matrix44Affine lTargetTransform, AABBox lTargetAABB,
                   CgsSceneManager::EntityId lTargetEntityId);
    void SetTestLookingAt(bool lbTestLookingAt);
    void SetVelocity(Vector3 lVelocity);
    bool IsVisibilityInterrupted() const;
    float GetMinTimeToVisibilityFailure() const;

    // ---- class-TU surface (bodies in ../BrnVisibilityCollisionPolicy.cpp) ----

    // The guards the two time queries assert on (the X360 inlines the embedded
    // predictors' flag reads into the wrappers).
    bool WillCollideWithGeometry() const { return mGeometryCollisionPredictor.WillCollide(); }
    bool WillCollideWithVehicle() const  { return mVehicleCollisionPredictor.HasPredictedCollision(); }

    // @0x821F38E0 (BrnCollisionPolicy.h:489) -- raise the desired-height override
    // latch, assert the height positive, store it.
    void SetDesiredHeight(f32 lfDesiredHeight);

    // @0x821F37C8 (BrnCollisionPolicy.h:425 wrapper + the embedded geometry
    // predictor's own :206 tripwire) -- predicted time until the camera hits
    // geometry.
    f32 TimeUntilCollisionWithGeometry() const;

    // @0x821F3858 (BrnCollisionPolicy.h:431 wrapper + the embedded vehicle
    // predictor's own BrnVehicleCollisionPredictor.h:69 tripwire) -- predicted
    // time until the camera hits the tracked vehicle.
    f32 TimeUntilCollisionWithVehicle() const;

private:
    // FLAG: reserved spans = rig members not yet recovered (LineTestNearestPostBox,
    //   VolumeTestDeepestPostBox, GroundConstraint etc.); the named members are the
    //   asm-attested carves from the three class-TU bodies.
    u8 maReservedToVehiclePredictor[0x70 - 0x08];              // X360 [+0x08, +0x70)
    Utils::VehicleCollisionPredictor mVehicleCollisionPredictor;   // X360 +0x70 (flag/time @+0x70/+0x74)
    u8 maReserved78[0x80 - 0x78];                              // X360 [+0x78, +0x80)
    GeometryCollisionPredictor mGeometryCollisionPredictor;    // X360 +0x80 (its +0x60/+0x64 pair == policy +0xE0/+0xE4)
    u8 maReservedE8[0x210 - 0xE8];                             // X360 [+0xE8, +0x210)
    f32 mfDesiredHeight;                                       // X360 +0x210 (SetDesiredHeight stores)
    u8 maReserved214[0x23C - 0x214];                           // X360 [+0x214, +0x23C)
    u8 mbHaveDesiredHeight;                                    // X360 +0x23C (SetDesiredHeight raises)
    u8 maReservedTail[0x240 - 0x23D];                          // X360 [+0x23D, +0x240)
};

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
// CameraShake -- adds XY/Z wobble to the camera matrix each frame.
//   FULL DEFINITION from DWARF BrnCameraShake.h.
// ============================================================================
class CameraShake
{
public:
    struct Parameters
    {
        // X360 visitor: `void Serialise<S>(S&)` (camera-tunings TextFile{Read,Write}Serialiser).
        // Per-instance body is a separate TU.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        f32  mfXYShakeMagnitudeDegs;   // +0x00
        f32  mfZShakeMagnitudeDegs;    // +0x04
        f32  mfXYWobbleMagnitudeDegs;  // +0x08
        f32  mfWobbleCenteringFactor;  // +0x0C
        void Construct();
    };

    void Construct();
    void Update(Matrix44Affine& lrTransform, const Parameters& lrParams,
                BrnDirector::Camera::Utils::Random lRandom,
                f32 lfTimestep, f32 lfSpeedRatio);

private:
    f32  mfCurrentWobbleX;    // +0x00
    f32  mfCurrentWobbleY;    // +0x04
    f32  mfCurrentWobbleXVel; // +0x08
    f32  mfCurrentWobbleYVel; // +0x0C
};

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
