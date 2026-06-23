// ---- GameSource/Network/SharedIO/BrnNetworkSharedIO.cpp ----
// Definition home for BrnNetworkSharedIO.h event records.
//
// BrnNetwork::RoadRulesDownloadEvent::Construct  @ 0x8254E678
//   (called by BrnNetwork::NetworkRoadRulesManager::_DownloadRoadRulesCallback)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 body:
//   * a 2-iteration loop (leEnumIndex 0..1) that stores a single 0 byte at +0 and +16, i.e.
//     null-terminates maPlayerNames[0].macName and maPlayerNames[1].macName (stride 0x10 == 16,
//     the PlayerName width). The loop carries the tripwire assert
//     "leEnumIndex <= E_SCORE_TYPE_COUNT" (BrnStreetData::E_SCORE_TYPE_COUNT == 2, the loop's
//     upper sanity bound; fires only if leEnumIndex > 2).
//   * stw 0 at +0x30 (48)  -> mRoadRulesData.mChallengeIndex = 0
//   * XMemSet(&mRoadRulesData (+0x20==32), 0, 8) -> zero mRoadRulesData.maScores[2] (8 bytes)
// (mRoadRulesData.mu64RoadRulesID at +40 is intentionally left untouched by Construct, matching
// the X360 stores.) Reproduced member-by-name; the streamed file-line assert is reduced to the
// project CGS_ASSERT convention.

#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnNetwork
{
    void RoadRulesDownloadEvent::Construct()
    {
        // X360: clear the two player-name slots to empty strings (single 0 byte each). The loop
        // bound mirrors BrnStreetData::E_SCORE_TYPE_COUNT (== 2 == the number of name slots here).
        const s32 KI_SCORE_TYPE_COUNT = 2;
        for (s32 liEnumIndex = 0; liEnumIndex < KI_SCORE_TYPE_COUNT; ++liEnumIndex)
        {
            maPlayerNames[liEnumIndex].macName[0] = 0;
            CGS_ASSERT(liEnumIndex + 1 <= KI_SCORE_TYPE_COUNT, "leEnumIndex <= E_SCORE_TYPE_COUNT");
        }

        // X360: mRoadRulesData.mChallengeIndex = 0, then zero the score array.
        mRoadRulesData.mChallengeIndex = 0;
        mRoadRulesData.maScores[0] = 0;
        mRoadRulesData.maScores[1] = 0;
    }
}
