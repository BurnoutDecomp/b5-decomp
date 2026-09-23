// ===================================================================================
// BrnNetwork::MarkedManMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnMarkedManMessage.h
//
// A CgsNetwork::ReliableMessage subclass carrying which player is the "marked man" plus a
// final-answer flag. Class shape from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnMarkedManMessage.h);
// the base (CgsNetwork::ReliableMessage) is the committed home in CgsReliableMessage.h and
// is reused BY NAME here -- not forked.
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::MarkedManMessage::GetName  @ 0x827DFD40
//     -> returns the literal "Marked Man Message" (lis/addi a rodata string, blr).
//        No member or base access.
//
// The other declared methods (Construct/Destruct/PrepareForSend/Retrieve/
// GetPackedMessageSize/PackOrUnpack) live in the sibling BrnMarkedManMessage.cpp TU and
// are declared here for class shape but NOT bodied in this TU.
#pragma once

#include "types.hpp"                                                               // bool, s32, u16
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"  // CgsNetwork::ReliableMessage (committed base)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                        // BrnNetwork::NetworkPlayerID (committed typedef)

namespace BrnNetwork
{
    // DWARF BrnMarkedManMessage.h:43. The DWARF spells the player-id field
    // RoadRulesRecvData::NetworkPlayerID; that nested name is an alias of the committed
    // BrnNetwork::NetworkPlayerID (== s32), reused here by name.
    struct MarkedManMessage : public CgsNetwork::ReliableMessage
    {
    private:
        NetworkPlayerID mMarkedMan;     // DWARF :80
        bool            mbFinalAnswer;  // DWARF :81

    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void Construct();
        void Destruct();
        void PrepareForSend(u16 lu16Frame, NetworkPlayerID lMarkedMan, bool lbFinalAnswer);
        bool Retrieve(NetworkPlayerID* lpMarkedMan, bool* lpbFinalAnswer);
        s32 GetPackedMessageSize() override;

        // LEDGER func @ 0x827DFD40 -- bodied in this TU.
        const char* GetName() const override;

    protected:
        CgsNetwork::PackOrUnpackResult PackOrUnpack() override;
    };

    // BrnNetwork::MarkedManMessage::GetName  @ 0x827DFD40
    inline const char* MarkedManMessage::GetName() const
    {
        return "Marked Man Message";
    }

    // Console size (the RegisterMessageType length), checked on a 32-bit build.
    static_assert(sizeof(void*) != 4 || sizeof(MarkedManMessage) == 0x30, "sizeof(MarkedManMessage) == 0x30");
} // namespace BrnNetwork
