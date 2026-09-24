// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCamSerialise.cpp
//
// Partfile of BrnBehaviourFixedCam.cpp: BehaviourFixedCam::Parameters::Serialise<S>, the fixed-cam
// parameter field-walk visitor, with the serialisers the X360 instantiates it over:
//   - Serialise<DebugMenuSerialiser>     @0x82214BF0  (Process<float> + SetStep per field)
//   - Serialise<TextFileWriteSerialiser> @0x822151C8  (FormatName + fprintf per field)
// SPLIT OUT 2026-09-24 (FX-DIRECTOR) for the same reason BrnBehaviourGyroCamSerialise.cpp and
// BrnCameraShakeUpdate.cpp exist: the behaviour TU is in the exe's link and its serialisers are not,
// so keeping the visitor there would open their symbols as unresolved externals.
//
// Field/label map (both instances' asm, ascending offset):
//   +0x08 mfFOV       "FOV"        (the rodata at 0x820051C0 reads "FOV\0Roll\0")
//   +0x0C mfMaxDutch  "Max Dutch"
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"

// The Parameters::Serialise<S> visitor drives serialiser S's per-leaf Serialise(const char*, f32&)
// overload by name; pull in the two serialisers this TU instantiates the visitor over.
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"

namespace BrnDirector
{
namespace Camera
{

template<class TSerialiser>
void BehaviourFixedCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("FOV", mfFOV);              // +0x08
    lrSerialiser.Serialise("Max Dutch", mfMaxDutch);   // +0x0C
}

// Explicit instantiations -- one per serialiser this block is walked through on X360.
template void BehaviourFixedCam::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void BehaviourFixedCam::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);

} // namespace Camera
} // namespace BrnDirector
