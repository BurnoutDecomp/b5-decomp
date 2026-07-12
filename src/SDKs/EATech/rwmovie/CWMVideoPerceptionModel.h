// =====================================================================================
// CWMVideoPerceptionModel -- perceptual analysis model for the WMV video-object encoder
// (CWMVideoObjectEncoder). Derives from CVideoAnalyst: it inherits the frame-analyst pool
// / temporal manager and layers the WMV-specific texture/adaptive-dead-zone/IDquant
// decisions on top, driving them from the owning encoder's picture context.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CWMVideoPerceptionModel::CWMVideoPerceptionModel @0x82A49980  [BLOCKED]
//   CWMVideoPerceptionModel::IsMBDominatedByTexture  @0x82A489B8  [BLOCKED]
//   CWMVideoPerceptionModel::IsTextureBlock          @0x82A48870  [BLOCKED]
//   CWMVideoPerceptionModel::ShiftPixels             @0x82A48E98  [BLOCKED]
//   CWMVideoPerceptionModel::analysisFrame           @0x82A49900  (bodied here)
//   CWMVideoPerceptionModel::decideApplyDquan        @0x82A495C0  [BLOCKED]
//   CWMVideoPerceptionModel::isApplyIDquanBlocks     @0x82A48E80  (bodied here)
//
// Derivation is attested by CWMVideoPerceptionModel::analysisFrame (a bare tail-call thunk
// `b CVideoAnalyst::analysisFrame` with `this` passed straight through) and by
// decideApplyDquan calling CVideoAnalyst::getCurFrameAnalyst(this). The ctor initialises the
// inherited CVideoAnalyst fields inline (vptr, the eight zeroed pool/temporal words, and
// miCurIndex/miSetIndex = -1) exactly as the base default construction would, then its own
// six words below.
//
// X360 layout (byte offsets from this; the base occupies +0x00..+0x37, so sizeof(base)==0x38
// on the 32-bit console). The PC target's 8-byte vptr/pointers shift every offset, so this
// class is accessed by NAMED member (semantic parity), not by byte offset -- the offsets
// below are documentation of the console form:
//   +0x00..+0x37  CVideoAnalyst base (vptr + macro-block dims + frame-analyst pool + temporal)
//   +0x38 mpVideoInfo        owning CWMVideoObjectEncoder picture context (a2 to the ctor)
//   +0x3C mpIDquanMap        per-macro-block IDquant decision map (XMemAlloc'd in decideApplyDquan)
//   +0x40 miIDquanStride     bytes per MB row in mpIDquanMap == (mbWidth + 7) >> 3
//   +0x44 miField44          enable flag, ctor-initialised to 1 (bit0 gates the texture-band probe)
//   +0x48 miTextureThreshold texture-activity threshold, ctor-initialised to 14 (clamped 14/30)
//   +0x4C miField4C          ctor-initialised to 0
// =====================================================================================
#pragma once

#include "types.hpp"

#include "CVideoAnalyst.h"
#include "CVideoFrameAnalyst.h"

// The owning encoder context is a large, not-yet-reconstructed vendor type; this class only
// stores a pointer to it (never a by-value member), so a forward declaration is sufficient
// and correct until CWMVideoObjectEncoder is homed.
class CWMVideoObjectEncoder;

class CWMVideoPerceptionModel : public CVideoAnalyst
{
public:
    // Homes the model against its owning encoder context and lazily creates the shared
    // dequant-helper singleton. [BLOCKED: the singleton is a placement-constructed object of
    // an un-homed class (vtable off_8210F890); reconstructing its ctor would fabricate a type.]
    explicit CWMVideoPerceptionModel(CWMVideoObjectEncoder* pVideoInfo);

    // Counts texture-dominated blocks in a macro-block and reports the sub-block breakdown.
    // [BLOCKED: dereferences the un-homed encoder context (mpVideoInfo) at raw offsets.]
    int IsMBDominatedByTexture(int iMBCol, int iMBRow, int* piOutTextureBias, int* piOutTextureCount);

    // Returns the representative texture-map sample for a block.
    // [BLOCKED: dereferences the un-homed encoder context (mpVideoInfo) at raw offsets.]
    int IsTextureBlock(int iBlockCol, int iBlockRow, int iSubBlock);

    // Edge-extends (pixel-shifts) the three YUV planes for the overlap filter.
    // [BLOCKED: reads the un-homed encoder context (mpVideoInfo +0x7B34) for the shift width.]
    void ShiftPixels(u8* pY, u8* pU, u8* pV);

    // Selects the current-frame analyst and runs the frame analysis. A name-hiding forwarder
    // to the inherited CVideoAnalyst::analysisFrame (bare tail-call in the asm).
    CVideoFrameAnalyst* analysisFrame(u8* pY, u8* pU, u8* pV, u8* pSourceId);

    // Decides whether to apply differential quantisation and, if so, the delta-QP.
    // [BLOCKED: dereferences the un-homed encoder context (mpVideoInfo) at raw offsets and
    // indexes the un-recovered quant-delta rodata tables unk_8210F7B0/7D0/7F0 (30-byte rows).]
    int decideApplyDquan(int* piOutDeltaQP);

    // Fetches the IDquant decision byte for macro-block (iRow, iCol) from mpIDquanMap.
    int isApplyIDquanBlocks(int iRow, int iCol);

private:
    CWMVideoObjectEncoder* mpVideoInfo;        // +0x38  owning encoder picture context
    u8*                    mpIDquanMap;        // +0x3C  per-MB IDquant decision map
    u32                    miIDquanStride;     // +0x40  bytes per MB row in mpIDquanMap
    s32                    miField44;          // +0x44  enable flag (ctor = 1)
    s32                    miTextureThreshold; // +0x48  texture-activity threshold (ctor = 14)
    s32                    miField4C;          // +0x4C  ctor = 0
};
