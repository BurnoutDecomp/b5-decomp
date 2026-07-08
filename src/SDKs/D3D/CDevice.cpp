#include "SDKs/D3D/CDevice.h"

#include <cstddef> // offsetof (uncalled _CDeviceAssertLayout)

// ===========================================================================
// D3D::CDevice -- reconstructed from BURNOUT_X360_ARTIST.XEX. See CDevice.h for
// the layout, the asm offset map, and the bodied-vs-declaration-only split.
//
// Source-of-truth: X360 asm only (no DWARF, no reference source). The five
// methods below are pure software flag / pointer-compare / busy-poll helpers
// whose asm is an unambiguous load/compare/store; they are reproduced faithfully
// with NO added control flow and NO inverted branches.
//
// The thirteen remaining methods are DECLARATION-ONLY and FLAGGED at the bottom
// of this file: each one needs the layout of a sibling D3D type that is not yet
// homed (CBlocker / CCommandBuffer / CRingAllocList), and/or builds a raw GPU
// PM4 command-packet stream or pokes GPU registers. The reconstruction rules
// forbid fabricating an un-homed sibling's layout or paraphrasing a hardware
// packet pipeline to scalar, so they are left unimplemented pending those types.
// ===========================================================================

namespace D3D
{

// --- @ 0x82945380 ----------------------------------------------------------
// Has the GPU advanced past the recorded stop point? A non-zero stop segment
// (+0x33B0) arms the test; the marker delta (+0x33B4 - iMarker) being -1, or
// zero while the stop segment is below uPosition, means "past".
bool CDevice::IsPastStopPoint(u32 uPosition, s32 iMarker)
{
    u32 uStopSeg = muStopSegment;       // lwz r10, 0x33B0(this)
    bool bResult = false;
    if (uStopSeg != 0)                  // cmplwi r10,0 ; beq
    {
        s32 iDelta = (s32)muStopMarker - iMarker; // subf r11, r30, r11
        if (iDelta == -1 ||             // cmpwi r11,-1 ; bne
            (iDelta == 0 && uStopSeg < uPosition)) // r11==0 && r10 < r29
        {
            bResult = true;
        }
    }
    return bResult;
}

// --- @ 0x829454E0 ----------------------------------------------------------
// Is the wrapped primary-ring range [uStart, uEnd) still being fetched by the
// GPU? Compares the GPU primary read pointer (+0x3C of the read-back block)
// against the range, handling the wrap case (uStart >= uEnd) separately.
s32 CDevice::IsPrimaryMemoryBusy(u32 uStart, u32 uEnd)
{
    u32 uRead = mpGpuReadback->muPrimaryRead; // lwz r11, 0x2A90(this) ; lwz r11, 0x3C(r11)
    if (uStart >= uEnd)                       // cmplw r31,r29 ; bge
    {
        if (uStart < uRead)                   // cmplw r31,r11 ; blt -> busy
            return 1;
    }
    else if (uStart >= uRead)                 // a2 < a3 path: cmplw r31,r11 ; bge -> not busy
    {
        return 0;
    }
    if (uRead > uEnd)                         // cmplw r11,r29 ; bgt -> not busy
        return 0;
    return 1;
}

// --- @ 0x829453E8 ----------------------------------------------------------
// Is the secondary ring busy for phase iPhase at uPosition? The 2-bit phase
// delta against the GPU secondary cursor (+0x04 of the read-back block) decides:
// delta 0 -> idle; delta 1 -> busy only if uPosition is past the aligned cursor;
// delta 2/3 -> busy.
bool CDevice::IsSecondaryMemoryBusy(u32 uPosition, s8 iPhase)
{
    s32 iCursor = (s32)mpGpuReadback->muSecondaryRead; // lwz r11, 0x2A90(this) ; lwz r11, 4(r11)
    u32 uDelta = ((u32)iPhase - (u32)iCursor) & 3;     // subf ; clrlwi. r10,r10,30
    return uDelta != 0 &&
           (uDelta != 1 || uPosition > ((u32)iCursor & 0xFFFFFFFCu)); // clrrwi r11,r11,2 ; cmplw
}

// --- @ 0x82945AC0 ----------------------------------------------------------
// Collapse all command cursors onto the dedicated out-of-memory fallback
// buffer and raise the OOM / segment-stalled flag (bit 0x20). The fallback
// buffer is 0x12C0 (4800) bytes long with a 0xA0 (160) byte tail margin.
void CDevice::MarkAsOutOfMemory()
{
    u32 uBase = muOutOfMemoryBase;     // lwz r11, 0x415C(this)
    muFlags = (u8)(muFlags | 0x20);    // lbz/ori/stb 0x2ABD
    muRingPut   = uBase;               // stw 0x30
    muRingEnd   = uBase + 0x12C0;      // stw 0x34 (base + 4800)
    muRingLimit = uBase + 0x1220;      // stw 0x38 (base + 4800 - 160)
}

// --- @ 0x829452B8 ----------------------------------------------------------
// Snapshot the live ring-buffer cursor set into the saved-state slots (read
// back by StartWorkerQueue). Pure word copy in the asm's load/store order.
void CDevice::SaveRingBufferState()
{
    u32 uRingEnd = muRingEnd;    // lwz r10, 0x34
    u32 uLive3   = muLiveSave3;  // lwz r9,  0x3A54
    u32 uLive2   = muLiveSave2;  // lwz r8,  0x3A50
    u32 uLive0   = muLiveSave0;  // lwz r7,  0x3A48
    u32 uLive1   = muLiveSave1;  // lwz r6,  0x3A4C
    muSaved0 = muRingPut;        // stw 0x3460 (word[12])
    muSaved1 = uRingEnd;         // stw 0x3464
    muSaved2 = uLive3;           // stw 0x3468
    muSaved3 = uLive2;           // stw 0x346C
    muSaved4 = uLive0;           // stw 0x3470
    muSaved5 = uLive1;           // stw 0x3474
}

// ===========================================================================
// DECLARATION-ONLY -- FLAGGED. Not implemented: each requires an un-homed
// sibling D3D type (CBlocker / CCommandBuffer / CRingAllocList) and/or builds a
// raw GPU command-packet stream or pokes GPU registers. Bodying any of these
// from the asm would mean fabricating a sibling layout or paraphrasing a
// multi-stage hardware pipeline, both forbidden by the reconstruction rules.
// They are intentionally left undefined here until those types are homed:
//
//   AddCallsToPrimaryBuffer    @ 0x82945DF8  -- PM4 call stream + ring poke
//                                               (MEMORY[0x7FC80714] GPU reg),
//                                               needs CBlocker via BlockOnPrimaryRange
//   AddCommandsToPrimaryBuffer @ 0x829455F0  -- PM4 copy into primary ring,
//                                               needs CBlocker via BlockOnPrimaryRange
//   BeginRingAlloc             @ 0x82945D00  -- needs CCommandBuffer/CRingAllocList
//                                               via RingBufferDeviceAllocate etc.
//   BeginRingBig               @ 0x82946D48  -- KickOff/StartNewSegment pipeline
//   BlockOnFence               @ 0x82946318  -- needs CBlocker
//   BlockOnPrimaryRange        @ 0x82945548  -- needs CBlocker (poll loop)
//   BlockOnSecondaryPosition   @ 0x82945448  -- needs CBlocker (poll loop)
//   CreateInvalidateBuffer     @ 0x82945F78  -- builds raw GPU invalidate packets
//   KickOff                    @ 0x82946AB8  -- full submit pipeline
//   KickOffSegment             @ 0x82946908  -- segment/fence pipeline
//   QueueIndirectBuffer        @ 0x82946238  -- needs CRingAllocList + spinlocks
//   SetFence                   @ 0x82946140  -- builds raw GPU fence packets
//   StartNewSegment            @ 0x82946740  -- needs CCommandBuffer/CRingAllocList
// ===========================================================================

// Pointer-invariant layout facts (uncalled). The scalar fields BEFORE the first
// host pointer (mpGpuReadback) carry their exact X360 byte offsets on the LLP64
// host too, so those are pinned absolutely. Offsets PAST the pointer shift by
// the 4->8 byte pointer widening and are therefore only checked for ordering,
// not absolute value.
void _CDeviceAssertLayout()
{
    static_assert(offsetof(CDevice, muRingPut)   == 0x30, "muRingPut @ +0x30 (word[12])");
    static_assert(offsetof(CDevice, muRingEnd)   == 0x34, "muRingEnd @ +0x34 (word[13])");
    static_assert(offsetof(CDevice, muRingLimit) == 0x38, "muRingLimit @ +0x38 (word[14])");
    static_assert(offsetof(CDevice, muOwnerThreadId) == 0x2A88,
                  "owning-thread id @ +0x2A88 (pointer-invariant, before mpGpuReadback)");
    static_assert(offsetof(CDevice, mpGpuReadback) == 0x2A90,
                  "GPU read-back pointer @ +0x2A90");
    static_assert(offsetof(CGpuReadback, muSecondaryRead) == 0x00,
                  "secondary cursor @ +0x00 of read-back block");
    static_assert(offsetof(CGpuReadback, muPrimaryRead) == 0x3C,
                  "primary cursor @ +0x3C of read-back block");
    // Past the host pointer the fields keep their relative ordering / 4-byte
    // stride even though their absolute X360 offsets shift by +4 on LLP64.
    static_assert(offsetof(CDevice, muStopMarker) - offsetof(CDevice, muStopSegment) == 4,
                  "stop marker immediately follows stop segment");
    static_assert(offsetof(CDevice, muSaved5) - offsetof(CDevice, muSaved0) == 20,
                  "six contiguous saved-state words");
    static_assert(offsetof(CDevice, muLiveSave3) - offsetof(CDevice, muLiveSave0) == 12,
                  "four contiguous live-state words");
    // GPU-block profiling fields poked by ~CBlocker: the seconds-per-tick scalar
    // precedes the two 64-bit tick accumulators. Only ORDER is pinned for the
    // scalar->counter step: on the LLP64 host the 8-byte-aligned u64 lands after
    // the misaligned-pointer shift, so its X360-absolute +8 spacing is not
    // reproducible (all access is by name). The two u64 counters ARE adjacent.
    static_assert(offsetof(CDevice, mfSecondsPerTick) < offsetof(CDevice, mu64BlockTicks),
                  "seconds-per-tick scalar precedes the block-ticks counters");
    static_assert(offsetof(CDevice, mu64BlockTicks3) - offsetof(CDevice, mu64BlockTicks) == 8,
                  "two contiguous 64-bit block-tick accumulators");
}

} // namespace D3D
