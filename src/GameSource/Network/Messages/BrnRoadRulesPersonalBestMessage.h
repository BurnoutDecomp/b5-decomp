#pragma once

// ===================================================================================
// BrnNetwork::RoadRulesPersonalBestMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnRoadRulesPersonalBestMessage.h
//
// A CgsNetwork::ReliableMessage subclass announcing a player's personal-best road-rules
// scores for one challenge (a pair of score values, the challenge index, and the
// street-data version they were scored against). Class shape is taken from the DecFIGS
// DWARF (references/DecFIGS/dwarfdump/GameSource/Network/Messages/
// BrnRoadRulesPersonalBestMessage.h) and gated on the X360 binary. The base
// CgsNetwork::ReliableMessage is the committed home in CgsReliableMessage.h and is
// reused BY NAME here -- not forked. Road::ChallengeIndex is the committed typedef in
// BrnNetworkSharedIO.h, reused by name.
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::RoadRulesPersonalBestMessage::GetName  @ 0x827DFD60
//     -> returns the literal "Road Rules Personal Best Message" (lis/addi a rodata
//        string, blr). No member or base access.
//
// The remaining declared methods (Construct/PrepareForSend/Retrieve/
// GetPackedMessageSize/PackOrUnpack) are bodied in BrnRoadRulesPersonalBestMessage.cpp.
// PrepareForSend takes the personal-best ChallengeData by value (the reference shape; the
// caller passes the 24-byte record in registers), so its StreetData home is included.
// ===================================================================================

#include "types.hpp"                                                                // s32, u16, bool
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"   // CgsNetwork::ReliableMessage (committed base)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                         // BrnNetwork::Road::ChallengeIndex, PlayerName (committed)
#include "SharedClasses/StreetData/BrnChallengeData.h"                              // BrnStreetData::ChallengeData (PrepareForSend by value)

namespace BrnStreetData
{
    class  ChallengeHighScoreEntry;  // committed home: GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h
}

namespace BrnNetwork
{
    struct RoadRulesPersonalBestMessage : public CgsNetwork::ReliableMessage
    {
    public:
        // Bodied in BrnRoadRulesPersonalBestMessage.cpp.
        void          Construct();
        void          PrepareForSend(u16 lu16Frame, Road::ChallengeIndex lChallengeIndex,
                                     BrnStreetData::ChallengeData lPbScore);
        bool          Retrieve(PlayerName* lpPlayerName, Road::ChallengeIndex* lpChallengeIndex,
                               BrnStreetData::ChallengeHighScoreEntry* lpChallengeHighScoreEntry);
        s32   GetPackedMessageSize() override;

        // LEDGER func @ 0x827DFD60 -- bodied in this TU (DWARF BrnRoadRulesPersonalBestMessage.h:99).
        const char* GetName() const override;

    protected:
        CgsNetwork::PackOrUnpackResult PackOrUnpack() override;

    private:
        s32                 maiScores[2];          // DWARF :85
        Road::ChallengeIndex mChallengeIndex;       // DWARF :86
        s32                 miStreetDataVersion;   // DWARF :88
    };

    // BrnNetwork::RoadRulesPersonalBestMessage::GetName  @ 0x827DFD60
    //   lis/addi a rodata string literal, blr -- no member or base access.
    inline const char* RoadRulesPersonalBestMessage::GetName() const
    {
        return "Road Rules Personal Best Message";
    }

    // Console size (the send/receive slot stride; the registration passes the
    // RoadRulesMessage length instead), checked on a 32-bit build.
    static_assert(sizeof(void*) != 4 || sizeof(RoadRulesPersonalBestMessage) == 0x38, "sizeof(RoadRulesPersonalBestMessage) == 0x38");
} // namespace BrnNetwork
