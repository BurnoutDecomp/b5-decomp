// Tiny embed/compile check for the rw::audio::core EALayer3 decoder thunks: includes
// the header and references each reconstructed forwarding event so the gate covers the
// two thunk TUs. (EaLayer3DecBase::CreateInstance is defined in its own TU and is only
// referenced, not defined here -- this is a compile-only gate.)
#include "rw/audio/core/EaLayer3Dec.h"

namespace
{
void EaLayer3DecEmbedCheck()
{
    using namespace rw::audio::core;

    sizeof(EaLayer31Dec);
    sizeof(EaLayer32PcmDec);

    s32 (*v1)(s32) = &EaLayer31Dec::CreateInstanceEvent;
    s32 (*v2)(s32) = &EaLayer32PcmDec::CreateInstanceEvent;
    (void)v1;
    (void)v2;
}
} // namespace
