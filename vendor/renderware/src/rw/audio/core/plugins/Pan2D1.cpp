// =====================================================================================
// rw::audio::core::Pan2D1::Speaker -- constant-power stereo gain pair.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Pan2D1::Speaker::Set @0x82B986F0
// The PowerPC asm is authoritative; the DecFIGS DWARF carries only the plug-in GUID
// (no layout) and no prior source exists. The body is a store-for-store
// equivalent of the asm: cos(theta) -> +0x00, sin(theta) -> +0x04, each rounded to
// single precision (frsp). See Pan2D1.h for the layout description.
// =====================================================================================

#include "rw/audio/core/plugins/Pan2D1.h"

#include <cmath>

namespace rw
{
namespace audio
{
namespace core
{

// ---------------------------------------------------------------------------
// Pan2D1::Speaker::Set @ 0x82B986F0
//
// r3=this, f1=theta. theta is preserved across the cos call in f31, then reused
// as the argument to sin. Both results are narrowed to f32 (frsp) before storage.
// ---------------------------------------------------------------------------
void Pan2D1::Speaker::Set(f64 theta)
{
    mfGainA = static_cast<f32>(cos(theta));
    mfGainB = static_cast<f32>(sin(theta));
}

} // namespace core
} // namespace audio
} // namespace rw
