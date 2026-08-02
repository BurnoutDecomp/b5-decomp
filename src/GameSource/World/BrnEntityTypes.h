#pragma once

// Canonical home for BrnWorld::EEntityTypeID -- the engine-wide "what kind of thing is this
// entity" tag (DWARF home GameSource/World/BrnEntityTypes.h:36).
//
// Until now three headers carried the opaque-enum declaration
//     namespace BrnWorld { enum EEntityTypeID : int; }
// with a note that the canonical home was "not yet reconstructed in-tree"
// (BrnContactSpyQueue.h, BrnContactSpyRunList.h, BrnDetachedWheelManager.h). This file is that
// home. The enumerator list is VERBATIM from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/World/BrnEntityTypes.h) -- no value is inferred.
//
// The underlying type is spelled `: int` so this definition is compatible with the existing
// fixed-underlying-type opaque declarations (C++ requires the definition to repeat the same
// fixed underlying type). `int` is also what the DWARF implies: E_ENTITYTYPE_INVALID == -1
// forces a signed underlying type.
//
// Cross-checks against already-committed code, all of which agree:
//   * ContactSpyQueue<T,N>::DebugGetEntityTypeName (X360 0x8259E1F0/0x8259E308/0x8259E428)
//     maps 0 -> "world", 1 -> "race car", 2 -> "traffic vehicle", 3 -> "prop",
//     6 -> "race car deformable part", 7 -> "traffic deformable part".
//   * BrnPropEntityID.h bakes the prop owner byte as 3.
//   * BrnBurnoutBodyPartID.h bakes KU_OWNER_TRAFFIC_VEHICLE == 2.
//   * BrnPropManager::RoutePropVsRaceCarContactToDummyCar retargets onto owner 11
//     (== E_ENTITYTYPE_PROP_COLLISION_RACECAR).

namespace BrnWorld
{
    // DWARF: BrnEntityTypes.h:36.
    enum EEntityTypeID : int
    {
        E_ENTITYTYPE_INVALID                 = -1,
        E_ENTITYTYPE_WORLD                   = 0,
        E_ENTITYTYPE_RACECAR                 = 1,
        E_ENTITYTYPE_TRAFFIC_VEHICLE         = 2,
        E_ENTITYTYPE_PROP                    = 3,
        E_ENTITYTYPE_TRIGGER                 = 4,
        E_ENTITYTYPE_WORLD_GRAPHICS          = 5,
        E_ENTITYTYPE_RACECAR_DEFORMABLE_PART = 6,
        E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART = 7,
        E_ENTITYTYPE_RACECAR_WHEEL           = 8,
        E_ENTITYTYPE_DETACHED_RACECAR_WHEEL  = 9,
        E_ENTITYTYPE_DETACHED_TRAFFIC_WHEEL  = 10,
        E_ENTITYTYPE_PROP_COLLISION_RACECAR  = 11,
        E_ENTITYTYPE_PROP_COLLISION_TRAFFIC  = 12,
        E_ENTITYTYPE_FIRST_REPLAY_TYPE       = 32,
        E_ENTITYTYPE_REPLAY_RACECAR          = 33,
        E_ENTITYTYPE_COUNT                   = 34,
    };
}
