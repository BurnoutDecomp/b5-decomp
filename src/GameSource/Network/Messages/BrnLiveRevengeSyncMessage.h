// ===================================================================================
// BrnNetwork::LiveRevengeSyncMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnLiveRevengeSyncMessage.h
//
// A CgsNetwork::ReliableMessage subclass that syncs a player's LiveRevengeRelationship
// across the network. Class shape from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnLiveRevengeSyncMessage.h);
// the base (CgsNetwork::ReliableMessage) is the committed home in CgsReliableMessage.h and
// is reused BY NAME here -- not forked. The LiveRevengeRelationship payload (and its
// CommonRelationship / CommonRelationshipStats parts) are the committed homes in
// BrnNetworkLiveRevengeRelationship.h, reused BY NAME.
//
// GetName is header-inline; every other method is bodied in BrnLiveRevengeSyncMessage.cpp.
#pragma once

#include "types.hpp"                                                               // bool, s32, u16
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"  // CgsNetwork::ReliableMessage (committed base)
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h"         // LiveRevengeRelationship / CommonRelationship / CommonRelationshipStats (committed homes)

namespace BrnNetwork
{
    // The reliable-message type the per-player sync send/recv pair is registered under; the
    // message stamps it on send.
    static const s32 KI_LIVE_REVENGE_SYNC_MESSAGE_TYPE = 24;

    // DWARF BrnLiveRevengeSyncMessage.h:42.
    struct LiveRevengeSyncMessage : public CgsNetwork::ReliableMessage
    {
    private:
        LiveRevengeRelationship mLiveRevengeRelationship;   // DWARF :94

    public:
        void Construct();
        void PrepareForSend(const LiveRevengeRelationship* lpLiveRevengeRelationship, u16 lu16Frame);
        bool Retrieve(LiveRevengeRelationship* lpLiveRevengeRelationship);
        void Release();
        void Destruct();
        s32 GetPackedMessageSize() override;

        // LEDGER func @ 0x827DFD00 -- bodied in this TU.
        const char* GetName() const override;

    protected:
        CgsNetwork::PackOrUnpackResult PackOrUnpack() override;

    private:
        // DWARF spells these BrnNetworkManager::PackOrUnpackResult; that nested name is an
        // alias of the committed CgsNetwork::PackOrUnpackResult (== u8), reused here by name.
        CgsNetwork::PackOrUnpackResult PackOrUnpack(CommonRelationship* lpCommonRelationship);
        CgsNetwork::PackOrUnpackResult PackOrUnpack(CommonRelationshipStats* lpCommonRelationshipStats);
    };

    // BrnNetwork::LiveRevengeSyncMessage::GetName  @ 0x827DFD00
    inline const char* LiveRevengeSyncMessage::GetName() const
    {
        return "Live Revenge Sync Message";
    }

    // Console size (the RegisterMessageType length), checked on a 32-bit build.
    static_assert(sizeof(void*) != 4 || sizeof(LiveRevengeSyncMessage) == 0xA0, "sizeof(LiveRevengeSyncMessage) == 0xA0");
} // namespace BrnNetwork
