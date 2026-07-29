#pragma once

// =============================================================================
// BrnTrafficLightCollection.h  (NEW OWNING HEADER)
//
// Home for BrnTraffic::TrafficLightType and BrnTraffic::TrafficLightCollection --
// the traffic-light instance tables embedded BY VALUE in TrafficData
// (TrafficData::mTrafficLights, DWARF BrnTrafficData.h:85).
//
// WHY IT EXISTS NOW: TrafficData used to carry this sub-aggregate as an opaque
// `u8 mTrafficLightsOpaque[300]` sized to the X360 footprint. That is wrong on the
// x64 host in two ways -- the collection holds EIGHT serialised pointer slots that
// TrafficData::FixUp relocates (X360 @0x827637D8 inlines TrafficLightCollection::FixUp
// and rebases +0x44/+0x48/+0x4C/+0x50/+0x54/+0x58/+0x160/+0x164, i.e. this struct's
// +0x08..+0x1C and +0x124/+0x128), and widening those slots moves every member after
// them. Modelling it opaquely would leave the eight lane-graph pointers unrelocated
// and put muNumPaintColours at the wrong offset.
//
// LAYOUT is DWARF-authoritative
// (references/DecFIGS/dwarfdump/SharedClasses/Traffic/Junctions/BrnTrafficLightCollection.h,
// struct @ :78, members :170-187). X360 (4-byte pointer) offsets:
//   +0x000 muNumTrafficLights            u16   :170
//   +0x002 muNumTrafficLightTypes        u16   :171
//   +0x004 muNumCoronas                  u16   :172
//   +0x008 mpaPosAndYRotations           P     :175  Vector3Plus*
//   +0x00C mpaInstanceIDs                P     :176  u32*
//   +0x010 mpauInstanceTypes             P     :177  u8*
//   +0x014 mpaTrafficLightTypes          P     :180  TrafficLightType*
//   +0x018 mpaCoronaTypes                P     :181  u8*
//   +0x01C mpaCoronaPositions            P     :182  Vector3*
//   +0x020 mauInstanceHashOffsets[129]   u16   :185
//   +0x124 mpauInstanceHashTable         P     :186  u32*
//   +0x128 mpauInstanceHashToIndexLookup P     :187  u16*
//   sizeof == 0x12C == 300  -- which is exactly the span TrafficData reserves between
//   mTrafficLights (+0x3C) and muNumPaintColours (+0x168), so the layout is pinned
//   from both ends.
//
// x64 NOTE: the eight pointers widen 4->8, so sizeof becomes 0x150 == 336. Members are
// reached BY NAME; the static_asserts below pin the host offsets that the serialised
// lane data (tools/assets/bundles/lane_transcode.py) is written to.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus
#include <cstddef>            // offsetof

namespace BrnTraffic
{
    // BrnTrafficLightCollection.h:60 -- one light "type" is a run of coronas inside the
    // shared corona tables.
    struct TrafficLightType
    {
        u8 muCoronaOffset;   // +0 (:63)
        u8 muNumCoronas;     // +1 (:64)
    };

    // BrnTrafficLightCollection.h:78 -- every traffic light instance in the city.
    struct TrafficLightCollection
    {
        u16 muNumTrafficLights;       // +0x00 (:170)
        u16 muNumTrafficLightTypes;   // +0x02 (:171)
        u16 muNumCoronas;             // +0x04 (:172)

        // ---- the eight relocated slots (TrafficData::FixUp rebases all of them) ----
        Vector3Plus*      mpaPosAndYRotations;            // (:175)  per-instance pos + Y rotation
        u32*              mpaInstanceIDs;                 // (:176)
        u8*               mpauInstanceTypes;              // (:177)  index into mpaTrafficLightTypes
        TrafficLightType* mpaTrafficLightTypes;           // (:180)
        u8*               mpaCoronaTypes;                 // (:181)
        Vector3*          mpaCoronaPositions;             // (:182)

        // KU_INSTANCE_ID_HASH_TABLE_SIZE == 129 (:168; mask 127 at :167).
        static const u32 KU_INSTANCE_ID_HASH_MASK       = 127;
        static const u32 KU_INSTANCE_ID_HASH_TABLE_SIZE = 129;
        u16 mauInstanceHashOffsets[KU_INSTANCE_ID_HASH_TABLE_SIZE];   // (:185)

        u32* mpauInstanceHashTable;            // (:186)
        u16* mpauInstanceHashToIndexLookup;    // (:187)

        u32 GetNumTrafficLights() const { return muNumTrafficLights; }   // (:82)

        // Load-time pointer relocation of the eight slots above. The X360 has no
        // standalone symbol for these -- TrafficData::FixUp/FixDown @0x827637D8 /
        // @0x82763CB8 inline them -- so they are de-inlined here to the DWARF-declared
        // members (:126 / :131) and called from TrafficData's bodies.
        void FixUp(const void* lpBaseData);     // (:126)
        void FixDown(const void* lpBaseData);   // (:131)
    };

    // Host layout pinned against tools/assets/bundles/lane_transcode.py's emitter.
    static_assert(sizeof(TrafficLightType) == 2, "TrafficLightType stride");
    static_assert(offsetof(TrafficLightCollection, mpaPosAndYRotations) == 0x08,
                  "TrafficLightCollection::mpaPosAndYRotations");
    static_assert(offsetof(TrafficLightCollection, mpaCoronaPositions) == 0x30,
                  "TrafficLightCollection::mpaCoronaPositions");
    static_assert(offsetof(TrafficLightCollection, mauInstanceHashOffsets) == 0x38,
                  "TrafficLightCollection::mauInstanceHashOffsets");
    static_assert(offsetof(TrafficLightCollection, mpauInstanceHashTable) == 0x140,
                  "TrafficLightCollection::mpauInstanceHashTable");
    static_assert(offsetof(TrafficLightCollection, mpauInstanceHashToIndexLookup) == 0x148,
                  "TrafficLightCollection::mpauInstanceHashToIndexLookup");
    static_assert(sizeof(TrafficLightCollection) == 0x150, "TrafficLightCollection host sizeof");
}
