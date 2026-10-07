// BrnGui::OnlinePause -- the online pause screen state. The resource accessor is the header's
// inline; this TU holds the class's static resource list (read from the image).

#include "GameSource/Gui/Flow/Screen/States/BrnOnlinePause.h"

namespace BrnGui
{
    const CgsGui::sResourceTuple OnlinePause::maResourceTuplesToLoad[] =
        { { 169, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlinePause::miNumResourcesToLoad = 1;
}
