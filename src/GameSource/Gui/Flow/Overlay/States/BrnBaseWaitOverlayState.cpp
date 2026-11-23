#include "GameSource/Gui/Flow/Overlay/States/BrnBaseWaitOverlayState.h"

#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"   // InputBuffer::GuiEventQueue (the in-queue type)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"       // GuiOverlayWaitFinishRequest

// BrnGui::BaseWaitOverlayState -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (2 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Overlay/States/BrnBaseWaitOverlayState.cpp):
//   BaseWaitOverlayState::SetupOverlay  @0x824B1B50
//   BaseWaitOverlayState::UpdateRunning @0x824B26D0

namespace BrnGui
{

// @ 0x824B1B50
void BaseWaitOverlayState::SetupOverlay(const GuiOverlayFullInfoResponse* lpResponse)
{
    BaseOverlayState::SetupOverlay(lpResponse);

    // A wait popup offers no actions: blank text, both glyphs invisible (the X360
    // passes the empty string @0x820046A7 and button 15 twice, remap on).
    mHelpItem1Component.SetItem("", FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE, true);
    mHelpItem2Component.SetItem("", FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE, true);
}

// @ 0x824B26D0
bool BaseWaitOverlayState::UpdateRunning()
{
    // DWARF locals (BrnGuiOverlayUnity CU): lpEvent cpp:43, liEventSize :44, liEventId :45,
    // lpLeaveRequest :53. Neither out-param is initialised and the queue member is
    // re-read for each call (ARTIST 0x824B26D0).
    const CgsModule::Event* lpEvent;
    s32 liEventSize;
    s32 liEventId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);
    while (lpEvent != NULL)
    {
        // 188: GuiOverlayWaitFinishRequest's wire id (its GetEventType()).
        if (liEventId == 188)
        {
            const GuiOverlayWaitFinishRequest* lpLeaveRequest =
                static_cast<const GuiOverlayWaitFinishRequest*>(static_cast<const void*>(lpEvent));
            if (lpLeaveRequest->mOverlayId == mCurrentOverlayId)
            {
                meLeaveMethod = GuiOverlayCompleteEvent::E_LEAVEMETHOD_NONE;
                return true;
            }
        }

        liEventId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
    }

    return false;
}
}
