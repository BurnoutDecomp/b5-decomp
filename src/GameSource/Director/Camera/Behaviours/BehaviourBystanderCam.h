#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_BYSTANDER_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_BYSTANDER_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT (the SetParameters type assert)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"               // CgsNumeric::Random (mRandom)
#include "GameSource/BurnoutConstants.h"                            // EActiveRaceCarIndex (SetTarget)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"        // THE canonical Camera::Behaviour base
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"          // VisibilityCollisionPolicy (mCollisionPolicy)
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"        // Utils::CameraShake (+ Parameters)
#include "GameSource/Director/Camera/Utils/BrnLooker.h"             // Utils::Looker (+ Parameters)
#include "GameSource/Director/Camera/Utils/BrnPositionFinder.h"     // Utils::PositionFinder

// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.h
//
// BrnDirector::Camera::BehaviourBystanderCam -- the "bystander cam": a camera planted at the
// roadside near a chosen car (found on the traffic lanes by a PositionFinder, or placed at a
// fixed offset in the car's own space), which then only turns and zooms to keep the car framed
// (Looker) with a little hand-held wobble (CameraShake). The crash-highlight moment
// MomentBystanderSeesAction and the player-jumping moment's bystander collection pool it.
//
// ⭐ RE-BASED 2026-09-24 (FX-DIRECTOR). The tree used to carry THREE reconstructions of this one
// class -- this header (a self-contained byte-span layout with no base and every sub-object
// opaque), BrnBehaviourBystanderCam.h (a slice modelling SetParameters/SetTarget/GetCol only) and
// BrnBehaviourBystanderCamSerialise.h (a Parameters-only view) -- and not one of them derived from
// Camera::Behaviour. NewBehaviour<BehaviourBystanderCam> therefore placed a vtable-less object in
// the behaviour pool and BehaviourHelper::Prepare @0x82255F48, which dispatches the pooled
// object's vtable slot 0 (Construct) first, read a null vptr: the access violation that stopped
// the crash-highlight moment tick (scratch/bugtest/runs/fxvoicepool/20260924_163149,
// NewBehaviour<BehaviourBystanderCam> <- MomentBystanderSeesAction::Update). The two other
// definitions are gone; this is the only one.
//
// LAYOUT AUTHORITY: the DecFIGS DWARF (BehaviourBystanderCam.h:107 `: public Behaviour`, members
// :156..:175, Parameters :181..:198), pinned by the ARTIST asm:
//   AllocateVoid<BehaviourBystanderCam> @0x82253B10  stw off_8200A5C0 -> +0x000 (this vtable),
//                                                    stw off_8200A158 -> +0x0D0 (the policy's),
//                                                    slot size li r7, 0x360
//   +0x020 mTransform          Update copies it into the camera (0x8224402C..0x82244048)
//   +0x060 mPositionFinder     Construct stb 0/0/1 -> +0x90/+0x91/+0x92 (finder +0x30..+0x32)
//   +0x0A0 mShake              Construct's four stfs 0.0 +0xA0..+0xAC; Update `addi r3, r31, 0xA0`
//   +0x0B0 mLooker             Construct +0xB0 / +0xC0 / +0xCC..+0xCF; Update `addi r3, r31, 0xB0`
//   +0x0D0 mCollisionPolicy    GetCollisionPolicy @0x821F9B28 is `addi r3, r3, 0xD0`
//   +0x310 mRandom             Construct's LCG fill; Update passes its six doublewords by value
//   +0x340 mTarget             SetTarget @0x821F3F80 stores +0x340..+0x34C
//   +0x350 mpParameters        SetParameters @0x821F3F10 `stw r31, 0x350(r30)`
//   +0x354 mfSquaredDistanceToTarget, +0x358 mfPerceivedDistanceModificationFactor,
//   +0x35C mbSetup, +0x35D mbGotPosition
// (x64: the base's pointer, the policy and mpParameters widen, so absolute offsets are PROVENANCE --
//  every access below is BY NAME.)
//
// VTABLE off_8200A5C0 (eight slots, the canonical Behaviour order):
//   0 Construct            0x822438E8        4 Release            0x8284CB38 (the base's empty body)
//   1 Prepare              0x821F9AD0        5 GetCollisionPolicy 0x821F9B28
//   2 Update               0x82243C80        6 SetupTweaker       0x821F9B30
//   3 PostCollisionUpdate  0x82C296C8 (`li r3, 1` -- the base's)   7 GetName    0x821F9C78
//
// The two impact controllers the DWARF also homes in this header (ImpactSlomoController :44,
// ImpactShakeController :75) live in BehaviourBystanderCamImpactControllers.h: the crash arbitrator
// state embeds both by value and share no member with the behaviour.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// The bystander-cam behaviour-type tag. SetParameters @0x821F3F10 compares the block's first word
// against 5 (`cmplwi r11, 5`); Parameters::Construct @0x821F9A00 stores 5 there.
enum EBehaviourTypeBystanderCam
{
    eBehaviourBystanderCam = 5
};

