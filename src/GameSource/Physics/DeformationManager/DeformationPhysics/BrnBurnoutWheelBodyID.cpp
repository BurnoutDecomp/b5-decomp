#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnBurnoutBodyPartID.h"
#include "GameSource/World/BrnEntityTypes.h"   // BrnWorld::E_ENTITYTYPE_*

// The header this Set is inline in on the original (ARTIST string 0x820937E0).
#define BRN_PHYSICALWHEEL_H_FILE "..\\..\\..\\GameSource\\Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h"

namespace BrnPhysics
{
namespace Deformation
{
    // BurnoutWheelBodyID::Set @ 0x825C1D40 (DWARF BrnPhysicalWheel.h:56, header-inline in the
    // original; ARTIST keeps this copy for DetachedWheelManager::DetachWheel). As
    // BurnoutBodyPartID::Set with the detached-wheel owners; a non-race-car owner must be a
    // traffic vehicle (assert BrnPhysicalWheel.h:244, 0x825C1DE0).
    void BurnoutWheelBodyID::Set(CgsSceneManager::EntityId lOwningVehicleID, u16 lu16WheelIndex,
                                 u16 lu16DeformableObjectIndex, u16 lu16PartPoolIndex)
    {
        if (lOwningVehicleID.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR)
        {
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set(BrnWorld::E_ENTITYTYPE_DETACHED_RACECAR_WHEEL,
                          lOwningVehicleID.GetEntityIndex(), lu16WheelIndex);
            RigidBodyId::Set(lEntityId, lu16DeformableObjectIndex, lu16PartPoolIndex);
        }
        else
        {
            CGS_ASSERT_AT(lOwningVehicleID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE,
                          "lOwningVehicleID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE",
                          BRN_PHYSICALWHEEL_H_FILE, 244);
            CgsSceneManager::EntityId lEntityId;
            lEntityId.Set(BrnWorld::E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL,
                          lOwningVehicleID.GetEntityIndex(), lu16WheelIndex);
            RigidBodyId::Set(lEntityId, lu16DeformableObjectIndex, lu16PartPoolIndex);
        }
    }
}
}
