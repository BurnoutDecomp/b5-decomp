#include "types.hpp"

#include "GameSource/Network/Messages/BrnRoadRulesMessage.h"
#include "SharedClasses/StreetData/BrnChallengeData.h"                                    // BrnStreetData::ScoreList::KAI_MIN/MAX_SCORES, E_SCORE_TYPE_COUNT
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"                             // CgsDev::StrStream (PrepareForSend asserts)
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::RoadRulesMessage::GetPackedMessageSize @ 0x8257B7C8
//   BrnNetwork::RoadRulesMessage::PackOrUnpack         @ 0x8257B808
//   BrnNetwork::RoadRulesMessage::Retrieve             @ 0x8257EA08
//   BrnNetwork::RoadRulesMessage::Construct / PrepareForSend
// (GetName is inline in the header.)
//
// A CgsNetwork::ReliableMessage subclass carrying a batch of up to 10 road-rules score
// records. Each RoadRulesMessageData (24 bytes, home BrnNetworkSharedIO.h) holds a pair
// of score values (maScores[2] @ rec+0x00/+0x04, one per BrnStreetData::ScoreType), a
// 64-bit road-rules id (mu64RoadRulesID @ rec+0x08, not packed by these funcs), and the
// challenge/road index (mChallengeIndex @ rec+0x10). The record array sits at +0x28, the
// live count (miNumRoadRulesScores) at +0x118, and the street-data version
// (miStreetDataVersion) at +0x11C.
//
// FLAGGED placeholder dependency: the per-score-type quantiser ranges
// (BrnStreetData::ScoreList::KAI_MIN_SCORES / KAI_MAX_SCORES, X360 .data @ 0x820A764C /
// 0x820A7654) are committed as the accepted over-permissive placeholders in
// BrnChallengeData.cpp. PackOrUnpack routes each record's scores through those ranges by
// name, so the wire range here inherits that placeholder until a .data dump pins the
// literals. (The same two tables back the score packing here and in the sibling
// RoadRulesPersonalBestMessage TU.)

namespace BrnNetwork
{
    // File-scope constants (DWARF BrnRoadRulesMessage.cpp:25-26). The X360 build packs the
    // record count in [0, 10] (10 == the fixed array capacity) and the per-record road
    // index in [0, 64], with the street-data version in [0, 50] and asserted == 5.
    const s32 KI_MIN_ROAD_RULES_MESSAGE_ENTRIES = 0;
    const s32 KI_MIN_ROAD_RULES_ROAD_INDEX      = 0;

    static const s32 KI_MAX_ROAD_RULES_MESSAGE_ENTRIES = 10;   // li r8,0xA / cap of maRoadRulesMessageData
    static const s32 KI_MAX_ROAD_RULES_ROAD_INDEX      = 64;   // li r6,0x40
    static const s32 KI_MIN_STREET_DATA_VERSION        = 0;
    static const s32 KI_MAX_STREET_DATA_VERSION        = 50;   // li r6,0x32
    static const s32 KI_EXPECTED_STREET_DATA_VERSION   = 5;    // cmpwi r11,5

    // The reliable message type the road-rules scores travel as.
    static const s32 KI_ROAD_RULES_MESSAGE_TYPE = 26;

    // The player ids go invalid, then the message base resets (the same body as every
    // two-player-id message's Construct).
    void RoadRulesMessage::Construct()
    {
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();
    }

