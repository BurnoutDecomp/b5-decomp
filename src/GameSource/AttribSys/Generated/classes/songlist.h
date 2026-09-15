#pragma once

// Attrib::Gen::songlist — generated AttribSys class (the music "song list" attribute
// schema, consumed by BrnSound::Module::Io::EaTraxHelper and BrnSound::Logic::MusicEffect).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::songlist::Songs     @ 0x82683998
//   Attrib::Gen::songlist::songlist  @ 0x82697978  (ctor — see note below)
//
// The X360 build inlines the generated `using Instance::…` API away; Songs is the one
// real array accessor recovered in this wave. Derives (privately) from Attrib::Instance,
// matching the committed sibling generated classes (song/surfacelist/propscrashbinlist/
// debrisparams).
//
// Both functions are now fully attested and bodied. The ctor @0x82697978 was recovered in
// a later wave (its asm chains the Attrib::Instance base ctor via sub_8280A248, checks the
// class key 0x7C94BB46, then defaults a 0x970-byte data area) and mirrors the committed
// sibling song::song exactly.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"   // Attrib::Private (canonical)
#include "types.hpp"

namespace Attrib
{
namespace Gen
{
    class songlist : private Instance
    {
    public:
        // ctor @ 0x82697978 — chain the Attrib::Instance base ctor, assert the collection's
        // class is ClassName::songlist (skipping the assert when the class is unset/0), then
        // give the instance a default data area (0x970 bytes) if it has none.
        explicit songlist(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // Bounds-checked accessor for the songlist's variable-length Song array.
        // X360 @0x82683998 (out-of-line; body in songlist.cpp). Reads mpAttributeData
        // (Instance+4), gets the array length via Attrib::Private::GetLength; if luIndex
        // (unsigned) >= length falls back to the shared zero-initialised default block
        // (one Song record, 0x18 bytes); otherwise indexes the array (stride 0x18, base +8).
        void* Songs(unsigned int luIndex) const;

        // The songlist's own element COUNT attribute, at +0x968 inside the 0x970-byte
        // data area (the Songs array occupies +0x08 .. +0x967: 8 + 100*0x18 == 0x968).
        // X360-attested by MusicEffect::UpdateParams @0x826FE5C8 -- 0x826FEBBC
        // `lwz r4, 0x968(r11)` with r11 == mpAttributeData, feeding
        // EaTraxData::SelectSong's aiNumSongs. The X360 inlines this generated
        // accessor away, which is why it was not recovered with Songs().
        s32 Num_Songs() const;
    };
}
}
