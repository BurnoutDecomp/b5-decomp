// BrnGui::OnlineInstantResultsState -- the online "instant results" post-event state. The
// resource accessor is the header's inline; this TU holds the class's static resource list
// (read from the image).

#include "GameSource/Gui/Flow/PostEvent/States/Online/BrnOnlineInstantResults.h"

namespace BrnGui
{
    const CgsGui::sResourceTuple OnlineInstantResultsState::maResourceTuplesToLoad[] =
        { { 226, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineInstantResultsState::miNumResourcesToLoad = 1;
}
