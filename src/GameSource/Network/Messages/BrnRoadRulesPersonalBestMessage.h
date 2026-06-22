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
// GetPackedMessageSize/PackOrUnpack) live in the sibling .cpp TU
// (BrnRoadRulesPersonalBestMessage.cpp) and are declared here for the class shape but
// NOT bodied in this TU. BrnStreetData::ChallengeData / ChallengeHighScoreEntry are
// only referenced (by ptr/value) in those sibling-.cpp signatures, so they are
// forward-declared here rather than pulling in their StreetData homes.
// ===================================================================================

#include "types.hpp"                                                                // s32, u16, bool
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"   // CgsNetwork::ReliableMessage (committed base)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                         // BrnNetwork::Road::ChallengeIndex, PlayerName (committed)

namespace BrnStreetData
{
    struct ChallengeData;            // committed home: SharedClasses/StreetData/BrnChallengeData.h
    class  ChallengeHighScoreEntry;  // committed home: GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h
}

namespace BrnNetwork
{
    struct RoadRulesPersonalBestMessage : public CgsNetwork::ReliableMessage
    {
    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void          Construct();
        void          PrepareForSend(u16 lu16Frame, Road::ChallengeIndex lChallengeIndex,
                                     const BrnStreetData::ChallengeData& lChallengeData);
        bool          Retrieve(PlayerName* lpPlayerName, Road::ChallengeIndex* lpChallengeIndex,
                               BrnStreetData::ChallengeHighScoreEntry* lpChallengeHighScoreEntry);
        virtual s32   GetPackedMessageSize();        // DWARF :98 (sibling .cpp)

        // LEDGER func @ 0x827DFD60 -- bodied in this TU (DWARF BrnRoadRulesPersonalBestMessage.h:99).
        virtual const char* GetName() const;

    protected:
        virtual CgsNetwork::PackOrUnpackResult PackOrUnpack();

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
} // namespace BrnNetwork
