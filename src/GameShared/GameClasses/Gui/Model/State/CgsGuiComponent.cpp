#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82847030.

namespace CgsGui
{
    // @ 0x82847030 - assert the incoming state interface is non-null, then store it
    // in mpStateInterface (the X360 writes it at +0x88). Path/line from the baked
    // assert string (GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h:169).
    void GuiComponent::SetStateInterface(StateInterface* lpStateInterface)
    {
        CGS_ASSERT(lpStateInterface != nullptr,
                   "Invalid state interface sent to GuiComponent::SetStateInterface");
        mpStateInterface = lpStateInterface;
    }
}
