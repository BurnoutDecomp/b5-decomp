#pragma once

// ===================================================================================
// BrnNetwork::BurningHomeRunSwitchRunnerMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnBurningHomeRunSwitchRunnerMessage.h
//
// SHAPE from DecFIGS DWARF gated against the X360 binary (BURNOUT_X360_ARTIST.XEX). A
// reliable message that hands the "burning home run" runner role to a new player: it
// adds a single NetworkPlayerID payload after the 0x28-byte CgsNetwork::ReliableMessage
// base:
//   +0x00  (CgsNetwork::ReliableMessage base, size 0x28)
//   +0x28  NetworkPlayerID mNewRunnerID
//
// The base (CgsNetwork::ReliableMessage: Message -> MessageWithPlayerIDs -> ReliableMessage)
// is reused BY NAME from its committed home header (CgsReliableMessage.h) -- not forked.
//
// LEDGER FUNCTIONS in this TU (X360):
//   GetName               @ 0x8257C298  -> returns "Burning home run switch runner message"
//   GetPackedMessageSize  @ 0x8257C288
//   PackOrUnpack          @ 0x8257C238
//   PrepareForSend        @ 0x8257F278
//   Retrieve              @ 0x8257F2A8
// all bodied in the sibling BrnBurningHomeRunSwitchRunnerMessage.cpp.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"
#include "GameSource/Network/BrnNetworkManager.h"   // BrnNetworkManager::PackOrUnpackResult (return type)

namespace BrnNetwork
{
    // Reliable notification that the burning-home-run runner role has switched to the
    // carried player id.
    struct BurningHomeRunSwitchRunnerMessage : CgsNetwork::ReliableMessage
    {
        NetworkPlayerID mNewRunnerID;   // +0x28

        void                             PrepareForSend(u16 lu16Frame, NetworkPlayerID lNewRunnerID);
        bool                             Retrieve(NetworkPlayerID* lpNewRunnerID);
        s32                              GetPackedMessageSize();
        BrnNetworkManager::PackOrUnpackResult PackOrUnpack();
        const char*                      GetName() const;
    };
} // namespace BrnNetwork
