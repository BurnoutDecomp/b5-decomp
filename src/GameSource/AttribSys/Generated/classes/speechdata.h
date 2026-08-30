#pragma once

// Attrib::Gen::speechdata — generated AttribSys class (per-car speech-effect voiceover
// set: license-upgrade / road-rage / stunt-run intro banks + online voiceovers; consumed
// by BrnSound::Logic::SpeechEffect). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::speechdata::speechdata                    @ 0x8269CAE0  (ctor)
//   Attrib::Gen::speechdata::Num_LicenseUpgradeVoiceOvers  @ 0x82686DE0
//   Attrib::Gen::speechdata::Num_RoadRageIntros            @ 0x82686EA0
//   Attrib::Gen::speechdata::Num_StuntRunIntros            @ 0x82686F50
//   Attrib::Gen::speechdata::Num_StuntRunIntrosShort       @ 0x82686FA8
//   Attrib::Gen::speechdata::OnlineVoiceOvers              @ 0x82686E38
//
// Each Num_* accessor resolves the named array attribute into a stack-resident 16-byte
// Attrib::Attribute cursor via Instance::Get(pOut=&cursor, lpName=this, liArg=key), reads
// its element count via Attribute::GetLength(), tears the cursor down (the X360 reuses/
// types the cursor slot as a BaseCollisionGenerator), and returns the length. Per DWARF
// the Num_* accessors are `unsigned int … () const`; Get is non-const on Instance, so the
// const bodies const_cast `this`. OnlineVoiceOvers instead indexes the array's data block
// directly (Attrib::Private header count + 24-byte-stride element). Derives (privately)
// from Attrib::Instance, matching the committed sibling corpus.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"   // Attrib::Private (canonical)

namespace CgsSceneManager
{
namespace CgsCollision
{
    // Tears down the stack-resident Attrib::Attribute cursor the Num_* accessors build
    // (the X360 cursor buffer is reused/typed as a BaseCollisionGenerator at this call
    // site; the real destructor body lives in its own TU). Declaration-only under cl /c.
    void BaseCollisionGenerator_Destruct(void* lpThis);
}
}

namespace Attrib
{
namespace Gen
{
    class speechdata : private Instance
    {
    public:
        explicit speechdata(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
        explicit speechdata(const RefSpec& lrRefSpec, void* lpOwner = nullptr)
            : Instance(lrRefSpec, lpOwner) {}

        // speechdata.h:81; FirstTimeTips is a RefSpec array keyed by 0x564FC966.
        // The caller wraps the selected RefSpec as a languagestreamconfiguration.
        const RefSpec& FirstTimeTips(u32 luIndex) const;

        // DWARF: each is `unsigned int … () const` — the entry count of the named array.
        unsigned int Num_LicenseUpgradeVoiceOvers() const;  // @0x82686DE0
        unsigned int Num_RoadRageIntros() const;            // @0x82686EA0
        unsigned int Num_StuntRunIntros() const;            // @0x82686F50
        unsigned int Num_StuntRunIntrosShort() const;       // @0x82686FA8

        // Indexed accessor into the "OnlineVoiceOvers" array (24-byte-stride opaque
        // element; field layout not attested). @0x82686E38.
        void* OnlineVoiceOvers(u32 luIndex);
    };

    // X360 ctor @0x8269CAE0: chain the Instance ctor, assert the collection's class is
    // ClassName::speechdata (skip when unset/0), then give the instance a default data
    // area (0x188 bytes) if it has none.
    inline speechdata::speechdata(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_SPEECHDATA_CLASS = -130413976; // Attrib::ClassName::speechdata (0xF83A0A68)
        if (GetClass() != KI_SPEECHDATA_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_SPEECHDATA_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x188u);
    }

    inline const RefSpec& speechdata::FirstTimeTips(u32 luIndex) const
    {
        static const u64 KU_FIRST_TIME_TIPS_KEY = 0x564FC966ull;
        speechdata* lpSelf = const_cast<speechdata*>(this);
        const RefSpec* lpTip = static_cast<const RefSpec*>(
            lpSelf->GetAttributePointer(KU_FIRST_TIME_TIPS_KEY, luIndex));
        if (!lpTip)
            lpTip = static_cast<const RefSpec*>(DefaultDataArea(0x18u));
        return *lpTip;
    }

