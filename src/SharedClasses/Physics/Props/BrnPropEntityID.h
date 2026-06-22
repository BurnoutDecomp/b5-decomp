#pragma once

// BrnWorld::PropEntityID — a prop entity handle that packs an owner / entity index /
// part index into a single 32-bit EntityId word. Reconstructed from the DecFIGS DWARF
// (member name) + the X360 ARTIST spine (bit layout + accessor bodies, authoritative).
//
// BIT LAYOUT (X360 retail, authoritative — OVERRIDES the Feb-2007 leak):
//   The asm for the out-of-line accessors fixes the packed layout as
//       owner       : bits [24..31]  (8 bits)   high byte; E_ENTITYTYPE_PROP == 3
//       entityIndex : bits [10..23]  (14 bits)
//       partIndex   : bits  [0..9]   (10 bits)
//   This is proven by:
//     GetEntityIndex @ 0x822B7B08  `extrwi r3,r11,14,8`  -> (muValue >> 10) & 0x3FFF
//     GetPartIndex   @ 0x822B7B68  `clrlwi r3,r11,22`     -> muValue & 0x3FF
//     SetEntityIndex @ 0x822B79D0  mask 0xFF0003FF | (idx<<10), idx < 0x4000 (14 bits)
//     SetPartIndex   @ 0x822B7A70  `clrrwi r11,r11,10`    & idx, idx < 0x400 (10 bits)
//   The Feb-2007 leak's CgsEntityId.h declared 12/12 entity/part bits; the shipped
//   X360 build uses 14/10. The shipped binary is the source of truth here, so the
//   masks/shifts below match the asm exactly (NOT the leaked 12/12 layout).
//
// The owner-byte assert ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", baked
// ..\\..\\..\\SharedClasses\\Physics/Props/BrnPropEntityID.h:278) reads the high byte
// of the big-endian word and compares == 3. The bounds asserts on the setters are
// baked into CgsEntityId.h (lines 160 / 167) and reproduced as house CGS_ASSERTs.
#include "BrnCommonTypes.h"                               // EntityId { u32 muValue }
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

namespace BrnWorld
{
    // Prop entity owner tag. The X360 asm only ever compares the owner byte against
    // the literal 3, so only the prop value is needed here (minimal, non-forking).
    enum EEntityType
    {
        E_ENTITYTYPE_PROP = 3
    };

    struct PropEntityID
    {
        // --- packed-field geometry (X360-authoritative; see header banner) ---
        static const u32 KU_NUM_BITS_FOR_OWNER      = 8;
        static const u32 KU_NUM_BITS_FOR_ENTITY_NUM = 14;
        static const u32 KU_NUM_BITS_FOR_PART_NUM   = 10;

        static const u32 KU_OWNER_BASE        = 24;
        static const u32 KU_ENTITY_INDEX_BASE = 10;
        static const u32 KU_PART_INDEX_BASE   = 0;

        static const u32 KU_OWNER_MASK        = 0xFF000000u; // bits [24..31]
        static const u32 KU_ENTITY_INDEX_MASK = 0x00FFFC00u; // bits [10..23]
        static const u32 KU_PART_INDEX_MASK   = 0x000003FFu; // bits  [0..9]

        // Out-of-line accessors the X360 ARTIST build emitted (bodies in
        // BrnPropEntityID.cpp). Signatures follow the Hex-Rays prototypes reconciled
        // against the call-site asm (the const-qualified getters read the owner byte
        // then return the masked field; the setters read-modify-write muValue).
        PropEntityID() {}                       // trivial (X360-inlined)
        explicit PropEntityID(u32 luEntityId);  // 0x822B7888 — store word + AssertIsProp

        u32 GetEntityIndex() const;             // 0x822B7B08
        u32 GetPartIndex() const;               // 0x822B7B68
        u32 GetValue() const;                   // 0x822B7BC8

        void SetEntityIndex(u16 luEntityIndex); // 0x822B79D0
        void SetPartIndex(u32 luPartIndex);     // 0x822B7A70

        void AssertIsProp() const;              // inlined tripwire (owner byte == 3)

        EntityId mEntityId;
    };
}
