#pragma once

// ===========================================================================
// XAUDIO::CPacketQueue -- the XAudio source-stream packet pool/free-list used
// by XAUDIO::CSourceStream in BURNOUT_X360_ARTIST.XEX. This header is the
// canonical OWNING home for:
//
//     XAUDIO::CPacketQueue::CPacketQueue  @ 0x8296C4A8  (constructor)
//     XAUDIO::CPacketQueue::Initialize    @ 0x8296C3E8
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed from the X360 asm. `XAUDIO` is an external middleware
// boundary, so its identifiers are preserved verbatim per the naming
// convention.
//
// LAYOUT (grounded in the constructor's stores @ 0x8296C4A8):
//   stw r3,0(r3) / stw r3,4(r3) / stw 0,8(r3)          -- list A head @ +0x00
//   addi r11,r3,0xC ; stw r11,0(r11)/4(r11) ; stw 0,8(r11) -- list B head @ +0x0C
// Each "head" is exactly an XAUDIOPACKETCTX_ (next @+0, prev @+4, base @+8): the
// constructor makes both heads self-circular (empty list) with base = 0. The
// queue object is itself the sentinel node of both lists (so `this == nbr`
// detects the end of iteration, matching XAUDIOPACKETCTX_::GetNextEntry). The
// pool pointer Initialize fills lives at +0x18.
//
//   +0x00  mInUseList  (XAUDIOPACKETCTX_)  list A: queued/in-use packets
//   +0x0C  mFreeList   (XAUDIOPACKETCTX_)  list B: free packets (filled by Initialize)
//   +0x18  mpPool      (u8*)               packet pool base (alloc'd by Initialize)
//
// Each pooled packet is a fixed 120-byte (0x78) record; its leading two words are
// the intrusive list link (next @+0, prev @+4) that the free-list threads.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XAudio/XAudioPacketCtx.h" // XAUDIO::XAUDIOPACKETCTX_, XAudioListLink

namespace XAUDIO
{

// The XAudio memory-manager interface CPacketQueue::Initialize allocates the
// packet pool through. The X360 call is `(*(*a3 + 0x14))(a3, size)`: a virtual
// dispatch through the manager's vtable slot at byte +0x14 (the 6th pointer
// slot) that allocates `size` bytes and returns the pool base. Only that slot is
// attested by this TU, so the four preceding slots are modelled as opaque
// pointer slots (their signatures are not recovered here -- FLAG) and the
// allocate slot is accessed BY NAME.
struct IXAudioMemoryManager
{
    struct VTable
    {
        void* mpSlot00;  // +0x00
        void* mpSlot04;  // +0x04
        void* mpSlot08;  // +0x08
        void* mpSlot0C;  // +0x0C
        void* mpSlot10;  // +0x10
        // +0x14 -- allocate `uSize` bytes from the manager; returns the block base.
        void* (*mpAllocate)(IXAudioMemoryManager* apSelf, u32 uSize);
    };

    const VTable* mpVTable; // +0x00
};

// Fixed-size pooled packet record. Only the leading intrusive list link (the two
// words the free-list threads) is attested by this TU; the remaining bytes are
// the (un-homed) packet payload, sized to the 0x78 stride Initialize strides by.
struct XAudioPacket
{
    XAudioListLink mLink;        // +0x00 .. +0x07  next/prev free-list link
    u8             mPayload[0x78 - sizeof(XAudioListLink)]; // +0x08 .. +0x77 opaque
};

class CPacketQueue
{
public:
    // @ 0x8296C4A8 -- construct an empty packet queue: both intrusive lists made
    // self-circular (empty), bases cleared.
    CPacketQueue();

    // @ 0x8296C3E8 -- allocate a pool of `uCount` packets (0x78 bytes each) from
    // `apManager` and thread them onto the free list. Returns the pool base (or
    // `this` unchanged when uCount == 0, matching the asm's zero-count early-out).
    void* Initialize(u8 uCount, IXAudioMemoryManager* apManager);

    XAUDIOPACKETCTX_ mInUseList; // +0x00  list A -- queued/in-use packets
    XAUDIOPACKETCTX_ mFreeList;  // +0x0C  list B -- free packets
    u8*              mpPool;     // +0x18  packet pool base
};

} // namespace XAUDIO
