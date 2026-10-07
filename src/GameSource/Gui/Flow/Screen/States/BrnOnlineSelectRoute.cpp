// ===================================================================================
// BrnGui::OnlineSelectRoute -- the online route-selection screen state.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineSelectRoute.cpp
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineSelectRoute.h"

namespace BrnGui
{
    // the route screen and the shared online frame.
    const CgsGui::sResourceTuple OnlineSelectRoute::maResourceTuplesToLoad[] =
        { { 178, CgsGui::E_GUI_RESOURCETYPE_APT }, { 191, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineSelectRoute::miNumResourcesToLoad = 2;

    // member construction only: the console body installs the state and
    // route-component vtables, runs the two MenuComponent constructors, the HelpBar
    // constructor and the map component's MapManager constructor, and installs the vtables
    // of the text fields, the eight animators, the borough outline and the cursor. No
    // scalar member is stored until OnEnter.
    OnlineSelectRoute::OnlineSelectRoute()
    {
    }
}
