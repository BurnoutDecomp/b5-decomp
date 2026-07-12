// =====================================================================================
// CWMVideoPerceptionModel -- perceptual analysis model for the WMV video-object encoder.
// See CWMVideoPerceptionModel.h for the layout map and the X360 addresses of every method.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
// Only the two functions that touch no un-homed collaborator are bodied here:
//   * analysisFrame        -- a name-hiding forwarder to the inherited CVideoAnalyst.
//   * isApplyIDquanBlocks  -- a byte fetch from this class's own mpIDquanMap.
// The other five (the ctor, IsMBDominatedByTexture, IsTextureBlock, ShiftPixels,
// decideApplyDquan) are left blocked: they dereference the un-reconstructed
// CWMVideoObjectEncoder picture context at raw offsets and/or index un-recovered quant-delta
// rodata (unk_8210F7B0/7D0/7F0). Reconstructing them would require fabricating those types
// or guessing those bytes; that work is deferred until the encoder context is homed.
// =====================================================================================
#include "types.hpp"

#include "CWMVideoPerceptionModel.h"
#include "CVideoAnalyst.h"
#include "CVideoFrameAnalyst.h"

// CWMVideoPerceptionModel::analysisFrame @0x82A49900.
// The X360 build emits a single `b CVideoAnalyst::analysisFrame` -- a tail-call thunk that
// passes `this` and all four plane arguments straight through to the inherited analyser.
CVideoFrameAnalyst* CWMVideoPerceptionModel::analysisFrame(u8* pY, u8* pU, u8* pV, u8* pSourceId)
{
    return CVideoAnalyst::analysisFrame(pY, pU, pV, pSourceId);
}

// CWMVideoPerceptionModel::isApplyIDquanBlocks @0x82A48E80.
// return mpIDquanMap[miIDquanStride * iRow + iCol], read as an unsigned byte (lbzx).
int CWMVideoPerceptionModel::isApplyIDquanBlocks(int iRow, int iCol)
{
    return mpIDquanMap[miIDquanStride * iRow + iCol];
}
