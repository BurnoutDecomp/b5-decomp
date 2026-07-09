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

// Each delegating Serialise<T> instantiation below needs T complete (its `Serialise` member template
// is odr-used) -- pull in every behaviour Parameters block this TU instantiates over.
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourPassengerCam.h"
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"     // BehaviourRig::Parameters + Utils::{CameraShake,Looker}::Parameters
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"            // BrnDirector::{IceMovie,ICEMoviePlaylist}

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

// ----------------------------------------------------------------------------
// TextFileReadSerialiser::Serialise(const char*, f32&/s32&/u32&) -- the scalar field readers.
//
// The READ counterparts of the write serialiser's scalar writers. Store-for-store from the inlined
// form the X360 emits at every scalar-field read site: if mpFile is open, fscanf one
// "<label> : <value>\n" line into lrValue, discarding the label token ("%s"). The value is read
// into the current value first so a missing "%f"/"%d" leaves it unchanged (fscanf default). Always
// inlined at the call sites (no standalone address); the name arg is unused on read.
// ----------------------------------------------------------------------------
void TextFileReadSerialiser::Serialise(const char* /*lpcName*/, f32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacLabel[64];                          // the discarded "<name> :" token
        std::fscanf(mpFile, "%s : %f\n", lacLabel, &lrValue);
    }
}

void TextFileReadSerialiser::Serialise(const char* /*lpcName*/, s32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacLabel[64];
        std::fscanf(mpFile, "%s : %d\n", lacLabel, &lrValue);
    }
}

void TextFileReadSerialiser::Serialise(const char* /*lpcName*/, u32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacLabel[64];
        std::fscanf(mpFile, "%s : %d\n", lacLabel, &lrValue);
    }
}

// ----------------------------------------------------------------------------
// TextFileReadSerialiser::Serialise<T> -- the ONE shared nested-block reader body.
//
// Store-for-store from the delegating asm shared by every instance (e.g. @0x8224D160):
//   file = *this;                                   ; lwz r3,0(r3) -- the wrapped FILE* (mpFile)
//   if (file) fscanf(file, "%s\n", &lacLabel);      ; aS_0 == "%s\n" -- consume the header line
//   return T::Serialise<TextFileReadSerialiser>(&params, this);   ; bl the per-T field reader (a3,a1)
//
// For a couple of leaf blocks (AttachmentTruck @0x82215C78, FixedCam @0x82214CD0) the X360 compiler
// inlined T's own reader into this body (the trailing "%s\n" header read + two/three "%s : %f\n"
// float reads ARE that inlined T::Serialise); per the project's inlining-reversal rule the source
// is this de-inlined delegating form. The name arg is unused (the read side only needs to advance
// past the header line the writer emitted). The DWARF visitor returns void; the X360 keeps the
// callee's FILE* live in r3 as a leftover-register artifact.
// ----------------------------------------------------------------------------
template<class T>
void TextFileReadSerialiser::Serialise(const char* /*lpcName*/, T& lrParams)
{
    if (mpFile != nullptr)
    {
        char lacLabel[64];                          // the section-header token, read and discarded
        std::fscanf(mpFile, "%s\n", lacLabel);
    }

    lrParams.Serialise(*this);                       // T::Serialise<TextFileReadSerialiser>(this)
}

