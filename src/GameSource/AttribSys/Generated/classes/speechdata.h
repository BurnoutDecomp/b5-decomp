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

    // Shared Num_* body: resolve the named array attribute into a stack cursor, read its
    // length, tear the cursor down, and return the length. The attribute key is staged as
    // the low 32 bits of a 64-bit immediate; the high half is a dead upper word.
    inline unsigned int speechdata::Num_LicenseUpgradeVoiceOvers() const
    {
        static const int KI_KEY = static_cast<int>(0x3216CDCCu); // 840355276
        AttributeValue lCursor;
        speechdata* lpSelf = const_cast<speechdata*>(this);
        Attribute* lpAttribute = reinterpret_cast<Attribute*>(
            lpSelf->Get(&lCursor, reinterpret_cast<int*>(lpSelf), KI_KEY));
        unsigned int luLength = static_cast<unsigned int>(lpAttribute->GetLength());
        CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lCursor);
        return luLength;
    }

    inline unsigned int speechdata::Num_RoadRageIntros() const
    {
        static const int KI_KEY = 1042661986; // 0x3E25C262
        AttributeValue lCursor;
        speechdata* lpSelf = const_cast<speechdata*>(this);
        Attribute* lpAttribute = reinterpret_cast<Attribute*>(
            lpSelf->Get(&lCursor, reinterpret_cast<int*>(lpSelf), KI_KEY));
        unsigned int luLength = static_cast<unsigned int>(lpAttribute->GetLength());
        CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lCursor);
        return luLength;
    }

    inline unsigned int speechdata::Num_StuntRunIntros() const
    {
        static const int KI_KEY = -1134185477; // 0xBC65B3FB (u32 3160781819)
        AttributeValue lCursor;
        speechdata* lpSelf = const_cast<speechdata*>(this);
        Attribute* lpAttribute = reinterpret_cast<Attribute*>(
            lpSelf->Get(&lCursor, reinterpret_cast<int*>(lpSelf), KI_KEY));
        unsigned int luLength = static_cast<unsigned int>(lpAttribute->GetLength());
        CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lCursor);
        return luLength;
    }

    inline unsigned int speechdata::Num_StuntRunIntrosShort() const
    {
        static const int KI_KEY = -23753109; // 0xFE958E6B == 4271214187
        AttributeValue lCursor;
        speechdata* lpSelf = const_cast<speechdata*>(this);
        Attribute* lpAttribute = reinterpret_cast<Attribute*>(
            lpSelf->Get(&lCursor, reinterpret_cast<int*>(lpSelf), KI_KEY));
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
