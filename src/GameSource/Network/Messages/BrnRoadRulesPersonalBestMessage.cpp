#include "types.hpp"

#include "GameSource/Network/Messages/BrnRoadRulesPersonalBestMessage.h"
#include "SharedClasses/StreetData/BrnChallengeData.h"                                    // BrnStreetData::ScoreList::KAI_MIN/MAX_SCORES, E_SCORE_TYPE_COUNT
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"                  // ChallengeHighScoreEntry::Construct / SetScore (Retrieve)
#include <cstring>                                                                       // std::memset (PrepareForSend)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::RoadRulesPersonalBestMessage::Construct             @ 0x8257B988
//   BrnNetwork::RoadRulesPersonalBestMessage::GetPackedMessageSize  @ 0x8257B998
//   BrnNetwork::RoadRulesPersonalBestMessage::PackOrUnpack          @ 0x8257BA28
//   BrnNetwork::RoadRulesPersonalBestMessage::PrepareForSend / Retrieve
//
// A CgsNetwork::ReliableMessage subclass announcing a player's personal-best road-rules
// scores for one challenge: a pair of score values (maiScores @+0x28/+0x2C, one per
// BrnStreetData::ScoreType), the challenge index (mChallengeIndex @+0x30), and the
// street-data version they were scored against (miStreetDataVersion @+0x34).
//
// FLAGGED placeholder dependency: the per-score-type quantiser ranges
// (BrnStreetData::ScoreList::KAI_MIN_SCORES / KAI_MAX_SCORES, X360 .data @ 0x820A764C /
// 0x820A7654) are committed as the accepted over-permissive {0,0}/{INT32_MAX,INT32_MAX}
// placeholders in BrnChallengeData.cpp. PackOrUnpack routes the scores through those
// ranges by name, so the wire range here inherits that placeholder until a .data dump
// pins the literals. (The X360 .data tables back both 0x820A764C and 0x820A7654 in
// GetPackedMessageSize and PackOrUnpack alike.)

namespace BrnNetwork
{
    // The reliable message type the personal bests travel as.
    static const s32 KI_ROAD_RULES_PERSONAL_BEST_MESSAGE_TYPE = 27;

    // The street-data version this build scores against.
    static const s32 KI_STREET_DATA_VERSION = 5;

    // Arm the message with one challenge's personal bests (a score per type the record holds,
    // zero for the others) unless a previous one is still pending in this slot.
    void RoadRulesPersonalBestMessage::PrepareForSend(u16 lu16CurrentFrame, Road::ChallengeIndex lChallengeIndex,
                                                      BrnStreetData::ChallengeData lPbScore)
    {
        if (IsMessageValid())
        {
            return;
        }

        mChallengeIndex = lChallengeIndex;
        std::memset(maiScores, 0, sizeof(maiScores));
        for (BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
             leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; leScoreType++)
        {
            if (lPbScore.ContainsData(leScoreType))
            {
                maiScores[leScoreType] = lPbScore.GetScore(leScoreType);
            }
        }
        miStreetDataVersion = KI_STREET_DATA_VERSION;

        CgsNetwork::ReliableMessage::PrepareForSend(KI_ROAD_RULES_PERSONAL_BEST_MESSAGE_TYPE, lu16CurrentFrame);
        CGS_ASSERT(IsReliable(), "IsReliable()");
    }

    // Hand back a pending personal best: the challenge index and a high-score entry holding
    // every non-zero score under the sender's name. Consumes the slot.
    bool RoadRulesPersonalBestMessage::Retrieve(PlayerName* lpPlayerName, Road::ChallengeIndex* lpChallengeIndex,
                                                BrnStreetData::ChallengeHighScoreEntry* lpPbScore)
    {
        if (!IsMessageValid())
        {
            return false;
        }

        *lpChallengeIndex = mChallengeIndex;
        lpPbScore->Construct();
        for (BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
             leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; leScoreType++)
        {
            if (maiScores[leScoreType] != 0)
            {
                lpPbScore->SetScore(leScoreType, maiScores[leScoreType], lpPlayerName);
            }
        }

        SetMessageInvalid();
        CGS_ASSERT(!IsMessageValid(), "!CgsNetwork::ReliableMessage::IsMessageValid()");
        return true;
    }

    // BrnNetwork::RoadRulesPersonalBestMessage::Construct @ 0x8257B988
    //   li r11,-1 ; stw r11,0x20(r3) ; stw r11,0x24(r3) ; b Message::Construct
    // Inlines the MessageWithPlayerIDs base init (the two player ids -> -1, at +0x20 /
    // +0x24) ahead of the Message ctor, exactly as the X360 build folds it; the leaf
    // score/index/version fields are left as the ctor finds them (set on PrepareForSend).
    void RoadRulesPersonalBestMessage::Construct()
    {
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();
    }

    // BrnNetwork::RoadRulesPersonalBestMessage::GetPackedMessageSize @ 0x8257B998
    // Fills every payload field with the largest representable value before measuring, so
    // the reported size is the worst-case packed size: each score is set to its per-type
    // KAI_MIN_SCORES seed (the X360 build copies the min-range table into maiScores), the
    // challenge index and street-data version are zeroed, then the bare-ReliableMessage
    // size query is delegated (identically folded with
    // TestConnectionMessage::GetPackedMessageSize).
    s32 RoadRulesPersonalBestMessage::GetPackedMessageSize()
    {
        s32 liEnumIndex = 0;
        for (s32 liScoreType = 0;
             liScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
             ++liScoreType)
        {
            ++liEnumIndex;
            maiScores[liScoreType] = BrnStreetData::ScoreList::KAI_MIN_SCORES[liScoreType];
            CGS_ASSERT(liEnumIndex <= BrnStreetData::E_SCORE_TYPE_COUNT,
                       "leEnumIndex <= E_SCORE_TYPE_COUNT");
        }

        mChallengeIndex     = 0;
        miStreetDataVersion = 0;

        // Reliable-base size probe.
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    // BrnNetwork::RoadRulesPersonalBestMessage::PackOrUnpack @ 0x8257BA28
    // (De)serialises the base reliable id, then the challenge index ([0, 64]), each score
    // (routed through its per-type KAI_MIN/MAX_SCORES range), and the street-data version
    // ([0, 50]); ORs every per-field status together (0 == all succeeded). After unpacking
    // it asserts the received street-data version matches this build's (== 5), warning that
    // the players are running mismatched game versions.
    CgsNetwork::PackOrUnpackResult RoadRulesPersonalBestMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::ReliableMessage::PackOrUnpack();

        lxResult |= CgsNetwork::PackOrUnpackInt(this, &mChallengeIndex, 0, 64);

        s32 liEnumIndex = 0;
        for (s32 liScoreType = 0;
             liScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
             ++liScoreType)
        {
            lxResult |= CgsNetwork::PackOrUnpackInt(this, &maiScores[liScoreType],
                                                    BrnStreetData::ScoreList::KAI_MIN_SCORES[liScoreType],
                                                    BrnStreetData::ScoreList::KAI_MAX_SCORES[liScoreType]);
            ++liEnumIndex;
            CGS_ASSERT(liEnumIndex <= BrnStreetData::E_SCORE_TYPE_COUNT,
                       "leEnumIndex <= E_SCORE_TYPE_COUNT");
        }

        lxResult |= CgsNetwork::PackOrUnpackInt(this, &miStreetDataVersion, 0, 50);

        CGS_ASSERT(miStreetDataVersion == 5,
                   "Received street data from player with different street data version. You need to play the same versions of the game!\n");

        return lxResult;
    }
} // namespace BrnNetwork
