// ============================================================================
// GameSource/Director/Camera/Utils/BrnOrientationLagSerialise.cpp
//
// Compilation home for BrnDirector::Camera::Utils::OrientationLag::Parameters::Serialise<S> --
// the orientation-smoother parameter-block field-walk visitor. This is a separate TU from
// BrnOrientationLag.cpp (which owns Update); it only carries the two template instantiations
// the camera-tunings serialiser system drives for this block:
//   - Serialise<TextFileReadSerialiser>  @0x82202F78
//   - Serialise<TextFileWriteSerialiser> @0x82215EA8
//
// ONE uniform templated body drives the serialiser S by name; each S supplies the per-field
// direction (write a "<label> : <value>\n" line / read it back). The two X360 instances are the
// same field/label sequence in the same order -- only S's inlined leaf helper differs (Hex-Rays
// renders the write as FormatName+fprintf / out-of-line Serialise calls, the read as fscanf).
// Reconstructed as the shared source that produces both.
//
// The leading version handling (mVersion tag + the version gate) is part of the uniform body:
//   - write @0x82215EA8: stores 1 into muVersion (`*a1 = 1`), inlines the VersionNumber leaf as
//     FormatName+fprintf "%s : %d\n", then -- only if the tag still reads 1 (`*a1 == 1`,
//     bne loc_82215F38) -- walks mfSlerpSpring (out-of-line TextFileWriteSerialiser::Serialise,
//     block+0x10) and mbUseSlerpSpring (the bool overload sub_82208958, block+0x14).
//   - read @0x82202F78: stores 1, fscanf "%s : %d\n" back into muVersion; the version read can
//     genuinely change the tag, so the `== 1` gate (bne loc_82203014) is load-bearing on read.
//     Then fscanf "%s : %f\n" into mfSlerpSpring (block+0x10) and fscanf "%s : %d\n" into a temp
//     whose zero/non-zero is stored as the mbUseSlerpSpring bool (block+0x14; cntlzw/xori idiom
//     == (value != 0)).
//
// Field/label map (from the write @0x82215EA8 + read @0x82202F78 asm; NOTE non-contiguous --
// the +0x04..+0x0C pitch/yaw/roll spring fields are NOT walked by this visitor, consistent
// across both instances' asm):
//   +0x00 muVersion        "Version Number (dont change)"   (VersionNumber, = KU_VERSION)
//   +0x10 mfSlerpSpring     "Slerp spring"                  (f32)
//   +0x14 mbUseSlerpSpring  "Use slerp spring"              (bool)
//
// FLAG: only two serialiser instances are attested in the X360 binary for this block
// (Read @0x82202F78 + Write @0x82215EA8); unlike the Looker / PositionLag sibling blocks there
// is NO attested DebugMenuSerialiser instance, so none is instantiated here (not fabricated).
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnOrientationLag.h"

#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

// ----------------------------------------------------------------------------
// OrientationLag::Parameters::Serialise<S> -- the ONE orientation-smoother field-walk visitor.
//
// Stamps the version tag (KU_VERSION = 1), hands the version field to S, then -- only when the
// version still reads 1 -- walks the slerp-spring scalar and its enable flag into S by name.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void OrientationLag::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    const u32 KU_VERSION = 1u;                                 // li r11,1 ; stw r11,0(block)

    muVersion.muVersion = KU_VERSION;
    lrSerialiser.Serialise("Version Number (dont change)", muVersion);
    if (muVersion.muVersion == KU_VERSION)
    {
        lrSerialiser.Serialise("Slerp spring", mfSlerpSpring);          // +0x10
        lrSerialiser.Serialise("Use slerp spring", mbUseSlerpSpring);   // +0x14
    }
}

// Explicit instantiations -- one per serialiser this block is saved/loaded through (the two
// attested X360 instances; see the DebugMenuSerialiser FLAG in the file header).
template void OrientationLag::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void OrientationLag::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
