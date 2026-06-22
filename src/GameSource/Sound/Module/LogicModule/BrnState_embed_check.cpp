// Tiny embed check: instantiate the reconstructed header surface so the gate
// exercises the BrnState RTTI surface and the bodied GetTypeName. Not part of
// the shipped TU.
#include "GameSource/Sound/Module/LogicModule/BrnState.h"

namespace
{
const char* EmbedCheck(BrnSound::Logic::BrnState& rState)
{
    // Reach the object through the engine State base.
    CgsSound::Logic::State* pState = &rState;
    (void)pState;
    return rState.GetTypeName();
}
} // namespace
