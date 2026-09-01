#pragma once

// Attrib::Gen::burnoutglobaldata — generated AttribSys class (the top-level game-global
// sound/gameplay attribute table: shift patterns, reverb settings, passby bins, stream
// mappings, HUD messages, world emitter list, etc). Reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU carries the ctor plus three indexed element-getter
// accessors that reach directly into the instance layout block (mpAttributeData):
//   burnoutglobaldata::burnoutglobaldata @ 0x82695938  (ctor)
//   burnoutglobaldata::mPassbyBins       @ 0x826820B8  (GetLength @+0x288, data @+0x290)
//   burnoutglobaldata::ReverbSettings    @ 0x82682120  (GetLength @+0x178, element base 24*(i+16))
//   burnoutglobaldata::ShiftPatterns     @ 0x82682188  (GetLength @+0,     data @+8)
//
// Each accessor is the generated bounds-checked array-attribute idiom: query the array
// length via Attrib::Private::GetLength() at a per-attribute byte offset into the
// instance's layout block (mpAttributeData, +4 in Attrib::Instance), and return either
// the requested element (24-byte stride) or the shared 24-byte DefaultDataArea fallback
// when the index is out of range. Field layout of the 24-byte element is NOT attested —
// only its stride. Derives from Attrib::Instance (same generated-ctor pattern as
// surfacelist/propscrashbinlist).
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"   // Attrib::Private (canonical)
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"

namespace Attrib
{
namespace Gen
{
    class burnoutglobaldata : private Instance
    {
    public:
        explicit burnoutglobaldata(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // Bounds-checked accessors over generated array attributes. Each returns a
        // pointer to element luIndex (24-byte stride) within the instance's layout
        // block, or the shared 24-byte default block when luIndex is out of range.
        void* mPassbyBins(u32 luIndex);     // @0x826820B8
        void* ReverbSettings(u32 luIndex);  // @0x82682120
        void* ShiftPatterns(u32 luIndex);   // @0x82682188

        // Named access to the three RefSpecs used by the global audio objects.
        // The offsets are the generated _LayoutStruct positions from DecFIGS and
        // the ARTIST call sites (Presentation +0x4d8, StreamMappings +0x458,
        // SpeechData +0x470).
        RefSpec PresentationActions() const;
        const RefSpec& StreamMappings() const;
        const RefSpec& SpeechData() const;
        const RefSpec& WorldEmitterList() const;
        const RefSpec& InAirCrashBin() const;
        const RefSpec& GlobalEngineData() const;
        u64 CollisionCrashBinListKey() const;
        u64 PropsCrashBinListKey() const;
        u64 PropToMaterialMappingsKey() const;
        const RefSpec& SampleTags(u32 luIndex) const;

        // The sound module is constructed before BurnoutGlobalData.bin is
        // registered. Rebind the generated instance after the AttribSys load
        // completion callback, matching SoundLogicModule::ResourcesAreReady.
        bool ResolveLoadedCollection();
    };

    // X360 ctor @0x82695938: chain the Instance ctor, then give the instance a default
    // data area (0x5C0 bytes) if construction left it without one. The class key is staged
    // as the low word of a 64-bit immediate (0x03FAC7F3_52E51383) — only the low word
    // (0x52E51383 == 1390744451) is the class id, matching the sibling key-staging pattern.
    // (The X360 body resolves the base collection from that key via FindCollection; the
    // committed sibling recons surfacelist/propscrashbinlist pass the caller's Collection*
    // through instead, which compiles and preserves the DefaultDataArea/class-id semantics.)
    inline burnoutglobaldata::burnoutglobaldata(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_BURNOUTGLOBALDATA_CLASS = static_cast<int>(1390744451u); // 0x52E51383
        (void)KI_BURNOUTGLOBALDATA_CLASS;
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x5C0u);
    }

    // X360 @0x826820B8: mPassbyBins array accessor.
    //   r30 = mpAttributeData (lwz r30,4(r3)); length = GetLength(mpAttributeData+0x288);
    //   if luIndex >= length -> DefaultDataArea(0x18); else mpAttributeData + 0x290 + 24*luIndex.
    // (slwi r11,idx,1; add r11,idx,r11 => idx*3; slwi r11,r11,3 => idx*24.)
    inline void* burnoutglobaldata::mPassbyBins(u32 luIndex)
    {
        u8* lpData = static_cast<u8*>(mpAttributeData);
        if (luIndex >= reinterpret_cast<const Private*>(lpData + 0x288)->GetLength())
            return DefaultDataArea(0x18u);
        return lpData + 0x290 + luIndex * 0x18u;
    }

    // X360 @0x82682120: ReverbSettings. length = GetLength(mpAttributeData+0x178);
    //   element = mpAttributeData + 24*(luIndex+16)  [addi r11,idx,0x10 before the *24].
    inline void* burnoutglobaldata::ReverbSettings(u32 luIndex)
    {
        u8* lpData = static_cast<u8*>(mpAttributeData);
        if (luIndex >= reinterpret_cast<const Private*>(lpData + 0x178)->GetLength())
            return DefaultDataArea(0x18u);
        return lpData + (luIndex + 16u) * 0x18u;
    }

