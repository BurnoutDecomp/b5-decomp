#pragma once

// Attrib::Gen::song — generated AttribSys class (a music-track attribute record: song
// name/artist/album, used by EaTraxHelper and MusicEffect). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::song::song @ 0x82697490
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as iceanim / surfacelist / debrisparams. The X360 build inlines the generated
// accessor / `using` API away, so the constructor is the only song function in the
// ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class song : private Instance
    {
    public:
        explicit song(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::song
    // (skipping the assert when the class is unset/0), then give the instance a
    // default data area (0x14 bytes) if it has none.
    inline song::song(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_SONG_CLASS = -110802100; // Attrib::ClassName::song (0xF9654B4C)
        if (GetClass() != KI_SONG_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_SONG_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x14u);
    }
}
}
