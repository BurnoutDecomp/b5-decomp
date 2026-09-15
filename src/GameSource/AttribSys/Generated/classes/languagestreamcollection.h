#pragma once

// Attrib::Gen::languagestreamcollection — generated AttribSys class (the "language stream
// collection" attribute schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::languagestreamcollection::languagestreamcollection @ 0x8269E5A8  (ctor)
//   Attrib::Gen::languagestreamcollection::Num_Items                 @ 0x82687F00
//
// The ctor chains Instance and asserts the class tag (no DefaultDataArea — the ctor asm
// has none, ending with `b __restgprlr_29` right after the AssertOnClassCheck path).
// Num_Items is DWARF-attested `unsigned int Num_Items() const;` (languagestreamcollection.h
// :76). DWARF derives the class publicly from Instance; the committed sibling corpus uses
// `private Instance` — kept private for corpus consistency and const_casts `this` in the
// const accessor (Get is public on Instance, reachable from the derived member).
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"

namespace CgsSceneManager
{
namespace CgsCollision
{
    // Tears down the stack-resident Attrib::Attribute cursor Num_Items builds. IDA matched
    // the generic 16-byte-object teardown stub to BaseCollisionGenerator::Destruct — a
    // symbol-collision artifact, not a real call into collision code. Declaration-only
    // under the cl /c gate (body lives in its own TU). Mirrors committed surfacelist.h.
    void BaseCollisionGenerator_Destruct(void* lpThis);
}
}

namespace Attrib
{
namespace Gen
{
    class languagestreamcollection : private Instance
    {
    public:
        explicit languagestreamcollection(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
        // SpeechEffect::Notify @0x826E7D40 builds one straight off an embedded RefSpec
        // (speechdata.mpAttributeData + 368, the Atomika free-burn VO collection).
        explicit languagestreamcollection(const RefSpec& lrRefSpec, void* lpOwner = nullptr)
            : Instance(lrRefSpec, lpOwner) {}
        bool IsValid() const { return Instance::IsValid(); }

        // languagestreamcollection.h:76 (DWARF): the number of entries in this instance's
        // "Items" array attribute. X360 @0x82687F00.
        unsigned int Num_Items() const;

        // SOUND-C 2026-09-15: the indexed element of that same "Items" array — a 24-byte
        // Attrib::RefSpec. SpeechEffect::Notify @0x826E7D64 (Atomika free-burn VOs) and
        // @0x826E831C (the per-game-mode "lost again" banks) both do exactly:
        //   lpSpec = GetAttributePointer(Items, index); if (!lpSpec) lpSpec = DefaultDataArea(0x18);
        const RefSpec& Items(u32 luIndex) const;
    };

    // X360 ctor @0x8269E5A8: chain the Instance ctor, then assert the collection's class is
    // ClassName::languagestreamcollection (skip the assert if the class already matches or
    // is unset, i.e. 0). No DefaultDataArea call — the X360 asm has none.
    inline languagestreamcollection::languagestreamcollection(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_LANGUAGESTREAMCOLLECTION_CLASS = 1573411476; // Attrib::ClassName::languagestreamcollection (0x5DC85A94)
        if (GetClass() != KI_LANGUAGESTREAMCOLLECTION_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_LANGUAGESTREAMCOLLECTION_CLASS, GetCollection());
    }

    // X360 @0x82687F00, store-for-store:
    //   r4 = this            (mr r4,r3)                    -> Get arg lpName
    //   r5 = 0x8AD89D51      (lis/ori/insrdi 64-bit imm)  -> Get arg liArg (the "Items" key)
    //   r3 = &scratch (16B)  (addi r1,0x50)               -> Get arg pOut
    //   bl Instance::Get; bl Attribute::GetLength (r31=result); bl <cursor teardown>;
    //   return r31.
    // Attrib::Hash::languagestreamcollection::Items = 2329451857 (0x8AD89D51 == signed
    // -1965515439) — DWARF-attested at languagestreamcollection.h:100.
    inline unsigned int languagestreamcollection::Num_Items() const
    {
        // ⭐ KEY WIDTH PAID 2026-09-15 (SOUND-C): the high word is staged next to the low
        // one at every call site -- `ori r4,r10,0x9D51` + `lis r10,0x67C4` / `ori r10,r10,0xA557`
        // + `insrdi r4,r10,32,0` (SpeechEffect::Notify @0x826E7D64). The low-word-only key MISSED.
        static const u64 KU_ITEMS_KEY = 0x67C4A5578AD89D51ull; // Attrib::Hash::languagestreamcollection::Items

        AttributeValue lScratch; // stack-resident Attrib::Attribute cursor, 4 machine words (attribinstance.h)
        // Get is non-const; Num_Items is const per DWARF -> const_cast the instance.
        languagestreamcollection* lpSelf = const_cast<languagestreamcollection*>(this);
        Attribute* lpCursor = reinterpret_cast<Attribute*>(
            lpSelf->Get(&lScratch, reinterpret_cast<int*>(lpSelf), KU_ITEMS_KEY));
        unsigned int luLength = static_cast<unsigned int>(lpCursor->GetLength());
        CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lScratch);
        return luLength;
    }

    inline const RefSpec& languagestreamcollection::Items(u32 luIndex) const
    {
        static const u64 KU_ITEMS_KEY = 0x67C4A5578AD89D51ull;
        languagestreamcollection* lpSelf = const_cast<languagestreamcollection*>(this);
        const RefSpec* lpSpec = static_cast<const RefSpec*>(
            lpSelf->GetAttributePointer(KU_ITEMS_KEY, luIndex));
        if (!lpSpec)
            lpSpec = static_cast<const RefSpec*>(DefaultDataArea(0x18u));
        return *lpSpec;
    }
}
}
