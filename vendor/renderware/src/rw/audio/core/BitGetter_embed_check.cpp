// Tiny embed/compile check for the rw::audio::core::BitGetter home: includes the
// header and exercises the reconstructed entry point so the gate covers the TU.
#include "rw/audio/core/BitGetter.h"

namespace
{
void BitGetterEmbedCheck()
{
    using namespace rw::audio::core;

    sizeof(BitGetter);

    const unsigned char buf[8] = { 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x9A };
    BitGetter bg;
    bg.mpBitBuffer = buf;
    bg.mBitPosition = 0;
    (void)BitGetter::GetBits(&bg, 5);
    (void)BitGetter::GetBits(&bg, 11);
    (void)BitGetter::GetBits(&bg, 0);
}
} // namespace
