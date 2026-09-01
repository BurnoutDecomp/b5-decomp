#pragma once

// Attrib::Gen::crashbinlist — generated AttribSys class (the crash-material bin-list
// attribute schema; drives BrnSound::Logic::Collision::CollisionStateManager's crash-sound
// bin table). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::crashbinlist::crashbinlist @ 0x82697060
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as surfacelist/debrisparams/propscrashbinlist. The X360 build inlines the
// generated accessor / `using` API away, so the constructor is the only crashbinlist
// function in the ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"

namespace Attrib
{
namespace Gen
{
    class crashbinlist : private Instance
    {
    public:
        static const u64 KU_CLASS_KEY = 0xF86D8C0755C79CC2ull;
        explicit crashbinlist(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        Collection* ChangeWithDefault(u64 luCollectionKey)
        {
            return Change(FindCollectionWithDefault(KU_CLASS_KEY, luCollectionKey));
        }

        u32 mNumCrashBins() const
        {
            return *reinterpret_cast<const u32*>(
                static_cast<const u8*>(mpAttributeData) + 0x4D0u);
        }

        const void* GetCrashBinRefData(u32 luIndex) const
        {
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            if (luIndex >= reinterpret_cast<const Private*>(lpData)->GetLength())
                return DefaultDataArea(0x18u);
            return lpData + 8u + luIndex * 24u;
        }

        u64 GetCrashBinCollectionKey(u32 luIndex) const
        {
            return *reinterpret_cast<const u64*>(
                static_cast<const u8*>(GetCrashBinRefData(luIndex)) + 8u);
        }
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::crashbinlist,
    // then give the instance a default data area (0x4D8 bytes) if it has none.
    inline crashbinlist::crashbinlist(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_CRASHBINLIST_CLASS = static_cast<int>(1439145154u); // Attrib::ClassName::crashbinlist (0x55C79CC2)
        if (GetClass() != KI_CRASHBINLIST_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_CRASHBINLIST_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x4D8u);
    }
}
}
