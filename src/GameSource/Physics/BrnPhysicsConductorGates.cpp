// =================================================================================================
// GameSource/Physics/BrnPhysicsConductorGates.cpp -- boot gates for PhysicsModule::Update callees
// that have no body in the tree. Each one is reached every frame, so it logs once and then is
// inert. Reconstruct the body in its own TU and delete the gate here (LNK2005 is the tripwire).
// =================================================================================================

#include "GameSource/Physics/BrnPhysicsModule.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"
#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags

namespace
{
    inline void GateLogOnce(bool& lrbLogged, const char* lpcMessage)
    {
        if (!lrbLogged)
        {
            lrbLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }
}

#define BRN_CONDUCTOR_GATE(TAG)                                                            \
    do { static bool s_bLogged = false;                                                    \
         GateLogOnce(s_bLogged, "conductor gate: " TAG " inert [FLAG PC boot gate]\n"); } while (0)

namespace BrnPhysics
{

namespace Vehicle
{
    // No body anywhere in the tree: no export JSON at its address and no name-index hit
    // (export hole). Declared in BrnVehicleManager.h; called from PhysicsModule::Update.
    void VehicleManager::ProcessCrashingNetworkCars(
        const VehicleDriverInputInterface*, BrnPhysics::Vehicle::VehicleOutputRequestInterface*,
        VehicleManagerOutputInterface*, BrnPhysics::Deformation::DeformationInputInterface*,
        VehicleOutputInterface*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::ProcessCrashingNetworkCars (export hole)");
    }
}

}
