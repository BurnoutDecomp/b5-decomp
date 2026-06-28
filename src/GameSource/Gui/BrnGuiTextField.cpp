// ===================================================================================
// BrnGui::TextField  -- implementation
//   class:BrnGui::TextField
//
//   SetColour  @0x82481E48
//   operator=  @0x824470F0
//
// Reconstructed store-for-store from the X360 pseudocode/asm. Member access is by name.
// TextField derives from CgsGui::GuiComponent (see BrnGuiTextField.h); its base name
// region + own members are what the X360 byte-copy at this+0x04 reproduces.
// ===================================================================================
#include "GameSource/Gui/BrnGuiTextField.h"

#include <cstring>   // std::memcpy (byte-exact copy of the fixed text regions)

namespace BrnGui
{
    // @0x82481E48
    void TextField::SetColour(u32 luColour)
    {
        // Store the colour word (X360 stw r6, 0x8C(r31)).
        muTextColour = luColour;

        // Format the colour as an unsigned decimal string into the colour buffer. The X360
        // passes 15 as the SPrintf length (one less than the 16-byte buffer) and the format
        // string "%u" (CgsCore::SPrintf @ this+0x94).
        CgsCore::SPrintf(macColour, KU_MAX_COLOUR_LEN - 1, "%u", luColour);

        // Clear the buffer's final byte (X360 stb r11=0, 0xA3(r31)) and flag the field as
        // using a colour (X360 stb r10=1, 0x124(r31)).
        macColour[KU_MAX_COLOUR_LEN - 1] = 0;
        mbUseColour                      = true;
    }

    // @0x824470F0
    TextField& TextField::operator=(const TextField& lrSource)
    {
        // The X360 copies every byte from +0x04 onward and leaves the +0x00 vtable slot
        // untouched. From +0x04 that is the inherited GuiComponent name region, then this
        // field's own members:
        //   +0x04..+0x83   macName            (base; 128-byte block)
        //   +0x84/+0x88    muHashedName / mpStateInterface (base words)
        //   +0x8C/+0x90    muTextColour / miScroll
        //   +0x94..+0xA3   macColour          (16-byte block)
        //   +0xA4..+0x123  macText            (128-byte block)
        //   +0x124/+0x125/+0x126  the three flag bytes
        std::memcpy(macName, lrSource.macName, sizeof(macName));
        muHashedName     = lrSource.muHashedName;
        mpStateInterface = lrSource.mpStateInterface;
        muTextColour     = lrSource.muTextColour;
        miScroll         = lrSource.miScroll;
        std::memcpy(macColour, lrSource.macColour, sizeof(macColour));
        std::memcpy(macText, lrSource.macText, sizeof(macText));
        mbUseColour   = lrSource.mbUseColour;
        mbResetScroll = lrSource.mbResetScroll;
        mbAutosize    = lrSource.mbAutosize;

        return *this;
    }
}
