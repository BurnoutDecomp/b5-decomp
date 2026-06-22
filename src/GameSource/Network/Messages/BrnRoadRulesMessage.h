#pragma once

// ===================================================================================
// BrnNetwork::RoadRulesMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnRoadRulesMessage.h
//
// A CgsNetwork::ReliableMessage subclass carrying a batch of road-rules score records
// (up to 10) plus the count and the street-data version they were scored against. Class
// shape is taken from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnRoadRulesMessage.h)
// and gated on the X360 binary. The base CgsNetwork::ReliableMessage is the committed
// home in CgsReliableMessage.h and is reused BY NAME here -- not forked. The score
// record type BrnNetwork::RoadRulesMessageData is the committed home in
// BrnNetworkSharedIO.h, reused here by name.
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::RoadRulesMessage::GetName  @ 0x827DFD50
//     -> returns the literal "Road Rules Message" (lis/addi a rodata string, blr).
//        No member or base access.
//
// The remaining declared methods (Construct/PrepareForSend/Retrieve/
// GetPackedMessageSize/PackOrUnpack) live in the sibling .cpp TU
// (BrnRoadRulesMessage.cpp) and are declared here for the class shape but NOT bodied in
// this TU.
// ===================================================================================

#include "types.hpp"                                                                // s32, u16, bool
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"   // CgsNetwork::ReliableMessage (committed base)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                         // BrnNetwork::RoadRulesMessageData (committed)

namespace BrnNetwork
{
    struct RoadRulesMessage : public CgsNetwork::ReliableMessage
    {
    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void          Construct();
        void          PrepareForSend(u16 lu16Frame, s32 liNumRoadRulesScores,
                                     RoadRulesMessageData* lpRoadRulesMessageData);
        bool          Retrieve(s32* lpiNumRoadRulesScores, RoadRulesMessageData* lpRoadRulesMessageData);
        virtual s32   GetPackedMessageSize();        // DWARF :92 (sibling .cpp)

        // LEDGER func @ 0x827DFD50 -- bodied in this TU (DWARF BrnRoadRulesMessage.h:93).
        virtual const char* GetName() const;

    protected:
        virtual CgsNetwork::PackOrUnpackResult PackOrUnpack();

    private:
        RoadRulesMessageData maRoadRulesMessageData[10]; // DWARF :79
        s32                  miNumRoadRulesScores;        // DWARF :80
        s32                  miStreetDataVersion;         // DWARF :82
    };

    // BrnNetwork::RoadRulesMessage::GetName  @ 0x827DFD50
    //   lis/addi a rodata string literal, blr -- no member or base access.
    inline const char* RoadRulesMessage::GetName() const
    {
        return "Road Rules Message";
    }
} // namespace BrnNetwork
