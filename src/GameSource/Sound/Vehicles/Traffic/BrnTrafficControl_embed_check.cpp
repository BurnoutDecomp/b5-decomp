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

    // The public base member and the attach-state accessor, reached by name.
    rObj.mbResourcesReady = false;
    (void)rObj.GetAttachState();

    rpReq = pReq;
}
} // namespace
