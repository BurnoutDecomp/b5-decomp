// =====================================================================================
// rw::audio::core::Encoder bodies -- the shared base slots of the rwaudio stream encoders.
//
// EARenderWare "rwaudio". Reconstructed from the console image; its PowerPC asm is
// authoritative. See Encoder.h for the layout and the v-table order. The base deleting
// destructor (reinstall the base table, free when asked) is the compiler's output for the
// empty virtual ~Encoder.
// =====================================================================================

#include "rw/audio/core/Encoder.h"
#include "rw/audio/core/PlugIn.h" // System (mpAllocator +0x14)

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// GetDataRateOverhead -- 10240 bytes of fixed overhead per channel (byte load of the
// channel count at +0x1C, times 0x2800).
// -------------------------------------------------------------------------------------
s32 Encoder::GetDataRateOverhead()
{
    return mucChannelCount * 10240;
}

// -------------------------------------------------------------------------------------
// Release -- tail-call the owning System allocator's Free(this, 0). The console goes
// straight to mpSystem->mpAllocator (slot +0x0C of the allocator) rather than through
// System::Free, so no allocator override is consulted.
// -------------------------------------------------------------------------------------
void Encoder::Release()
{
    mpSystem->mpAllocator->Free(this, 0);
}

} // namespace core
} // namespace audio
} // namespace rw
