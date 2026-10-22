#include "GameSource/Gui/CustomRenderer/BrnGuiCustRendererDebugComponent.h"
#include "GameSource/Gui/BrnCustomRendererManager.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX GuiCustRendererDebugComponent::UpdateBoostColours
// @0x824F7C48. The full class declaration is shared with the manager-owned debug component; the
// DecFIGS accessor returns the embedded BoostBarRenderer without importing the console's +0xE520 offset.

namespace BrnGui
{
void GuiCustRendererDebugComponent::UpdateBoostColours()
{
    // The console reads inner then outer and stores each triple into the registered slider fields.
    BoostBarRenderer* lpRenderer = mpCustRenderManager->GetBoostBarRendererInstance();
    const Vector3 lv3Inner = lpRenderer->GetInnerBoostBarColour();
    const Vector3 lv3Outer = lpRenderer->GetOuterBoostBarColour();

    mfBoostInnerRed   = lv3Inner.x;  // guest +0x24
    mfBoostInnerGreen = lv3Inner.y;  // guest +0x28
    mfBoostInnerBlue  = lv3Inner.z;  // guest +0x2C
    mfBoostOuterRed   = lv3Outer.x;  // guest +0x18
    mfBoostOuterGreen = lv3Outer.y;  // guest +0x1C
    mfBoostOuterBlue  = lv3Outer.z;  // guest +0x20
}
}
