#pragma once

// ===================================================================================
// BrnGui::CrashNavBorough -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnCrashNavBorough.h
//
// The "borough" outline on the online route map: an apt clip that is labelled with the
// selected county's name and moved/sized over that county's rectangle in device space.
// Embedded by value in BrnGui::GuiNetworkRouteInfo (the "Borough_mc" component).
//
// The county rectangles are a class-static table (maBoroughPositions) that Construct
// finishes in place: each entry's centre is computed from its corner and extent.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                             // Vector2
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"     // CgsGui::GuiComponent (base)
#include "GameSource/Gui/SatNav/BrnMainMap.h"                           // MainMapComponent::ZoomFactor
#include "SharedClasses/World/BrnWorldRegion.h"                         // ECounty

namespace BrnGui
{
    struct CrashNavBorough : public CgsGui::GuiComponent
    {
        // One county's map rectangle (world units) and the zoom it is shown at.
        struct BoroughPositions
        {
            Vector2                       mv2Position;        // the rectangle's corner
            Vector2                       mv2WidthHeight;     // its extent
            Vector2                       mv2CenterPosition;  // filled by Construct
            MainMapComponent::ZoomFactor  meZoomFactor;
        };

        // Name the component, then finish every borough's centre point.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // Label the clip with the county's name and move/size it over the county.
        void SetCurrentBorough(BrnWorld::ECounty leBorough);

    private:
        static const s32 KI_NUM_BOROUGHS = 6;   // the five counties plus the whole-map entry

        static BoroughPositions maBoroughPositions[KI_NUM_BOROUGHS];

        static const char macIconState[10];      // "apt_label"
        static const char macXposVar[3];         // "_x"
        static const char macYposVar[3];         // "_y"
        static const char macWidthposVar[7];     // "_width"
        static const char macHeightposVar[8];    // "_height"

        BrnWorld::ECounty meCurrentlySelectedBorough;
    };
}
