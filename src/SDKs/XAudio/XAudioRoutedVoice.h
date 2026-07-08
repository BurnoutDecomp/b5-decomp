#pragma once

// ===========================================================================
// XAUDIO::CRoutedVoice -- an XAudio voice that fans its processed audio out to
// a set of destination ("output") voices through a per-output effect chain, in
// BURNOUT_X360_ARTIST.XEX. It derives from XAUDIO::CVoice and is itself the base
// of the concrete CSourceVoice / CSubmixVoice. This header is the canonical
// OWNING home for the XAUDIO::CRoutedVoice TU.
//
// There is no reference source and no DWARF for this TU, so the SHAPE below is
// reconstructed purely from the X360 asm of the CRoutedVoice methods. `XAUDIO`
// is an external middleware boundary, so its identifiers are preserved verbatim
// per the naming convention.
//
// LAYOUT (grounded in the asm load/store displacements; X360 byte offsets in
// comments, pointer members widened to x64). The CVoice base occupies +0x00..
// +0x43 (see XAudioVoice.h); CRoutedVoice appends:
//   +0x44  muNumRoutes        -- byte route count. Seeded by Initialize from the
//                                creation descriptor (`stb config[0x18]`); it is
//                                the number of 12-byte route slots and the number
//                                of per-output effects. Bounds the ~CRoutedVoice
//                                effect-teardown loop and the null-config arm of
//                                SetVoiceOutput.
//   +0x45  muNumActiveOutputs -- byte count of currently-attached output voices.
//                                Written by SetVoiceOutput; bounds the
//                                DetachOutputVoices / OnStopVoice / ProcessEffects
//                                loops.
//   +0x46..+0x47              -- alignment padding (X360).
//   +0x48  mpRoutes           -- pointer to the route array (SRoutedVoiceOutput
//                                [muNumRoutes]; 12-byte stride in the X360 build).
//                                Allocated by Initialize through the CVoice
//                                allocator; released per-slot by the destructor.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XAudio/XAudioVoice.h"   // XAUDIO::CVoice (base)
#include "SDKs/XAudio/XAudioEffect.h"  // XAUDIO::CEffect (per-route effect)

namespace XAUDIO
{

// One route slot of the CRoutedVoice output array (mpRoutes). The X360 build
// strides these at 12 bytes (`v7 += 12`, and Initialize allocates
// `12 * muNumRoutes`); members are accessed by name here, pointers widened to the
// natural x64 layout.
struct SRoutedVoiceOutput
{
    CVoice*  mpOutputVoice; // +0x00  destination voice (AddRef'd on attach)
    CEffect* mpEffect;      // +0x04  per-route effect (created by CVoice::CreateEffect)
    void*    mpReserved8;   // +0x08  (FLAG: not exercised by this TU)
};

// One entry of the output-configuration array passed to SetVoiceOutput. The
// X360 build strides these at 8 bytes (`v6 += 8`); AttachOutputVoice reads the
// destination voice at +0x00 and the channel map at +0x04.
struct SVoiceOutputEntry
{
    CVoice* mpOutputVoice; // +0x00  destination voice (0 => engine default)
    u32     muChannelMap;  // +0x04  channel map (0 => derive the default)
};

// The output-configuration descriptor handed to SetVoiceOutput. Byte count at
// +0x00 and the entry-array pointer at +0x04 (`lbz r11,0(a2)` / `lwz r11,4(a2)`).
struct SVoiceOutputConfig
{
    u8                 muCount;   // +0x00
    // +0x01..+0x03 alignment padding (X360)
    SVoiceOutputEntry* mpEntries; // +0x04
};

class CRoutedVoice : public CVoice
{
public:
    // @ 0x82C31B88 -- destructor: reseats the vtable, stops + detaches, then
    // releases each per-route effect before running the CVoice base destructor.
    // BLOCKED body (see XAudioRoutedVoice.cpp): the per-effect teardown dispatches
    // through the CEffect vtable at an offset the CEffect TU has not reconstructed,
    // and it calls the still-todo CVoice::Stop / ~CVoice. Declared here (virtual,
    // overriding ~CVoice) to keep the class shape and vptr correct.
    virtual ~CRoutedVoice();

    // @ 0x82C31C28 -- (re)bind this voice's output set. Detaches the current
    // outputs, then either attaches each output in `apConfig` (recording the
    // count) or, when `apConfig` is null, attaches the single default output.
    s32 SetVoiceOutput(const SVoiceOutputConfig* apConfig);

    // @ 0x82C319D0 -- attach one destination voice into a route slot (resolve the
    // output + channel map, AddRef the output, push the channel map). Declared
    // for SetVoiceOutput; BLOCKED body (engine/default-config globals + CVoice/
    // destination-voice vtable slots not yet reconstructed).
    s32 AttachOutputVoice(const SVoiceOutputEntry* apEntry, SRoutedVoiceOutput* apRoute);

    // @ 0x82C31A90 -- release every attached destination voice and clear the
    // active-output count. Declared for SetVoiceOutput / the destructor; BLOCKED
    // body (destination-voice Release dispatches through an unreconstructed
    // CVoice vtable slot).
    s32 DetachOutputVoices();

    // Class-specific deallocation routing through the XAudio XMem manager -- the
    // free arm of the scalar deleting destructor @ 0x82C31EC8.
    void operator delete(void* pMemory);

    // ----- layout (declared in X360 offset order; see the header banner) -----
    u8                  muNumRoutes;        // +0x44
    u8                  muNumActiveOutputs; // +0x45
    // +0x46..+0x47 alignment padding (X360)
    SRoutedVoiceOutput* mpRoutes;           // +0x48
};

} // namespace XAUDIO
