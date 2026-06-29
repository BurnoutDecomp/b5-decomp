#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)

// BrnNetwork::EventScoresManagerDebugComponent - the in-game debug-menu hook for the online
// event-scores manager. Derives from the real CgsDev::DebugComponent; it holds only a back-
// pointer to its manager and exposes one manual menu action ("Set Cavalry Burning Route Score")
// that fabricates an EventScoreData batch and pushes it through the normal upload path.
//
// Layout (X360 binary; every member reaches the back-pointer via *(this + 12)):
//   DebugComponent base sub-object  +0x00..+0x0B
//   mpEventScoresManager            +0x0C   (== the *(this+12) back-pointer)
//
// The X360 build attests Construct / GetName / OnActivate plus the static action callback;
// Destruct (@ 0x82586578) is the folded base-destruct shared body (it only clears the back-
// pointer then chains the base), so the base CgsDev::DebugComponent::Destruct handles teardown.

namespace BrnNetwork
{
    class EventScoresManager;   // back-pointer + member access; full def in BrnEventScoresManager.h

    class EventScoresManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct( EventScoresManager* lpEventScoresManager );   // @ 0x82586530

    protected:
        const char* GetName() const override;   // @ 0x82586588 -> "Event Scores"
        void        OnActivate() override;       // @ 0x82591E98

    private:
        // Static debug-menu action callback registered in OnActivate; the void* user-data the
        // menu passes back IS this component.
        static void SetCalvalryBurningRouteBest( void* lpData );   // @ 0x82591D00

        EventScoresManager* mpEventScoresManager;   // +0x0C
    };
}