    // X360 @0x82682188: ShiftPatterns. length = GetLength(mpAttributeData);
    //   element = mpAttributeData + 24*luIndex + 8.
    inline void* burnoutglobaldata::ShiftPatterns(u32 luIndex)
    {
        u8* lpData = static_cast<u8*>(mpAttributeData);
        if (luIndex >= reinterpret_cast<const Private*>(lpData)->GetLength())
            return DefaultDataArea(0x18u);
        return lpData + luIndex * 0x18u + 8;
    }

    inline RefSpec burnoutglobaldata::PresentationActions() const
    {
        // PresentationEffect::Attach @ ARTIST 0x8269E2F0 constructs this
        // reference explicitly: the generated class key is an immediate and
        // only the collection key is loaded from BurnoutGlobalData +0x4D8.
        // Treating +0x4D8 as the start of a generic RefSpec shifts the key by
        // one qword and resolves an unrelated following attribute.
        static const u64 KU_PRESENTATION_ACTION_LIST_CLASS =
            0x781A45228B2D1E2Full;
        const u64 luCollectionKey = *reinterpret_cast<const u64*>(
            static_cast<const u8*>(mpAttributeData) + 0x4D8u);
        return RefSpec(KU_PRESENTATION_ACTION_LIST_CLASS, luCollectionKey);
    }

    inline const RefSpec& burnoutglobaldata::StreamMappings() const
    {
        return *reinterpret_cast<const RefSpec*>(
            static_cast<const u8*>(mpAttributeData) + 0x458u);
    }

    inline const RefSpec& burnoutglobaldata::SpeechData() const
    {
        return *reinterpret_cast<const RefSpec*>(
            static_cast<const u8*>(mpAttributeData) + 0x470u);
    }

    inline const RefSpec& burnoutglobaldata::WorldEmitterList() const
    {
        return *reinterpret_cast<const RefSpec*>(
            static_cast<const u8*>(mpAttributeData) + 0x488u);
    }

    inline const RefSpec& burnoutglobaldata::InAirCrashBin() const
    {
        // InAirEffect::Attach @ ARTIST 0x826F46F8 reads the RefSpec at the
        // BurnoutGlobalData layout's fixed +0x4E8 slot.
        return *reinterpret_cast<const RefSpec*>(
            static_cast<const u8*>(mpAttributeData) + 0x4E8u);
    }

    inline const RefSpec& burnoutglobaldata::GlobalEngineData() const
    {
        // Brn3DEffectControl::Prepare @ ARTIST 0x82696870 reads this RefSpec.
        return *reinterpret_cast<const RefSpec*>(
            static_cast<const u8*>(mpAttributeData) + 0x518u);
    }

    inline u64 burnoutglobaldata::CollisionCrashBinListKey() const
    {
        // CollisionStateManager::Prepare @ ARTIST 0x826F8B78 loads the
        // collection key from the global layout's +0x538 slot.
        return *reinterpret_cast<const u64*>(
            static_cast<const u8*>(mpAttributeData) + 0x538u);
    }

    inline u64 burnoutglobaldata::PropsCrashBinListKey() const
    {
        // Same call stages the props-crash list key from +0x4C0.
        return *reinterpret_cast<const u64*>(
            static_cast<const u8*>(mpAttributeData) + 0x4C0u);
    }

    inline u64 burnoutglobaldata::PropToMaterialMappingsKey() const
    {
        // Same call stages the prop-material mapping key from +0x4A8.
        return *reinterpret_cast<const u64*>(
            static_cast<const u8*>(mpAttributeData) + 0x4A8u);
    }

    inline const RefSpec& burnoutglobaldata::SampleTags(u32 luIndex) const
    {
        // SoundLogicModule::GetSampleTags @ ARTIST 0x82683900: five RefSpecs,
        // variable-array header at +0xF8 and first 24-byte entry at +0x100.
        CGS_ASSERT(luIndex < 5u,
                   "leSampleTags < AttribSys::Enums::eSampleTags::SampleTagCount");
        const u8* lpData = static_cast<const u8*>(mpAttributeData);
        if (luIndex >= reinterpret_cast<const Private*>(lpData + 0x0F8u)->GetLength())
            return *static_cast<const RefSpec*>(DefaultDataArea(sizeof(RefSpec)));
        return *reinterpret_cast<const RefSpec*>(lpData + 0x100u + luIndex * sizeof(RefSpec)); // serialized AttribSys array
    }

    inline bool burnoutglobaldata::ResolveLoadedCollection()
    {
        static const u64 KU_BURNOUTGLOBALDATA_CLASS_KEY =
            0x03FAC7F352E51383ull;
        // BurnoutGlobalData.bin contains one collection for this class.  The
        // X360 constructor receives this key through r4 (the runtime
        // mGlobalDataKey); it is not the zero/default collection.  The value is
        // read directly from that vault's CollectionLoadData export.
        static const u64 KU_BURNOUTGLOBALDATA_COLLECTION_KEY =
            0x34690FE28DBD2FEFull;
        Collection* lpCollection =
            FindCollection(KU_BURNOUTGLOBALDATA_CLASS_KEY,
                           KU_BURNOUTGLOBALDATA_COLLECTION_KEY);
        Change(lpCollection);
        return lpCollection != nullptr;
    }
}
}
