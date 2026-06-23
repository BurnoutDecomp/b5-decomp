#pragma once

// ===================================================================================
// BrnGui::TextField  -- owning header
//   b5-decomp/src/GameSource/Gui/BrnGuiTextField.h
//
// A GUI text field: a renderable string slot with an associated colour, a small
// formatted-value scratch buffer, and a secondary text region, plus per-field dirty
// flags. Embedded by value inside map-icon GUI elements (e.g. CrashNavMapIcon, whose
// operator= copies its TextField member).
//
// Layout proven from BURNOUT_X360_ARTIST.XEX:
//   * operator= @0x824470F0 - copies the field byte-for-byte EXCEPT the leading 4 bytes
//       (the copy anchor is this+0x04): a 128-byte block @+0x04..+0x83, four words
//       @+0x84/+0x88/+0x8C/+0x90, a 16-byte block @+0x94..+0xA3, a 128-byte block
//       @+0xA4..+0x123, then three bytes @+0x124/+0x125/+0x126. The +0x00 word is
//       intentionally NOT assigned (a construction-time slot preserved across copy).
//   * SetColour @0x82481E48 - stores the colour word @+0x8C, formats it as "%u" (max 15
//       chars) into the 16-byte buffer @+0x94, clears the buffer's last byte @+0xA3, and
//       raises the dirty flag @+0x124.
//
// sizeof == 0x128 (0x127 last touched byte +1, word-aligned). All access is by name; the
// fixed string regions are modelled as named char buffers at their proven offsets.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf (SetColour)

namespace BrnGui
{
    class TextField
    {
    public:
        // Length of the formatted-value scratch buffer at +0x94 (SetColour passes 15 as the
        // SPrintf capacity and clears index 15, the last byte, so the buffer is 16 bytes).
        static const u32 KU_VALUE_BUFFER_SIZE = 16;

        // @0x82481E48 - set the field's colour and refresh its displayed value. Stores
        // luColour, formats it as an unsigned decimal string into the value buffer, clears
        // a secondary flag and marks the field dirty.
        void SetColour(u32 luColour);

        // @0x824470F0 - copy-assign from lrSource. Reproduces the X360 byte-copy, which
        // copies everything from +0x04 onward and leaves the +0x00 slot untouched.
        TextField& operator=(const TextField& lrSource);

    private:
        // +0x00: a 4-byte construction-time slot the X360 operator= deliberately does NOT
        // copy (the byte-copy anchor is this+0x04). Named so it is preserved across copy.
        u32  muReservedHead;        // +0x00

        char maText[128];           // +0x04 .. +0x83  (primary text region)
        u32  muFieldA;              // +0x84
        u32  muFieldB;              // +0x88
        u32  muColour;              // +0x8C  (set by SetColour)
        u32  muFieldD;              // +0x90
        char maValueText[16];       // +0x94 .. +0xA3  (formatted-value scratch buffer)
        char maSecondaryText[128];  // +0xA4 .. +0x123 (secondary text region)
        u8   mbDirty;               // +0x124 (raised by SetColour)
        u8   mbFlagB;               // +0x125
        u8   mbFlagC;               // +0x126
    };
}
