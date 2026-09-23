#pragma once

// ===================================================================================
// BrnGui::GuiEventNetworkPlayerStats  -- owning header
//   b5-decomp/src/GameSource/Gui/Events/BrnGuiEventNetworkPlayerStats.h
//
// The GUI event that carries one player's online stats snapshot: a NetworkPlayerStats
// record (the recovered base, homed in BrnNetworkPlayerStats.h, 0x84 bytes) followed by
// the player's id, rank and name. BrnNetworkManager::OutputPlayerStatsToGui fills the tail
// (id, the rank from the player's params or info, the name strncpy'd to 16) and posts the
// 156-byte record; operator= copies the base, then the words at +0x84 and +0x88, then the
// 16 name bytes at +0x8C.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"   // BrnNetwork::NetworkPlayerStats
#include <cstddef>   // offsetof (layout pins)

namespace BrnGui
{
    struct GuiEventNetworkPlayerStats : public BrnNetwork::NetworkPlayerStats
    {
        BrnNetwork::NetworkPlayerID mPlayerID;          // +0x84
        s32                         miWorldRank;        // +0x88 FLAG name
        char                        macPlayerName[16];  // +0x8C

        // Owned by THIS TU: member-wise copy assignment (@0x82485830).
        GuiEventNetworkPlayerStats& operator=(const GuiEventNetworkPlayerStats& lOther);

        // The queued event-type id. Not GuiEvent<N>-derived, so the id is carried here;
        // X360-attested by BrnNetworkModule::AddOutputGuiEvent<GuiEventNetworkPlayerStats>
        // @0x82565C60 -> AddEvent(&event, 248, 156).
        s32 GetEventType() const { return 248; }

    private:
        // Never called -- complete-class context for the layout pins.
        static void _AssertLayout()
        {
            static_assert(sizeof(BrnNetwork::NetworkPlayerStats) == 0x84, "stats base is 0x84 bytes");
            static_assert(offsetof(GuiEventNetworkPlayerStats, mPlayerID) == 0x84, "player id at +0x84");
            static_assert(offsetof(GuiEventNetworkPlayerStats, miWorldRank) == 0x88, "rank at +0x88");
            static_assert(offsetof(GuiEventNetworkPlayerStats, macPlayerName) == 0x8C, "name at +0x8C");
            static_assert(sizeof(GuiEventNetworkPlayerStats) == 156, "posted with size 156");
        }
    };
}
