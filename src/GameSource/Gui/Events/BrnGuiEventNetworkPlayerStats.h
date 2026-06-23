#pragma once

// ===================================================================================
// BrnGui::GuiEventNetworkPlayerStats  -- owning header
//   b5-decomp/src/GameSource/Gui/Events/BrnGuiEventNetworkPlayerStats.h
//
// The GUI event that carries one player's online stats snapshot: a NetworkPlayerStats
// record (the recovered base, homed in BrnNetworkPlayerStats.h) plus a small block of
// GUI-side fields the event adds on top (a slot/index word and a 16-byte display blob).
//
// Only operator= (@0x82485830) is binary-recovered for this TU. It chains the base
// NetworkPlayerStats::operator= and then member-wise copies the GUI-side fields. No
// prior source carries this struct (the DecFIGS DWARF forward-declares it only); the
// derived members are recovered from the X360 copy store sequence (base copy, then
// word @+0x88, then a 16-byte byte-copy @+0x8C relative to the object). Members are
// named and accessed by name.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"   // BrnNetwork::NetworkPlayerStats

namespace BrnGui
{
    struct GuiEventNetworkPlayerStats : public BrnNetwork::NetworkPlayerStats
    {
        // GUI-side fields appended after the base record. The X360 operator= copies a
        // word at object+0x88 (the first byte past the 136-byte base record) and then a
        // 16-byte run at object+0x8C.
        s32 miSlotIndex;        // object +0x88 (== base + 0)
        u8  maDisplayData[16];  // object +0x8C (== base + 4)

        // Owned by THIS TU: member-wise copy assignment (@0x82485830).
        GuiEventNetworkPlayerStats& operator=(const GuiEventNetworkPlayerStats& lOther);
    };
}
