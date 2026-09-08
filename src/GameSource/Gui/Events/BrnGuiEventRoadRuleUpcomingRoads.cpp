// BrnGuiEventRoadRuleUpcomingRoads.cpp -- the road-rule GUI event family TU.
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   BrnGui::GuiEventRoadRuleUpcomingRoads::ConvertG (truncated) @ 0x824F6170
//   BrnGui::GuiEventRoadRuleEnter::Construct()                  @ 0x824F60B8
//   BrnGui::GuiEventRoadRuleUpcomingRoads::Construct()          @ 0x824F6108
//
// H2 (2026-08-25): this TU's old local header declared its OWN
// `GuiEventRoadRuleUpcomingRoads : CgsModule::Event` -- an ODR fork of the full
// BrnGuiEventTypeDefs.h struct (the documented fork hazard, caught before mounting).
// The header is retired; this TU now works against the single TypeDefs home, and the
// two default Constructs (both this family's reset paths, called by
// RoadRuleComponent's Construct / HandleLeaveRoadEvent / ShowUpcomingRoads) land here.
//
// Maps a 3-valued game-state enum to this event's road category id. Two non-fatal
// guards bracket the switch:
//
//   1. if !(luGameState in [0,2]):  build "Invalid enum after cast - original value was
//      <v>, cast to <v>\n" into the assert buffer and fire (DWARF :513).
//   2. switch: 0 -> return 0; 1 -> return 2; 2 -> return 1; default: build
//      "Invalid gamestate enum (<v>)\n" and fire (DWARF :539), then fall through to
//      return 0.
//
// The X360 builds each assert message by streaming the runtime value through a
// CgsDev::StrStream over the shared assert message buffer; reproduced here with the
// same stream so the formatted value is preserved. The X360-baked file/line are
// discarded per project convention.

#include <cstring>
#include "GameSource/Gui/Events/BrnGuiEventRoadRuleData.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"               // the single struct home (fork retired)

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CgsDev::Assert::Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream
#include "SharedClasses/StreetData/BrnStreetData.h"           // KI_INVALID_ROAD_INDEX (dword_820A766C)

namespace BrnGui
{

// @ 0x824F60B8 -- reset the enter payload. The X360 loop zeroes, per score type: the
// friend-name lead byte (+0x4C+16i), the AI-leader qword (+0x08+8i), the ACTIVE
// leader word (+0x18+4i), the challenge flag (+0x6C+i) and the ACTIVE best value
// (+0x30+4i); then the road id and the road index. The offline/online mirrors are
// deliberately NOT touched (they survive a reset).
void GuiEventRoadRuleEnter::Construct()
{
    for ( s32 liType = 0; liType < BrnStreetData::E_SCORE_TYPE_COUNT; ++liType )
    {
        maFriendLeader[liType].macName[0] = '\0';
        maAILeaderId[liType]              = 0;
        maeRoadRuleLeaderType[liType]     = E_ROADRULELEADERTYPE_AI;
        mabChallenge[liType]              = false;
        maiBestValues[liType]             = 0;
    }
    mRoadId     = 0;
    miRoadIndex = 0;
}

// @ 0x824F6108 -- reset the upcoming-roads payload. Per side: zero the ACTIVE leader
// pair (+0x30+8s), the road id, the road state; turning index := KI_INVALID_ROAD_INDEX
// (the X360 loads the shared -1 global dword_820A766C); zero the entrance position.
// Then current sign state := E_ROADSTATE_COUNT (3) and current index := -1. The
// offline/online leader mirrors are deliberately NOT touched.
void GuiEventRoadRuleUpcomingRoads::Construct()
{
    for ( s32 liSide = 0; liSide < E_ROAD_COUNT; ++liSide )
    {
        maaeLeaderTypes[liSide][0]   = E_ROADRULELEADERTYPE_AI;
        maaeLeaderTypes[liSide][1]   = E_ROADRULELEADERTYPE_AI;
        mRoadIds[liSide]             = 0;
        meRoadStates[liSide]         = E_ROADSTATE_NORMAL;
        maiTurningRoadIndices[liSide] = BrnStreetData::KI_INVALID_ROAD_INDEX;
        maRoadEntrancePosition[liSide].SetZero();
    }
    meCurrentSignState = E_ROADSTATE_COUNT;
    miCurrentRoadIndex = BrnStreetData::KI_INVALID_ROAD_INDEX;
}

// @ 0x824F6170
s32 GuiEventRoadRuleUpcomingRoads::ConvertGameStateToCategory( u32 luGameState )
{
    // ---- guard 1: post-cast range check (X360: signed<0 || >=3) ----
    if ( luGameState > 2u )
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Invalid enum after cast - original value was "
                   << static_cast<s32>( luGameState )
                   << ", cast to "
                   << static_cast<s32>( luGameState )
                   << "\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lStrStream.GetBuffer(), __FILE__, __LINE__ );
        CgsDev::Assert::EndAssert();
    }