// ----------------------------------------------------------------------------
// TextFileReadSerialiser::Serialise<INDEX> -- the per-component vec-float reader (X360
// @0x82213BA0 / 0x82213C30 / 0x82213CC0 for INDEX 0/1/2 == X/Y/Z).
//
// Store-for-store from the asm (r31 == the f32** vec-arg slot the dispatcher filled):
//   file = *this;                                   ; lwz r3,0(r3) -- mpFile
//   if (file) {
//       p = *lppVec;                                ; lwz r10,0(r31) -- the destination vector ptr
//       lfValue = p[INDEX];                         ; lfs f0,(INDEX*4)(r10) ; stfs -> v8 (default)
//       fscanf(file, "%s : %f\n", &lacLabel, &lfValue);   ; aSF == "%s : %f\n"
//       p[INDEX] = lfValue;                         ; VMX load / vrlimi128 lane-insert / store
//   }
//   return file;
//
// The VMX cascade (lvx128/vspltw/vrlimi128/stvx128) is exactly a single-lane write preserving the
// other three lanes; writing p[INDEX] alone has the same net effect. The incoming label arg is
// unused (the reader's own "%s" token discards whatever header the dispatcher wrote). The X360
// reads the current lane first so a missing "%f" leaves the value unchanged (fscanf default).
// ----------------------------------------------------------------------------
template<int INDEX>
FILE* TextFileReadSerialiser::Serialise(const char* /*lpcLabel*/, f32** lppVec)
{
    FILE* lpFile = mpFile;
    if (lpFile != nullptr)
    {
        f32*  lpVec = *lppVec;                       // the destination vector (lwz r10,0(r31))
        char  lacLabel[64];                          // the discarded "<name><suffix>" token
        float lfValue = lpVec[INDEX];                // current lane value = fscanf default
        std::fscanf(lpFile, "%s : %f\n", lacLabel, &lfValue);
        lpVec[INDEX] = lfValue;                      // write parsed value into lane INDEX
    }
    return lpFile;
}

// ----------------------------------------------------------------------------
// Explicit instantiations. The delegating reader body is identical for every T; the three
// component readers differ only by the axis lane written.
//
// BLOCKED (not instantiated -- nested Parameters type has no reconstructed home yet):
// AttachmentTruck::Parameters (its read leaf @0x82215C78 inlines that block's two-float reader; the
// nested block lands with the AttachmentTruck / gyro-cam rig TU).
// ----------------------------------------------------------------------------
template void TextFileReadSerialiser::Serialise<BehaviourAftertouchCam::Parameters>(const char*, BehaviourAftertouchCam::Parameters&);
template void TextFileReadSerialiser::Serialise<BehaviourAftertouchCrash::Parameters>(const char*, BehaviourAftertouchCrash::Parameters&);
template void TextFileReadSerialiser::Serialise<BehaviourBystanderCam::Parameters>(const char*, BehaviourBystanderCam::Parameters&);
template void TextFileReadSerialiser::Serialise<BehaviourFailsafe::Parameters>(const char*, BehaviourFailsafe::Parameters&);
template void TextFileReadSerialiser::Serialise<BehaviourFixedCam::Parameters>(const char*, BehaviourFixedCam::Parameters&);
template void TextFileReadSerialiser::Serialise<BehaviourGameplayBumper::Parameters>(const char*, BehaviourGameplayBumper::Parameters&);
template void TextFileReadSerialiser::Serialise<BehaviourGameplayExternal::Parameters>(const char*, BehaviourGameplayExternal::Parameters&);
template void TextFileReadSerialiser::Serialise<BehaviourGyroCam::Parameters>(const char*, BehaviourGyroCam::Parameters&);
template void TextFileReadSerialiser::Serialise<BehaviourHeliCam::Parameters>(const char*, BehaviourHeliCam::Parameters&);
template void TextFileReadSerialiser::Serialise<BehaviourLooseAttachment::Parameters>(const char*, BehaviourLooseAttachment::Parameters&);
template void TextFileReadSerialiser::Serialise<BehaviourPassengerCam::Parameters>(const char*, BehaviourPassengerCam::Parameters&);
template void TextFileReadSerialiser::Serialise<BehaviourRig::Parameters>(const char*, BehaviourRig::Parameters&);
template void TextFileReadSerialiser::Serialise<Utils::CameraShake::Parameters>(const char*, Utils::CameraShake::Parameters&);
template void TextFileReadSerialiser::Serialise<Utils::Looker::Parameters>(const char*, Utils::Looker::Parameters&);
template void TextFileReadSerialiser::Serialise< ::BrnDirector::ICEMoviePlaylist>(const char*, ::BrnDirector::ICEMoviePlaylist&);
template void TextFileReadSerialiser::Serialise< ::BrnDirector::IceMovie>(const char*, ::BrnDirector::IceMovie&);

template FILE* TextFileReadSerialiser::Serialise<0>(const char*, f32**);   // X lane
template FILE* TextFileReadSerialiser::Serialise<1>(const char*, f32**);   // Y lane
template FILE* TextFileReadSerialiser::Serialise<2>(const char*, f32**);   // Z lane

} // namespace Camera
} // namespace BrnDirector
