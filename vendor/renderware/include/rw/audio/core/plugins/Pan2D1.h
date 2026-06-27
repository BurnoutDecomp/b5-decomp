#pragma once

// =====================================================================================
// rw::audio::core::Pan2D1 -- the "Pan2D1" 2D stereo panner plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm
// of Pan2D1::Speaker::Set @0x82B986F0 is authoritative. The DecFIGS DWARF for this type
// carries only the plug-in registration GUID (pan2d1.h:48, DecoderDesc::Guid =
// 1349399089); no struct layout and no prior source exist for it, so the per-
// speaker gain layout below is grounded directly in the disassembly.
//
// Sibling to the committed rw::audio::core homes (PlugIn, Iir2, Unpack0, ...).
//
// A Speaker holds the constant-power stereo gain pair for one output speaker, computed
// from a pan angle theta (radians): the asm stores cos(theta) at +0x00 and sin(theta)
// at +0x04 (mfGainA / mfGainB). SpeakerConfig calls Speaker::Set per speaker to build
// the panning matrix.
// =====================================================================================

#include "types.hpp" // f32

namespace rw
{
namespace audio
{
namespace core
{

class Pan2D1
{
public:
    // Plug-in registration id from the DecFIGS DWARF (pan2d1.h:48).
    enum { KU_GUID = 1349399089u };

    // One output speaker's constant-power stereo gain pair. Two single-precision
    // floats; Set() writes cos(theta) to the first and sin(theta) to the second.
    // FLAG (rwaudio PDB reconcile): Pan2D1::Speaker [sizeof=8] confirmed; PDB names
    // the pair x/y (float +0x00, float +0x04); guessed mfGainA/mfGainB renamed to x/y.
    struct Speaker
    {
        f32 x; // +0x00 -- cos(theta)
        f32 y; // +0x04 -- sin(theta)

        // @ 0x82B986F0 -- set the pair from a pan angle `theta` (radians).
        // (r3=this, f1=theta; the asm computes cos into +0x00 and sin into +0x04,
        // rounding each double result to single precision with frsp.)
        void Set(f64 theta);
    };
};

} // namespace core
} // namespace audio
} // namespace rw
