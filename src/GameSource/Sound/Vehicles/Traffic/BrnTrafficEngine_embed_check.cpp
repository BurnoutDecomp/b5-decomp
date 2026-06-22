// Tiny embed check: include the BrnTrafficEngine home and reach Traffic3DControl
// through its Brn3DEffectControl base, confirming the inheritance spine compiles
// and the destructor body is reachable (incl. the inherited Attrib::Instance member
// teardown). Compile-only (cl /c); not part of the shipped TU.
#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficEngine.h"

namespace
{
void BrnTrafficEngineEmbedCheck()
{
    BrnSound::Logic::Traffic::Traffic3DControl lObj;

    // Reach the object through its inherited base path.
    BrnSound::Logic::Brn3DEffectControl* lpBase = &lObj;
    (void)lpBase;

    // The inherited Attrib::Instance member tears down via ~Brn3DEffectControl;
    // exercising construction + destruction here confirms the chain links.
}
} // namespace

int main()
{
    BrnTrafficEngineEmbedCheck();
    return 0;
}
