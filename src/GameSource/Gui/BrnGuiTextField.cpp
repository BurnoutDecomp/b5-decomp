// ===================================================================================
// BrnGui::TextField  -- implementation
//   class:BrnGui::TextField
//
//   SetColour  @0x82481E48
//   operator=  @0x824470F0
//
// Reconstructed store-for-store from the X360 pseudocode/asm. Member access is by name.
// ===================================================================================
#include "GameSource/Gui/BrnGuiTextField.h"

#include <cstring>   // std::memcpy (byte-exact copy of the fixed text regions)

namespace BrnGui
{
    // @0x82481E48
    void TextField::SetColour(u32 luColour)
    {
        // Store the colour word (X360 stw r6, 0x8C(r31)).
        muColour = luColour;

        // Format the colour as an unsigned decimal string into the value buffer. The X360
        // passes 15 as the SPrintf length (one less than the 16-byte buffer) and the format
        // string "%u" (CgsCore::SPrintf @ this+0x94).
        CgsCore::SPrintf(maValueText, KU_VALUE_BUFFER_SIZE - 1, "%u", luColour);

        // Clear the buffer's final byte (X360 stb r11=0, 0xA3(r31)) and mark the field dirty
        // (X360 stb r10=1, 0x124(r31)).
        maValueText[KU_VALUE_BUFFER_SIZE - 1] = 0;
        mbDirty                               = 1;
    }

    // @0x824470F0
    TextField& TextField::operator=(const TextField& lrSource)
    {
        // The X360 copies every byte from +0x04 onward and leaves the +0x00 slot
        // (muReservedHead) untouched. Reproduced here as the same named-region copies:
        //   +0x04..+0x83   maText            (128-byte block)
        //   +0x84/+88/+8C/+90  the four words
        //   +0x94..+0xA3   maValueText       (16-byte block)
        //   +0xA4..+0x123  maSecondaryText   (128-byte block)
        //   +0x124/+0x125/+0x126  the three flag bytes
        std::memcpy(maText, lrSource.maText, sizeof(maText));
        muFieldA = lrSource.muFieldA;
        muFieldB = lrSource.muFieldB;
        muColour = lrSource.muColour;
        muFieldD = lrSource.muFieldD;
        std::memcpy(maValueText, lrSource.maValueText, sizeof(maValueText));
        std::memcpy(maSecondaryText, lrSource.maSecondaryText, sizeof(maSecondaryText));
        mbDirty  = lrSource.mbDirty;
        mbFlagB  = lrSource.mbFlagB;
        mbFlagC  = lrSource.mbFlagC;

        return *this;
    }
}
