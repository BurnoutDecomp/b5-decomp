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
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::LiveRevengeSyncMessage::GetName  @ 0x827DFD00
//     -> returns the literal "Live Revenge Sync Message" (lis/addi a rodata string, blr).
//        No member or base access.
//
// The other declared methods (Construct/PrepareForSend/Retrieve/Release/Destruct/
// GetPackedMessageSize/PackOrUnpack overloads) live in the sibling
// BrnLiveRevengeSyncMessage.cpp TU and are declared here for class shape but NOT bodied
// in this TU.
#pragma once

#include "types.hpp"                                                               // bool, s32, u16
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"  // CgsNetwork::ReliableMessage (committed base)
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h"         // LiveRevengeRelationship / CommonRelationship / CommonRelationshipStats (committed homes)

namespace BrnNetwork
{
    // DWARF BrnLiveRevengeSyncMessage.h:42.
    struct LiveRevengeSyncMessage : public CgsNetwork::ReliableMessage
    {
    private:
        LiveRevengeRelationship mLiveRevengeRelationship;   // DWARF :94

    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void Construct();
        void PrepareForSend(const LiveRevengeRelationship* lpLiveRevengeRelationship, u16 lu16Frame);
        bool Retrieve(LiveRevengeRelationship* lpLiveRevengeRelationship);
        void Release();
        void Destruct();
        virtual s32 GetPackedMessageSize();

        // LEDGER func @ 0x827DFD00 -- bodied in this TU.
        virtual const char* GetName() const;

    protected:
        virtual CgsNetwork::PackOrUnpackResult PackOrUnpack();

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
} // namespace BrnNetwork
