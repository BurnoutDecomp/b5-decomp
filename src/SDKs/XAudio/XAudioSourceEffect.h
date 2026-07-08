#pragma once

// ===========================================================================
// XAUDIO::CSourceEffect -- an XAudio streaming source effect in
// BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for:
//
//     XAUDIO::CSourceEffect::CSourceEffect                       @ 0x82964418
//     XAUDIO::CSourceEffect::~CSourceEffect                      @ 0x82964728
//     XAUDIO::CSourceEffect::Initialize                          @ 0x829641A8
//     XAUDIO::CSourceEffect::GetParam                            @ 0x829641D8
//     XAUDIO::CSourceEffect::SetParam                            @ 0x829642D0
//     XAUDIO::CSourceEffect::CompletePacket                      @ 0x82964D18
//     XAUDIO::CSourceEffect::`scalar deleting destructor'        @ 0x82964C90
//     XAUDIO::CSourceEffect::`vector deleting destructor'`adjustor{12}' @ 0x82964480
//
// The two deleting destructors are compiler-synthesised thunks of the virtual
// destructor + the (inherited) class-specific operator delete; they are not
// written out (the adjustor{12} variant is the CSourceStream-base entry, which
// steps `this` back by 0x0C before the scalar deleting destructor).
//
// There is no reference source and no DWARF for this TU, so the SHAPE is
// reconstructed from the X360 asm. `XAUDIO` is an external middleware boundary,
// so its identifiers are preserved verbatim per the naming convention.
//
// LAYOUT (grounded in the asm; X360 byte offsets in comments, widened to x64):
//   - Base CEffect            @ +0x00 : primary base (vptr / muReserved4 / mpContext).
//                                       The ctor seats muReserved4 = 1 and
//                                       mpContext = params->mpContext; the dtor
//                                       reseats CEffect's vtable (off_82108FF4).
//   - Base CSourceStream      @ +0x0C : secondary polymorphic base (streaming
//                                       source); ctor/dtor chain through it.
//   - mpCompleteCallback      @ +0xAC : completion callback (set by Initialize
//                                       from params, fired by CompletePacket).
//   - muParamB               @ +0xB0 : effect parameter (set by Initialize).
// ===========================================================================

#include "types.hpp"
#include "SDKs/XAudio/XAudioEffect.h"        // XAUDIO::CEffect (primary base)
#include "SDKs/XAudio/XAudioSourceStream.h"  // XAUDIO::CSourceStream (secondary base)

namespace XAUDIO
{

// The notification record CSourceEffect::CompletePacket builds on the stack and
// passes to the completion callback. The X360 asm reserves 12 words of scratch
// (_DWORD v8[12]) but writes only the first three; the rest are left
// uninitialised. FLAG: only the three attested words are modelled.
struct SCompleteNotification
{
    void* mpContext;    // [0] = CEffect::mpContext
    u32   muUserData;   // [1] = SCompletedPacket::muUserData (packet +0x24)
    int   miParam;      // [2] = CompletePacket's second argument
};

// The completion callback stored in the effect and invoked by CompletePacket.
typedef void (*TCompleteCallback)(SCompleteNotification* apNotification);

// The value descriptor Get/SetParam operate on (Hex-Rays `_DWORD* a4`). Its
// first word is the value -- read as a raw pointer (section 2 / type 3 paths) or
// as a 32-bit float (the type-1 paths) -- and the second word is a size
// discriminator (56 / 60) selecting between the two section-2/type-3 forms.
struct SAudioParamValue
{
    union
    {
        void* mpValue;  // +0x00 pointer form (section 2 / type 3)
        float mfValue;  // +0x00 float form   (type 1 setters)
    };
    u32 muSize;         // +0x04 size discriminator (0x38 / 0x3C)
};

// Create parameters passed to the constructor and Initialize by the concrete
// effect ctors (CXMASourceEffect / CPCMSourceEffect). Un-homed: only the fields
// this TU reads are modelled (with the X360 byte offsets they were read at);
// the embedded CSourceStream config block -- whose address Initialize forwards
// to CSourceStream::Initialize -- is opaque here. FLAG: reconstructed field set,
// not a complete descriptor.
struct SSourceEffectCreateParams
{
    u32               muReserved0;        // +0x00 not read by this TU (FLAG)
    void*             mpContext;          // +0x04 -> CEffect::mpContext (ctor)
    // +0x08..+0x43 : CSourceStream configuration block. Initialize passes its
    // address (params + 8) to CSourceStream::Initialize. Opaque to this TU.
    u8                mStreamConfig[0x44 - 0x08]; // +0x08
    u8                muFlagA;            // +0x44 -> CSourceStream::Initialize
    u8                muFlagB;            // +0x45 -> CSourceStream::Initialize
    u8                mPad46[2];          // +0x46 alignment
    TCompleteCallback mpCompleteCallback; // +0x48 -> effect completion callback
    u32               muParamB;           // +0x4C -> effect parameter
};

class CSourceEffect : public CEffect, public CSourceStream
{
public:
    // @ 0x82964418 -- seat muReserved4 = 1 and mpContext, then chain to the
    // CSourceStream base ctor.
    explicit CSourceEffect(const SSourceEffectCreateParams* apParams);

    // @ 0x82964728 -- teardown (bases handle the vtable reseats / base dtors).
    virtual ~CSourceEffect();

    // @ 0x829641A8 -- store the effect params (callback + muParamB) and forward
    // the stream config + flags to CSourceStream::Initialize.
    s32 Initialize(const SSourceEffectCreateParams* apParams, int aParam);

    // @ 0x829641D8 -- dispatch a get to the matching CSourceStream param
    // accessor by (section, type[, size]); returns E_INVALIDARG (0x80070057)
    // for an unrecognised selector.
    s32 GetParam(u8 aSection, u8 aType, SAudioParamValue* apValue);

    // @ 0x829642D0 -- the set counterpart of GetParam.
    s32 SetParam(u8 aSection, u8 aType, SAudioParamValue* apValue);

    // @ 0x82964D18 -- override: complete a packet through the CSourceStream base,
    // then fire the effect's completion callback if one is registered.
    virtual SCompletedPacket* CompletePacket(int aParam1, int aParam2);

private:
    TCompleteCallback mpCompleteCallback; // +0xAC  completion callback
    u32               muParamB;           // +0xB0  effect parameter
};

} // namespace XAUDIO
