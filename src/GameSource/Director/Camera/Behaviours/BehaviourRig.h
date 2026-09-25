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
// FLAG: CameraRig is still defined inline here -- it has no home header of its own yet,
//   and BrnCameraRig.cpp reaches its Params layout through this file. Replace it with a
//   canonical home when that TU lands; the member/method NAMES are stable.
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
#include "GameSource/Director/Camera/Utils/BrnOrientationLag.h" // Utils::OrientationLag (+ Parameters)
                                                              //   -- THE home; this header's own
                                                              //   full definition is retired
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

    // The twenty authored rig presets, declared in the DWARF's header order (BrnCameraRig.h:63..
    // :82). BrnCameraRigParams.cpp DEFINES them in a slightly different order -- ParamsRearQFwd
    // sits between SideLookingForwards and RigFrontQBwd there (BrnCameraRigParams.cpp:175), which
    // is also its .data slot (console 0x82CDA810.., a 0x40 stride; see that file).
    // BehaviourRig::Parameters::Construct copies ParamsFrontQuarterClose; the takedown look-back
    // copies ParamsBonnetLow; the camera parameter bank copies nine more.
    static Params ParamsRearLongFlat;
    static Params ParamsFrontQuarterLong;
    static Params ParamsFrontQuarterClose;
    static Params ParamsFrontQuarterCloseDeep;
    static Params ParamsHighSideFlat;
    static Params ParamsSideFlat;
    static Params ParamsBonnetHigh;
    static Params ParamsBootHigh;
    static Params ParamsBonnetLow;
    static Params ParamsFrontQCuFwd;
    static Params ParamsSideLookingForwards;
    static Params ParamsRigFrontQBwd;
    static Params ParamsFrontRearview;
    static Params ParamsBootViewFwd;
    static Params ParamsFrontQLowBwd;
    static Params ParamsRoofFwd;
    static Params ParamsBootFwd;
    static Params ParamsRearQFwd;
    static Params ParamsFrontQCuFwd2;
    static Params ParamsUnderbelly;

    // DWARF BrnCameraRig.h:88 / BrnCameraRig.cpp:31 -- @0x8220B0E8. Build the rig transform
    // (camera relative to the target car) from an authored preset, the car's bounds and the
    // mirror flag. Body: Camera/Utils/BrnCameraRigConstruct.cpp.
    void Construct(const Params& lrData, const AABBox& lrAABB, bool lbReverse);

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

// The rig-cam behaviour-type tag. Parameters::Construct @0x821F9680 stores 2 in the block's first
// word (`li r9, 2 ; stw r9, 0(r3)`), and SetParameters @0x821F3B10 compares against it
// (`cmplwi r11, 2`, "lpParameters->GetType() == eBehaviourRig", BehaviourRig.h:255).
enum EBehaviourTypeRig
{
    eBehaviourRig = 2
};

// ============================================================================
// BehaviourRig -- the "rig" camera behaviour.
//   Inherits Behaviour; all private members are by name (DWARF BehaviourRig.h:185-205).
//   Console vtable 0x8200A5A0, object 0x470 bytes (NewBehaviour<BehaviourRig> @0x82260A18).
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

    // DWARF BehaviourRig.h:304 -- header inline, no console symbol. Its one X360 site,
    // MomentTakedownLookback::Update @0x822662F0 (0x822666FC..0x82266714), stores
    //     mLookingAtRef {+0x450 meType = 1 (E_RACE_CAR), +0x454 index, +0x458 = 0, +0x45C set = 1}
    //     mbLooking (+0x46A) = 1 ; mbSnap (+0x46B) = 1
    // -- the inlined VehicleRef::SetToRaceCar (its index tripwire folds away for the constant
    // index), then the two latches. The PS3 inline at 0x89F44 stores the same six words.
    // FLAG: that one site passes true, so it cannot show which latch the bool feeds. It is
    // taken as mbSnap, because mbLooking is what "start looking" means.
    void StartLookingAtRaceCar(EActiveRaceCarIndex leIndex, bool lbSnap)
    {
        mLookingAtRef.SetToRaceCar(leIndex);
        mbLooking = true;
        mbSnap    = lbSnap;
    }

    // DWARF BehaviourRig.cpp:415 / :429. Bodies in BehaviourRig.cpp.
    void  AttachToRaceCar(EActiveRaceCarIndex leIndex);
    void  SetDetached(bool lbDetached);

    // DWARF BehaviourRig.h:314 -- declaration-only; no caller in the tree.
    f32   GetAccelSpringOffsetRatio() const;

private:
    // Members in DWARF order (BehaviourRig.h:185-205). By name; sizes differ on PC. The console
    // offsets are the ones Construct @0x82242488 and Update @0x822427C0 address.
    VisibilityCollisionPolicy      mCollisionPolicy;         // X360 +0x020
    Utils::CameraRig               mRig;                    // X360 +0x260 (mfFOV at +0x2A0)
    Utils::CameraShake             mShake;                  // X360 +0x2B0
    Utils::OrientationLag          mOrientationLag;         // X360 +0x2C0
    Utils::PositionLag             mPositionLag;            // X360 +0x310
    Matrix44Affine                 mLastAttachedToTransform; // X360 +0x340
    Matrix44Affine                 mLastRigTransform;       // X360 +0x380
    BrnPhysics::Spring1D           mAccelSpring;            // X360 +0x3C0
    Utils::Looker                  mLooker;                 // X360 +0x3E4
    Utils::Random                  mRandom;                 // X360 +0x410 (typedef CgsNumeric::Random)
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
// BehaviourRig::SetParameters @0x821F3B10 (DWARF BehaviourRig.h:253; the assert cites :255)
//     lwz r11, 0(p) ; cmplwi r11, 2 ; beq -> assert "lpParameters->GetType() == eBehaviourRig"
//     lwz r11, 4(p)            the block's debug name
//     stw p, 0x460(this)       mpParameters
//     stw r11, 0x10(this)      the base's mpcDebugParametersName
//     stb 0, 8(this)           mbIsPrepared = false
// [FX-DIRECTOR2 2026-09-25] the debug-name store was missing.
// ============================================================================
inline void
BehaviourRig::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourRig, "lpParameters->GetType() == eBehaviourRig");
    mpParameters = lpParameters;
    SetDebugParametersName(lpParameters->GetDebugName());
    SetNotPrepared();
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_RIG_H
