// =====================================================================================
// rw::audio::core::TimerHandle::TimerHandle -- default constructor.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX @0x82B6BEA8; the
// PowerPC asm is authoritative. No LegacySrc reference source and no DecFIGS DWARF exist.
//
// Store-for-store from the asm:
//   stw r10(0),  0(r3)    -> mItemHandle.pNode = 0
//   stw r11("Unknown"), 0xC(r3) -> mpName = "Unknown"
//   stw r10(0),  0x10(r3) -> mCpuTicks = 0
//   stb r9(3),   0x14(r3) -> mStage = TIMERSTAGE_NONE
// =====================================================================================

#include "rw/audio/core/TimerHandle.h"

namespace rw
{
namespace audio
{
namespace core
{

TimerHandle::TimerHandle()
    : mItemHandle(), mpName("Unknown"), mCpuTicks(0), mStage(TIMERSTAGE_NONE)
{
}

} // namespace core
} // namespace audio
} // namespace rw
