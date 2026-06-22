#pragma once

// ===================================================================================
// BrnNetwork::RestartTrafficMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnRestartTrafficMessage.h
//
// A CgsNetwork::ReliableMessage subclass that broadcasts a traffic restart: the set of
// active traffic hulls, the player ids owning the active hulls, and the frame at which
// the traffic should reset. Class shape is taken from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnRestartTrafficMessage.h)
// and gated on the X360 binary. The base CgsNetwork::ReliableMessage is the committed
// home in CgsReliableMessage.h and is reused BY NAME here -- not forked.
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::RestartTrafficMessage::GetName  @ 0x827DFCF0
//     -> returns the literal "Restart Traffic Message" (lis/addi a rodata string, blr).
//        No member or base access.
//
// The remaining declared methods (Construct/Destruct/PrepareForSend/Retrieve/
// GetPackedMessageSize/PackOrUnpack) live in the sibling .cpp TU
// (BrnRestartTrafficMessage.cpp) and are declared here for the class shape but NOT
// bodied in this TU. The player-id field is the committed BrnNetwork::NetworkPlayerID
// typedef (the DWARF spells it RoadRulesRecvData::NetworkPlayerID -- the same alias),
// reused here by name.
// ===================================================================================

#include "types.hpp"                                                                // s32, u16, bool
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"   // CgsNetwork::ReliableMessage (committed base)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                         // BrnNetwork::NetworkPlayerID (committed typedef)

namespace BrnNetwork
{
    struct RestartTrafficMessage : public CgsNetwork::ReliableMessage
    {
    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void          Construct();
        void          Destruct();
        void          PrepareForSend(u16 lu16FrameSinceStartToResetTraffic, u16 lu16NumActiveHulls,
                                     const u16* lpu16ActiveTrafficHulls,
                                     const NetworkPlayerID* lpPlayerIDsForActiveHulls);
        bool          Retrieve(u16* lpu16FrameSinceStartToResetTraffic, u16* lpu16ActiveTrafficHulls,
                               NetworkPlayerID* lpPlayerIDsForActiveHulls);
        virtual s32   GetPackedMessageSize();        // DWARF :95 (sibling .cpp)

        // LEDGER func @ 0x827DFCF0 -- bodied in this TU (DWARF BrnRestartTrafficMessage.h:96).
        virtual const char* GetName() const;

    protected:
        virtual CgsNetwork::PackOrUnpackResult PackOrUnpack();

    private:
        NetworkPlayerID maPlayerIDsForActiveHulls[8];    // DWARF :84
        u16             mau16ActiveTrafficHulls[8];       // DWARF :85
        u16             mu16FrameSinceStartToResetTraffic; // DWARF :86
    };

    // BrnNetwork::RestartTrafficMessage::GetName  @ 0x827DFCF0
    //   lis/addi a rodata string literal, blr -- no member or base access.
    inline const char* RestartTrafficMessage::GetName() const
    {
        return "Restart Traffic Message";
    }
} // namespace BrnNetwork
