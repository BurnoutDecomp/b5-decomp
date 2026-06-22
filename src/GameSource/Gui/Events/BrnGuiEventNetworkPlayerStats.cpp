// ===================================================================================
// BrnGui::GuiEventNetworkPlayerStats  -- implementation
//   class:BrnGui::GuiEventNetworkPlayerStats
//
// operator= @ 0x82485830
//   Chains the base NetworkPlayerStats::operator= then member-wise copies the GUI-side
//   fields the event adds (the slot/index word and the 16-byte display blob).
//   Reconstructed from the X360 pseudocode/asm: base assign, then word @+0x88, then a
//   16-byte byte-copy @+0x8C. The base tail word @+0x84 (mbIsCalulated) is copied by the
//   base operator= itself (its reconstruction is a full member-wise copy that includes
//   the trailing status/flag block), so the derived does not re-copy it -- mbIsCalulated
//   is a private base member and is correctly assigned through the base operator= call.
// ===================================================================================
#include "GameSource/Gui/Events/BrnGuiEventNetworkPlayerStats.h"

namespace BrnGui
{
    // @ 0x82485830
    GuiEventNetworkPlayerStats&
    GuiEventNetworkPlayerStats::operator=(const GuiEventNetworkPlayerStats& lOther)
    {
        // Base record (the recovered NetworkPlayerStats member-wise copy). This already
        // assigns the full base object including the tail status/flag block at +0x84
        // (mbIsCalulated), so the base +0x84 word is covered here and is not (and cannot
        // be) re-copied below -- mbIsCalulated is a private member of the base.
        BrnNetwork::NetworkPlayerStats::operator=(lOther);

        // GUI-side fields appended by this event.
        miSlotIndex = lOther.miSlotIndex;       // word @ object +0x88

        for (s32 li = 0; li < 16; ++li)         // byte-copy @ object +0x8C, 16 bytes
        {
            maDisplayData[li] = lOther.maDisplayData[li];
        }

        return *this;
    }
}
