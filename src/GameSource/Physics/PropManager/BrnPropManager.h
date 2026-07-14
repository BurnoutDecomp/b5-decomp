#pragma once

#include "types.hpp"
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"     // PropInstance, PropEntityID
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h" // PropPartInstance
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                  // CgsContainers::BitArray

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

        // ADDITIVE GROW (prop/part instance-query slice). The three const-ish query getters
        // below (FindPropIndex / HasPropJustBeenRemoved / HasPartJustBeenRemoved) only touch
        // the PropManager-owned instance arrays and their "used" bit-sets -- never the
        // still-ungroundable BaseCollisionGenerator base sub-object, the event queues, or the
        // physics/IO dependency family that keeps the rest of BrnPropManager.cpp blocked. So
        // this slice can be reconstructed on its own (mirroring the RoutePropVsRaceCarContact-
        // ToDummyCar split-out). The member NAMES + types are the DecFIGS DWARF layout for this
        // path (references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.h:98-113):
        //   mpaPropInstances @X360+0x7C, mUsedProps @+0x80, muNumberOfPropInstances @+0x88,
        //   mpaPartInstances @+0x8C, mUsedParts @+0x90, muNumberOfPartInstances @+0x98 -- and the
        //   X360 asm of the three getters attests each stride/offset (PropInstance stride 112 with
        //   mEntityId @+0x60; PropPartInstance stride 64 with mEntityId @+0x30; the two BitArrays
        //   read at this+0x80 / this+0x90). Only these six DWARF members are added (the preceding
        //   base + shell members remain unreconstructed, so absolute host offsets are NOT pinned;
        //   the bodies are by-name and behaviour-faithful, matching the split-out precedent).
        PropInstance*                mpaPropInstances;
        CgsContainers::BitArray<15>  mUsedProps;
        u32                          muNumberOfPropInstances;
        PropPartInstance*            mpaPartInstances;
        CgsContainers::BitArray<30>  mUsedParts;
        u32                          muNumberOfPartInstances;

        // X360 0x82606148 (DWARF BrnPropManager.h:250). Linear-scan the used-prop bit-set;
        // return the slot whose stored PropEntityID matches, else -1.
        int32_t FindPropIndex( PropEntityID lEntityId ) const;

        // X360 0x825DEAC0 (DWARF :314). True iff the prop slot is now free OR holds a
        // different entity than lEntityId (i.e. the prop was removed/recycled this frame).
        bool HasPropJustBeenRemoved( PropEntityID lEntityId, int32_t liPropIndex );

        // X360 0x825DE930 (DWARF :317). Part-instance analogue of HasPropJustBeenRemoved.
        bool HasPartJustBeenRemoved( PropEntityID lEntityId, int32_t liPartIndex );
    };
}
}
