#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"   // CgsUnicode::CgsUtf8 (Prepare/SetText/GetText)

// Pointer-only collaborators (forward-declared to avoid a transitive header
// cascade through the apt/font stack; only pointers/references are used here):
struct RGBA;                          // Construct/mpAlternateTextColours referent
struct AptAllocateStringParameters;   // filled by the caller, consumed by Prepare
namespace CgsGui { struct FontCollection; }

// CgsGui::CgsAptString - the apt/Flash text-object wrapper a BrnFlapt::TextFieldRef
// reaches through. The leading two words form an identity/type tag that, while the
// object is still in its default (un-set-up) state, equals a pair of module-static
// sentinel words; GetText() asserts the tag has moved off that sentinel before it
// hands back the resolved text pointer it caches at mpText.
//
// Layout from BURNOUT_X360_ARTIST.XEX @ 0x8246D9A0 (GetText): the tag pair is read
// at +0x00/+0x04 and compared against dword_82FB3C50 / dword_82FB3C54, and the text
// pointer is returned from +0x74. Path/line from the baked assert string
// (GameShared/GameClasses/Gui/View/AptInterface/CgsAptString.h:129).
//
// X360 total size is 0x84: the embedded text object (the +0x00 tag pair + cached
// text @ +0x74) spans 0x7C, then the two "alternate text colours" fields the X360
// Construct (@0x82851EF8) stores at +0x7C / +0x80 (mpAlternateTextColours,
// miNumAlternateColours). Members are accessed BY NAME; the PC (x64) layout differs
// in pointer width, so no byte-size static_assert is asserted here (the X360 offsets
// are recorded in comments per the project's x64 semantic-parity rule).
namespace CgsGui
{
    class CgsAptString
    {
    public:
        // The cached resolved text handle the apt object resolved to. The X360 stores a
        // 32-bit pointer and hands it back by value to BrnFlapt::TextFieldRef::GetText,
        // which treats it as an opaque word; modelled as a 32-bit handle so the on-disk
        // +0x74 offset is host-pointer-width independent.
        typedef u32 TextHandle;

        // Text-effect mode passed to Prepare (DWARF CgsAptString.h:66). The flapt text
        // path requests E_EFFECT_NONE.
        enum ETextEffects
        {
            E_EFFECT_NONE       = 0,
            E_EFFECT_DROPSHADOW = 1,
            E_EFFECT_GRADIENT   = 2,
            E_EFFECT_EMBOSSED   = 3,
            E_EFFECT_ALL        = 4
        };

        // @ 0x82851EF8 - cache the alternate-text-colour table (lpColours/liNumColours)
        // then construct the embedded text object. DWARF CgsAptString.h:81.
        void Construct(const RGBA* lpColours, s32 liNumColours);

        // @ 0x82854A40 - lay the parametrised string (lpParams->szString) out into
        // lpStringBuffer using lpFonts, the requested effect, and a size scale, and
        // cache the result on this string object. DWARF CgsAptString.h:91.
        void Prepare(const FontCollection* lpFonts,
                     AptAllocateStringParameters* lpParams,
                     CgsUnicode::CgsUtf8* lpStringBuffer,
                     ETextEffects eEffect,
                     f32 lfSizeScale);

        // @ 0x8246D9A0 - assert the object has been set up (its leading type-tag pair
        // is no longer the un-set-up sentinel pair) and return the cached text handle.
        TextHandle GetText() const;

    private:
        // Leading identity/type-tag words. While the object is un-set-up these equal
        // the module sentinel pair (KU_UNSET_TAG_HI / KU_UNSET_TAG_LO at the matching
        // offsets); GetText() fires its assert when they still match.
        u32     muTypeTag0;     // +0x00
        u32     muTypeTag1;     // +0x04

        // Opaque apt-object body up to the cached text pointer at +0x74. Modelled as a
        // byte span so the cached-text offset is reproduced exactly without inventing
        // the intervening apt fields.
        u8      maBody[0x74 - 0x08];    // +0x08 .. +0x73

        TextHandle muText;      // +0x74 - cached resolved text handle (32-bit pointer)

        u8      maBodyTail[0x7C - 0x78];   // +0x78 .. +0x7B  (rest of the embedded text object)

        // The alternate-text-colour table the X360 Construct caches (mpAlternateTextColours
        // @ +0x7C, miNumAlternateColours @ +0x80). DWARF CgsAptString.h:106/107.
        const RGBA* mpAlternateTextColours;   // +0x7C
        s32         miNumAlternateColours;    // +0x80

    public:
        // The un-set-up sentinel the leading tag pair holds before SetUp(). Defined in
        // the .cpp from the module-static pair the X360 compares against.
        static const u32 KU_UNSET_TAG0;
        static const u32 KU_UNSET_TAG1;
    };
}
