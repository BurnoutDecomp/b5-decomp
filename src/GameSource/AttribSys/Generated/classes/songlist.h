#pragma once

// Attrib::Gen::songlist — generated AttribSys class (the music "song list" attribute
// schema, consumed by BrnSound::Module::Io::EaTraxHelper and BrnSound::Logic::MusicEffect).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::songlist::Songs     @ 0x82683998
//   Attrib::Gen::songlist::songlist  @ 0x82697978  (ctor — see note below)
//
// The X360 build inlines the generated `using Instance::…` API away; Songs is the one
// real array accessor recovered in this wave. Derives (privately) from Attrib::Instance,
// matching the committed sibling generated classes (surfacelist/propscrashbinlist/
// debrisparams).
//
// NOTE: the songlist ctor @0x82697978 could not be recovered in this wave (the decompiler
// failed to produce output for it), so its class-key constant and DefaultDataArea size are
// unattested. Rather than fabricate them, the ctor is left DECLARATION-ONLY here (it
// compiles under the per-TU cl /c gate; a full body must be authored from the ctor asm in
// a follow-up). The Songs accessor is fully attested and complete.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"   // Attrib::Private (canonical)

namespace Attrib
{
namespace Gen
{
    class songlist : private Instance
    {
    public:
        // ctor @ 0x82697978 — declaration-only (body unrecovered in this wave; see header
        // note). Compiles under cl /c; nothing in this header instantiates it.
        explicit songlist(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // Bounds-checked accessor for the songlist's variable-length Song array.
        // X360 @0x82683998: read mpAttributeData (Instance+4 -> r30), get the array
        // length via Attrib::Private::GetLength(mpAttributeData); if luIndex (r4,
        // unsigned) >= length (unsigned compare `cmplw`), fall back to the shared
        // zero-initialised default block sized for one Song record (0x18 bytes).
        // Otherwise index into the array: element stride is 0x18 (24) bytes
        //   (slwi r11,r31,1 -> idx*2; add -> idx*3; slwi r11,r11,3 -> idx*24)
        // and the array starts at +8 within the attribute-data block
        //   (add r11,r11,r30 -> +mpAttributeData; addi r3,r11,8 -> +8).
        void* Songs(unsigned int luIndex) const
        {
            u8* lpData = static_cast<u8*>(GetLayoutPointer()); // Instance+4 == mpAttributeData
            if (luIndex >= reinterpret_cast<const Private*>(lpData)->GetLength())
                return DefaultDataArea(0x18u);
            return lpData + 8 + 24u * luIndex;
        }
    };
}
}
