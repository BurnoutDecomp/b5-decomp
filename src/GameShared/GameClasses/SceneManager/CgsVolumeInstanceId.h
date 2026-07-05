#pragma once

// CgsSceneManager::VolumeInstanceId — a packed 64-bit handle (entity id + reserved
// bits + volume index) identifying a collision volume instance. Reconstructed from
// the DecFIGS DWARF (the storage word) + the X360 ARTIST spine (the two field
// setters below, authoritative on bit layout).
//
// PACKED 64-BIT LAYOUT (X360 retail, authoritative):
//   The 64-bit muId carries an embedded 32-bit EntityId word in its HIGH dword
//   (bits [32..63]); the low dword holds reserved bits + the volume index. This
//   matches BrnPropEntityID.h's PropVolumeInstanceID documentation
//   (KU_ENTITY_ID_START_INDEX == 32). Within the embedded 32-bit entity word the
//   X360 EntityId fields are:
//       owner       : bits [24..31]  (8 bits)  — high byte of the entity word
//       entityIndex : bits [10..23]  (14 bits)
//       partIndex   : bits  [0..9]   (10 bits)
//   (Same 8/14/10 geometry the shipped binary uses for BrnWorld::PropEntityID.)
//
// The two setters here are the out-of-line members the X360 ARTIST build emitted:
//   SetEntityIDOwner       @ 0x822B0E00 — splice the 8-bit owner into the entity
//                                         word's high byte; assert owner <= 0xC
//                                         (baked CgsEntityId.h:153 "Burnout Specfic:
//                                         Bad entity type set").
//   SetEntityIDEntityIndex @ 0x822B0E70 — splice the 14-bit entity index at bit 10
//                                         of the entity word; assert index < 1<<14
//                                         (baked CgsEntityId.h:160 "luEntityIndex <
//                                         (1U << KU_NUM_BITS_FOR_ENTITY_NUM)").
// Both read the HIGH dword (entity word), modify it, and store it back to the high
// dword while preserving the low dword (the asm `stw r,0(this)` only touches the
// big-endian high 32 bits, then `ld; or; std` re-ORs against the preserved low
// half). Store order / bit-fields are reproduced exactly in the .cpp.
#include "types.hpp"

namespace CgsSceneManager
{
    struct VolumeInstanceId
    {
        // ---- packed-field geometry of the embedded entity word (X360-authoritative) ----
        static const u32 KU_NUM_BITS_FOR_ENTITY_NUM = 14; // entity-index width
        static const u32 KU_NUM_BITS_FOR_OWNER      = 8;  // owner width
        static const u32 KU_MAX_ENTITY_TYPE         = 0xC; // owner upper bound (<=) per assert

        static const u32 KU_OWNER_BASE        = 24;        // owner at bits [24..31]
        static const u32 KU_ENTITY_INDEX_BASE = 10;        // index at bits [10..23]

        static const u32 KU_OWNER_MASK        = 0xFF000000u; // bits [24..31] of entity word
        static const u32 KU_ENTITY_INDEX_MASK = 0x00FFFC00u; // bits [10..23] of entity word

        // 0x822B0E00 — set the entity-type / owner byte of the embedded entity word.
        VolumeInstanceId* SetEntityIDOwner(u8 lu8Owner);

        // 0x822B0E70 — set the 14-bit entity index of the embedded entity word.
        VolumeInstanceId* SetEntityIDEntityIndex(u32 luEntityIndex);

        // Read the 14-bit entity index back out of the embedded entity word. ADDITIVE inline
        // accessor (header-only; no out-of-line symbol). The X360 inlines this read at its call
        // sites: take the HIGH dword (entity word, `ld; srdi r,r,32`) and extract the 14-bit
        // field at bit 10 (`extrwi r,r,14,8` -- 14 bits starting at the big-endian bit 8 == the
        // low-endian field [10..23]). e.g. inlined into BrnWorld::RaceCarCrash::GetOwner
        // @ 0x827B1538.
        u32 GetEntityIDEntityIndex() const
        {
            return static_cast<u32>(muId >> (32 + KU_ENTITY_INDEX_BASE)) & 0x3FFFu;
        }

        u64 muId;
    };
}

// KeyBits overload for the CgsContainers::IndexedHashTable<VolumeInstanceId,...> instantiation
// (the hash/ordering works on the raw 64-bit key word; VolumeInstanceId has no implicit u64
// conversion, so the generic KeyBits<T> default does not apply). Additive; ODR-safe (inline).
namespace CgsContainers
{
    inline u64 KeyBits(const CgsSceneManager::VolumeInstanceId& lrKey) { return lrKey.muId; }
}
