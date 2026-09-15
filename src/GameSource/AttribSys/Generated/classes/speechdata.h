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

        // ---- SOUND-C 2026-09-15: the rest of the schema SpeechEffect::Notify reads ----
        // Every key below is the FULL 64-bit attribute key, read off the
        // `ori r4,rN,<lo>` / `insrdi r4,rN,32,0` pair at the named Notify call site.
        unsigned int Num_RoadRageIntrosShort() const;       // @0x82686EF8 (Notify 0x826E86F4)

        // RefSpec arrays (24-byte elements; GetAttributePointer + DefaultDataArea(0x18)
        // fallback, exactly as Notify does it).
        const RefSpec& LicenseUpgradeVoiceOvers(u32 luIndex) const;  // Notify @0x826E8098
        const RefSpec& RoadRageIntros(u32 luIndex) const;            // Notify @0x826E8730
        const RefSpec& RoadRageIntrosShort(u32 luIndex) const;       // Notify @0x826E8798
        const RefSpec& StuntRunIntros(u32 luIndex) const;            // Notify @0x826E887C
        const RefSpec& StuntRunIntrosShort(u32 luIndex) const;       // Notify @0x826E8814

        // Five per-game-mode "you lost again" RefSpecs and the Atomika free-burn VO
        // collection are EMBEDDED RefSpec fields, not array attributes: Notify reaches
        // them as `mSpeechData.mpAttributeData + <byte offset>` and hands the address
        // straight to Attrib::Instance::ChangeWithDefault / the languagestreamcollection
        // ctor (0x826E7EA0..0x826E7F40 and 0x826E7D40).  The offsets are 24 apart, i.e.
        // one RefSpec each, and all sit inside the 0x188-byte speechdata data area.
        const RefSpec& StuntRunLostVoiceOvers()      const { return EmbeddedRefSpec(248); }
        const RefSpec& RoadRageLostVoiceOvers()      const { return EmbeddedRefSpec(272); }
        const RefSpec& RaceLostVoiceOvers()          const { return EmbeddedRefSpec(296); }
        const RefSpec& MarkedManLostVoiceOvers()     const { return EmbeddedRefSpec(320); }
        const RefSpec& BurningRouteLostVoiceOvers()  const { return EmbeddedRefSpec(344); }
        const RefSpec& AtomikaFreeburnVos()          const { return EmbeddedRefSpec(368); }

    private:
        // `lwz r11, 0x74(r31)` (mSpeechData.mpAttributeData) + `addi rX, r11, <off>`.
        const RefSpec& EmbeddedRefSpec(u32 luByteOffset) const
        {
            return *reinterpret_cast<const RefSpec*>(
                static_cast<const u8*>(mpAttributeData) + luByteOffset);
        }
    public:
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
        static const u64 KU_FIRST_TIME_TIPS_KEY = 0x9DFB2D73564FC966ull;
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
        // ⭐ KEY WIDTH PAID 2026-09-15 (SOUND-C): the full 64-bit key is staged by the
        // `ori r4,rN,<lo>` + `insrdi r4,rN,32,0` pair at SpeechEffect::Notify's own call
        // sites (see the per-key address below). The old low-word-only key MISSED.
        static const u64 KU_KEY = 0xDC7D75833216CDCCull; // Attrib::Hash::speechdata::LicenseUpgradeVoiceOvers
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
        // ⭐ KEY WIDTH PAID 2026-09-15 (SOUND-C): the full 64-bit key is staged by the
        // `ori r4,rN,<lo>` + `insrdi r4,rN,32,0` pair at SpeechEffect::Notify's own call
        // sites (see the per-key address below). The old low-word-only key MISSED.
        static const u64 KU_KEY = 0x214BFB693E25C262ull; // Attrib::Hash::speechdata::RoadRageIntros
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
        // ⭐ KEY WIDTH PAID 2026-09-15 (SOUND-C): the full 64-bit key is staged by the
        // `ori r4,rN,<lo>` + `insrdi r4,rN,32,0` pair at SpeechEffect::Notify's own call
        // sites (see the per-key address below). The old low-word-only key MISSED.
        static const u64 KU_KEY = 0x0858BBD8BC65B3FBull; // Attrib::Hash::speechdata::StuntRunIntros
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
        // ⭐ KEY WIDTH PAID 2026-09-15 (SOUND-C): the full 64-bit key is staged by the
        // `ori r4,rN,<lo>` + `insrdi r4,rN,32,0` pair at SpeechEffect::Notify's own call
        // sites (see the per-key address below). The old low-word-only key MISSED.
        static const u64 KU_KEY = 0x5C56AC73FE958E6Bull; // Attrib::Hash::speechdata::StuntRunIntrosShort
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

    // ---- SOUND-C bodies ----------------------------------------------------------
    inline unsigned int speechdata::Num_RoadRageIntrosShort() const
    {
        static const u64 KU_KEY = 0x91D3F2CAF1C8CEBCull; // Notify @0x826E86F4/0x826E8704
        AttributeValue lCursor;
        speechdata* lpSelf = const_cast<speechdata*>(this);
        Attribute* lpAttribute = reinterpret_cast<Attribute*>(
            lpSelf->Get(&lCursor, reinterpret_cast<int*>(lpSelf), KU_KEY));
        unsigned int luLength = static_cast<unsigned int>(lpAttribute->GetLength());
        CgsSceneManager::CgsCollision::BaseCollisionGenerator_Destruct(&lCursor);
        return luLength;
    }

    // The shared RefSpec-array body Notify open-codes at every one of these call sites:
    //   lpSpec = GetAttributePointer(key, index);  if (!lpSpec) lpSpec = DefaultDataArea(0x18);
    inline const RefSpec& speechdata::LicenseUpgradeVoiceOvers(u32 luIndex) const
    {
        static const u64 KU_KEY = 0xDC7D75833216CDCCull;
        speechdata* lpSelf = const_cast<speechdata*>(this);
        const RefSpec* lpSpec = static_cast<const RefSpec*>(
            lpSelf->GetAttributePointer(KU_KEY, luIndex));
        if (!lpSpec)
            lpSpec = static_cast<const RefSpec*>(DefaultDataArea(0x18u));
        return *lpSpec;
    }

    inline const RefSpec& speechdata::RoadRageIntros(u32 luIndex) const
    {
        static const u64 KU_KEY = 0x214BFB693E25C262ull;
        speechdata* lpSelf = const_cast<speechdata*>(this);
        const RefSpec* lpSpec = static_cast<const RefSpec*>(
            lpSelf->GetAttributePointer(KU_KEY, luIndex));
        if (!lpSpec)
            lpSpec = static_cast<const RefSpec*>(DefaultDataArea(0x18u));
        return *lpSpec;
    }

    inline const RefSpec& speechdata::RoadRageIntrosShort(u32 luIndex) const
    {
        static const u64 KU_KEY = 0x91D3F2CAF1C8CEBCull;
        speechdata* lpSelf = const_cast<speechdata*>(this);
        const RefSpec* lpSpec = static_cast<const RefSpec*>(
            lpSelf->GetAttributePointer(KU_KEY, luIndex));
        if (!lpSpec)
            lpSpec = static_cast<const RefSpec*>(DefaultDataArea(0x18u));
        return *lpSpec;
    }

    inline const RefSpec& speechdata::StuntRunIntros(u32 luIndex) const
    {
        static const u64 KU_KEY = 0x0858BBD8BC65B3FBull;
        speechdata* lpSelf = const_cast<speechdata*>(this);
        const RefSpec* lpSpec = static_cast<const RefSpec*>(
            lpSelf->GetAttributePointer(KU_KEY, luIndex));
        if (!lpSpec)
            lpSpec = static_cast<const RefSpec*>(DefaultDataArea(0x18u));
        return *lpSpec;
    }

    inline const RefSpec& speechdata::StuntRunIntrosShort(u32 luIndex) const
    {
        static const u64 KU_KEY = 0x5C56AC73FE958E6Bull;
        speechdata* lpSelf = const_cast<speechdata*>(this);
        const RefSpec* lpSpec = static_cast<const RefSpec*>(
            lpSelf->GetAttributePointer(KU_KEY, luIndex));
        if (!lpSpec)
            lpSpec = static_cast<const RefSpec*>(DefaultDataArea(0x18u));
        return *lpSpec;
    }
}
}
