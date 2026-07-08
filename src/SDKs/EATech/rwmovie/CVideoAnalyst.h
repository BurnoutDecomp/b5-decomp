// =====================================================================================
// CVideoAnalyst -- frame-analysis manager for the RTCMV/WMV video-object encoder. Owns a
// pool of CVideoFrameAnalyst records (one per in-flight frame) plus a small temporal
// index buffer, and hands the perception model the analyst for the current frame.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CVideoAnalyst::Clean              @0x82A491D8
//   CVideoAnalyst::Init               @0x82A49260
//   CVideoAnalyst::analysisFrame      @0x82A49450
//   CVideoAnalyst::getCurFrameAnalyst @0x82A47238
//   CVideoAnalyst::setCurFrameAnalyst @0x82A47290
//   CVideoAnalyst::setFreeAnalyst     @0x82A493E8
//   CVideoAnalyst::setTemporal        @0x82A472E0
//   CVideoAnalyst::~CVideoAnalyst     @0x82A49908
//
// The object has a vtable (the destructor is virtual: the X360 dtor writes off_8210F8A0
// into +0x00); every other method is a direct (non-virtual) call. On the X360 the vptr
// and the pointer members are 4 bytes, so the exact byte offsets below are the X360 form.
// The PC target's 8-byte vptr/pointers shift them, so this class is accessed by NAMED
// member (semantic parity), not by byte offset -- the X360 offsets are documentation.
//
// X360 layout (byte offsets from this):
//   +0x00 vptr
//   +0x04 miMacroBlockArea      4 * ceil(w/16) * ceil(h/16)
//   +0x08 miMacroBlockAreaHalf  miMacroBlockArea >> 1
//   +0x0C miWidth
//   +0x10 miHeight
//   +0x14 miWidthInBlocks       w >> 16 ... (w >> 4)
//   +0x18 mpFrameAnalysts       heap array of CVideoFrameAnalyst (past its count cookie)
//   +0x1C miNumAnalysts         element count of mpFrameAnalysts
//   +0x20 mpTemporal            temporal index buffer (miMaxRefFrames ints)
//   +0x24 miTemporalCount       high-water count written into mpTemporal
//   +0x28 miMaxRefFrames        min(maxRef, numAnalysts) (or numAnalysts if maxRef 0)
//   +0x2C miCurIndex            index of the current analyst (-1 = none)
//   +0x30 miSetIndex            index selected by setCurFrameAnalyst (-1 = none)
//   +0x34 miFreeCount           number of analysts flagged free for reuse
// =====================================================================================
#pragma once

#include "types.hpp"

#include "CVideoFrameAnalyst.h"

class CVideoAnalyst
{
public:
    virtual ~CVideoAnalyst();

    // (Re)allocates the frame-analyst pool (uNumAnalysts records) and the temporal index
    // buffer for a width x height frame; uMaxRef caps the temporal buffer. Returns the
    // last allocation/Init result (vestigial). Calls Clean() first.
    int Init(s32 iWidth, s32 iHeight, u32 uNumAnalysts, u32 uMaxRef);

    // Frees the frame-analyst pool and the temporal buffer and resets the indices.
    int Clean();

    // Selects (or reuses) an analyst, records the source plane on it, builds its texture
    // map and, when the pool is populated, its per-block averages. pU/pV default to the
    // U/V planes packed after pY; pSourceId defaults to pY. Returns the analyst, or null.
    CVideoFrameAnalyst* analysisFrame(u8* pY, u8* pU, u8* pV, u8* pSourceId);

    // Returns the analyst for the current (or newest reference) frame, or null.
    CVideoFrameAnalyst* getCurFrameAnalyst();

    // Finds the analyst holding pSourceId, records its index in miSetIndex and returns it
    // (null if none match).
    CVideoFrameAnalyst* setCurFrameAnalyst(u8* pSourceId);

    // Flags the analyst holding pSourceId as free for reuse. Returns this.
    CVideoAnalyst* setFreeAnalyst(u8* pSourceId);

    // Writes iValue into the temporal buffer at iIndex, growing miTemporalCount; iIndex 0
    // resets the count first. Returns this.
    CVideoAnalyst* setTemporal(s32 iIndex, s32 iValue);

private:
    s32                 miMacroBlockArea;      // +0x04
    s32                 miMacroBlockAreaHalf;  // +0x08
    s32                 miWidth;               // +0x0C
    s32                 miHeight;              // +0x10
    s32                 miWidthInBlocks;       // +0x14
    CVideoFrameAnalyst* mpFrameAnalysts;       // +0x18
    s32                 miNumAnalysts;         // +0x1C
    s32*                mpTemporal;            // +0x20
    u32                 miTemporalCount;       // +0x24
    s32                 miMaxRefFrames;        // +0x28
    s32                 miCurIndex;            // +0x2C
    s32                 miSetIndex;            // +0x30
    s32                 miFreeCount;           // +0x34
};
