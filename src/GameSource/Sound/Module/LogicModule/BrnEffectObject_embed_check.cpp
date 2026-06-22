// Tiny embed check: instantiate the reconstructed header surface so the gate
// exercises the layout, the inline ResourcesAreReady/Detach bodies, and the
// dual-base dispatch. Not part of the shipped TU.
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"

namespace
{
void EmbedCheck(BrnSound::Logic::BrnEffectObject& rObj,
                BrnSound::Logic::IResourceRequester*& rpReq)
{
    // Reach the object through both inheritance paths.
    CgsSound::Logic::EffectObject*       pEffect = &rObj;
    BrnSound::Logic::IResourceRequester* pReq    = &rObj;
    (void)pEffect;

    rObj.ResourcesAreReady();
    bool lbDetached = rObj.Detach();
    (void)lbDetached;

    BrnSound::Logic::ResourceRegistrar& rReg = pReq->GetResourceRegistrar();
    (void)rReg;

    rpReq = pReq;

    BrnSound::Logic::BrnEffectObject::SampleTag lTag;
    lTag.mfVolume     = 0.0f;
    lTag.miSampleIndex = 0;
    (void)lTag;
}
} // namespace
