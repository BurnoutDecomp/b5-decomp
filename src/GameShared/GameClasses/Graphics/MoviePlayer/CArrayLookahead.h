// ============================================================================
// CArrayLookahead.h  --  circular look-ahead ring of analysed video frames
//
//   A fixed-capacity ring buffer used by the video pre-processor
//   (SessionFrameEncoder) to hold a small window of recent frames so the
//   scene-cut / dissolve detector can look a few frames ahead. Each slot owns a
//   CHistogram plus per-frame change flags; a CSeqDissolveDetector is embedded
//   for cross-fade detection.
//
//   X360 methods (asm authoritative, BURNOUT_X360_ARTIST.XEX):
//       CArrayLookahead::CArrayLookahead              @ 0x82A04BB8
//       CArrayLookahead::~CArrayLookahead             @ 0x82A055B8
//       CArrayLookahead::Initialize                   @ 0x82A04DE8
//       CArrayLookahead::ResetElement                 @ 0x82A04C68
//       CArrayLookahead::AddArrayElement              @ 0x82A04D30
//       CArrayLookahead::SetDissolveDumpFile          @ 0x82A05258
//       CArrayLookahead::Filter1stOrderDiffLumaVarMean@ 0x82A05048
//       CArrayLookahead::DetectDissolve               @ 0x82A05648
//       CArrayLookahead::DetectEventInArray           @ 0x82A05710
//   (The last two drive the embedded CSeqDissolveDetector; its Reset/Detect/
//    ComputeLocalFeature engine bodies are a separate TU -- declarations in
//    CSeqDissolveDetector.h suffice under the per-TU compile gate.)
//
//   Class keeps its native `C`-prefixed name (third-party-style video helper),
//   matching the sibling CHistogram; no Cgs/Brn namespace is imposed.
// ============================================================================
#pragma once

#include <cstddef>  // offsetof
#include "types.hpp"
#include "GameShared/GameClasses/Graphics/MoviePlayer/CHistogram.h"
#include "GameShared/GameClasses/Graphics/MoviePlayer/CSeqDissolveDetector.h"

// ---- raw Xbox 360 XDK heap API (platform externs) ---------------------------
// The asm calls the XDK XMemAlloc(SIZE_T,DWORD)->LPVOID / XMemFree(LPVOID,DWORD)
// directly (r3=size/pAddress, r4=attributes), with NO `this` argument -- the raw
// platform API, matching the sibling rwmovie / XCam TUs. Supplied by the
// platform layer.
extern void* XMemAlloc(u32 uSize, u32 uAttributes);
extern void  XMemFree(void* pAddress, u32 uAttributes);

// Attribute word baked into every XMemAlloc/XMemFree call site of this TU
// (lis 0x248C / ori 0x8000 -> 0x248C8000).
static const u32 KU_LOOKAHEAD_XMEM_ATTRIBUTES = 0x248C8000u;

class CArrayLookahead
{
public:
    // One ring slot (0x28 = 40 bytes). +0x00 image data + +0x04 histogram are
    // preserved across ResetElement; +0x08.. are per-frame analysis flags.
    struct SElement
    {
        const u8*   mpImageData;      // +0x00  frame plane data (fed to CHistogram::CalcHistogram)
        CHistogram* mpHistogram;      // +0x04  owned per-slot histogram (XMemAlloc'd)
        s32         miHistChanged0;   // +0x08  primary hist-changed / distance result
        s32         miLumaChanged;    // +0x0C  luma-ratio change flag
        s32         miHistChanged1;   // +0x10  secondary hist-changed result
        s32         miReserved14;     // +0x14
        s32         miHistChanged2;   // +0x18  tertiary hist-changed result
        s32         miReserved1C;     // +0x1C
        s32         miEventType;      // +0x20  slot event/state (2/3/4 cut/dissolve codes)
        s32         miProcessed;      // +0x24  set to 1 once the slot has been analysed
    };

    CArrayLookahead();                                              // 0x82A04BB8
    ~CArrayLookahead();                                             // 0x82A055B8

