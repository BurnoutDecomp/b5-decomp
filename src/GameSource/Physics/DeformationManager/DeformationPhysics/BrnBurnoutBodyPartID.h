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
        // The body @0x825C1A10 stores BOTH muSubA (this+4) and muSubB (this+6) -- the 4-arg
        // signature is authoritative here. FLAG: some call sites (e.g. PhysicalBodyPartPool::
        // CreatePart, whose asm is `Set(&id, a4, 0, a3)`) supply only THREE value args, leaving the
        // 4th (luSubB) register indeterminate at the call. The default below lets those 3-arg call
        // sites compile without fabricating a meaningful subB; it is NOT an asm-attested value.
        // 2026-08-24 (physics mount wave B2): the walls-leg-4 INLINE approximation that lived
        // here (raw `owner-preserving` mask-pack, FLAGGED role-derived) is RETIRED -- the
        // asm-faithful body @0x825C1A10 now mounts in BrnBurnoutBodyPartID.cpp. The real body
        // DIFFERS: it retags the owner field to KU_OWNER_PLAYER_BODY_PART(6) /
        // KU_OWNER_TRAFFIC_BODY_PART(7) instead of preserving the vehicle's own owner byte,
        // and carries the two non-gating index tripwires. Behaviour change toward the console.
        void Set(u32 luOwningVehicleID, u16 luPartIndex, u16 luSubA, u16 luSubB = 0);

        // ⭐ ADDED 2026-08-27 (detach-2 wave). THE NAME IS THE CONSOLE'S OWN: it is baked verbatim
        // into PhysicalBodyPartPool::UpdatePart's assert string @0x8260CB08 --
        //   "maParts[ lu16PartIndex ].GetRigidBodyId().GetBaseRigidBodyID() == lpUpdateEvent->mID"
        // (BrnPhysicalBodyPartPool.cpp:173). The console reads the handle whole with a single
        // `ld 0x1D0(part)` (AddToSim @0x8260AD80, AddToScene @0x8260A9C0/F8, FixupBodyPartVehicle-
        // Contact @0x825A0D64 all do), so the u64 IS the record's big-endian byte image:
        // entity word in the high dword, {muSubA, muSubB} in the low.
        //
        // ⚠️ THE PACKING IS LOAD-BEARING IN TWO DIRECTIONS and both consumers are attested:
        //   * `(id >> 56) & 0xFF` is the OWNER tag DetachedPartManager::UpdatePostPhysics
        //     branches on -- its asm is `ld ; srdi 32 ; srwi 24 ; cmplwi 6 / 7` @0x8260E194..A8;
        //   * `id & 0xFFFF` is the POOL SLOT PhysicalBodyPartPool::UpdatePart indexes with --
        //     its asm is `ld ; clrlwi r28, r11, 16` @0x8260CB20.
        // Spelling the pack ONCE here is what keeps those two readings consistent; the previous
        // spelling of the owner read as a raw host byte offset (+4) silently selected the entity
        // word's LOW byte on a little-endian host and could never equal 6 or 7.
        u64 GetBaseRigidBodyID() const
        {
            return (static_cast<u64>(muEntityWord) << 32)
                 | (static_cast<u64>(muSubA) << 16)
                 |  static_cast<u64>(muSubB);
        }

        // The owner tag the deformation consumers branch on -- the entity word's top byte.
        u32 GetOwner() const { return muEntityWord >> BurnoutBodyPartIDLayout::KU_OWNER_BASE; }

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
