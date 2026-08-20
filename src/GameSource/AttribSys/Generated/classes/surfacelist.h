#pragma once

// Attrib::Gen::surfacelist — generated AttribSys class (the "surface list" attribute
// schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::surfacelist::surfacelist       @ 0x825C2DF8  (ctor)
//   Attrib::Gen::surfacelist::Num_Surfaces      @ 0x82278C40
//   Attrib::Gen::surfacelist::ChangeWithDefault @ 0x8227EFC8
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so only
// the ledger-attested out-of-line functions are modelled here (ctor + the two accessors).
// Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollectionWithDefault (canonical)

namespace CgsSceneManager
{
namespace CgsCollision
{
    // Tears down the stack-resident Attrib::Attribute cursor Num_Surfaces builds (IDA
    // matched the generic 16-byte-object teardown stub to BaseCollisionGenerator::Destruct
    // — a symbol-collision artifact, not a real collision-code call). Declaration-only.
    void BaseCollisionGenerator_Destruct(void* lpThis);
}
}

namespace Attrib
{
namespace Gen
{
    class surfacelist : private Instance
    {
    public:
        // The surfacelist class key the ctor checks and ChangeWithDefault resolves against
        // (0x85B5C4F4 == -2051685132; the low word of the 64-bit insrdi immediate).
        static const int KI_SURFACELIST_CLASS = -2051685132; // 0x85B5C4F4

        explicit surfacelist(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // The number of surfaces in this instance's "Surfaces" array attribute. REAL X360
        // function @0x82278C40.
        int Num_Surfaces();

        // Resolve the surfacelist collection (with default) for this class key and swap
        // this instance onto it, returning the previous collection. REAL X360 function
        // @0x8227EFC8.
        Collection* ChangeWithDefault(u64 luCollectionKey = 0);

        // The generated "Surfaces" indexed-array accessor (DWARF Attrib::Gen::surfacelist
        // ::Surfaces): return the attribute pointer for element luIndex of the Surfaces
        // array. Inlined at its call sites in the X360 build (e.g. WheelStateMachine::
        // Update 0x82293EB8: GetAttributePointer(0x0ADCE56E_F3DA7F1F, surfaceId)); the
        // key is the 64-bit "Surfaces" attribute key.
        void* Surfaces(u32 luIndex) const
        {
            static const u64 KU_SURFACES_ATTRIBUTE_KEY = 0x0ADCE56EF3DA7F1Full;
            return GetAttributePointer(KU_SURFACES_ATTRIBUTE_KEY, luIndex);
        }
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::surfacelist,
    // then give the instance a default data area if construction left it without one.
    inline surfacelist::surfacelist(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        if (GetClass() != KI_SURFACELIST_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_SURFACELIST_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x18u);
    }

    // X360 @0x82278C40: resolve this instance's "Surfaces" attribute into a stack-resident
    // Attrib::Attribute cursor (16 bytes) via Instance::Get(pOut=&cursor, lpName=this,
    // liArg=key). Key is the low 32 bits of the 64-bit immediate 0x0ADCE56E_F3DA7F1F; the
    // high half is dead. Read the cursor's length, tear the cursor down, return length.
    inline int surfacelist::Num_Surfaces()
    {
        // ⚠️ WIDENED 2026-07-31. This used to pass the low word alone (`int` -203784417
        // == 0xF3DA7F1F) with a comment saying "the high half is dead". It is not: the
        // attribute table hashes the whole doubleword, so the truncated key MISSES. The
        // full key is the same 0x0ADCE56E_F3DA7F1F the indexed Surfaces() accessor above
        // already uses.
        static const u64 KU_SURFACES_KEY = 0x0ADCE56EF3DA7F1Full;

        AttributeValue lCursor; // stack-resident Attrib::Attribute cursor (4 machine words)
        Attribute* lpAttribute = reinterpret_cast<Attribute*>(
            Get(&lCursor, reinterpret_cast<int*>(this), KU_SURFACES_KEY));
        int liLength = lpAttribute->GetLength();
        CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lCursor);
        return liLength;
    }

    // X360 @0x8227EFC8: CollectionWithDefault = FindCollectionWithDefault(key); then
    // this->Change(CollectionWithDefault). Change() is public on Attrib::Instance, reachable
    // from surfacelist's members under `private Instance`.
    //
    // ⚠️ THE KEY IS 64-BIT (corrected 2026-08-17). The X360 assembles both halves --
    // `ori r3, r11, 0xC4F4` then `insrdi r3, r11, 32, 0` with r11 = 0x42C25F49 -- so r3
    // holds 0x42C25F49_85B5C4F4 entering the call. The old note here said the callee "reads
    // only the low word" and passed the truncated int; that is the same truncation
    // attrib_findcollection.h records fixing for FindCollection, where it had been silently
    // mis-resolving every lookup.
    static const u64 KU_SURFACELIST_CLASS_KEY = 0x42C25F4985B5C4F4ull;   // @0x8227EFD8-EC

    inline Collection* surfacelist::ChangeWithDefault(u64 luCollectionKey)
    {
        Collection* lpCollectionWithDefault =
            FindCollectionWithDefault(KU_SURFACELIST_CLASS_KEY, luCollectionKey);
        return Change(lpCollectionWithDefault);
    }
}
}
