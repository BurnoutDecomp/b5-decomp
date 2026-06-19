#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)

// BrnNetwork::RoadRulesManagerDebugComponent - the in-game debug-menu hook for the online road-rules
// manager. Derives from the real CgsDev::DebugComponent; it holds only a back-pointer to its manager
// and exposes two manual menu actions ("Trigger Personal Best", "Get Road Rules High Scores").
//
// Layout (DecFIGS DWARF BrnNetworkRoadRulesManagerDebugComponent.h + X360 binary):
//   DebugComponent base sub-object  +0x00..+0x0B
//   mpRoadRulesManager              +0x0C   (== the *(this+12) back-pointer in every member)
//
// The X360 build attests only Construct / GetName / OnActivate plus the two static action callbacks;
// GetPath / Destruct from the PS3 DWARF are not in the X360 ledger for this TU (folded / PS3-only),
// so the base CgsDev::DebugComponent::GetPath default applies.

namespace BrnNetwork
{
    class NetworkRoadRulesManager;   // back-pointer + method calls; full def in BrnNetworkRoadRulesManager.h (the .cpp includes it)

    class RoadRulesManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct( NetworkRoadRulesManager* lpRoadRulesManager );   // @ 0x82586350

    protected:
        const char* GetName() const override;   // @ 0x82586398 -> "Road Rules"
        void        OnActivate() override;       // @ 0x8258AF90

    private:
        // Static debug-menu action callbacks registered in OnActivate; the void* user-data the menu
        // passes back IS this component.
        static void TriggerPersonalBest( void* lpData );          // @ 0x8258AEB0
        static void RequestRoadRulesHighScores( void* lpData );   // @ 0x825863A8

        NetworkRoadRulesManager* mpRoadRulesManager;   // +0x0C
    };
}
