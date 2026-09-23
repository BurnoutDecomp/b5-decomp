#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"   // CgsSceneManager::EntityId
#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT

#include <cstring>   // memset (matching the X360 memset of the BBox scratch tail)

// ==================================================================================================
// BrnPhysics::Deformation::PhysicalBodyPart::Construct @0x825B4178 -- SPLIT OUT of
// BrnPhysicalBodyPart.cpp on 2026-08-03 (task #116). BUILD-MECHANICS SPLIT ONLY: MOVED verbatim.
// See the marker left in that file for the measurement that forced the split.
// ==================================================================================================

namespace BrnPhysics
{
namespace Deformation
{
    // ⛔ RENAMED 2026-08-27 (detach-2 wave) -- IT IS NOT kfPartMass. This 5.0 is an UNNAMED rodata
    // literal, flt_8200426C, that Construct @0x825B41C4/D0 loads for ExternalPhysicsBody::SetMass.
    // The DWARF's namespace-scope `kfPartMass` is a DIFFERENT symbol, at 0x82F2A380, and it holds
    // 100.0 -- see the recovered run in BrnPhysicalBodyPart.cpp. Calling this one KF_PART_MASS made
    // the two look like one value and made the sibling file's placeholder-zero look already solved.
    // `static const` so each TU keeps its own copy -- no ODR surface.
    static const f32 KF_PART_CONSTRUCT_MASS = 5.0f;   // flt_8200426C -- Construct's own literal


    // =========================================================================================
    // Construct @ 0x825B4178   [EXECUTED in goal trace]
    //
    // Zero/identity-init the part: zero mLocalJointPositionPlusRotation (+352), seed mRigidBodyId
    // (+464) with K_INVALID_RIGID_BODY_ID, null mpIKPart (+476), mpDeformableObject (+480),
    // mbAddedToScene (+485) and mbFrozen (+486); Construct the embedded body; seed mfMass = 5.0 (the
    // lvlx/vspltw of the 5.0 stack temp into +208); Prepare the body; then zero the packed
    // graphics/COM/initial-joint rows and the two collision accumulators (+368/+384/+400/+432/+448).
    // SIX Vector3Plus rows are zeroed in all (+352 first, the other five last).
    // =========================================================================================
    void PhysicalBodyPart::Construct()
    {
        // 0x825B4194 vspltisw128 v127,0 ; 0x825B419C li r9,0x160 ; 0x825B41A8 stvx128 v127,r31,r9
        // (word 0x13FF49CF, the same encoding as the +0x190 store below) -- the FIRST store: all
        // 16 bytes of mLocalJointPositionPlusRotation (+0x160) = 0. Crash parity G28-D1
        // (2026-09-23): Hex-Rays rendered r9 as `HIDWORD(qword_82F2A3A8)` and this line's old comment
        // copied it ("clears the high half" of the id); nothing wrote the row. PhysicalBodyPart::
        // Prepare re-zeroes it before any reader, so only a never-prepared slot differed.
        mLocalJointPositionPlusRotation.SetZero();

        // 0x825B4190 lis r11,0x82F3 ; 0x825B41A0 ld r10,qword_82F2A3A8 ; 0x825B41AC std r10,0x1D0(r31):
        // all EIGHT bytes of the packed id. x360rd reads 0x82F2A3A8..AF as FF x8 -- plain image data
        // (findinit: 9 `ld` readers, no CRT writer), i.e. CgsPhysics::K_INVALID_RIGID_BODY_ID (~0ull),
        // the same constant RemovePart re-seeds from (0x8260CAEC; ClearPoolSlotBindings). Crash parity
        // G28-D2 (2026-09-23): the old "rodata-not-recovered" FLAG seeded only the entity word and left
        // muSubA/muSubB zero, so GetBaseRigidBodyID() read 0xFFFFFFFF00000000 instead of all ones.
        mRigidBodyId.muEntityWord = 0xFFFFFFFFu;
        mRigidBodyId.muSubA       = 0xFFFFu;
        mRigidBodyId.muSubB       = 0xFFFFu;

        mpIKPart           = 0;       // *(this+476) = 0
        mpDeformableObject = 0;       // *(this+480) = 0
        mbAddedToScene     = false;   // *(this+485) = 0
        mbFrozen           = false;   // *(this+486) = 0

        // BrnPhysics::ExternalPhysicsBody::Construct() on the embedded body.
        mRwBody.Construct();

        // v14 = 5.0 ; lvlx/vspltw v0 ; stvx128 v0 -> this+208  (mfMass = 5.0, broadcast).
        // mfMass lives inside mRwBody (+208 in the console layout). The asm stores 5.0 BETWEEN
        // ExternalPhysicsBody::Construct() (above) and ::Prepare() (below). It is the recovered
        // literal flt_8200426C, not a placeholder -- and not kfPartMass (100.0), see the rename note.
        mRwBody.SetMass(KF_PART_CONSTRUCT_MASS);

        // BrnPhysics::ExternalPhysicsBody::Prepare().
        mRwBody.Prepare();

        // Zero the three packed Vector3Plus rows + the two collision accumulators (the five
        // stvx128 v127 at 0x825B4200..0x825B4210):
        //   +368 mLocalGraphicsPositionPlusJointVelocity
        //   +384 mLocalInitialComPositionPlusMaxJointAngle
        //   +400 mLocalInitialJointPositionPlusLimitStress
        //   +432 mWorldPenetrationPlusCollisionMagnitude
        //   +448 mAverageCollisionPointPlusNumCollisions
        mLocalGraphicsPositionPlusJointVelocity.SetZero();
        mLocalInitialComPositionPlusMaxJointAngle.SetZero();
        mLocalInitialJointPositionPlusLimitStress.SetZero();
        mWorldPenetrationPlusCollisionMagnitude.SetZero();
        mAverageCollisionPointPlusNumCollisions.SetZero();
    }
}
}
