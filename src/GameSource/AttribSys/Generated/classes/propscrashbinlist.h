#pragma once

// Attrib::Gen::propscrashbinlist — generated AttribSys class (per-prop crash-sound bin
// list, used by BrnSound::Logic::Collision::CollisionStateManager). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::propscrashbinlist::propscrashbinlist @ 0x82697230
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as surfacelist/debrisparams. The X360 build inlines the generated accessor /
// `using Instance::…` API away, so the constructor is the only propscrashbinlist
// function in the ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"   // Attrib::Private (array-length header)

namespace Attrib
{
namespace Gen
{
    class propscrashbinlist : private Instance
    {
    public:
        explicit propscrashbinlist(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // GROWN for BrnSound::Logic::Collision::BinLookupCache::Build<propscrashbinlist,
        // propscrashbin> @0x826A8710 (the crash-bin cache builder reads these two inlined
        // accessors off the list). Both are generated Instance accessors the X360 build
        // inlines away at the call site, so neither is its own ledger function; their
        // bodies are the raw data-area loads the Build leaf performs.

        // Live crash-bin count. X360: *(u32*)(mpAttributeData + 0x4D0) -- the scalar
        // count field at the tail of the list's 0x4D8-byte data area. This is the value
        // the Build assert names `lList.mNumCrashBins()`.
        u32 mNumCrashBins() const
        {
            return *reinterpret_cast<const u32*>(
                reinterpret_cast<const u8*>(mpAttributeData) + 0x4D0);
        }

        // Data pointer for the i-th crash-bin entry of the list's RefSpec array attribute.
        // The array lives at the start of mpAttributeData, fronted by the 8-byte
        // Attrib::Private length header; elements are 24-byte RefSpec records. Mirrors the
        // committed languagestreamconfiguration::GetContentSpecsData idiom: bounds-check
        // luIndex against Private::GetLength(), return the shared DefaultDataArea(0x18)
        // null-RefSpec sentinel (24 bytes) when out of range, else &element[luIndex]
        // (= mpAttributeData + 8 + luIndex*24, matching the X360 `dataArea + i*24 + 8`).
        const void* GetCrashBinRefData(u32 luIndex) const
        {
            const void* lpArrayData = mpAttributeData;
            if (luIndex >= static_cast<const Private*>(lpArrayData)->GetLength())
                return DefaultDataArea(0x18u);
            return reinterpret_cast<const u8*>(lpArrayData) + 8 + luIndex * 24;
        }
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::propscrashbinlist,
    // then give the instance a default data area (0x4D8 bytes) if it has none.
    inline propscrashbinlist::propscrashbinlist(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PROPSCRASHBINLIST_CLASS = 1326501919; // Attrib::ClassName::propscrashbinlist
        if (GetClass() != KI_PROPSCRASHBINLIST_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PROPSCRASHBINLIST_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x4D8u);
    }
}
}
