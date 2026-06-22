#include "GameSource/Sound/Global/BrnMixerControl.h"

// Exercise BrnSound::Logic::MixerControl::`vector deleting destructor' @ 0x826BAED8
// and its adjustor{4} thunk @ 0x826BAED0 (both reached through the virtual
// destructor declared on the dual-base BrnEffectControl shape).
namespace
{
void ExerciseMixerControlTeardown()
{
    // Construct + destroy through the most-derived type (drives ~MixerControl,
    // which chains the inherited BrnEffectControl base teardown).
    BrnSound::Logic::MixerControl lMixer;
    (void)lMixer;

    // Delete through the IResourceRequester second base (this+4): exercises the
    // virtual destructor dispatch the adjustor{4} thunk implements.
    BrnSound::Logic::MixerControl* lpMixer = new BrnSound::Logic::MixerControl();
    BrnSound::Logic::IResourceRequester* lpReq = lpMixer;
    delete lpReq;
}
}

// MixerControl reuses the committed BrnEffectControl base by name and only adds
// non-owning members, so it must be at least as large as that base.
static_assert(sizeof(BrnSound::Logic::MixerControl) >= sizeof(BrnSound::Logic::BrnEffectControl),
              "MixerControl must embed the BrnEffectControl base");
