// Compile anchor for the BrnRoadRuleComponent.h HEADER TU (the three X360-emitted
// header-inlines -- EnterRoad @0x82441450 / HandleLeaveRoadEvent @0x82413D78 /
// HandleUpcomingRoadEvent @0x82441338 -- live in the header; this TU pulls it
// through the compile gate).

#include "GameSource/Gui/Flow/HUD/Components/BrnRoadRuleComponent.h"

namespace BrnGui
{
    namespace
    {
        // Embed check: the class + the road-rules event grows stay complete and the
        // owned inlines stay instantiable as committed headers evolve (never executed).
        void BrnRoadRuleComponent_EmbedCheck(RoadRuleComponent* lpComponent,
                                             const GuiEventRoadRuleUpcomingRoads* lpEvent,
                                             CgsID lRoadId)
        {
            lpComponent->HandleLeaveRoadEvent(lRoadId);
            lpComponent->HandleUpcomingRoadEvent(lpEvent);
        }
        void (*gpBrnRoadRuleComponentEmbedCheck)(RoadRuleComponent*,
                                                 const GuiEventRoadRuleUpcomingRoads*, CgsID) =
            &BrnRoadRuleComponent_EmbedCheck;
    }
}
