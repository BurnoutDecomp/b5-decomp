// BrnGui::OnlineYouWin -- the online "you win" flow state. The resource accessor is the
// header's inline; this TU holds the class's static resource list (read from the image).

#include "GameSource/Gui/Flow/Screen/States/OnlineYouWin.h"

namespace BrnGui
{
    const CgsGui::sResourceTuple OnlineYouWin::maResourcesToLoad[] =
        { { 171, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 OnlineYouWin::muNumResourcesToLoad = 1;
}
