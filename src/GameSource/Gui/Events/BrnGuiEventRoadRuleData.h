#pragma once
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnCgsPlayerName.h"
namespace BrnGui {
// ARTIST 0x82504A08 and AddGuiEvent<GuiEventRoadRuleData>: raw 88-byte payload.
struct GuiEventRoadRuleData
{
    struct RoadRuleType
    {
        s32 maiScores[3];
        CgsNetwork::PlayerName mFriendName;
        CgsID mRivalId;
    };
    CgsID mRoadID;
    RoadRuleType mRules[2];
    s32 GetEventType() const { return 334; }
    void Construct(const BrnGameState::GameStateModuleIO::RoadRulesEnterRoadAction* action);
};
static_assert(sizeof(GuiEventRoadRuleData) == 88, "ARTIST road-rule data");
}
