#pragma once

// BrnPhysics::Deformation::BurnoutBodyPartID / ::BurnoutWheelBodyID -- the deformation-side
// rigid-body handles for a body part / detached wheel of a vehicle.
//
// DWARF: both derive from CgsPhysics::RigidBodyId (BrnPhysicalBodyPart.h:47
// `struct BurnoutBodyPartID : RigidBodyId`, BrnPhysicalWheel.h:48 `struct BurnoutWheelBodyID :
// RigidBodyId`), i.e. they ARE the 64-bit packed id: the owning part's EntityId in the high dword,
// UserIDA (the deformable-object index) at bits 16..31 and UserIDB (the pool slot) at bits 0..15.
// ARTIST reads and writes them whole (`ld`/`std` at part+0x1D0 / wheel+0x70) and builds them
// through RigidBodyId::Set (BurnoutBodyPartID::Set 0x825C1A10, BurnoutWheelBodyID::Set
// 0x825C1D40). Both types live in their DWARF homes' headers in the original; they share this
// header here.
//
// OWNER TAG: Set derives the part's owner from the owning vehicle's EntityId owner byte:
//   BurnoutBodyPartID : race car (1) -> RACECAR_DEFORMABLE_PART (6), else TRAFFIC_DEFORMABLE_PART (7)
//   BurnoutWheelBodyID: race car (1) -> DETACHED_RACECAR_WHEEL (9), else (asserted traffic vehicle,
//                       BrnPhysicalWheel.h:244) DETACHED_TRAFFIC_WHEEL (10)
// The entity / part index bounds asserts are EntityId::Set's own (CgsEntityId.h:116/117).

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"   // CgsPhysics::RigidBodyId (the DWARF base)
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"

namespace BrnPhysics
{
namespace Deformation
{
    // Shared packed-field geometry for the embedded 32-bit EntityId word (8/14/10), used by the
    // consumers that rebuild an EntityId from a vehicle id (PhysicalBodyPart::AddContactSpy).
    struct BurnoutBodyPartIDLayout
    {
        static const u32 KU_NUM_BITS_FOR_ENTITY_NUM = 14;
        static const u32 KU_NUM_BITS_FOR_PART_NUM   = 10;
        static const u32 KU_ENTITY_INDEX_BASE       = 10;
        static const u32 KU_OWNER_BASE              = 24;

        // Owning-vehicle owner bytes the Set bodies branch on (X360-authoritative).
        static const u32 KU_OWNER_PLAYER_VEHICLE    = 1;  // BrnWorld::E_ENTITYTYPE_RACECAR
        static const u32 KU_OWNER_TRAFFIC_VEHICLE   = 2;  // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE
    };

    // DWARF BrnPhysicalBodyPart.h:47.
    struct BurnoutBodyPartID : public CgsPhysics::RigidBodyId
    {
        // Deformation owner tags written into the embedded EntityId word.
        static const u32 KU_OWNER_PLAYER_BODY_PART  = 6;   // BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART
        static const u32 KU_OWNER_TRAFFIC_BODY_PART = 7;   // BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART

        BurnoutBodyPartID() {}

        // h:55 -- out of line at 0x825C1A10 (BrnBurnoutBodyPartID.cpp).
        void Set(CgsSceneManager::EntityId lOwningVehicleID, u16 lu16PartIndex,
                 u16 lu16DeformableObjectIndex, u16 lu16PartPoolIndex);

        // h:59 -- the whole id.
        void Set(u64 lu64Id) { RigidBodyId::Set(lu64Id); }

        // h:106 -- adopt a plain RigidBodyId (DeformationManager::CreateDetachedPartContactEvent
        // builds its DWARF local lBodyId, cpp:1943, from the contact spy's mIDA this way).
        const BurnoutBodyPartID& operator=(const CgsPhysics::RigidBodyId& lrId)
        {
            Set(lrId.GetRawId());
            return *this;
        }

        // h:62 -- the owner tag the deformation consumers branch on (`ld; srdi 32; srwi 24`).
        u8 GetOwner() const { return GetEntityIDOwner(); }

        // h:78 -- the IK part index the Set packs into the entity word's part field
        // (PhysicalBodyPart::AddContactSpy 0x8260B99C `clrlwi r6, r11, 22`).
        u16 GetIKPartIndex() const { return GetEntityId().GetPartIndex(); }

        // h:82 / h:86 -- the two user ids.
        u16 GetDeformableObjectIndex() const { return GetUserIDA(); }
        u16 GetPartPoolIndex() const { return GetUserIDB(); }

        // h:94 -- the whole handle as its base id. PhysicalBodyPartPool::UpdatePart's assert
        // (0x8260CB08, BrnPhysicalBodyPartPool.cpp:173) names it:
        //   "maParts[ lu16PartIndex ].GetRigidBodyId().GetBaseRigidBodyID() == lpUpdateEvent->mID"
        CgsPhysics::RigidBodyId GetBaseRigidBodyID() const { return *this; }
    };

    // DWARF BrnPhysicalWheel.h:48.
    struct BurnoutWheelBodyID : public CgsPhysics::RigidBodyId
    {
        static const u32 KU_OWNER_PLAYER_WHEEL  = 9;    // BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL
        static const u32 KU_OWNER_TRAFFIC_WHEEL = 10;   // BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL

        BurnoutWheelBodyID() {}

        // h:56 -- out of line at 0x825C1D40 (BrnBurnoutWheelBodyID.cpp).
        void Set(CgsSceneManager::EntityId lOwningVehicleID, u16 lu16WheelIndex,
                 u16 lu16DeformableObjectIndex, u16 lu16PartPoolIndex);
        // h:60.
        void Set(u64 lu64Id) { RigidBodyId::Set(lu64Id); }

        // h:80 / h:84 -- the two user ids.
        u16 GetDeformableObjectIndex() const { return GetUserIDA(); }
        u16 GetPartPoolIndex() const { return GetUserIDB(); }

        // h:92.
        CgsPhysics::RigidBodyId GetBaseRigidBodyID() const { return *this; }

        // h:104 -- adopt a plain RigidBodyId (DetachedWheelManager::UpdatePostPhysics builds its
        // DWARF local lBodyId, cpp:148, from the updated body's mID this way).
        const BurnoutWheelBodyID& operator=(const CgsPhysics::RigidBodyId& lrId)
        {
            Set(lrId.GetRawId());
            return *this;
        }
    };
}
}
