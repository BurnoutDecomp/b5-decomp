#ifndef BRN_FLAPT_FILE_INSTANCE_H
#define BRN_FLAPT_FILE_INSTANCE_H

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"   // BrnFlapt::MovieClipRef

// ============================================================================
// GameSource/Gui/Flapt/BrnFlaptFileInstance.h
//
// BrnFlapt::FlaptFileInstance — a live, instanced FlaptFile (one entry in
// FlaptManager::maFlaptFileInstances). Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Attested offsets (this group):
//   +0x00  mbIsActive            (lbz 0(this); IsActive() flag, GetRootMovieClip)
//   +0x24  mpRootMovieClipInstance (lwz 0x24(this); the root MovieClipInstance)
//
// The element stride is 52 bytes (0x34) — proven by FlaptManager::GetFile, which
// indexes maFlaptFileInstances with `mulli r5,0x34`. Interior bytes between the
// attested members are honestly opaque padding; grow this header additively as
// more FlaptFileInstance member TUs (SetData / Update / Render / FindComponent)
// land.
// ============================================================================

namespace BrnFlapt
{
    struct FlaptFileInstance
    {
        // GetRootMovieClip @ 0x8246B360 : assert IsActive() and a live root clip,
        // then return a MovieClipRef {mpRootMovieClipInstance, 0} by value (written
        // into the caller-provided out buffer).
        MovieClipRef* GetRootMovieClip(MovieClipRef* lpOutRef) const;

        // FindComponent @ 0x8246EBxx : look up a named timeline component by its
        // pre-hashed name and write a MovieClipRef onto it into lpOutRef. Declared
        // here (asm-attested arg order this/hash/out/name from the BrnFlaptFileRef.cpp
        // call site, r3..r6) so callers compile; the body lands with this class's TU.
        MovieClipRef* FindComponent(u32 luHash, MovieClipRef* lpOutRef,
                                    const char* lpcName) const;

        u8    mbIsActive;             // +0x00
        u8    mau8Opaque01[0x23];     // +0x01..0x23  pad to +0x24
        void* mpRootMovieClipInstance;// +0x24
        u8    mau8Opaque28[0x0C];     // +0x28..0x33  pad to 0x34 (52-byte stride)
    };
}

#endif // BRN_FLAPT_FILE_INSTANCE_H
