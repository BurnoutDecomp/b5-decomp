// Tiny embed check: include the BrnPassbyEffect home and reach Passby3DControl
// through its Brn3DEffectControl base, confirming the inheritance spine compiles
// and that the destructor (which tears down the inherited Attrib::Instance member
// mEngineDataAtrib) is reachable. Compile-only (cl /c); not part of the shipped TU.
#include "GameSource/Sound/Passby/BrnPassbyEffect.h"

namespace
{
void BrnPassby3DControlEmbedCheck(BrnSound::Logic::Passby::Passby3DControl& rObj)
{
    // Reach the object through its inheritance spine.
    BrnSound::Logic::Brn3DEffectControl* pBrn3d = &rObj;
    CgsSound::Logic::Cgs3dEffectControl* pCgs3d = &rObj;
    CgsSound::Logic::EffectControl*      pEff   = &rObj;
    (void)pCgs3d;
    (void)pEff;

    // The inherited member the destructor tears down, reachable by name.
    Attrib::Instance* pEngineData = &pBrn3d->mEngineDataAtrib;
    (void)pEngineData;
}
} // namespace
