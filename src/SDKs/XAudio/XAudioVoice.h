#pragma once

// ===========================================================================
// XAUDIO::CVoice -- the base XAudio voice object in BURNOUT_X360_ARTIST.XEX.
// It is the shared base of the concrete voices (CSourceVoice, CSubmixVoice,
// CMasteringVoice, CRoutedVoice). This header is the canonical OWNING home for
// the XAUDIO::CVoice TU.
//
// There is no reference source and no DWARF for this TU, so the SHAPE below is
// reconstructed purely from the X360 asm of the CVoice methods. `XAUDIO` is an
// external middleware boundary, so its identifiers are preserved verbatim per
// the naming convention.
//
// LAYOUT (grounded in the asm load/store displacements; X360 byte offsets in
// comments, pointer members widened to x64):
//   +0x00  vptr          -- polymorphic; ctor seats the base vtable
//                            (off_821B5514) then the CVoice vtable (off_821B5770),
//                            dtor reseats them in reverse down to off_82108FF4.
//   +0x04  miRefCount    -- intrusive refcount seeded to 1 by the ctor.
//   +0x08  mpAllocator   -- ref-counted allocator/collaborator passed to the
//                            ctor (AddRef'd via its vtable[0] on construct,
//                            Release'd via vtable[1] on destruct, and used to
//                            allocate the effect array in Initialize).
//   +0x0C  muVoiceType   -- byte voice type (ctor arg a3; GetVoiceType).
//   +0x10  mpListANext   -- intrusive circular list node A (self-linked by ctor).
//   +0x14  mpListAPrev
//   +0x18  mpListBNext   -- intrusive circular list node B (self-linked by ctor;
//                            (un)linked by OnStartVoice / OnStopVoice).
//   +0x1C  mpListBPrev
//   +0x20  mpFrameBuffer -- frame buffer (Initialize; released in the dtor).
//   +0x24  mpEffects     -- effect record array (SVoiceEffect[muNumEffects]).
//   +0x28  mpReserved28  -- word zeroed by the ctor; role not exercised by any
//                            CVoice method (FLAG: reserved).
//   +0x2C  mpProcessCallback -- per-process callback (Initialize; fired in Process).
//   +0x30  mpVoiceContext    -- user context pointer (Initialize; GetVoiceContext).
//   +0x34  mFormat       -- voice format pair (Get/SetVoiceFormat; Initialize).
//   +0x3C  muNumEffects  -- byte effect count (bounds every effect accessor).
//   +0x3D  muState       -- byte state/flag bits (GetVoiceState; Start/Stop/Process).
//   +0x3E  muMaxFrameSize    -- u16 running max effect frame size (CreateEffect).
//   +0x40  muFrameCountdown  -- u16 frame countdown (Stop seeds it, Process
//                                decrements it).
// ===========================================================================

#include "types.hpp"

namespace XAUDIO
{

// One entry of the CVoice effect array (mpEffects). The X360 build packs these
// at an 8-byte stride (the array is allocated as `8 * muNumEffects` bytes);
// members are accessed by name here, widened to the natural x64 layout.
struct SVoiceEffect
{
    void* mpEffect;   // +0x00  effect object (foreign vtable; opaque here)
    u8    muFlagA;    // +0x04  create flag (CreateEffect arg)
    u8    muState;    // +0x05  enabled/state byte (Get/SetEffectState)
    u8    muFlagB;    // +0x06  routing/mode flags
    u8    mPad07;     // +0x07  alignment
};

// The two-word voice format record carried at +0x34. Its exact field semantics
// are not exercised by this TU; the pair is copied verbatim by Get/SetVoiceFormat
// and seeded by Initialize.
struct SVoiceFormat
{
    u32 muField0; // +0x00
    u32 muField1; // +0x04
};

class CVoice
{
public:
    // Out-of-line virtual destructor anchors the vtable (reconstructed in a later
    // wave); declared here to keep the vptr at +0x00 as the asm layout requires.
    virtual ~CVoice();

    // @ 0x82C2FC78 -- write the byte voice type through the out-param. Returns 0.
    s32 GetVoiceType(u8* apType);

    // @ 0x82C2FC68 -- write the stored voice-context pointer (+0x30). Returns 0.
    s32 GetVoiceContext(void** apContext);

    // @ 0x82C2FC88 -- copy the two-word voice format (+0x34) out. Returns 0.
    s32 GetVoiceFormat(SVoiceFormat* apFormat);

    // @ 0x82C32178 -- copy the two-word voice format (+0x34) in. (The X360 asm
    // leaves r3 holding `this`; the source-level shape is a void setter.)
    void SetVoiceFormat(const SVoiceFormat* apFormat);

    // @ 0x82C30828 -- write the byte state/flags (+0x3D) out. Returns 0.
    s32 GetVoiceState(u8* apState);

    // @ 0x82C32468 -- read an effect's state byte by index. Returns E_INVALIDARG
    // (0x80070057) when the index is out of range.
    s32 GetEffectState(u8 aIndex, u8* apState);

    // @ 0x82C324A0 -- write an effect's state byte by index. Returns E_INVALIDARG
    // (0x80070057) when the index is out of range.
    s32 SetEffectState(u8 aIndex, u8 aState);

    // ----- layout (declared in X360 offset order; see the header banner) -----
    int          miRefCount;        // +0x04
    void*        mpAllocator;       // +0x08
    u8           muVoiceType;       // +0x0C
    void*        mpListANext;       // +0x10
    void*        mpListAPrev;       // +0x14
    void*        mpListBNext;       // +0x18
    void*        mpListBPrev;       // +0x1C
    void*        mpFrameBuffer;     // +0x20
    SVoiceEffect* mpEffects;        // +0x24
    void*        mpReserved28;      // +0x28  (FLAG: zeroed by ctor, role unknown)
    void*        mpProcessCallback; // +0x2C
    void*        mpVoiceContext;    // +0x30
    SVoiceFormat mFormat;           // +0x34
    u8           muNumEffects;      // +0x3C
    u8           muState;           // +0x3D
    u16          muMaxFrameSize;    // +0x3E
    u16          muFrameCountdown;  // +0x40
};

} // namespace XAUDIO
