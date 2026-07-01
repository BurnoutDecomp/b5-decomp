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
}
}
