// Tiny embed check: exercise the BrnStateManager RTTI surface and the bodied
// GetTypeName. BrnStateManager is concrete (all IResourceRequester overrides are
// bodied); exercised through a reference/pointer to avoid pulling in a real
// StateManager construction context. Not part of the shipped TU.
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"

namespace
{
const char* EmbedCheck(BrnSound::Logic::BrnStateManager& rMgr)
{
    // Reach the object through both inheritance paths.
    CgsSound::Logic::StateManager*       pMgr = &rMgr;
    BrnSound::Logic::IResourceRequester* pReq = &rMgr;
    (void)pMgr;
    (void)pReq;
    return rMgr.GetTypeName();
}
} // namespace