    // Allocate the ring (liLookahead + 4 slots, min 5) plus a histogram per
    // slot; store frame dimensions. Returns 0 on success, -100 on OOM.
    s32  Initialize(s32 liLookahead, s32 liWidth, s32 liHeight);    // 0x82A04DE8

    // Zero the per-frame analysis fields (and reset the histogram) of one slot.
    void ResetElement(u32 luIndex);                                // 0x82A04C68

    // Push one frame's plane data into the slot at the write head; advances the
    // head modulo capacity. Returns 0, or -100 if uninitialised / ring full.
    s32  AddArrayElement(const u8* lpImageData);                   // 0x82A04D30

    // Conditionally clear the dissolve dump-file handle (see .cpp for the exact,
    // faithful predicate -- the handle is never stored here, only cleared).
    void SetDissolveDumpFile(void* lpFile);                        // 0x82A05258

    // First-order temporal IIR smoothing of each slot histogram's luma variance
    // and mean (and their frame-to-frame deltas) across the look-ahead window.
    void Filter1stOrderDiffLumaVarMean();                          // 0x82A05048

    // Run the embedded dissolve detector over the slot three frames behind the
    // read head (feeds it the variance/mean deltas of the current and previous
    // frames, or resets it when that slot already carries a dissolve marker).
    void DetectDissolve();                                         // 0x82A05648

    // Drain every pending frame from the ring: build each frame's histogram,
    // compute up-to-3-frame change flags, advance the read head, run the IIR
    // filter + dissolve detector, and classify scene cuts / dissolves per slot.
    void DetectEventInArray();                                     // 0x82A05710

private:
    friend void _CArrayLookahead_AssertLayout();

    SElement*            mpArray;              // +0x00  ring storage (XMemAlloc'd)
    u32                  muCapacity;           // +0x04  slot count
    s32                  miImageWidth;         // +0x08
    s32                  miImageHeight;        // +0x0C
    s32                  miImageSize;          // +0x10  width * height
    u32                  muReadIndex;          // +0x14  tail (oldest) index
    u32                  muWriteIndex;         // +0x18  head (next-write) index
    u32                  muFrameCount;         // +0x1C  frames accumulated so far
    s32                  miDissolveEnabled;    // +0x20  1 = run dissolve detection (set in ctor)
    CSeqDissolveDetector mDissolveDetector;    // +0x24  embedded cross-fade detector (0x88 bytes)
    void*                mpDissolveDumpFile;   // +0xAC  optional debug dump handle
};

// Field-ORDER pins only. This object contains pointers, so its host (x64,
// 8-byte pointer) byte offsets legitimately differ from the X360 (4-byte
// pointer) offsets documented in the comments above; only the declaration
// order is invariant across widths, and it is what keeps the reconstruction's
// per-index / per-slot access semantics faithful. (CSeqDissolveDetector and
// CHistogram are pointer-free, so their exact 32-bit offsets are pinned in
// their own headers.)
inline void _CArrayLookahead_AssertLayout()
{
    static_assert(offsetof(CArrayLookahead::SElement, mpImageData)
                < offsetof(CArrayLookahead::SElement, mpHistogram), "SElement order: data<histogram");
    static_assert(offsetof(CArrayLookahead::SElement, mpHistogram)
                < offsetof(CArrayLookahead::SElement, miEventType), "SElement order: histogram<eventType");
    static_assert(offsetof(CArrayLookahead::SElement, miEventType)
                < offsetof(CArrayLookahead::SElement, miProcessed), "SElement order: eventType<processed");

    static_assert(offsetof(CArrayLookahead, mpArray)
                < offsetof(CArrayLookahead, muCapacity), "CArrayLookahead order: array<capacity");
    static_assert(offsetof(CArrayLookahead, miDissolveEnabled)
                < offsetof(CArrayLookahead, mDissolveDetector), "CArrayLookahead order: enabled<detector");
    static_assert(offsetof(CArrayLookahead, mDissolveDetector)
                < offsetof(CArrayLookahead, mpDissolveDumpFile), "CArrayLookahead order: detector<dumpFile");
}
