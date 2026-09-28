// Tiny embed check: include the BrnEffectControl home and reach the object through
// both inheritance paths (the EffectControl primary base and the IResourceRequester
// second base), confirming the dual-base layout compiles and the destructor body is
// reachable. Compile-only (cl /c); not part of the shipped TU.
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"

namespace
{
void BrnEffectControlEmbedCheck(BrnSound::Logic::BrnEffectControl& rObj,
                                BrnSound::Logic::IResourceRequester*& rpReq)
{
    // Reach the object through both inheritance paths.
    CgsSound::Logic::EffectControl*      pControl = &rObj;
    BrnSound::Logic::IResourceRequester* pReq     = &rObj;
    (void)pControl;

    // Exercise the inherited teardown state without accessing protected fields.
    rObj.ResetOnDestroy();

    rpReq = pReq;
}
} // namespace
