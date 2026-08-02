#pragma once

#include "types.hpp"
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"     // PropInstance, PropEntityID
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h" // PropPartInstance
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                  // CgsContainers::BitArray

namespace CgsPhysics { namespace PhysicsSimulationIO { struct InAddPotentialContact; } }
namespace CgsMemory { struct SimpleDataStreamProducer; }   // pointer-only member (mpPrimitiveWithTriangleStream)

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

        // ---- ADDITIVE GROW (perf-monitor slice; DWARF BrnPropManager.h:266..277, in
        //      declaration order, immediately after muNumberOfPartInstances) --------------------
        // These eight members are the ones ConstructPreScenePerfMonitors /
        // ConstructContactGenerationPerfMonitors / Construct write, and their X360 offsets form a
        // gap-free run straight out of the DWARF sequence, which is what makes the
        // offset->name mapping PROVEN rather than proposed:
        //
        //   +0x90 mUsedParts (BitArray<30>, 8B) .. +0x98 muNumberOfPartInstances (already
        //   committed, and both are written by Construct: `std r30,0x90(r31)`)
        //   +0x9C  miNumJobsAdded                    (BeginPropWorldContactGeneration @0x82628CFC
        //                                             does `stw r26,0x9C(r31)` with r26 == 0)
        //   +0xA0  mpPrimitiveWithTriangleStream     (Construct: `stw r30,0xA0(r31)`, r30 == 0;
        //                                             BeginPropWorldContactGeneration then stores
        //                                             a stream producer there)
        //   +0xA4  miContactGeneratorWaitPM          (ConstructContactGenerationPerfMonitors --
        //                                             the ONLY store that function makes, and the
        //                                             function's own name names the member)
        //   +0xA8  miProcessRemovePropPM             \
        //   +0xAC  miProcessRemovePartPM              |  the four ConstructPreScenePerfMonitors
        //   +0xB0  miProcessAddPropInstancePM         |  zeroes (stores in the order B0,B4,A8,AC)
        //   +0xB4  miProcessAddPartInstancePM        /
        //   +0xB8  miProcessBreakPropPM              (written by NEITHER constructor -- stated as a
        //                                             fact of the asm, not smoothed over)
        //
        // The run then continues at +0xC0 with maPropJointPositions[15] (16-aligned Vector3s),
        // and the rest of the DWARF sequence lands gap-free all the way to
        // miNumPropsAddedToContactGen at +0x6570 -- see the layout table in BrnPropManager.cpp.
        s32                          miNumJobsAdded;
        CgsMemory::SimpleDataStreamProducer* mpPrimitiveWithTriangleStream;
        s32                          miContactGeneratorWaitPM;
        s32                          miProcessRemovePropPM;
        s32                          miProcessRemovePartPM;
        s32                          miProcessAddPropInstancePM;
        s32                          miProcessAddPartInstancePM;
        s32                          miProcessBreakPropPM;

        // X360 0x825BAC60 (DWARF BrnPropManager.h). Called by PhysicsModule::Construct
        // @0x825AE308 on the embedded mPropManager. In the shipped ARTIST image this is four
        // instructions: `li r11,0 ; stw r11,0xA4(r3) ; blr`. Defined in BrnPropManager.cpp.
        void ConstructContactGenerationPerfMonitors();

        // X360 0x825BAC70. Six instructions: zero the four pre-scene process-event monitor ids.
        // Defined in BrnPropManager.cpp.
        void ConstructPreScenePerfMonitors();

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
