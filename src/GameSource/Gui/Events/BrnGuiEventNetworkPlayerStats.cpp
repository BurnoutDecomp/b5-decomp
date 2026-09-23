// ===================================================================================
// BrnGui::GuiEventNetworkPlayerStats  -- implementation
//   class:BrnGui::GuiEventNetworkPlayerStats
//
// operator= @ 0x82485830
//   Chains the base NetworkPlayerStats::operator= then copies the event's own tail: the
//   player id (+0x84), the rank (+0x88) and the 16 name bytes (+0x8C).
// ===================================================================================
#include "GameSource/Gui/Events/BrnGuiEventNetworkPlayerStats.h"

namespace BrnGui
{
    // @ 0x82485830
    GuiEventNetworkPlayerStats&
    GuiEventNetworkPlayerStats::operator=(const GuiEventNetworkPlayerStats& lOther)
    {
        BrnNetwork::NetworkPlayerStats::operator=(lOther);

        mPlayerID   = lOther.mPlayerID;         // +0x84
        miWorldRank = lOther.miWorldRank;       // +0x88

        for (s32 li = 0; li < 16; ++li)         // +0x8C, 16 bytes
        {
            macPlayerName[li] = lOther.macPlayerName[li];
        }

        return *this;
    }
}
