#include "GameSource/Gui/Flow/Screen/Components/BrnEventPanel.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGui
{

// @ 0x82417A10
    void EventPanel::SetPlayerRank(s32 iRank)
    {
        CGS_ASSERT(iRank >= 0, "0<=liCurrentRank");   // @0x82417A4C  h:258

        miPlayerRank = iRank;   // +0x8B4
    }

// @ 0x82417A70
    void EventPanel::SetModeRanks(s32 iRaceRank, s32 iRoadRageRank,
                                 s32 iStuntAttackRank, s32 iMarkedManRank)
    {
        CGS_ASSERT(iRaceRank >= 0,        "0<=liCurrentRaceRank");        // @0x82417AB4  h:279
        CGS_ASSERT(iRoadRageRank >= 0,    "0<=liCurrentRoadRageRank");    // @0x82417AD8  h:280
        CGS_ASSERT(iStuntAttackRank >= 0, "0<=liCurrentStuntAttackRank"); // @0x82417AFC  h:281
        CGS_ASSERT(iMarkedManRank >= 0,   "0<=liCurrentMarkedManRank");   // @0x82417B20  h:282

        miCurrentRaceRank        = iRaceRank;        // +0x8B8
        miCurrentRoadRageRank    = iRoadRageRank;    // +0x8BC
        miCurrentStuntAttackRank = iStuntAttackRank; // +0x8C0
        miCurrentMarkedManRank   = iMarkedManRank;   // +0x8C4
    }

// @ 0x824B3600
    BrnProgression::RaceEventData::EModeType
    EventPanel::ConvertLocalEventDefToProgressionEventDef(EventPanel::EEventType eLocalType)
    {
        // Map the panel's local event-filter type onto a progression race-event mode. The
        // mapping is identity for the five concrete modes; E_EVENT_TYPE_ALL (5) and any
        // unknown value collapse to E_MODE_COUNT (X360 sets r27 = 6 at entry and returns it
        // for both the case-5 arm and the default arm).
        switch (eLocalType)
        {
            case E_EVENT_TYPE_RACE:          return BrnProgression::RaceEventData::E_MODE_RACE;          // 0 -> 0
            case E_EVENT_TYPE_ROAD_RAGE:     return BrnProgression::RaceEventData::E_MODE_ROAD_RAGE;     // 1 -> 1
            case E_EVENT_TYPE_STUNT_ATTACK:  return BrnProgression::RaceEventData::E_MODE_STUNT_ATTACK;  // 2 -> 2
            case E_EVENT_TYPE_SURVIVOR:      return BrnProgression::RaceEventData::E_MODE_SURVIVOR;      // 3 -> 3
            case E_EVENT_TYPE_BURNING_ROUTE: return BrnProgression::RaceEventData::E_MODE_BURNING_ROUTE; // 4 -> 4
            case E_EVENT_TYPE_ALL:           return BrnProgression::RaceEventData::E_MODE_COUNT;         // 5 -> 6
            default:
                // X360 streamed "Unknown event type " + (int)eLocalType + "\n" into the assert
                // message buffer; collapse to one CGS_ASSERT keeping only the rodata literal.
                CGS_ASSERT(false, "Unknown event type ");
                return BrnProgression::RaceEventData::E_MODE_COUNT;   // r27 = 6
        }
    }

}
