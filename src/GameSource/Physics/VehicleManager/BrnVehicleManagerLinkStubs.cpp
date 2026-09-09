// =================================================================================================
// BrnVehicleManagerLinkStubs.cpp -- boot gate for the one VehicleManager::UpdateVehiclePhysics
// callee that still has no body in the tree. It is reached per live car per frame, so it logs
// once and then is inert. Reconstruct the body in its own TU and delete the gate (LNK2005 is the
// tripwire).
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h"
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"
#include "GameSource/AttribSys/Generated/classes/physicssurface.h"
#include "GameSource/AttribSys/Generated/classes/gameplaysurface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint / gxMessageFilterFlags (the boot gates)

#include <cmath>

namespace BrnPhysics
{
namespace Vehicle
{
    // DebugComponent::Update(f32): no body in the tree. Its home TU
    // B5PhysicsHandlingDebugComponent.cpp is mounted but defines only GetPath and
    // SetLastWallTriangle; reconstruct Update there and delete this gate.
    void DebugComponent::Update(f32)
    {
        static bool sbLogged = false;
        if (!sbLogged)
        {
            sbLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "conductor gate: BrnPhysics::Vehicle::DebugComponent::Update -- per-car "
                       "debug tick inert (reconstruct with the B5PhysicsHandlingDebugComponent "
                       "pass) [FLAG PC boot gate]\n";
        }
    }
}
}
