#pragma once

// Attrib::Gen::sampletags — generated AttribSys class (per-sample-tag arrays: first/last
// sample indices + volumes, indexed by a variable-length "count"). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::sampletags::sampletags   @ 0x82696528  (ctor)
//   Attrib::Gen::sampletags::FirstIndices @ 0x82682AC8  (len hdr +0xD0, element (i+0x6C)*2)
//   Attrib::Gen::sampletags::LastIndices  @ 0x82682B28  (len hdr +0x88, element (i+0x48)*2)
//   Attrib::Gen::sampletags::Volumes      @ 0x82682B88  (len hdr +0,    element (i+2)*4)
//
// Each accessor reads the variable-length array's element count via
// Attrib::Private::GetLength() over the length-prefixed sub-block at a per-array byte
// offset into the instance data (mpAttributeData, Instance +4), and returns either the
// element at the attested stride or the shared zeroed DefaultDataArea fallback (X360-
// faithful: the generated accessor never returns nullptr). Element field layout is not
// attested — only the strides. Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"   // Attrib::Private (canonical)

namespace Attrib
{
namespace Gen
{
    class sampletags : private Instance
    {
    public:
        explicit sampletags(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // &FirstIndices[luIndex] (u16 element) if in range, else DefaultDataArea(2).
        void* FirstIndices(unsigned int luIndex);
        // LastIndices[luIndex] as a const s16& (DWARF-attested shape) if in range, else
        // the shared 2-byte default element.
        const s16& LastIndices(unsigned int luIndex) const;
        // &Volumes[luIndex] (4-byte element) if in range, else DefaultDataArea(4).
        void* Volumes(unsigned int luIndex) const;
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::sampletags,
    // then give the instance a default data area (0x118 bytes) if it has none. The class
    // key is the low word of the 64-bit cmpld immediate (0x2D0B9434_9ED5019A) — 0x9ED5019A.
    inline sampletags::sampletags(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_SAMPLETAGS_CLASS = -1630207590; // Attrib::ClassName::sampletags (0x9ED5019A)
        if (GetClass() != KI_SAMPLETAGS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_SAMPLETAGS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x118u);
    }

    // X360 @0x82682AC8: length = GetLength(mpAttributeData+0xD0); in range ->
    // mpAttributeData + 2*(luIndex+108) [= +0xD8 + 2*luIndex]; else DefaultDataArea(2).
    inline void* sampletags::FirstIndices(unsigned int luIndex)
    {
        u8* lpData = static_cast<u8*>(GetLayoutPointer());
        if (luIndex >= reinterpret_cast<const Private*>(lpData + 0xD0)->GetLength())
            return DefaultDataArea(2u);
        return lpData + 2 * (luIndex + 108);
    }

    // X360 @0x82682B28: length = GetLength(mpAttributeData+0x88); in range -> element at
    // byte offset (luIndex+0x48)*2 [= +0x90 + 2*luIndex]; else the shared 2-byte default.
    // The x2 stride applies to the WHOLE (index+0x48): element 0 sits at 0x90, not 0x48.
    inline const s16& sampletags::LastIndices(unsigned int luIndex) const
    {
        u8* lpData = static_cast<u8*>(mpAttributeData);
        if (luIndex >= reinterpret_cast<const Private*>(lpData + 0x88)->GetLength())
            return *static_cast<const s16*>(DefaultDataArea(2u));
        return *reinterpret_cast<const s16*>(lpData + (luIndex + 0x48) * 2);
    }

    // X360 @0x82682B88: length = GetLength(mpAttributeData+0); in range -> mpAttributeData
    // + 4*(luIndex+2) [= +8 + 4*luIndex]; else DefaultDataArea(4).
    inline void* sampletags::Volumes(unsigned int luIndex) const
    {
        u8* lpData = static_cast<u8*>(mpAttributeData);
        if (luIndex >= reinterpret_cast<const Private*>(lpData)->GetLength())
            return DefaultDataArea(4u);
        return lpData + 4 * (luIndex + 2);
    }
}
}