    // ---- the X360 branch table (0->0, 1->2, 2->1) ----
    switch ( luGameState )
    {
        case 0u:
            return 0;
        case 1u:
            return 2;
        case 2u:
            return 1;
        default:
            break;
    }

    // ---- guard 2: unreachable for valid input; the X360 default arm ----
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Invalid gamestate enum ("
                   << static_cast<s32>( luGameState )
                   << ")\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lStrStream.GetBuffer(), __FILE__, __LINE__ );
        CgsDev::Assert::EndAssert();
    }
    return 0;
}

} // namespace BrnGui

namespace BrnGui {
// ARTIST 0x82504718. Comparisons use (candidate, current best), except the
// online tie test, which deliberately gives a tied personal best to the player.
void GuiEventRoadRuleEnter::SetupRoadRule(const RoadRulesEnterRoadAction* action, BrnStreetData::ScoreType type)
{
    using BrnStreetData::ChallengeData;
    maeRoadRuleLeaderType[type] = E_ROADRULELEADERTYPE_AI;
    action->mParScores.GetScore(type, &maiBestValues[type], &maAILeaderId[type]);
    maeOfflineRoadRuleLeaderType[type] = E_ROADRULELEADERTYPE_AI;
    action->mParScores.GetScore(type, &maiBestOfflineValues[type], &maAILeaderId[type]);
    maiBestOnlineValues[type] = 0;
    maeOnlineRoadRuleLeaderType[type] = E_ROADRULELEADERTYPE_FRIEND;
    maAILeaderId[type] = action->maParRivalIds[type];
    if (action->mFriendScores.ContainsData(type))
    {
        s32 score; CgsNetwork::PlayerName name;
        action->mFriendScores.GetScore(type, &score, &name);
        std::memcpy(&maFriendLeader[type], &name, sizeof(name));
        if (ChallengeData::CompareScores(type, score, maiBestValues[type]) < 0)
        {
            maiBestValues[type] = score;
            maeRoadRuleLeaderType[type] = E_ROADRULELEADERTYPE_FRIEND;
        }
        maiBestOnlineValues[type] = score;
    }
    else
    {
        CgsNetwork::PlayerName name;
        name.Construct(type == BrnStreetData::E_SCORE_TYPE_TIME ? "HUD_NOTIME" : "HUD_NOCRASH");
        std::memcpy(&maFriendLeader[type], &name, sizeof(name));
    }
    if (action->mUserScores.ContainsData(type))
    {
        const s32 score = action->mUserScores.GetScore(type);
        if (ChallengeData::CompareScores(type, score, maiBestValues[type]) < 0)
        {
            maiBestValues[type] = score;
            maeRoadRuleLeaderType[type] = E_ROADRULELEADERTYPE_PLAYER;
        }
        if (ChallengeData::CompareScores(type, score, maiBestOfflineValues[type]) < 0)
        {
            maeOfflineRoadRuleLeaderType[type] = E_ROADRULELEADERTYPE_PLAYER;
            maiBestOfflineValues[type] = score;
        }
        if (!action->mFriendScores.ContainsData(type) ||
            ChallengeData::CompareScores(type, maiBestOnlineValues[type], score) >= 0)
        {
            maeOnlineRoadRuleLeaderType[type] = E_ROADRULELEADERTYPE_PLAYER;
            maiBestOnlineValues[type] = score;
        }
    }
}

// ARTIST sub_8250B9E0 (the action constructor).
void GuiEventRoadRuleEnter::Construct(const RoadRulesEnterRoadAction* action)
{
    SetupRoadRule(action, BrnStreetData::E_SCORE_TYPE_CRASH);
    SetupRoadRule(action, BrnStreetData::E_SCORE_TYPE_TIME);
    mRoadId = action->mRoadId;
    miRoadIndex = action->miRoadIndex;
}

// ARTIST 0x82504B78.
void GuiEventRoadRuleUpcomingRoads::FindRoadRuler(ERoadSide side,
    const UpcomingRoadChangeAction* action, BrnStreetData::ScoreType type)
{
    using BrnStreetData::ChallengeData;
    const auto& par = side == E_ROAD_LEFT ? action->mLeftParScore : action->mRightParScore;
    const auto& friends = side == E_ROAD_LEFT ? action->mLeftFriendHighScore : action->mRightFriendHighScore;
    const auto& user = side == E_ROAD_LEFT ? action->mLeftUserScore : action->mRightUserScore;
    CGS_ASSERT(par.ContainsData(type), "Failed to find road rule scores for upcoming road");
    maaeLeaderTypes[side][type] = E_ROADRULELEADERTYPE_AI;
    s32 parScore; CgsID rival;
    par.GetScore(type, &parScore, &rival);
    s32 bestScore = parScore, friendScore = 0;
    if (friends.ContainsData(type))
    {
        CgsNetwork::PlayerName name;
        friends.GetScore(type, &friendScore, &name);
        if (ChallengeData::CompareScores(type, friendScore, bestScore) < 0)
        {
            bestScore = friendScore;
            maaeLeaderTypes[side][type] = E_ROADRULELEADERTYPE_FRIEND;
        }
    }
    if (user.ContainsData(type))
    {
        const s32 score = user.GetScore(type);
        if (ChallengeData::CompareScores(type, score, bestScore) < 0)
            maaeLeaderTypes[side][type] = E_ROADRULELEADERTYPE_PLAYER;
        maaeOfflineLeaderTypes[side][type] = ChallengeData::CompareScores(type, score, parScore) < 0
            ? E_ROADRULELEADERTYPE_PLAYER : E_ROADRULELEADERTYPE_AI;
        maaeOnlineLeaderTypes[side][type] = friends.ContainsData(type) && ChallengeData::CompareScores(type, friendScore, score) < 0
            ? E_ROADRULELEADERTYPE_FRIEND : E_ROADRULELEADERTYPE_PLAYER;
    }
    else
    {
        maaeOfflineLeaderTypes[side][type] = E_ROADRULELEADERTYPE_AI;
        maaeOnlineLeaderTypes[side][type] = E_ROADRULELEADERTYPE_FRIEND;
    }
}

// ARTIST sub_8250BA98.
void GuiEventRoadRuleUpcomingRoads::Construct(const UpcomingRoadChangeAction* action)
{
    CGS_ASSERT(action != 0, "lpGamePlayAction");
    mRoadIds[0] = action->mLeftRoadId;
    mRoadIds[1] = action->mRightRoadId;
    for (s32 i = 0; i < 2; ++i)
    {
        const bool interstate = i == 0 ? action->mu8LeftRoadIsInterstate != 0 : action->mu8RightRoadIsInterstate != 0;
        if (interstate) meRoadStates[i] = E_ROADSTATE_NORMAL;
        else
        {
            meRoadStates[i] = static_cast<ERoadState>(ConvertGameStateToCategory(i == 0 ? action->miLeftRoadHighlightState : action->miRightRoadHighlightState));
            if (mRoadIds[i] != 0)
            {
                FindRoadRuler(static_cast<ERoadSide>(i), action, BrnStreetData::E_SCORE_TYPE_TIME);
                FindRoadRuler(static_cast<ERoadSide>(i), action, BrnStreetData::E_SCORE_TYPE_CRASH);
            }
        }
    }
    meCurrentSignState = static_cast<ERoadState>(ConvertGameStateToCategory(action->miCurrentRoadHighlightState));
    maRoadEntrancePosition[0] = action->mJunctionPosition;
    maRoadEntrancePosition[1] = action->mSecondJunctionPosition;
    miCurrentRoadIndex = action->miCurrentRoadIndex;
    maiTurningRoadIndices[0] = action->miLeftRoadIndex;
    maiTurningRoadIndices[1] = action->miRightRoadIndex;
}
}

namespace BrnGui {
// ARTIST 0x82504A08, scores for the map's current-road panel.
void GuiEventRoadRuleData::Construct(const BrnGameState::GameStateModuleIO::RoadRulesEnterRoadAction* action)
{
    CGS_ASSERT(action != 0, "lpGamePlayAction");
    mRoadID = action->mRoadId;
    for (s32 i = 0; i < 2; ++i)
    {
        const auto type = static_cast<BrnStreetData::ScoreType>(i);
        auto& rule = mRules[i];
        rule.mFriendName.Construct("");
        rule.mRivalId = 0;
        rule.maiScores[0] = rule.maiScores[1] = rule.maiScores[2] = 0;
        if (action->mParScores.ContainsData(type))
        {
            CgsID rival;
            action->mParScores.GetScore(type, &rule.maiScores[0], &rival);
            rule.mRivalId = action->maParRivalIds[i];
        }
        if (action->mUserScores.ContainsData(type)) rule.maiScores[1] = action->mUserScores.GetScore(type);
        if (action->mFriendScores.ContainsData(type))
            action->mFriendScores.GetScore(type, &rule.maiScores[2], &rule.mFriendName);
    }
}
}
