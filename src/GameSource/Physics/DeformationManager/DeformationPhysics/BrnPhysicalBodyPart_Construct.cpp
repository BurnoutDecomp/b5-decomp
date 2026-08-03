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
    // Carried across with the body (BrnPhysicalBodyPart.cpp:110). `static const` so each TU keeps its
    // own copy -- no ODR surface. The recovered literal (the lvlx/vspltw 5.0 stack temp), NOT a
    // placeholder.
    static const f32 KF_PART_MASS = 5.0f;   // Construct stores 5.0 into mfMass (+208)


    // =========================================================================================
    // Construct @ 0x825B4178   [EXECUTED in goal trace]
    //
    // Zero/identity-init the part: clear the COM/box-orientation scratch, null mpIKPart (+476),
    // mpDeformableObject (+480), mbAddedToScene (+485) and mbFrozen (+486); Construct the embedded
    // body; seed mfMass = 5.0 (the lvlx/vspltw of the 5.0 stack temp into +208); Prepare the body;
    // then zero the four packed joint/graphics/COM/collision Vector3Plus rows (+368/+384/+400/+432/
    // +448 -- the five stvx128 of the zero vector). The mRigidBodyId word at +464 is seeded from the
    // static qword_82F2A3A8 (an invalid-id constant).
    // =========================================================================================
    void PhysicalBodyPart::Construct()
    {
        // *(this+464) = qword_82F2A3A8 ; the stvx128 v127(=0) at +HIDWORD(qword) clears the high half.
        // qword_82F2A3A8 is the "invalid body-part id" seed (concrete value rodata-not-recovered;
        // modelled as the all-ones invalid handle the BurnoutBodyPartID family uses).
        mRigidBodyId.muEntityWord = 0xFFFFFFFFu;   // FLAG: qword_82F2A3A8 seed -> invalid id
        mRigidBodyId.muSubA = 0u;
        mRigidBodyId.muSubB = 0u;

        mpIKPart           = 0;       // *(this+476) = 0
        mpDeformableObject = 0;       // *(this+480) = 0
        mbAddedToScene     = false;   // *(this+485) = 0
        mbFrozen           = false;   // *(this+486) = 0

        // BrnPhysics::ExternalPhysicsBody::Construct() on the embedded body.
        mRwBody.Construct();

        // v14 = 5.0 ; lvlx/vspltw v0 ; stvx128 v0 -> this+208  (mfMass = 5.0, broadcast).
        // mfMass lives inside mRwBody (+208 in the console layout). The asm stores 5.0 BETWEEN
        // ExternalPhysicsBody::Construct() (above) and ::Prepare() (below). KF_PART_MASS = 5.0f is the
        // recovered literal (the lvlx/vspltw 5.0 stack temp), not a placeholder.
        mRwBody.SetMass(KF_PART_MASS);

        // BrnPhysics::ExternalPhysicsBody::Prepare().
        mRwBody.Prepare();

        // Zero the four packed Vector3Plus rows + the collision accumulator (the five stvx128 v127):
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
