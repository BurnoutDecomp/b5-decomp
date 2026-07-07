#pragma once

#include "types.hpp"

namespace CgsPhysics { namespace PhysicsSimulationIO { struct InAddPotentialContact; } }

namespace BrnPhysics
{
namespace Props
{
    class PropManager
    {
    public:
        bool mbRenderCentreOfMass;
        bool mbDisableFreezing;

        // X360 0x825BACB0 (private prop/race-car helper; DWARF BrnPropManager.h:311).
        // Retargets the RACE-CAR side of a prop/race-car potential contact onto the shared
        // "dummy" race car by overwriting that RigidBodyId's EntityId owner-type with the
        // dummy-car owner (11). Does not touch PropManager state (no `this` use), so it is
        // reconstructable ahead of the still-blocked PropManager layout. Defined out-of-line
        // in BrnPropManager_RoutePropVsRaceCarContactToDummyCar.cpp.
        void RoutePropVsRaceCarContactToDummyCar(
            bool                                             lbPropIsEntityA,
            CgsPhysics::PhysicsSimulationIO::InAddPotentialContact* lpOutContact );
    };
}
}
