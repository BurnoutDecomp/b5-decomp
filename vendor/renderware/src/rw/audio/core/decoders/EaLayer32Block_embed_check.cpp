// Tiny embed/compile check for the rw::audio::core::EaLayer32Block home: includes the
// header and touches each reconstructed entry point so the gate exercises the TU.
#include "rw/audio/core/decoders/EaLayer32Block.h"

namespace
{
void EaLayer32BlockEmbedCheck()
{
    using namespace rw::audio::core;

    EaLayer32Block blk = {};
    s16 hdr[3] = { 0, 0, 0 };
    (void)blk.Read(hdr);
    (void)blk.GetUsableSamples();
}
} // namespace
