#include "types.hpp"

#include "GameSource/Network/Messages/BrnRoadRulesPersonalBestMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsTestConnectionMessage.h"  // TestConnectionMessage::GetPackedMessageSize delegate
#include "SharedClasses/StreetData/BrnChallengeData.h"                                    // BrnStreetData::ScoreList::KAI_MIN/MAX_SCORES, E_SCORE_TYPE_COUNT
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::RoadRulesPersonalBestMessage::Construct             @ 0x8257B988
//   BrnNetwork::RoadRulesPersonalBestMessage::GetPackedMessageSize  @ 0x8257B998
//   BrnNetwork::RoadRulesPersonalBestMessage::PackOrUnpack          @ 0x8257BA28
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
    // size query is delegated (ICF-tail-call to TestConnectionMessage::GetPackedMessageSize,
    // which is a bare ReliableMessage probe == the base size).
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

        // @0x8257BA1C: b CgsNetwork__TestConnectionMessage__GetPackedMessageSize -- a bare
        // ReliableMessage probe (no extra payload), so this measures the base size with
        // `this` reinterpreted as that sibling (matches the X360 tail call).
        return reinterpret_cast<CgsNetwork::TestConnectionMessage*>(this)->GetPackedMessageSize();
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
