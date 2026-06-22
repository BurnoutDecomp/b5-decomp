// =====================================================================================
// rw::audio::core::TimerHandle::TimerHandle -- default constructor.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX @0x82B6BEA8; the
// PowerPC asm is authoritative. No Feb-2007 leak source and no DecFIGS DWARF exist.
//
// Store-for-store from the asm:
//   stw r10(0),  0(r3)    -> miField0  = 0
//   stw r11("Unknown"), 0xC(r3) -> mpName = "Unknown"
//   stw r10(0),  0x10(r3) -> miField10 = 0
//   stb r9(3),   0x14(r3) -> mbState  = 3   (single byte)
//
// IDA renders the method as `int(int result)`; the sole register arg r3 is `this`.
// =====================================================================================

#include "rw/audio/core/TimerHandle.h"

namespace rw
{
namespace audio
{
namespace core
{

TimerHandle *TimerHandle::TimerHandle_ctor(TimerHandle *self)
{
    self->miField0 = 0;        // +0x00
    self->mpName = "Unknown";  // +0x0C
    self->miField10 = 0;       // +0x10
    self->mbState = 3;         // +0x14 (stb)
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
