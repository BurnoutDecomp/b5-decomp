#include "GameSource/Gui/Flow/Screen/States/BrnScreenLoading.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. On leaving the loading screen the
// state unregisters from the two GUI events it observed while active. The
// observed-event id table lives in .rdata (@0x820666FC) and resolves at link time.

namespace BrnGui
{
    const s32 ScreenLoading::miNumEventsObserved = 2;

    // @ 0x824B8FD0
    void ScreenLoading::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }
}
