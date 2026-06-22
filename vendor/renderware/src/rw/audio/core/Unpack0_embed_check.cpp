// Tiny embed/compile check for the rw::audio::core::Unpack0 home: includes the
// header and touches each reconstructed entry point so the gate exercises the TU.
#include "rw/audio/core/Unpack0.h"

namespace
{
void Unpack0EmbedCheck()
{
    using namespace rw::audio::core;

    sizeof(Unpack0);

    const unsigned char src[8] = { 0 };
    unsigned int out = 0;
    (void)Unpack0::UnpackInt32(src, &out);
    (void)Unpack0::UnpackUInt32(src, &out);
}
} // namespace
