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

        // ---- packed-field geometry of the WHOLE 64-bit word (DWARF CgsVolumeInstanceId.h) ----
        // ADDED 2026-08-18 (wave Q4, collision seam). These are the DecFIGS DWARF's own
        // private constants for this type, transcribed verbatim from the dumpfile
        // (references/DecFIGS/dwarfdump/GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h):
        //   :130 KU_NUM_BITS_FOR_ENTITY_ID       = 32
        //   :131 KU_NUM_RESERVED_BITS            = 24
        //   :132 KU_NUM_BITS_FOR_VOLUME_INDEX    = 8
        //   :135 KU_VOLUME_INDEX_START_INDEX     = 0   (the dump prints no initialiser for a
        //                                               zero-valued const; the value is pinned
        //                                               by KU_VOLUME_INDEX_MASK == 255 below
        //                                               and by the X360 splice `clrrdi r,id,8`)
        //   :136 KU_RESERVED_START_INDEX         = 8
        //   :137 KU_ENTITY_ID_START_INDEX        = 32
        //   :141 KU_RESERVED_UNSHIFTED_MASK      = 16777215   (0x00FFFFFF)
        //   :142 KU_VOLUME_INDEX_UNSHIFTED_MASK  = 255        (0xFF)
        //   :144 KU_RESERVED_MASK                = 4294967040 (0xFFFFFF00)
        //   :145 KU_VOLUME_INDEX_MASK            = 255        (0xFF)
        // (KU_ENTITY_ID_UNSHIFTED_MASK == 0xFFFFFFFF and KU_ENTITY_ID_MASK == 0xFFFFFFFF00000000
        //  are the same geometry expressed at bit 32; the dumpfile renders the latter as an
        //  unparsed byte array, so it is spelled out from the shift/width pair rather than
        //  copied from the dump.)
        static const u64 KU_NUM_BITS_FOR_ENTITY_ID      = 32;
        static const u64 KU_NUM_RESERVED_BITS           = 24;
        static const u64 KU_NUM_BITS_FOR_VOLUME_INDEX   = 8;

        static const u64 KU_VOLUME_INDEX_START_INDEX    = 0;
        static const u64 KU_RESERVED_START_INDEX        = 8;
        static const u64 KU_ENTITY_ID_START_INDEX       = 32;

        static const u64 KU_ENTITY_ID_UNSHIFTED_MASK    = 0xFFFFFFFFull;
        static const u64 KU_RESERVED_UNSHIFTED_MASK     = 0x00FFFFFFull;
        static const u64 KU_VOLUME_INDEX_UNSHIFTED_MASK = 0xFFull;

        static const u64 KU_ENTITY_ID_MASK              = 0xFFFFFFFF00000000ull;
        static const u64 KU_RESERVED_MASK               = 0xFFFFFF00ull;
        static const u64 KU_VOLUME_INDEX_MASK           = 0xFFull;

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

        // ---- the OWNER byte of the embedded entity word (bits [56..63] of muId) ------------
        // ADDED 2026-08-18 (wave Q4, collision seam). DWARF CgsVolumeInstanceId.h:106
        //     uint8_t GetEntityIDOwner() const;
        // Header inline (no out-of-line symbol in the X360 export set). The console folds it
        // as the two-instruction pair the prop contact-generation bodies show verbatim:
        //     0x822DF8D8  srdi r11, r23, 32     ; the embedded entity word
        //     0x822DF8DC  srwi r31, r11, 24     ; its high byte == the owner
        // and the value is what the "mVolumeInstanceId.GetEntityIDOwner() == E_ENTITYTYPE_PROP"
        // tripwire (BrnPropEntityID.h:455) compares. The same fold appears in
        // InSceneUpdateInterface::AddVolumeInstance @0x822CB664/0x822CB66C, there compared
        // against KU_MAX_ENTITY_TYPE instead.
        u8 GetEntityIDOwner() const
        {
            return static_cast<u8>(muId >> (KU_ENTITY_ID_START_INDEX + KU_OWNER_BASE));
        }

        // ---- the VOLUME-INDEX field (bits [0..7]) ------------------------------------------
        // ADDED 2026-08-18 (wave Q4, collision seam). DWARF CgsVolumeInstanceId.h:92 declares
        //     void SetVolumeIndex(uint8_t);
        // and :115 declares
        //     uint16_t GetVolumeIndex() const;
        // Both are HEADER INLINES: neither has an out-of-line symbol anywhere in the X360
        // export set (progress/identity.json has no "SetVolumeIndex"/"GetVolumeIndex" row at
        // all), and every user shows the splice folded in place.
        //
        // X360 ATTESTATION of the splice, read off the four prop contact-generation bodies
        // that carry it (all four fold the same two instructions):
        //     0x822DF8A0  clrrdi r9,  r23, 8     ; muId & ~0xFF          -- drop the old index
        //     0x822DF8AC  or     r23, r9,  r31   ; | (volumeIndex & 0xFF)
        // (identical pairs at 0x822DFB40/0x822DFB44, 0x822C63A0/0x822C63A4 and
        //  0x822C64B4/0x822C64B8). `clrrdi rA,rS,8` clears the LOW 8 bits of the 64-bit word,
        // which is exactly ~KU_VOLUME_INDEX_MASK -- the reserved bits [8..31] and the whole
        // embedded entity word [32..63] are preserved.
        void SetVolumeIndex(u8 lu8VolumeIndex)
        {
            muId = (muId & ~KU_VOLUME_INDEX_MASK)
                   | (static_cast<u64>(lu8VolumeIndex) & KU_VOLUME_INDEX_UNSHIFTED_MASK);
        }

        // DWARF spells the return uint16_t even though the stored field is 8 bits wide
        // (KU_NUM_BITS_FOR_VOLUME_INDEX == 8); the declaration is kept DWARF-shaped.
        u16 GetVolumeIndex() const
        {
            return static_cast<u16>(muId & KU_VOLUME_INDEX_MASK);
        }

        // DWARF CgsVolumeInstanceId.h :79 / :82. The sentinel is all-ones: ARTIST tests it as
        // `ld r11,0x18(r30)` + `cmpdi cr6,r11,-1` (CrashPlayManager::Update @0x82306648), and the
        // assert string baked next to that test is literally
        // "mPlayerCarVolumeInstanceID.IsValid()", so both the value and the method name are
        // attested by the binary itself.
        static const u64 KU_INVALID_ID = 0xFFFFFFFFFFFFFFFFull;

        void SetInvalid()    { muId = KU_INVALID_ID; }
        bool IsValid() const { return muId != KU_INVALID_ID; }

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
