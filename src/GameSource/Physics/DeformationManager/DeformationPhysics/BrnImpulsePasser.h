#pragma once

// BrnPhysics::Deformation::ImpulsePasser -- the small fixed map of collidable bodies an
// impulse can be passed on through (the deformation system's "chain" mechanism: when a
// sensor receives an impulse it may forward a scaled remainder to neighbouring bodies via
// this map). Homed at its mirrored DWARF path
// (references/DecFIGS/dwarfdump/.../DeformationPhysics/BrnImpulsePasser.h:53).
//
// EMBEDDED BY VALUE in DeformableObject (mImpulsePasser), so its full layout is needed for
// DeformableObject's member sequence -- this is its real DWARF layout, not a placeholder.
//
// LAYOUT (DWARF BrnImpulsePasser.h:85): a single fixed-size array of CollidableBody pointers,
// one slot per collidable body the owning object can chain through. The bound 25 matches
// KI_MAX_NUM_COLLIDABLE_BODIES_PER_DEFORMABLE_OBJECT (BrnDeformationConstants.h:48). On the
// 64-bit host the pointer array widens vs the 32-bit console; members are pinned BY NAME, not
// raw offset, so the absolute size is not load-bearing.
//
// All methods are DECLARED-ONLY here: their bodies are owned by the BrnImpulsePasser TU.

#include "types.hpp"
#include "BrnCommonTypes.h"   // VecFloat
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnCollidableBody.h"  // CollidableBody, ImpulseParams

namespace BrnPhysics
{
namespace Deformation
{
    struct ImpulsePasser
    {
        // Slots per owning object's collidable-body chain. DWARF maps the array as [25];
        // matches KI_MAX_NUM_COLLIDABLE_BODIES_PER_DEFORMABLE_OBJECT.
        static const s32 KI_MAX_COLLIDABLE_BODIES = 25;

        // ---- lifecycle (DWARF :56-65) ----------------------------------------------------
        void Construct();   // DWARF :56
        void Destruct();    // DWARF :59
        bool Prepare();     // DWARF :62
        bool Release();     // DWARF :65

        // DWARF :70. Bind slot liIndex of the chain map to a collidable body.
        void SetCollidableBodyMap(s32 liIndex, CollidableBody* lpBody);

        // DWARF :76. Pass an impulse on to the body at slot luIndex with the chain's own
        // magnitude (forwards to that body's RecievePassedOnImpulse).
        // ⭐ THE PARAMETER NAME IS THE DWARF'S (BrnImpulsePasser.cpp:154 declares
        // `VecFloat lvfHierarchicalImpulse`) and it is load-bearing, not cosmetic. This value is
        // the HIERARCHY's quantity -- `absorbed * 0.5f`, computed by DeformationSensor::
        // ApplyLocalImpulse -- and it is what a downstream SENSOR dents by. It is NOT the impulse a
        // rigid body applies: VehicleRigidBody::RecievePassedOnImpulse ignores it entirely and takes
        // lpImpulseParams->mvfImpulseMagnitude instead (`vmulfp128 v1, v13, v0` @0x8260E008
        // overwrites the argument register before any read). Calling it "the passed magnitude"
        // invited exactly the misreading that produced fdfda858's "45 kN.s while absorption is zero
        // contradicts the chain" -- it does not; see BrnVehicleRigidBody.cpp's banner.
        void PassOnImpulse(u8 lu8Index, const ImpulseParams* lpImpulseParams,
                           VecFloat lvfHierarchicalImpulse);

        // DWARF :81. Apply one impulse to a specific collidable body directly.
        void ApplyLocalImpulse(CollidableBody* lpBody, ImpulseParams* lpImpulseParams);

    private:
        // DWARF :85. The chain map: one CollidableBody* per chain slot.
        CollidableBody* mapCollidableBodies[KI_MAX_COLLIDABLE_BODIES];

        // DWARF :88. Zero the map (called by Construct).
        void ClearVariables();
    };
}
}
