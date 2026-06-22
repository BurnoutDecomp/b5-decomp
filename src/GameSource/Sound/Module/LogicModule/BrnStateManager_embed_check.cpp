// Tiny embed check: exercise the BrnStateManager RTTI surface and the bodied
// GetTypeName. BrnStateManager still carries un-bodied pure virtuals
// (IResourceRequester), so it is exercised through a reference/pointer rather
// than instantiated. Not part of the shipped TU.
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
