#pragma once

// ===========================================================================
// XAUDIO::CSourceStream -- the XAudio streaming-source base that
// XAUDIO::CSourceEffect derives from (its secondary base at +0x0C) in
// BURNOUT_X360_ARTIST.XEX.
//
// This TU (class:XAUDIO::CSourceEffect) does NOT own CSourceStream's bodies --
// its ctor / dtor / Initialize / CompletePacket and its param-accessor virtuals
// live in their own (un-homed) TU. This header is the MINIMAL owning home that
// gives CSourceEffect the base type it needs: the members / virtuals CSourceEffect
// demonstrably touches, reconstructed from the CSourceEffect X360 asm. When the
// CSourceStream TU itself is worked it extends this header (adds the remaining
// members / virtuals and the bodies).
//
// `XAUDIO` is an external middleware boundary, so its identifiers are preserved
// verbatim per the naming convention.
//
// SHAPE (grounded in the CSourceEffect asm @ 0x829641A8 / 0x829641D8 /
// 0x829642D0 / 0x82964418 / 0x82964D18):
//   - Polymorphic: CSourceEffect installs its own secondary vtable (off_82109120)
//     into the CSourceStream subobject and overrides its virtual destructor (the
//     `vector deleting destructor adjustor{12}` thunk adjusts `this` by -0x0C to
//     reach the CSourceEffect object) and CompletePacket.
//   - CSourceEffect::Get/SetParam dispatch through this class's vtable to eight
//     consecutive param-accessor slots at byte offsets 0x34..0x50 (Get/Set
//     pairs). The intervening lower slots (0x00 dtor .. 0x30) are other
//     CSourceStream virtuals NOT reconstructed here, so the C++ vtable slot
//     numbers differ from the X360 image -- this is semantic parity, dispatched
//     by name. FLAG: accessor NAMES are inferred from the dispatch selectors.
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

// The packet record CSourceStream::CompletePacket returns on success. Only the
// word at +0x24 (forwarded to the CSourceEffect completion callback -- see
// CSourceEffect::CompletePacket) is attested by this TU; the preceding bytes are
// opaque here. FLAG: minimal reconstruction of an un-homed runtime record.
struct SCompletedPacket
{
    u8  mOpaqueHeader[0x24]; // +0x00 opaque packet header/payload
    u32 muUserData;          // +0x24 -> completion-callback notification word
};

class CSourceStream
{
public:
    CSourceStream();

    // Virtual (secondary-vtable slot 0x00). CSourceEffect overrides it -- its
    // `vector deleting destructor adjustor{12}' thunk lives in this vtable.
    virtual ~CSourceStream();

    // Virtual streaming-completion hook. CSourceEffect overrides it and chains to
    // this base implementation (a qualified base call in the X360 asm). Returns
    // the completed packet, or null when there is nothing to complete.
    virtual SCompletedPacket* CompletePacket(int aParam1, int aParam2);

    // The eight param accessors CSourceEffect::Get/SetParam dispatch to
    // (X360 secondary-vtable slots 0x34..0x50, Get/Set pairs). FLAG: names and
    // parameter types are inferred from the CSourceEffect dispatch asm.
    virtual s32 GetParamA(void* apArg0, void* apArg1); // 0x34
    virtual s32 SetParamA(void* apArg0, u32 aArg1);    // 0x38
    virtual s32 GetParamB(void* apValue);              // 0x3C
    virtual s32 SetParamB(float afValue);              // 0x40
    virtual s32 GetParamC(void* apValue);              // 0x44
    virtual s32 SetParamC(float afValue);              // 0x48
    virtual s32 GetParamD(void* apValue);              // 0x4C
    virtual s32 SetParamD(float afValue);              // 0x50

    // Non-virtual base service invoked directly (bl) by CSourceEffect::Initialize
    // with the CSourceStream subobject pointer, the config sub-block address, two
    // byte flags and the caller's context argument. Returns the platform status.
    s32 Initialize(void* apConfig, u8 aFlagA, u8 aFlagB, int aParam);
};

} // namespace XAUDIO
