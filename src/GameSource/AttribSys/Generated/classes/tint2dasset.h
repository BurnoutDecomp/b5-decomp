#pragma once

// Attrib::Gen::tint2dasset -- generated AttribSys class (the 2D screen-tint post-effect
// asset: one Colour attribute; DecFIGS DWARF tint2dasset.h:13, ClassName::tint2dasset
// == 746366090 (0x2C7CA48A) is the LOW word of the class key below).
//
// Same generated-ctor pattern as the sibling vignetteasset: resolve the class's collection
// (the caller's key, passed straight through), chain the Instance ctor over it, give the
// instance a default data area if construction left it without one. The one reader is
// BrnEffects::TintData2d::Construct(const u64&) @0x82678268, which copies the single
// 16-byte Colour vector at layout +0x00 (so the data area is 0x10 bytes).
//
// The ARTIST ctor has no export of its own in the IDA set (it is reached only through that
// Construct); the class key is attribhash64("tint2dasset") -- the same lookup8 that names
// every other class in the vaults (0x92F96F629C02B73F for vignetteasset, verified against
// its dumped constant).

#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
namespace Gen
{
    class tint2dasset : private Instance
    {
    public:
        static const int KI_TINT2DASSET_CLASS = 746366090;   // 0x2C7CA48A (DWARF ClassName::tint2dasset)
        static const u64 KU_TINT2DASSET_CLASS_KEY = 0x47A154ED2C7CA48AULL;

        // luCollectionKey is the caller's r4 the X360 ctor passes straight through to
        // FindCollection. BrnEffects::TintData2d::Construct hands it
        // Attrib::StringToKey(<the PFX group's tint2d id>).
        // [PC] the collection key is the FULL u64 Attrib::StringToKey hash. The X360 ctor takes
        // the low word (its vaults key collections by that word); this build's vaults and
        // Attrib::FindCollection(u64, u64) compare the whole key, so a truncated key never
        // resolves (POSTFXVAULT.BIN: e.g. FF8129C8E1D9E071, whose low word is the console key).
        explicit tint2dasset(u64 luCollectionKey = 0, void* lpOwner = nullptr);
        using Instance::IsValid;
        using Instance::GetLayoutPointer;
    };

    inline tint2dasset::tint2dasset(u64 luCollectionKey, void* lpOwner)
        : Instance(FindCollection(KU_TINT2DASSET_CLASS_KEY, luCollectionKey), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x10u);
    }
}
}
