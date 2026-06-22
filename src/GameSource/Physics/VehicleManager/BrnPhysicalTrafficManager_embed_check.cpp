// Embed/ODR check for the PhysicalTrafficManager TU: include the owning header and exercise
// the public surface so the class instantiates and the member templates (CreateIOBuffer<>) and
// container instantiations (BitArray<20>, EventQueue<s8,50>, Array<EntityId,20>) compile here.
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"

namespace
{
void EmbedCheck()
{
    using namespace BrnPhysics::Vehicle;

    PhysicalTrafficManager lManager;

    EntityId lId;
    lId.muValue = (3u << 10) | (2u << 24);

    volatile bool lbSimple = lManager.IsTrafficVehicleSimple(lId);
    (void)lbSimple;

    volatile EntityId lPhys = lManager.GetPhysicsEntityId(0);
    (void)lPhys;

    volatile EntityId lGlobal = lManager.GetGlobalTrafficEntityId(0);
    (void)lGlobal;

    lManager.ResetAboveGroundTestResults();

    // sizeof sanity: the manager embeds 20 full-physics blocks of 0x1430 each, so it is large.
    static_assert(sizeof(PhysicalTrafficManager) > sizeof(TrafficPhysics) * 19,
                  "PhysicalTrafficManager must embed the full-physics array");
}
}