    // Arm the message with a batch of 1..10 score records unless a previous batch is still
    // pending in this slot; the batch is stamped with this build's street-data version.
    void RoadRulesMessage::PrepareForSend(u16 lu16Frame, s32 liNumRoadRulesScores,
                                          RoadRulesMessageData* lpRoadRulesMessageData)
    {
        if (IsMessageValid())
        {
            return;
        }

        if (liNumRoadRulesScores <= 0)
        {
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Trying to send 0 or less road rules: " << liNumRoadRulesScores;
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }
        if (liNumRoadRulesScores > KI_MAX_ROAD_RULES_MESSAGE_ENTRIES)
        {
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Trying to send too many road rules: " << liNumRoadRulesScores << "\n";
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        miNumRoadRulesScores = liNumRoadRulesScores;
        std::memcpy(maRoadRulesMessageData, lpRoadRulesMessageData,
                    static_cast<u32>(liNumRoadRulesScores) * sizeof(RoadRulesMessageData));
        miStreetDataVersion = KI_EXPECTED_STREET_DATA_VERSION;

        CgsNetwork::ReliableMessage::PrepareForSend(KI_ROAD_RULES_MESSAGE_TYPE, lu16Frame);
        CGS_ASSERT(IsReliable(), "IsReliable()");
    }

    // BrnNetwork::RoadRulesMessage::GetPackedMessageSize @ 0x8257B7C8
    // Sizes for the worst case: the count is pinned to the array capacity (10) and every
    // packed field of every record is zeroed (the two scores + the challenge index; the
    // 64-bit road-rules id is left untouched, matching the asm's three stores per record),
    // then the street-data version is zeroed, before deferring to the bare-ReliableMessage
    // size query (identically folded with TestConnectionMessage::GetPackedMessageSize).
    s32 RoadRulesMessage::GetPackedMessageSize()
    {
        miNumRoadRulesScores = KI_MAX_ROAD_RULES_MESSAGE_ENTRIES;   // stw 10, +0x118

        s32 liIndex = 0;
        do
        {
            RoadRulesMessageData& lrRecord = maRoadRulesMessageData[liIndex];
            lrRecord.maScores[0]    = 0;   // rec+0x00
            ++liIndex;
            lrRecord.maScores[1]    = 0;   // rec+0x04
            lrRecord.mChallengeIndex = 0;  // rec+0x10
        }
        while (liIndex < miNumRoadRulesScores);

        miStreetDataVersion = 0;   // stw 0, +0x11C

        // Tail call to the reliable-base size probe.
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    // BrnNetwork::RoadRulesMessage::PackOrUnpack @ 0x8257B808
    // (De)serialises the base reliable id, then the record count ([0, 10]); for each live
    // record it packs the pair of scores (each routed through its per-type
    // KAI_MIN/MAX_SCORES range) and the challenge/road index ([0, 64]); finally the
    // street-data version ([0, 50]). Every per-field status is OR-accumulated (0 == all
    // succeeded). After unpacking it asserts the received street-data version matches this
    // build's (== 5), warning that the players are running mismatched game versions.
    CgsNetwork::PackOrUnpackResult RoadRulesMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::ReliableMessage::PackOrUnpack();

        lxResult = CgsNetwork::PackOrUnpackInt(this, &miNumRoadRulesScores,
                                               KI_MIN_ROAD_RULES_MESSAGE_ENTRIES,
                                               KI_MAX_ROAD_RULES_MESSAGE_ENTRIES) | lxResult;

        for (s32 liIndex = 0; liIndex < miNumRoadRulesScores; ++liIndex)
        {
            RoadRulesMessageData& lrRecord = maRoadRulesMessageData[liIndex];

            for (s32 liScoreType = 0; liScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; ++liScoreType)
            {
                lxResult = CgsNetwork::PackOrUnpackInt(this, &lrRecord.maScores[liScoreType],
                                                       BrnStreetData::ScoreList::KAI_MIN_SCORES[liScoreType],
                                                       BrnStreetData::ScoreList::KAI_MAX_SCORES[liScoreType]) | lxResult;
            }

            lxResult = CgsNetwork::PackOrUnpackInt(this, &lrRecord.mChallengeIndex,
                                                   KI_MIN_ROAD_RULES_ROAD_INDEX,
                                                   KI_MAX_ROAD_RULES_ROAD_INDEX) | lxResult;
        }

        lxResult = CgsNetwork::PackOrUnpackInt(this, &miStreetDataVersion,
                                               KI_MIN_STREET_DATA_VERSION,
                                               KI_MAX_STREET_DATA_VERSION) | lxResult;

        CGS_ASSERT(miStreetDataVersion == KI_EXPECTED_STREET_DATA_VERSION,
                   "Received street data from player with different street data version. You need to play the same versions of the game!\n");

        return lxResult;
    }

    // BrnNetwork::RoadRulesMessage::Retrieve @ 0x8257EA08
    // Hands back the received score batch iff a message is pending in this slot: copies the
    // live count out, then bulk-copies exactly that many 24-byte records, and consumes the
    // slot by clearing the VALID flag.
    bool RoadRulesMessage::Retrieve(s32* lpiNumRoadRulesScores, RoadRulesMessageData* lpRoadRulesMessageData)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            return false;
        }

        *lpiNumRoadRulesScores = miNumRoadRulesScores;   // *a2 = +0x118

        // Copy out exactly the received records (count * 24 bytes; the X360 computes
        // count*24 as (count + count*2) << 3).
        std::memcpy(lpRoadRulesMessageData, maRoadRulesMessageData,
                    static_cast<u32>(miNumRoadRulesScores) * sizeof(RoadRulesMessageData));

        // Consume the slot: clear VALID, then assert it is now invalid.
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                   "!CgsNetwork::ReliableMessage::IsMessageValid()");

        return true;
    }
} // namespace BrnNetwork
