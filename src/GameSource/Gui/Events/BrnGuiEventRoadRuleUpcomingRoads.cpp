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