    // Shared Num_* body: resolve the named array attribute into a stack cursor, read its
    // length, tear the cursor down, and return the length. The attribute key is staged as
    // the low 32 bits of a 64-bit immediate; the high half is a dead upper word.
    inline unsigned int speechdata::Num_LicenseUpgradeVoiceOvers() const
    {
        // ⚠️ WIDENED 2026-07-31: Attrib::Instance::Get's key is 64 bits (the attribute
        // table hashes the whole doubleword). Only the LOW word of this key is recovered --
        // the X360 stages the high half with a separate lis/ori pair that was not recorded
        // when this accessor was reconstructed. Zero-extended so it cannot sign-extend into
        // garbage; FLAG: the lookup will still MISS until the high word is read back off the
        // call site (the same defect shotgroup::Num_ShotList and surfacelist::Num_Surfaces had).
        static const u64 KU_KEY = 0x3216CDCCull; // 840355276 (low word only)
        AttributeValue lCursor; // stack-resident Attrib::Attribute cursor (4 machine words)
        speechdata* lpSelf = const_cast<speechdata*>(this);
        Attribute* lpAttribute = reinterpret_cast<Attribute*>(
            lpSelf->Get(&lCursor, reinterpret_cast<int*>(lpSelf), KU_KEY));
        unsigned int luLength = static_cast<unsigned int>(lpAttribute->GetLength());
        CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lCursor);
        return luLength;
    }

    inline unsigned int speechdata::Num_RoadRageIntros() const
    {
        // ⚠️ WIDENED 2026-07-31: Attrib::Instance::Get's key is 64 bits (the attribute
        // table hashes the whole doubleword). Only the LOW word of this key is recovered --
        // the X360 stages the high half with a separate lis/ori pair that was not recorded
        // when this accessor was reconstructed. Zero-extended so it cannot sign-extend into
        // garbage; FLAG: the lookup will still MISS until the high word is read back off the
        // call site (the same defect shotgroup::Num_ShotList and surfacelist::Num_Surfaces had).
        static const u64 KU_KEY = 0x3E25C262ull; // 1042661986 (low word only)
        AttributeValue lCursor; // stack-resident Attrib::Attribute cursor (4 machine words)
        speechdata* lpSelf = const_cast<speechdata*>(this);
        Attribute* lpAttribute = reinterpret_cast<Attribute*>(
            lpSelf->Get(&lCursor, reinterpret_cast<int*>(lpSelf), KU_KEY));
        unsigned int luLength = static_cast<unsigned int>(lpAttribute->GetLength());
        CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lCursor);
        return luLength;
    }

    inline unsigned int speechdata::Num_StuntRunIntros() const
    {
        // ⚠️ WIDENED 2026-07-31: Attrib::Instance::Get's key is 64 bits (the attribute
        // table hashes the whole doubleword). Only the LOW word of this key is recovered --
        // the X360 stages the high half with a separate lis/ori pair that was not recorded
        // when this accessor was reconstructed. Zero-extended so it cannot sign-extend into
        // garbage; FLAG: the lookup will still MISS until the high word is read back off the
        // call site (the same defect shotgroup::Num_ShotList and surfacelist::Num_Surfaces had).
        static const u64 KU_KEY = 0xBC65B3FBull; // 3160781819 (low word only)
        AttributeValue lCursor; // stack-resident Attrib::Attribute cursor (4 machine words)
        speechdata* lpSelf = const_cast<speechdata*>(this);
        Attribute* lpAttribute = reinterpret_cast<Attribute*>(
            lpSelf->Get(&lCursor, reinterpret_cast<int*>(lpSelf), KU_KEY));
        unsigned int luLength = static_cast<unsigned int>(lpAttribute->GetLength());
        CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lCursor);
        return luLength;
    }

    inline unsigned int speechdata::Num_StuntRunIntrosShort() const
    {
        // ⚠️ WIDENED 2026-07-31: Attrib::Instance::Get's key is 64 bits (the attribute
        // table hashes the whole doubleword). Only the LOW word of this key is recovered --
        // the X360 stages the high half with a separate lis/ori pair that was not recorded
        // when this accessor was reconstructed. Zero-extended so it cannot sign-extend into
        // garbage; FLAG: the lookup will still MISS until the high word is read back off the
        // call site (the same defect shotgroup::Num_ShotList and surfacelist::Num_Surfaces had).
        static const u64 KU_KEY = 0xFE958E6Bull; // 4271214187 (low word only)
        AttributeValue lCursor; // stack-resident Attrib::Attribute cursor (4 machine words)
        speechdata* lpSelf = const_cast<speechdata*>(this);
        Attribute* lpAttribute = reinterpret_cast<Attribute*>(
            lpSelf->Get(&lCursor, reinterpret_cast<int*>(lpSelf), KU_KEY));
        unsigned int luLength = static_cast<unsigned int>(lpAttribute->GetLength());
        CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lCursor);
        return luLength;
    }

    // X360 @0x82686E38: index the "OnlineVoiceOvers" array directly. mpAttributeData (+4)
    // fronts an 8-byte Attrib::Private header; length = header->GetLength(); in range ->
    // header + 8 + 24*luIndex; else the shared 0x18-byte default element.
    inline void* speechdata::OnlineVoiceOvers(u32 luIndex)
    {
        Private* lpArrayHeader = reinterpret_cast<Private*>(mpAttributeData);
        if (luIndex >= lpArrayHeader->GetLength())
            return DefaultDataArea(0x18u);
        return reinterpret_cast<u8*>(lpArrayHeader) + 8 + 24 * luIndex;
    }
}
}
