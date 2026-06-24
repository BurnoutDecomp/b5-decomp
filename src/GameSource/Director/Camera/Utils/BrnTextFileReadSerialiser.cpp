// ============================================================================
// GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.cpp
//
// Compilation home for the BrnDirector::Camera::TextFileReadSerialiser slice this TU owns:
//   - TextFileReadSerialiser::Serialise(FILE**, const char*, f32*) @0x82219AE0
//
// Reached from the camera-rig parameter template
//   Camera::Utils::CameraRig::Params::Serialise<TextFileReadSerialiser>
// when a vec3 tuning is loaded from a human-readable text file.
//
// The dispatcher builds, for each vector component, a label "<name><suffix>" into a fixed 64-byte
// stack buffer via a CgsDev::StrStream sink, then forwards to the matching per-component text
// reader (Serialise<0/1/2>). Store-for-store from the asm at 0x82219AE0:
//
//   off_82000D00 -> &state ; CgsContainers::BasePriorityQueue::Clear(&state)  (base head reset)
//   off_82000D08 -> &state ; state.mpcBuffer = &acLabel ; state.miBufferSize = 0x40 ; acLabel[0]=0
//   loop i in {0,1,2}:
//     CgsDev::StrStream::Reset(&state)
//     sink(&state, name ? name : "<NULLSTRING>")          ; (*(vtable+4)) = operator<<(const char*)
//     sink(&state, KAPC_COMPONENT_SUFFIX[i])              ; unk_8200653C / unk_82006538 / unk_82006534
//     vecArg[0] = pVec3
//     result = Serialise<i>(file, state.mpcBuffer, &vecArg)
//
// NOTE on iteration order: the asm appends the suffix at 0x8200653C first, then 0x82006538, then
// 0x82006534 (descending addresses), pairing those with Serialise<0>, Serialise<1>, Serialise<2>
// respectively. The suffix table below preserves that exact pairing by address.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"

#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// FLAG (unrecovered rodata): the three per-component label suffixes the dispatcher appends live
//   in .rodata at @0x8200653C, @0x82006538, @0x82006534 (4 bytes apart -> each a <=3-char
//   string + NUL). Their TEXT is not recoverable from the export (the asm only references their
//   addresses), so they are declared extern here and NOT fabricated. The X360 build defines them
//   in this TU's rodata; when that rodata is recovered, define KAPC_COMPONENT_SUFFIX[0..2] with
//   the literal bytes at @0x8200653C / @0x82006538 / @0x82006534 in that order. The dispatch
//   control flow below is exact; only the suffix glyphs are pending.
// ----------------------------------------------------------------------------
extern const char* const KAPC_COMPONENT_SUFFIX[3];   // {@0x8200653C, @0x82006538, @0x82006534}

// ----------------------------------------------------------------------------
// The three per-component text readers @0x82219B90 / 0x82219BEC / 0x82219C48 are separate
// template-instance TUs (BrnDirector::Camera::TextFileReadSerialiser::Serialise<0/1/2>,
// over rw::math::vpu::VecFloatRef<N>). They are declared here and called by name; their bodies
// land with their own TUs. The asm passes (FILE** file, char* label, void* vecArgSlot) where the
// vecArgSlot's first word holds the destination f32* (vecArg[0] = pVec3).
// ----------------------------------------------------------------------------
extern FILE* TextFileReadSerialiser_SerialiseComponent0(FILE** lppFile, char* lpcLabel, f32** lppVecArg);
extern FILE* TextFileReadSerialiser_SerialiseComponent1(FILE** lppFile, char* lpcLabel, f32** lppVecArg);
extern FILE* TextFileReadSerialiser_SerialiseComponent2(FILE** lppFile, char* lpcLabel, f32** lppVecArg);

// ----------------------------------------------------------------------------
// BrnDirector::Camera::TextFileReadSerialiser::Serialise @0x82219AE0
// ----------------------------------------------------------------------------
int TextFileReadSerialiser::Serialise(FILE** lppFile, const char* lpcName, f32* lpfVec3)
{
    // The X360 builds the StrStream label sink over a fixed 64-byte stack buffer. (The asm first
    // installs the base-head vtable off_82000D00 and runs BasePriorityQueue::Clear(&state) to
    // reset the head, then installs the StrStream sink vtable off_82000D08 and wires the buffer;
    // CgsDev::StrStream's ctor performs the equivalent head init + buffer clear.)
    char acLabel[0x40];                                    // [sp .. +0x40] the label buffer (size 0x40)
    CgsDev::StrStream lState(acLabel, sizeof(acLabel));    // mpcBuffer=&acLabel, miBufferSize=0x40, [0]=0

    f32* lpVecArg = 0;                                     // var_90: the per-call vec-arg slot (vecArg[0])
    int  liResult = 0;

    // Per-component reader for each suffix, paired by descending suffix address (see header note).
    typedef FILE* (*TpfnComponentReader)(FILE**, char*, f32**);
    static const TpfnComponentReader KAPFN_READERS[3] =
    {
        &TextFileReadSerialiser_SerialiseComponent0,   // suffix @0x8200653C
        &TextFileReadSerialiser_SerialiseComponent1,   // suffix @0x82006538
        &TextFileReadSerialiser_SerialiseComponent2,   // suffix @0x82006534
    };

    for (int liComponent = 0; liComponent < 3; ++liComponent)
    {
        lState.Reset();                                        // CgsDev::StrStream::Reset(&state)

        // sink(&state, name ? name : "<NULLSTRING>")  -- (*(vtable+4)) virtual operator<<(const char*)
        lState << (lpcName ? lpcName : "<NULLSTRING>");
        // sink(&state, suffix)
        lState << KAPC_COMPONENT_SUFFIX[liComponent];

        lpVecArg = lpfVec3;                                    // vecArg[0] = pVec3 (stw r29, var_90)
        liResult = static_cast<int>(reinterpret_cast<usize>(
            KAPFN_READERS[liComponent](lppFile, lState.GetBuffer(), &lpVecArg)));
    }

    return liResult;
}

} // namespace Camera
} // namespace BrnDirector
