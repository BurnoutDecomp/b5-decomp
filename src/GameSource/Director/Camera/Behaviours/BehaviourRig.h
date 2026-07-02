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
#include "GameSource/Director/Camera/Utils/CameraUtils.h"     // AABBox, VersionNumber
#include "GameSource/Director/Camera/Utils/BrnLooker.h"       // Looker + Random typedef
#include "GameSource/Director/Camera/Utils/BrnPositionLag.h"  // PositionLag
#include "GameSource/Director/Utils/BrnVehicleRef.h"          // BrnDirector::VehicleRef (base)
#include "GameSource/Physics/PhysicsUtilities/Spring1D.h"     // BrnPhysics::Spring1D

namespace BrnDirector { class WorldMap; }   // GameSource/Director/Utils/BrnDirectorWorldMap.h (minimal slice)
// BrnPlayerInfo.h (VehicleInfo) is included by the .cpp; the header only embeds named members.

namespace BrnDirector
{

// FLAG: minimal Timestep stub (the full Timestep TU is not yet recovered). The Rig
//   behaviour reads a timestep scalar from the SharedInfo by type index. The E_TYPE_*
//   enumerators are from the DWARF; the assert in Update references "E_TIMESTEP_COUNT".
struct Timestep
{
    enum EType { E_TIMESTEP_INVALID = 0, E_TYPE_DEFAULT = 1, E_TIMESTEP_COUNT = 3 };
    f32 Get(EType leType) const;
};

namespace Camera
{

// Forward decls for types used only as pointer/reference parameters.
struct BehaviourSharedInfo;
struct BehaviourSharedPrepareReleaseInfo;

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
};

// ============================================================================
// FLAG: minimal slice of the visibility collision policy used by BehaviourRig.
//   Private members are stored as an opaque blob (the full definition needs
//   unrecovered types: LineTestNearestPostBox, VehicleCollisionPredictor, etc.).
//   Nominal X360 size 0x240 bytes; on PC the blob width gives a conservative bound.
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

private:
    // FLAG: opaque placeholder; the real member set needs LineTestNearestPostBox,
    //   VehicleCollisionPredictor, VolumeTestDeepestPostBox, GroundConstraint etc.
    u8 mRawStorage[0x240 - sizeof(void*)];  // span calibrated from X360 IceAnim sibling
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
// FLAG: minimal slice of the camera dev-tools tweaker.
//   SetupTweaker tail-calls Tweaker::Construct(lrTweaker), so only that static
//   is modelled. The real home is BrnCameraTweaker.h (not yet reconstructed).
// ============================================================================
class Tweaker
{
public:
    // Build the tweaker binding table. Called as BehaviourRig::SetupTweaker's
    // first act (@0x821F9870: result = BrnDirector::Camera::Utils::Tweaker::Construct(a2)).
    static Tweaker* Construct(Tweaker& lrTweaker);

    // FLAG: raw storage (size inferred from AxisMapping[3][9] + mapping tables).
    u8 maReserved[0x800];
};

} // namespace Utils

// ============================================================================
// FLAG: minimal slices of the shared-info blocks consumed by BehaviourRig's virtuals.
// ============================================================================

// The prepare/release info (Prepare arg2). The rig's Prepare does not call any
// accessor on it; the type is needed only for the virtual override signature.
struct BehaviourSharedPrepareReleaseInfo
{
};

// The per-frame shared info (Update arg2). The rig reads the "world" pointer for
// VehicleRef::Get resolution and timestep values via raw offsets in the pseudo;
// modelled here by a minimal accessor the callee-shaped reconstruction uses.
struct BehaviourSharedInfo
{
    // @pseudo +1468: the "world" context for BrnDirector::VehicleRef::Get.
    const void* GetWorld() const;

    // @pseudo +352*4+timestep*4: per-frame delta seconds indexed by timestep type.
    f32 GetTimestep(BrnDirector::Timestep::EType leType) const;

    // ADDITIVE GROW (PositionFinder::Update @0x8223FD3C, which loads the map pointer
    // at sharedInfo+0x5DC and hands it to the WorldMap safe-position query).
    const ::BrnDirector::WorldMap* GetWorldMap() const;
};

// ============================================================================
// FLAG: minimal slice of the Behaviour base class. Only the members BehaviourRig's
//   six functions read/write through `this` are named here. Replace with the
//   Behaviour TU's canonical home when it lands; member NAMES are stable.
//   DWARF Behaviour.h:167: vtable + meTimestepType + 5 bools + mpcDebugParametersName.
// ============================================================================
class Behaviour
{
public:
    // FLAG: nested VehicleRef extends BrnDirector::VehicleRef with BehaviourSharedInfo
    //   resolution. Inherits the base's four fields (meType/miRaceCarIndex/muRef/mbSet,
    //   asm-retyped by the VehicleRef class TU); adds no data members.
    class VehicleRef : public BrnDirector::VehicleRef
    {
    public:
        bool IsValid() const;
        const Matrix44Affine& GetTransform(const BehaviourSharedInfo& lrInfo) const;
    };

    // FLAG: nested Parameters (DWARF Behaviour.h:265). Only the base Construct is called
    //   by BehaviourRig::Parameters::Construct.
    class Parameters
    {
    public:
        void Construct();
        u32         mType;         // +0x00 the behaviour-type tag
        const char* mpcDebugName;  // +0x04
    };

    virtual ~Behaviour() {}
    virtual void Construct();
    virtual bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo);
    virtual bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo);
    virtual bool PostCollisionUpdate(Camera& lrCamera, const BehaviourSharedInfo& lrInfo);
    virtual void Release(const BehaviourSharedPrepareReleaseInfo& lrInfo);
    virtual CollisionPolicy* GetCollisionPolicy();
    virtual void SetupTweaker(Utils::Tweaker& lrTweaker);
    virtual const char* GetName() const;

    // Timestep type selector (DWARF Behaviour.h:334).
    BrnDirector::Timestep::EType GetTimestepType() const { return meTimestepType; }
    void SetTimestepType(BrnDirector::Timestep::EType leType) { meTimestepType = leType; }

protected:
    // DWARF Behaviour.h:334..343 (after vtable).
    BrnDirector::Timestep::EType meTimestepType;  // +0x04
    bool                         mbIsPrepared;    // +0x08
    bool                         mbHasFailed;     // +0x09
    bool                         mbTweakerAttached; // +0x0A
    bool                         mbCanSwitchToMeNow;   // +0x0B
    bool                         mbCanSwitchFromMeNow;  // +0x0C
    const char*                  mpcDebugParametersName; // +0x10

    void SetPrepared()    { mbIsPrepared = true; }
    void SetNotPrepared() { mbIsPrepared = false; }
    bool IsPrepared()     { return mbIsPrepared != 0; }
};

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
    CGS_ASSERT(lpParameters->mType == 2u, "lpParameters->GetType() == eBehaviourRig");
    mpParameters = lpParameters;
    mbIsPrepared = false;
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_RIG_H
