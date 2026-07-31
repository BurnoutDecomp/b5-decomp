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

        // languagestreamcollection.h:76 (DWARF): the number of entries in this instance's
        // "Items" array attribute. X360 @0x82687F00.
        unsigned int Num_Items() const;
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
        // ⚠️ WIDENED 2026-07-31: Attrib::Instance::Get's key is 64 bits (the attribute
        // table hashes the whole doubleword). Only the LOW word of this key is recovered --
        // the X360 stages the high half with a separate lis/ori pair that was not recorded
        // when this accessor was reconstructed. Zero-extended so it cannot sign-extend into
        // garbage; FLAG: the lookup will still MISS until the high word is read back off the
        // call site (the same defect shotgroup::Num_ShotList and surfacelist::Num_Surfaces had).
        static const u64 KU_ITEMS_KEY = 0x8AD89D51ull; // Attrib::Hash::languagestreamcollection::Items (2329451857), low word only

        AttributeValue lScratch; // stack-resident Attrib::Attribute cursor, 4 machine words (attribinstance.h)
        // Get is non-const; Num_Items is const per DWARF -> const_cast the instance.
        languagestreamcollection* lpSelf = const_cast<languagestreamcollection*>(this);
        Attribute* lpCursor = reinterpret_cast<Attribute*>(
            lpSelf->Get(&lScratch, reinterpret_cast<int*>(lpSelf), KU_ITEMS_KEY));
        unsigned int luLength = static_cast<unsigned int>(lpCursor->GetLength());
        CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lScratch);
        return luLength;
    }
}
}
