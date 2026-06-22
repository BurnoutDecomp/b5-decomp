// Tiny embed check: include the BrnCameraControl home and reach the object
// through its committed BrnEffectControl base and the IResourceRequester second
// base, confirming the dual-base layout compiles and the destructor (which
// anchors the adjustor{4} thunk) is reachable. Compile-only (cl /c); not part of
// the shipped TU.
#include "GameSource/Sound/Global/BrnCameraControl.h"

namespace
{
void BrnCameraControlEmbedCheck(BrnSound::Logic::CameraControl& rObj,
                                BrnSound::Logic::IResourceRequester*& rpReq)
{
    // Reach the object through both inheritance paths of the committed base.
    BrnSound::Logic::BrnEffectControl*   pBase = &rObj;
    CgsSound::Logic::EffectControl*      pEff  = &rObj;
    BrnSound::Logic::IResourceRequester* pReq  = &rObj; // the +4 sub-object base
    (void)pBase;
    (void)pEff;

    // Inherited teardown members reachable by name through the committed base.
    rObj.mbResourcesReady = false;
    rObj.meAttachState    = CgsSound::Logic::EffectBase::E_ATTACH_STATE_NONE;
    rObj.meDetachState    = CgsSound::Logic::EffectBase::E_DETACH_STATE_FINISHED;

    rpReq = pReq;
}
} // namespace
