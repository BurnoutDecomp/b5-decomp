// Tiny embed/compile check for the rw::audio::core::Pan2D1 home: includes the header
// and touches the reconstructed entry point so the gate exercises the TU.
#include "rw/audio/core/plugins/Pan2D1.h"

namespace
{
void Pan2D1EmbedCheck()
{
    using namespace rw::audio::core;

    sizeof(Pan2D1::Speaker);

    Pan2D1::Speaker spk = { 0.0f, 0.0f };
    spk.Set(0.5);
    (void)spk.mfGainA;
    (void)spk.mfGainB;
}
} // namespace
