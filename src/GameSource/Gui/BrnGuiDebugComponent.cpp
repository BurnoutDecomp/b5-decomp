// BrnGuiDebugComponent.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   BrnGui::GuiDebugComponent::GetSingletonPtr @ 0x824ECC40
//
// Loads the static singleton pointer (X360 off_82FB5080), runs a non-fatal null guard
// (the X360 fires the "mspSingletonThis" assert when the pointer is null but returns it
// either way), and returns it. The X360-baked assert file/line are discarded per project
// convention; the stringized condition matches the X360 assert message text.

#include "GameSource/Gui/BrnGuiDebugComponent.h"

namespace BrnGui
{

// X360 off_82FB5080 -- the live singleton instance, populated when the debug component
// is constructed. Defaults to null until then.
GuiDebugComponent* GuiDebugComponent::mspSingletonThis = nullptr;

// @ 0x824ECC40
GuiDebugComponent* GuiDebugComponent::GetSingletonPtr()
{
    GuiDebugComponent* lpSingleton = mspSingletonThis;
    CGS_ASSERT( lpSingleton != nullptr, "mspSingletonThis" );
    return lpSingleton;
}

} // namespace BrnGui
