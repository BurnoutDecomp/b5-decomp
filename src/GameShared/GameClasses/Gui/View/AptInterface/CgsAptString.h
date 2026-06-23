#pragma once

#include "types.hpp"

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

    public:
        // The un-set-up sentinel the leading tag pair holds before SetUp(). Defined in
        // the .cpp from the module-static pair the X360 compares against.
        static const u32 KU_UNSET_TAG0;
        static const u32 KU_UNSET_TAG1;
    };

    static_assert(sizeof(CgsAptString) == 0x78, "CgsAptString must be 0x78 bytes (text at +0x74)");
}
