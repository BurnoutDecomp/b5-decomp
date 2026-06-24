#pragma once

// BrnPhysics::Deformation::BurnoutBodyPartID / ::BurnoutWheelBodyID — 64-bit packed
// deformation-side handles for a body part / detached wheel of a vehicle. Reconstructed
// from the X360 ARTIST spine (BurnoutBodyPartID::Set @0x825C1A10, BurnoutWheelBodyID::Set
// @0x825C1D40 are authoritative on layout). There is no DecFIGS DWARF for these two types,
// so the member NAMES below are best-effort labels for the asm-attested fields and are
// FLAGGED as such; the bit layout / store widths are recovered store-for-store.
//
// PACKED LAYOUT (X360 big-endian, authoritative). The Set bodies build a 64-bit word
// (`std` to this+0) plus two 16-bit halfwords spliced into its low dword:
//   high dword  (this+0, bytes 0..3) == a 32-bit EntityId word:
//       owner       : bits [24..31] (8 bits)  — the deformation owner tag (see below)
//       entityIndex : bits [10..23] (14 bits) — KU_NUM_BITS_FOR_ENTITY_NUM == 14
//       partIndex   : bits  [0..9]  (10 bits) — KU_NUM_BITS_FOR_PART_NUM   == 10
//   low dword   (this+4, bytes 4..5) == muSubA (u16, spliced as <<16 of the low dword)
//               (this+6, bytes 6..7) == muSubB (u16, low 16 of the low dword)
//
// OWNER TAG: derived from the owning vehicle's EntityId owner byte:
//   BurnoutBodyPartID : player vehicle (owner 1) -> tag 6 ; otherwise (traffic) -> tag 7
//   BurnoutWheelBodyID: player vehicle (owner 1) -> tag 9 ; otherwise (traffic) -> tag 10
// The wheel Set additionally tripwires that a non-player owner is exactly the traffic
// vehicle owner (asm: "lOwningVehicleID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE",
// baked ..\..\..\GameSource\Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h:244).
// Both Set bodies tripwire luEntityIndex < (1<<14) and luPartIndex < (1<<10) (baked
// ..\..\..\GameShared\GameClasses\SceneManager/CgsEntityId.h:117).
//
// HOST-vs-X360 NOTE: the packed word is built with explicit shifts/masks (endian-
// independent), so the 64-bit storage is console-faithful on the little-endian host.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnPhysics
{
namespace Deformation
{
    // Shared packed-field geometry for the embedded 32-bit EntityId word (8/14/10).
    // Matches the X360 deformation Set asm and the BrnPhysicalTrafficManager EntityId
    // construction (14-bit entity index above the 10-bit part index).
    struct BurnoutBodyPartIDLayout
    {
        static const u32 KU_NUM_BITS_FOR_ENTITY_NUM = 14;
        static const u32 KU_NUM_BITS_FOR_PART_NUM   = 10;
        static const u32 KU_ENTITY_INDEX_BASE       = 10;
        static const u32 KU_OWNER_BASE              = 24;

        // Owning-vehicle owner bytes the Set bodies branch on (X360-authoritative).
        static const u32 KU_OWNER_PLAYER_VEHICLE    = 1;  // BrnWorld::E_ENTITYTYPE_PLAYER_VEHICLE
        static const u32 KU_OWNER_TRAFFIC_VEHICLE   = 2;  // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE
    };

    // 64-bit packed body-part handle.
    struct BurnoutBodyPartID
    {
        // Deformation owner tags written into the embedded EntityId word.
        static const u32 KU_OWNER_PLAYER_BODY_PART  = 6;
        static const u32 KU_OWNER_TRAFFIC_BODY_PART = 7;

        BurnoutBodyPartID() {}

        // 0x825C1A10 — pack {owningVehicle owner+entityIndex, partIndex, subA, subB}.
        void Set(u32 luOwningVehicleID, u16 luPartIndex, u16 luSubA, u16 luSubB);

        u32 muEntityWord;   // this+0 (high dword): owner | entityIndex | partIndex
        u16 muSubA;         // this+4
        u16 muSubB;         // this+6
    };

    // 64-bit packed detached-wheel handle (same layout, wheel owner tags + traffic tripwire).
    struct BurnoutWheelBodyID
    {
        static const u32 KU_OWNER_PLAYER_WHEEL  = 9;
        static const u32 KU_OWNER_TRAFFIC_WHEEL = 10;

        BurnoutWheelBodyID() {}

        // 0x825C1D40 — pack {owningVehicle owner+entityIndex, partIndex, subA, subB}.
        void Set(u32 luOwningVehicleID, u16 luPartIndex, u16 luSubA, u16 luSubB);

        u32 muEntityWord;   // this+0 (high dword): owner | entityIndex | partIndex
        u16 muSubA;         // this+4
        u16 muSubB;         // this+6
    };
}
}
