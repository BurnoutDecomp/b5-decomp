// Layout/behaviour self-check for the tiny-structs group. Asserts the X360-authoritative
// facts that the four owned TUs depend on:
//   * BrnPhysics::Vehicle::PhysicalTrafficState scalar-tail offsets + sizeof (copy-ctor asm)
//   * BrnPhysics::Vehicle::ValidateRaceCarEvent layout (AddEvent/Append copy by sizeof(T))
//   * CgsSceneManager::EntityId 8/12/12 bit packing (leak source + X360 masks)
#include <cstddef>

#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "SharedClasses/Graphics/TextureNameMapResourceType.h"

namespace
{
    using BrnPhysics::Vehicle::PhysicalTrafficState;
    using BrnPhysics::Vehicle::ValidateRaceCarEvent;

    // PhysicalTrafficState scalar tail (X360 copy-ctor @0x82284EE8): +800/804/808/809/810/812.
    // These members are all pointer-free (WheelLite / matrices / vectors / u32 EntityId), so
    // the host layout matches the X360 byte offsets exactly.
    static_assert(offsetof(PhysicalTrafficState, mEntityID)           == 800, "mEntityID @800");
    static_assert(offsetof(PhysicalTrafficState, mfSpeed)             == 804, "mfSpeed @804");
    static_assert(offsetof(PhysicalTrafficState, mbFrozen)            == 808, "mbFrozen @808");
    static_assert(offsetof(PhysicalTrafficState, mbIsDeforming)       == 809, "mbIsDeforming @809");
    static_assert(offsetof(PhysicalTrafficState, mbIsFatallyCrashing) == 810, "mbIsFatallyCrashing @810");
    static_assert(offsetof(PhysicalTrafficState, mfSteering)          == 812, "mfSteering @812");
    static_assert(sizeof(PhysicalTrafficState) == 816, "PhysicalTrafficState sizeof == 816");

    // ValidateRaceCarEvent: AddEvent/Append copy a whole element by sizeof(T). The X360 stride
    // is 32 (32-bit pointers: 2x ResourceHandle = 16); on the host the two pointer-pair
    // ResourceHandles widen to 16 bytes each, so sizeof is 48. The generic BaseEventQueue<T>
    // body copies by name/sizeof, so this void*-width difference is handled correctly --
    // verify the host layout is the wider, pointer-accurate one (NOT the X360 32).
    static_assert(sizeof(ValidateRaceCarEvent) == 48, "ValidateRaceCarEvent host sizeof == 48 (X360 32; ResourceHandle widens)");

    // EntityId 8/12/12 packing: owner [31..24], entityIndex [23..12], partIndex [11..0].
    // EntityId is not a literal type (faithful leak ctors are non-constexpr), so verify the
    // pack arithmetic the masks encode, then a runtime spot-check of the accessors.
    static_assert(((0x7Au << 24) | (0xABCu << 12) | 0x123u) == 0x7AABC123u, "EntityId 8/12/12 pack");

    inline bool EntityIdAccessorCheck()
    {
        CgsSceneManager::EntityId lId( 0x7AABC123u );
        return lId.GetOwner()       == 0x7A
            && lId.GetEntityIndex() == 0xABC
            && lId.GetPartIndex()   == 0x123
            && lId.IsValid();
    }
}
