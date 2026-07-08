#pragma once

// ===========================================================================
// D3D::CDevice -- the X360 GPU command-processor "device" inside
// BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for the
// CDevice TYPE and its 18 methods (homed in CDevice.cpp).
//
// CDevice is the software side of the X360 graphics ring buffer: it owns the
// primary/secondary command-buffer cursors, the ring allocator, the GPU
// fence/segment bookkeeping and the read-back block the GPU writes its
// progress into. The methods fall into two groups:
//
//   * Pure software flag / pointer-compare / busy-poll helpers -- recovered
//     FAITHFULLY from the asm and bodied here:
//         IsPastStopPoint, IsPrimaryMemoryBusy, IsSecondaryMemoryBusy,
//         MarkAsOutOfMemory, SaveRingBufferState
//
//   * Multi-stage GPU command-stream / ring-allocator / fence pipeline that
//     either builds raw PM4 packets, pokes GPU registers, or needs the layout
//     of a sibling D3D type that is NOT yet homed (CBlocker, CCommandBuffer,
//     CRingAllocList). These are DECLARATION-ONLY here and FLAGGED in the .cpp
//     -- bodying them would require fabricating an un-homed sibling's layout or
//     paraphrasing a hardware packet pipeline, which the reconstruction rules
//     forbid:
//         AddCallsToPrimaryBuffer, AddCommandsToPrimaryBuffer, BeginRingAlloc,
//         BeginRingBig, BlockOnFence, BlockOnPrimaryRange,
//         BlockOnSecondaryPosition, CreateInvalidateBuffer, KickOff,
//         KickOffSegment, QueueIndirectBuffer, SetFence, StartNewSegment
//
// `D3D` is the X360 graphics-SDK boundary, so its identifiers (CDevice, the
// method names, the sibling C* types) are preserved verbatim per the naming
// convention.
//
// LAYOUT NOTE -- there is NO DWARF and NO reference source for this TU. The
// member shape below is reconstructed purely from the byte offsets the bodied
// methods load/store in the X360 asm. Only the fields those methods touch are
// named; the rest of the (very large) device object is opaque padding. The
// X360 object uses 4-byte pointers, so absolute byte offsets PAST the first
// real host pointer (mpGpuReadback) shift on an LLP64 host and are therefore
// NOT asserted; the scalar offsets BEFORE that pointer are pointer-invariant
// and ARE pinned with offsetof() in _AssertLayout (see CDevice.cpp).
// ===========================================================================

#include "types.hpp"

namespace D3D
{

// Forward-declared sibling D3D types. CBlocker is now homed in D3DBlocker.h
// (its ctor/dtor/Check poke the device fields added below via `friend class
// CBlocker`); CCommandBuffer / CRingAllocList are still un-homed and used only
// through opaque pointers so the declaration-only methods can name them in
// their signatures without fabricating their internals.
class CBlocker;
class CCommandBuffer;
class CRingAllocList;

// The X360 GPU read-back block: the command processor writes its primary /
// secondary fetch progress here so the CPU can poll how far the GPU has
// consumed the ring. Only the two progress words the busy-polls read are
// named; +0x00 (mu...Primary) is the secondary cursor, +0x3C is the primary
// read pointer. Offsets are from the asm of IsPrimary/IsSecondaryMemoryBusy.
struct CGpuReadback
{
    u32 muSecondaryRead;  // +0x00  secondary fetch cursor (read by IsSecondaryMemoryBusy)
    u8  mPad04[0x38];     // +0x04
    u32 muPrimaryRead;    // +0x3C  primary fetch cursor   (read by IsPrimaryMemoryBusy)
};

class CDevice
{
public:
    // -------- FAITHFULLY BODIED (pure software, recovered from asm) --------

    // @ 0x82945380 -- has the GPU passed the stop point recorded at
    // (+0x33B0 segment / +0x33B4 marker)? Pure compare; no side effects.
    bool IsPastStopPoint(u32 uPosition, s32 iMarker);

    // @ 0x829454E0 -- is the [a2,a3) primary-ring range still being consumed by
    // the GPU? Compares the GPU read pointer against the wrapped range.
    s32 IsPrimaryMemoryBusy(u32 uStart, u32 uEnd);

    // @ 0x829453E8 -- is the secondary ring at position a2 still busy for the
    // given 2-bit phase a3? Compares against the GPU secondary cursor.
    bool IsSecondaryMemoryBusy(u32 uPosition, s8 iPhase);

    // @ 0x82945AC0 -- reset the command cursors back to the out-of-memory
    // fallback buffer and raise the OOM flag (bit 0x20). Pure pointer/flag
    // store.
    void MarkAsOutOfMemory();

    // @ 0x829452B8 -- snapshot the live ring-buffer cursor set into the saved
    // slot (used by StartWorkerQueue). Pure word copy.
    void SaveRingBufferState();

    // -------- DECLARATION-ONLY (FLAGGED in CDevice.cpp) --------
    // Each of these needs an un-homed sibling type's layout and/or builds a
    // raw GPU packet stream / pokes GPU registers. Bodying them faithfully is
    // blocked until CBlocker / CCommandBuffer / CRingAllocList are homed.

