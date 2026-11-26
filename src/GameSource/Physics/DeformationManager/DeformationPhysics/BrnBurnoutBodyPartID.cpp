#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnBurnoutBodyPartID.h"
#include "GameSource/World/BrnEntityTypes.h"   // BrnWorld::E_ENTITYTYPE_*

// The packed ids are the 8-byte RigidBodyId (`std` to this+0).
CGS_STATIC_ASSERT((sizeof(BrnPhysics::Deformation::BurnoutBodyPartID) == 8),  "BurnoutBodyPartID == 8 bytes");
CGS_STATIC_ASSERT((sizeof(BrnPhysics::Deformation::BurnoutWheelBodyID) == 8), "BurnoutWheelBodyID == 8 bytes");

namespace BrnPhysics
{
namespace Deformation
{
    // BurnoutBodyPartID::Set @ 0x825C1A10 (DWARF BrnPhysicalBodyPart.h:55, header-inline in the
    // original; ARTIST keeps this copy for PhysicalBodyPartPool::Create). The part's EntityId takes
    // the deformable-part owner chosen by the vehicle's owner byte, the vehicle's entity index and
    // the part index (EntityId::Set, asserts CgsEntityId.h:116/117 in each arm); RigidBodyId::Set
    // then stores it with the two user ids.
    void BurnoutBodyPartID::Set(CgsSceneManager::EntityId lOwningVehicleID, u16 lu16PartIndex,
                                u16 lu16DeformableObjectIndex, u16 lu16PartPoolIndex)
    {
        if (lOwningVehicleID.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR)
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set(BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART,
                          lOwningVehicleID.GetEntityIndex(), lu16PartIndex);
            RigidBodyId::Set(lEntityId, lu16DeformableObjectIndex, lu16PartPoolIndex);
        }
        else
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set(BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART,
                          lOwningVehicleID.GetEntityIndex(), lu16PartIndex);
            RigidBodyId::Set(lEntityId, lu16DeformableObjectIndex, lu16PartPoolIndex);
        }
    }
}
}
