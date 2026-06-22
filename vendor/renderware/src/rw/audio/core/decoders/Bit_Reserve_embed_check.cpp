// Tiny embed/compile check for the rw::audio::core::Bit_Reserve home: includes the
// header and touches each reconstructed entry point so the gate exercises the TU.
#include "rw/audio/core/decoders/Bit_Reserve.h"

namespace
{
void BitReserveEmbedCheck()
{
    using namespace rw::audio::core;

    Bit_Reserve br = {};
    (void)br.hget1bit();
    (void)br.rewindNbits(3);
}
} // namespace