    s32     AddCallsToPrimaryBuffer(s32* pCalls, s32 iCount, s32 iTag);
    s32     AddCommandsToPrimaryBuffer(u32* pCommands, s32 iCount);
    s32     BeginRingAlloc(s32 iDwords, s32 iAlign);
    s32     BeginRingBig(s32 iDwords);
    s32     BlockOnFence(s32 iFence, s32 iWhich, s32 iReason);
    s32     BlockOnPrimaryRange(s32 iStart, s32 iCount);
    s32     BlockOnSecondaryPosition(s32 iPosition, s32 iPhase);
    s32     CreateInvalidateBuffer(u32* pAddrOut, s32* pSizeOut);
    s32     KickOff();
    u32     KickOffSegment(s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7);
    u32*    QueueIndirectBuffer(u32* pPacket, s32 iAddr, s32 iDwords, s32 iFlags, CRingAllocList* pList);
    u32*    SetFence(u32* pPacket);
    s32     StartNewSegment(s32 iDwords);

private:
    // ---- ring-buffer cursors (word indices [12]/[13]/[14] in the asm) ----
    u8  mPad0000[0x30];     // +0x0000
    u32 muRingPut;          // +0x0030  current command write cursor   (word[12])
    u32 muRingEnd;          // +0x0034  current segment end             (word[13])
    u32 muRingLimit;        // +0x0038  segment fence limit             (word[14])

    // ---- up to the GPU read-back pointer (first real host pointer) ----
    u8  mPad003C[0x2A88 - 0x3C]; // +0x003C
    u32 muOwnerThreadId;         // +0x2A88  thread id that owns the primary ring
                                 //          (read by CBlocker::Check)
    u8  mPad2A8C[0x2A90 - (0x2A88 + 4)]; // +0x2A8C
    CGpuReadback* mpGpuReadback; // +0x2A90  GPU progress read-back block

    // ---- flag byte at +0x2ABD (bit 0x20 == out-of-memory / segment-stalled) -
    // NOTE: byte offsets below are X360-absolute; on the LLP64 host they shift
    // by the +4 pointer widening above. They are reproduced here for fidelity
    // documentation only and are pinned RELATIVELY (not asserted absolutely).
    u8  mPad2A98[0x2ABD - (0x2A90 + sizeof(void*))]; // +0x2A98
    u8  muFlags;            // +0x2ABD  device flag byte (0x20 == OOM/stalled)

    // ---- stop-point segment / marker (words [3308]/[3309]) ----
    u8  mPad2ABE[0x2AFC - (0x2ABD + 1)]; // +0x2ABE
    u32 muHangDetectArmed;  // +0x2AFC  non-zero while a submit is in flight on the
                            //          owning thread; gates CBlocker's fence reset
    u8  mPad2B00[0x33B0 - (0x2AFC + 4)]; // +0x2B00
    u32 muStopSegment;      // +0x33B0  active stop-point segment (0 == none)
    u32 muStopMarker;       // +0x33B4  stop-point marker value

    // ---- saved ring-buffer-state destination slots (words [3352]..[3357]) ----
    // SaveRingBufferState copies, in order:
    //   [3352] <- muRingPut (word[12])     [3353] <- muRingEnd (word[13])
    //   [3354] <- muLiveSave3 (word[3733]) [3355] <- muLiveSave2 (word[3732])
    //   [3356] <- muLiveSave0 (word[3730]) [3357] <- muLiveSave1 (word[3731])
    u8  mPad33B8[0x3460 - (0x33B4 + 4)]; // +0x33B8
    u32 muSaved0;           // +0x3460  saved word[12]   (word[3352])
    u32 muSaved1;           // +0x3464  saved word[13]   (word[3353])
    u32 muSaved2;           // +0x3468  saved word[3733] (word[3354])
    u32 muSaved3;           // +0x346C  saved word[3732] (word[3355])
    u32 muSaved4;           // +0x3470  saved word[3730] (word[3356])
    u32 muSaved5;           // +0x3474  saved word[3731] (word[3357])

    // ---- GPU-block profiling callback (word [3359]) ----
    // Optional hook invoked by D3D::CBlocker::~CBlocker when a block completes;
    // null when GPU-block profiling is off. Held opaque here -- the call
    // signature is defined at its only call site in D3DBlocker.cpp.
    u8   mPad3478[0x347C - (0x3474 + 4)]; // +0x3478
    void* mpProfileCallback;              // +0x347C  GPU-block profiling hook
    u8   mPad3480[0x3A48 - (0x347C + sizeof(void*))]; // +0x3480

    // ---- live ring-buffer-state source slots (words [3730]..[3733]) ----
    u32 muLiveSave0;        // +0x3A48  live word[3730]
    u32 muLiveSave1;        // +0x3A4C  live word[3731]
    u32 muLiveSave2;        // +0x3A50  live word[3732]
    u32 muLiveSave3;        // +0x3A54  live word[3733]

    // ---- out-of-memory fallback buffer base (word [4183]) ----
    u8  mPad3A58[0x415C - (0x3A54 + 4)]; // +0x3A58
    u32 muOutOfMemoryBase;  // +0x415C  fallback ring base used by MarkAsOutOfMemory

    // ---- GPU-block profiling accumulators (read/written by ~CBlocker) ----
    // mfSecondsPerTick scales an elapsed time-base delta to seconds; the two
    // 64-bit counters accumulate blocked time-base ticks, split by blocker type
    // (type 3 -> the second counter, every other type -> the first).
    u8  mPad4160[0x5458 - (0x415C + 4)]; // +0x4160
    f32 mfSecondsPerTick;   // +0x5458  seconds per CPU time-base tick
    u8  mPad545C[0x5460 - (0x5458 + 4)]; // +0x545C
    u64 mu64BlockTicks;     // +0x5460  accumulated blocked ticks (type != 3)
    u64 mu64BlockTicks3;    // +0x5468  accumulated blocked ticks (type == 3)

    // (remainder of the device object is opaque and not modelled)

    friend void _CDeviceAssertLayout();
    friend class CBlocker;
};

} // namespace D3D