class BehaviourBystanderCam : public Behaviour
{
public:
    // DWARF BehaviourBystanderCam.h:181 -- the bystander-cam parameter block (console size 0x9C,
    // the stride of the parameter bank's seven-block bystander run).
    class Parameters : public Behaviour::Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block's fields into the camera-tunings
        // serialiser S. Body + instantiations: BrnBehaviourBystanderCamSerialise.cpp.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        // DWARF :201 (BehaviourBystanderCam.cpp:186) @0x821F9A00. Body in the .cpp.
        void Construct();

        Utils::CameraShake::Parameters mShakeParams;                 // :184  console +0x08
        Utils::Looker::Parameters      mLookerParams;                // :185  console +0x18
        f32  mfVelocityInfluenceOnPosition;                          // :187  console +0x7C
        f32  mfMaxInitialDistanceKM;                                 // :188  console +0x80
        f32  mfDistanceForFailKM;                                    // :189  console +0x84
        f32  mfTargetSpaceX;                                         // :191  console +0x88
        f32  mfTargetSpaceY;                                         // :192  console +0x8C
        f32  mfTargetSpaceZ;                                         // :193  console +0x90
        f32  mfHeight;                                               // :195  console +0x94
        bool mbUseTargetSpaceInsteadOfPositionFinder;                // :197  console +0x98
        bool mbUseRangeTesting;                                      // :198  console +0x99
    };

    // DWARF h:213, @0x821F3F10 (the assert cites BehaviourBystanderCam.h:215): assert the block's
    // type tag, store the pointer (+0x350) and cache the block's debug name in the base's +0x10 slot.
    // NOT a virtual override -- it takes the DERIVED Parameters, so it hides the base's pair.
    void SetParameters(const Parameters* lpParameters)
    {
        CGS_ASSERT(lpParameters->GetType() == eBehaviourBystanderCam,
                   "lpParameters->GetType() == eBehaviourBystanderCam");   // h:215
        mpParameters = lpParameters;                                        // stw r31, 0x350(r30)
        SetDebugParametersName(lpParameters->GetDebugName());              // lwz 4(lp) ; stw 0x10(this)
    }

    // DWARF h:250, @0x821F3F80: the inlined VehicleRef::SetToRaceCar over mTarget (its four stores
    // and its BrnVehicleRef.h:222 index assert, 0x821F3FA0..0x821F3FD4), then mbSetup = 1.
    void SetTarget(EActiveRaceCarIndex leRaceCarIndex)
    {
        mTarget.SetToRaceCar(leRaceCarIndex);
        mbSetup = true;                                                     // stb r30(=1), 0x35C(r31)
    }

    // DWARF h:259 -- the squared camera-to-subject distance the last Update measured.
    f32 GetSquaredDistanceToTarget() const { return mfSquaredDistanceToTarget; }

    // DWARF h:268. No out-of-line console symbol; MomentBystanderSeesAction::
    // SetPerceivedDistanceModificationFactor @0x822197E0 inlines it: `fcmpu` the new value against
    // +0x358, equal -> nothing; else store it and `stb 1` to +0xCF == mLooker (+0xB0) +0x1F, the
    // looker's mbForceZoomTargetUpdate, so the next Zoom re-snaps its FOV band to the new framing.
    void SetPerceivedDistanceModificationFactor(f32 lfFactor)
    {
        if (lfFactor != mfPerceivedDistanceModificationFactor)
        {
            mfPerceivedDistanceModificationFactor = lfFactor;
            mLooker.ForceZoomTargetUpdate();
        }
    }

    // ---- the virtual interface (vtable off_8200A5C0) -----------------------------------------
    void Construct() override;                                                    // slot 0 (cpp:221)
    bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;        // slot 1 (cpp:251)
    bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo) override;     // slot 2 (cpp:270)
    CollisionPolicy* GetCollisionPolicy() override;                                // slot 5 (cpp:396)
    void SetupTweaker(Utils::Tweaker& lrTweaker) override;                         // slot 6 (cpp:410)
    const char* GetName() const override;                                          // slot 7 (cpp:435)

private:
    // ---- layout (DWARF :156..:175) ---------------------------------------------------------------
    Matrix44Affine             mTransform;                               // :156  console +0x020
    Utils::PositionFinder      mPositionFinder;                          // :158  console +0x060
    Utils::CameraShake         mShake;                                   // :159  console +0x0A0
    Utils::Looker              mLooker;                                  // :160  console +0x0B0
    VisibilityCollisionPolicy  mCollisionPolicy;                         // :162  console +0x0D0
    Utils::Random              mRandom;                                  // :164  console +0x310
    Behaviour::VehicleRef      mTarget;                                  // :166  console +0x340
    const Parameters*          mpParameters;                             // :168  console +0x350
    f32                        mfSquaredDistanceToTarget;                // :170  console +0x354
    f32                        mfPerceivedDistanceModificationFactor;    // :172  console +0x358
    bool                       mbSetup;                                  // :174  console +0x35C
    bool                       mbGotPosition;                            // :175  console +0x35D
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_BYSTANDER_CAM_H
