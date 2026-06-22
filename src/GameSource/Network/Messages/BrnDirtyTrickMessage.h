#pragma once

// ===================================================================================
// BrnNetwork::DirtyTrickMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnDirtyTrickMessage.h
//
// A CgsNetwork::ReliableMessage subclass announcing a dirty-trick event (aggressor /
// victim player ids + trick type + status). Class shape is taken from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnDirtyTrickMessage.h)
// and gated on the X360 binary. The base CgsNetwork::ReliableMessage is the committed
// home in CgsReliableMessage.h and is reused BY NAME here -- not forked.
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::DirtyTrickMessage::GetName  @ 0x827DFD10
//     -> returns the literal "Dirty Trick Message" (lis/addi a rodata string, blr).
//        No member or base access.
//
// The remaining declared methods (Construct/Destruct/PrepareForSend/Retrieve/
// GetPackedMessageSize/PackOrUnpack) live in the sibling .cpp TU
// (BrnDirtyTrickMessage.cpp) and are declared here for the class shape but NOT bodied
// in this TU. The two player-id fields are the committed BrnNetwork::NetworkPlayerID
// typedef (the DWARF spells it RoadRulesRecvData::NetworkPlayerID -- the same alias),
// reused here by name.
// ===================================================================================

#include "types.hpp"                                                                // s32, u8, bool
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"   // CgsNetwork::ReliableMessage (committed base)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                         // BrnNetwork::NetworkPlayerID (committed typedef)

namespace BrnNetwork
{
    struct DirtyTrickMessage : public CgsNetwork::ReliableMessage
    {
    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void          Construct();
        void          Destruct();
        void          PrepareForSend(u16 lu16Frame, NetworkPlayerID lAggressorPlayerID,
                                     NetworkPlayerID lVictimPlayerID, u8 lu8DirtyTrickType,
                                     u8 lu8DirtyTrickStatus);
        bool          Retrieve(NetworkPlayerID* lpAggressorPlayerID, NetworkPlayerID* lpVictimPlayerID,
                               u8* lpu8DirtyTrickType, u8* lpu8DirtyTrickStatus);
        virtual s32   GetPackedMessageSize();        // DWARF :96 (sibling .cpp)

        // LEDGER func @ 0x827DFD10 -- bodied in this TU (DWARF BrnDirtyTrickMessage.h:101).
        virtual const char* GetName() const;

    protected:
        virtual CgsNetwork::PackOrUnpackResult PackOrUnpack();

    private:
        NetworkPlayerID mAggressorNetworkPlayerID;   // DWARF :88
        NetworkPlayerID mVictimNetworkPlayerID;       // DWARF :89
        u8              mu8DirtyTrickType;            // DWARF :90
        u8              mu8DirtyTrickStatus;          // DWARF :91
    };

    // BrnNetwork::DirtyTrickMessage::GetName  @ 0x827DFD10
    //   lis/addi a rodata string literal, blr -- no member or base access.
    inline const char* DirtyTrickMessage::GetName() const
    {
        return "Dirty Trick Message";
    }
} // namespace BrnNetwork
