// Tiny embed check: instantiate the reconstructed header surface so the gate
// exercises the BrnState base relationship and the bodied scalar deleting
// destructor (@0x826C84D8, the only function this TU's dossier attests). Not
// part of the shipped TU.
#include "GameSource/Sound/Module/LogicModule/BrnState.h"

namespace
{
void EmbedCheck()
{
    BrnSound::Logic::BrnState* pState = new BrnSound::Logic::BrnState();

    // Reach the object through the engine State base (BrnState.h's minimal
    // local State; see BrnState.h's RE-HOME note for why it is not yet the
    // full CgsState.h base).
    CgsSound::Logic::State* pBase = pState;
    (void)pBase;

    // Exercise the scalar deleting destructor: ~BrnState() -> DestroyEffects()
    // (inherited by name from CgsSound::Logic::State, declaration-only there).
    delete pState;
}
} // namespace

int main()
{
    EmbedCheck();
    return 0;
}
