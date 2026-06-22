// BrnCustomRendererManager.cpp
// BrnGui::CustomRendererManager -- owner of the GUI custom-renderer set.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::CustomRendererManager::SetReplaySerialiser  @ 0x82445648
//
// X360 pseudocode: `*(result + 114384) = a2; return result;` i.e. store the serialiser
// pointer into the member at guest +0x1BED0 and return this. Reconstructed by NAME.

#include "GameSource/Gui/BrnCustomRendererManager.h"

namespace BrnGui
{

CustomRendererManager* CustomRendererManager::SetReplaySerialiser( void* lpReplaySerialiser )
{
    mpReplaySerialiser = lpReplaySerialiser;
    return this;
}

} // namespace BrnGui
