#pragma once

// CgsSceneManager::VolumeId — a packed 64-bit collision-volume handle (owner byte +
// index/payload bits). Reconstructed from the DecFIGS DWARF (the storage word + the
// GetOwner/GetIndex/Set members) + the X360 ARTIST spine of its users (authoritative
// on the owner-byte position). This is the storage VolumeId that BrnWorld::PropVolumeID
// wraps and re-packs.
//
// OWNER-BYTE POSITION (X360-authoritative): the BrnWorld::PropVolumeID::Set body
// (0x822B7C20) reads the owner via `ld r11,0(this); srdi r11,r11,16; clrlwi r11,r11,24`
// (bits [16..23] of the low 32 bits of the big-endian word) and compares == 3
// (E_ENTITYTYPE_PROP). The same body re-installs the owner with `oris r11,r11,3`
// (0x30000), i.e. the literal 3 at bits [16..23]. So GetOwner() == (mId >> 16) & 0xFF.
#include "types.hpp"

namespace CgsSceneManager
{
    struct VolumeId
    {
        // Owner byte lives at bits [16..23] of the packed word (X360-authoritative; see
        // the BrnWorld::PropVolumeID::Set asm in the header banner).
        static const u32 KU_OWNER_BASE = 16;
        static const u64 KU_OWNER_MASK = 0x0000000000FF0000ull; // bits [16..23]

        VolumeId() {}                              // CgsVolumeId.h:63 (trivial)
        explicit VolumeId(u64 lu64Id) : mId(lu64Id) {} // CgsVolumeId.h:64

        // CgsVolumeId.h:57 — the owner tag (top byte of the low word).
        u8 GetOwner() const { return static_cast<u8>((mId >> KU_OWNER_BASE) & 0xFFu); }

        // CgsVolumeId.h:65 — raw packed word.
        operator u64() const { return mId; }

        u64 mId;
    };
}
