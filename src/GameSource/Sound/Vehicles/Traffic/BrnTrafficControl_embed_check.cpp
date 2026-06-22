// Tiny embed check: include the BrnTrafficControl home and reach the object
// through its committed BrnEffectControl base and the IResourceRequester second
// base, confirming the dual-base layout compiles and the destructor body is
// reachable. Compile-only (cl /c); not part of the shipped TU.
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficControl.h"

namespace
{
void BrnTrafficControlEmbedCheck(BrnSound::Logic::Traffic::TrafficControl& rObj,
                                 BrnSound::Logic::IResourceRequester*& rpReq)
{
    // Reach the object through both inheritance paths of the committed base.
    BrnSound::Logic::BrnEffectControl*   pBase = &rObj;
    CgsSound::Logic::EffectControl*      pEff  = &rObj;
    BrnSound::Logic::IResourceRequester* pReq  = &rObj; // the +4 sub-object base
    (void)pBase;
    (void)pEff;

    // Inherited teardown members reachable by name through the committed base.
    rObj.meDetachState    = CgsSound::Logic::EffectBase::E_DETACH_STATE_FINISHED;
    rObj.mbResourcesReady = false;
    rObj.meAttachState    = CgsSound::Logic::EffectBase::E_ATTACH_STATE_NONE;

    rpReq = pReq;
}
} // namespace
