// ============================================================================
// GameSource/Director/Camera/Utils/BrnPositionLagSerialise.cpp
//
// Compilation home for BrnDirector::Camera::Utils::PositionLag::Parameters::Serialise<S> -- the
// position-smoother parameter-block field-walk visitor. A separate TU from BrnPositionLag.cpp
// (which owns Construct / Update), matching the sibling BrnOrientationLagSerialise.cpp split: the
// visitor is only reachable through the camera-tunings serialiser system, whose serialiser types
// are not part of the shipping link.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnPositionLag.h"

// Parameters::Serialise<S> drives the camera-tunings serialiser S by name; each S supplies the
// per-field scalar overloads (write=FormatName+fprintf, read=fscanf, debug-menu=Process+SetStep).
// Pull in the three serialisers this block is saved/loaded/tuned through so the explicit
// instantiations below have their full definitions available.
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Utils::PositionLag::Parameters::Serialise<S> -- the ONE position-lag
// field-walk visitor body. Instantiated over the three camera-tunings serialisers:
//   - Serialise<TextFileWriteSerialiser>
//   - Serialise<TextFileReadSerialiser>
//   - Serialise<DebugMenuSerialiser>
//
// Stamps the version tag (KU_VERSION = 1), hands the version field to S, then -- only when the
// version still reads 1 -- walks the four tunable f32 fields (ascending offset) into S by name.
// The body is uniform across S; the three asm instances differ only because each S's inlined
// scalar overloads expand differently:
//   - TextFileWriteSerialiser: version -> FormatName+fprintf "%s : %d\n" (inlined, guarded on
//     mpFile); each f32 -> out-of-line TextFileWriteSerialiser::Serialise(name, f32&) call.
//   - TextFileReadSerialiser:  version -> fscanf "%s : %d\n" (inlined, guarded on *mpFile); each
//     f32 -> fscanf "%s : %f\n" (inlined, each re-guarded on *mpFile). Here the version read can
//     genuinely change muVersion, so the `== KU_VERSION` gate is load-bearing on read.
//   - DebugMenuSerialiser: its u32 (version) overload registers nothing, so the version call and
//     the always-true gate fold away in the asm (which stores 1 then walks straight into the
//     floats); each f32 -> Process<float>(name, value) + mpDebugComponent->SetStep(&value, 0.01f)
//     (inlined). The differing per-field direction is entirely inside each S's overloads (their
//     own TUs); this visitor body is a single source template.
//
// Field/label map (from the write+read+debug-menu asm, ascending offset):
//   +0x00 muVersion    "Version Number (dont change)"   (u32, = KU_VERSION)
//   +0x04 mfXResponse  "X Response"
//   +0x08 mfYResponse  "Y Response"
//   +0x0C mfZResponse  "Z Response"
//   +0x10 mfSmoothing  "Smoothing"
// ----------------------------------------------------------------------------
template<class TSerialiser>
void PositionLag::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    const u32 KU_VERSION = 1u;                                 // li r_,1 ; stw ...,0(block)

    muVersion = KU_VERSION;
    lrSerialiser.Serialise("Version Number (dont change)", muVersion);
    if (muVersion == KU_VERSION)
    {
        lrSerialiser.Serialise("X Response", mfXResponse);     // +0x04
        lrSerialiser.Serialise("Y Response", mfYResponse);     // +0x08
        lrSerialiser.Serialise("Z Response", mfZResponse);     // +0x0C
        lrSerialiser.Serialise("Smoothing",  mfSmoothing);     // +0x10
    }
}

// Explicit instantiations -- one per serialiser this block is saved/loaded/tuned through.
// (Unqualified names resolve to the enclosing BrnDirector::Camera namespace.)
template void PositionLag::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void PositionLag::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void PositionLag::Parameters::Serialise<Camera::DebugMenuSerialiser>(Camera::DebugMenuSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
