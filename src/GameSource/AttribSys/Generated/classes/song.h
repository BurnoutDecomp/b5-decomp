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
#include <cstring>

namespace Attrib
{
namespace Gen
{
    class song : private Instance
    {
    public:
        explicit song(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // The song's stream NAME, a const char* stored at +0x04 inside the 0x14-byte
        // data area. X360-attested by MusicEffect::UpdateParams @0x826FE5C8 --
        // 0x826FEC9C..0x826FECA8:
        //     lwz r11, var_13C(r1)   ; the song Instance's mpAttributeData
        //     lwz r3,  4(r11)        ; -> CgsSound::Playback::Name::MakeHash
        // (the generated accessor itself is inlined away on the console).
        // The 4-byte slot is read and widened the way every committed generated text
        // accessor does it (crashbin::TextAt, propscrashbin::TextAt): attribute data
        // keeps its console 32-bit pointer slots and the game heap lives below 4 GB.
        const char* Stream() const
        {
            const unsigned char* lpData =
                static_cast<const unsigned char*>(GetLayoutPointer());
            if (!lpData)
                return 0;
            unsigned int luAddress = 0;
            std::memcpy(&luAddress, lpData + 4, sizeof(luAddress));
            return reinterpret_cast<const char*>(
                static_cast<unsigned long long>(luAddress));
        }
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
