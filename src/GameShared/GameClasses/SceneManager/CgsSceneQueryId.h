#pragma once

// CgsSceneManager::SceneQueryId — a packed (owner, index) handle for an in-flight
// scene query. Reconstructed from the DecFIGS DWARF (CgsSceneQueryId.h:48..:67).
#include "types.hpp"

namespace CgsSceneManager
{
    struct SceneQueryId
    {
        // DWARF CgsSceneQueryId.h:54 / :57 / :60 (added 2026-09-24, crash parity FX-SCENEMGR).
        // Neither ARTIST nor DecFIGS has an out-of-line body for any of the three: every use is
        // inlined, and the inlined forms fix the packing BY VALUE -- the owner byte is bits
        // [16..23], the index is the low half:
        //   Set(5, slot)  PlaceOnTrackManager::PostSceneUpdate 0x822D3254 `clrlwi r8, r28, 16`
        //                 then 0x822D3264 `oris r8, r8, 5`
        //   GetOwner()    PlaceOnTrackManager::PrePhysicsUpdate 0x822F6F78 `extrwi r10, r11, 8, 8`
        //                 == (mId >> 16) & 0xFF, compared with 5 at 0x822F6F7C
        //   GetIndex()    0x822F6F84 `clrlwi r20, r11, 16` == mId & 0xFFFF (the asking slot)
        // Read the word, never its bytes: the console's memory byte 1 is bits [16..23] only
        // because it is big-endian; on this host byte 1 is bits [8..15].
        void Set(u8 lu8Owner, u16 lu16Index)
        {
            mId = (static_cast<u32>(lu8Owner) << 16) | static_cast<u32>(lu16Index);
        }
        u8  GetOwner() const { return static_cast<u8>((mId >> 16) & 0xFFu); }
        u16 GetIndex() const { return static_cast<u16>(mId & 0xFFFFu); }

        u32 mId;   // CgsSceneQueryId.h:67 (private in the DWARF; public here, read by name)
    };
}
