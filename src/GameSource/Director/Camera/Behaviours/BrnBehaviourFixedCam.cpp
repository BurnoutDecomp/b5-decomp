// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourFixedCam slice this TU owns.
// SetParameters @0x821F4518 is defined inline in the header; this .cpp is the
// translation-unit anchor that pulls the header into the compile gate and forces an
// out-of-line emission of SetParameters. The rest of the behaviour (Construct/Prepare/
// Update and the full rig) lands with its own TU.
//
// SetParameters is adopted by the scripted-moment / arbitrator code (the static-cam impact
// moment and the arbitrator testbed) when they install a fixed-cam parameter block.
//
// This TU also homes BehaviourFixedCam::Parameters::Serialise<S> -- the fixed-cam parameter
// field-walk visitor -- with the two file/menu serialisers it is instantiated over on X360:
//   - Serialise<DebugMenuSerialiser>    @0x82214BF0  (Process<float> + SetStep per field)
//   - Serialise<TextFileWriteSerialiser> @0x822151C8 (FormatName + fprintf per field)
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

// FLAG (unrecovered rodata @0x820051C0): the field label the fixed-cam Serialise passes for the
// +0x08 tunable. Both instances reference only the address (lis/addi unk_820051C0 -- the
// DebugMenu visitor @0x82214C08 and the TextFileWrite visitor @0x822151F0); the literal string
// bytes are not in the export, so the label is declared extern and NOT fabricated (same
// unrecovered rodata address the bumper-cam visitor references for its own +0x24 label). When
// that rodata is recovered, define it with the literal bytes at @0x820051C0.
extern const char* const KPC_LABEL_820051C0;   // @0x820051C0 -- +0x08 field label

// Out-of-line anchor: forces SetParameters to be emitted in this TU.
void BehaviourFixedCam_SetParametersAnchor(
    BehaviourFixedCam& lrBehaviour,
    const BehaviourFixedCam::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

// ----------------------------------------------------------------------------
// BehaviourFixedCam::Parameters::Serialise<S> -- the ONE fixed-cam field-walk visitor body.
//
// Hands each of the block's two f32 tunables to the serialiser S by name, in ascending-offset
// order. S supplies the per-field direction, inlined per instance:
//   DebugMenuSerialiser    -- Process<float>(name, field) + SetStep(mpDebugComponent, &field, 0.01f)
//                             (@0x82214BF0: bl Process<float>; lwz r3,0x48(S); bl SetStep, step=0.01)
//   TextFileWriteSerialiser -- if mpFile: FormatName(buf, name) + fprintf(mpFile,"%s : %f\n",...)
//                             (@0x822151C8: two `if(mpFile)` guarded FormatName+fprintf pairs)
// The field sequence + labels are identical across both instances' asm; only S's inlined leaf
// helper differs, so the source body is uniform. The DebugMenuSerialiser instance drives its
// f32 leaf overload's Process<float>/SetStep pair; the +0x0C step (0.01f) lives inside that
// overload, matching the SetStep constant in the asm.
//
// Field/label map (from both instances' asm, ascending offset):
//   +0x08 mfField08  <unk_820051C0>   (label rodata unrecovered)
//   +0x0C mfMaxDutch "Max Dutch"
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourFixedCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise(KPC_LABEL_820051C0, mfField08);   // +0x08 -- label unrecovered (extern)
    lrSerialiser.Serialise("Max Dutch", mfMaxDutch);         // +0x0C
}

// Explicit instantiations -- one per serialiser this block is walked through on X360.
template void BehaviourFixedCam::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void BehaviourFixedCam::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);

} // namespace Camera
} // namespace BrnDirector
