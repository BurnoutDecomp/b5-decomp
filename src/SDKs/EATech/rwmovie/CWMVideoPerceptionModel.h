// =====================================================================================
// CWMVideoPerceptionModel -- perceptual analysis model for the WMV video-object encoder
// (CWMVideoObjectEncoder). Derives from CVideoAnalyst: it inherits the frame-analyst pool
// / temporal manager and layers the WMV-specific texture / adaptive-dead-zone / IDquant
// decisions on top, driving them from the owning encoder's picture context.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no debug-info declaration exists for this TU.
//
// Derivation is attested by CWMVideoPerceptionModel::analysisFrame (a bare tail-call thunk
// to CVideoAnalyst::analysisFrame with `this` passed straight through) and by
// decideApplyDquan calling CVideoAnalyst::getCurFrameAnalyst(this). The console vtable of
// this class holds {analysisFrame (this class), setTemporal, setFreeAnalyst} (inherited).
// The ctor initialises the inherited CVideoAnalyst fields inline (the eight pool/temporal
// words, with miCurIndex/miSetIndex = -1), then its own words below, and lazily creates
// the shared CWMVFARoutines instance; the dtor frees the IDquant map and that instance.
//
// Console layout (byte offsets from this; the base occupies +0x00..+0x37 on the 32-bit
// console). The PC target's 8-byte vptr/pointers shift every offset, so this class is
// accessed by NAMED member; the offsets below document the console form:
//   +0x00..+0x37  CVideoAnalyst base (vptr + geometry + frame-analyst pool + temporal)
//   +0x38 mpVideoInfo        owning CWMVideoObjectEncoder picture context (ctor argument)
//   +0x3C mpIDquanMap        per-macro-block IDquant decision map (allocated lazily by
//                            decideApplyDquan, freed by the dtor)
//   +0x40 miIDquanStride     bytes per MB row of mpIDquanMap == (mbWidth + 7) >> 3
//   +0x44 miField44          ctor = 1; bit0 enables the high-texture-band threshold probe
//   +0x48 miTextureThreshold texture-activity threshold, ctor = 14 (decideApplyDquan
//                            re-derives it, clamped to 14..30)
//   +0x4C miField4C          ctor = 0
// =====================================================================================
#pragma once

#include "types.hpp"

#include "CVideoAnalyst.h"
#include "CVideoFrameAnalyst.h"
#include "CWMVideoObjectEncoder.h"

class CWMVideoPerceptionModel : public CVideoAnalyst
{
public:
    // Homes the model against its owning encoder context and lazily creates the shared
    // CWMVFARoutines instance (gpWMVFARoutines).
    explicit CWMVideoPerceptionModel(CWMVideoObjectEncoder* pVideoInfo);

    // Frees the IDquant map and the shared CWMVFARoutines instance.
    ~CWMVideoPerceptionModel();

    // Classifies the four 8x8 activity samples of macro-block (iMBRow, iMBCol) in the
    // current frame (both fields of a field pair): counts the samples in the texture
    // band [30, 60) and, when piOutLowActivityCount is given, the samples at or below
    // miTextureThreshold. Returns 1 when at least three samples are texture.
    int IsMBDominatedByTexture(int iMBRow, int iMBCol, int* piOutLowActivityCount, int* piOutTextureCount);

    // Returns the texture-map sample of sub-block iSubBlock (0..3) of macro-block
    // (iMBRow, iMBCol) in the current frame -- the larger of the two fields for a field
    // pair -- or 0 when no frame is current.
    int IsTextureBlock(int iMBRow, int iMBCol, int iSubBlock);

    // Shifts the three YUV planes down/right in place by the overlap margin (2 pixels,
    // 4 when the encoder asks for it; half that for chroma), replicating the edge samples.
    void ShiftPixels(u8* pY, u8* pU, u8* pV);

    // Selects the current-frame analyst and runs the frame analysis. A name-hiding forwarder
    // to the inherited CVideoAnalyst::analysisFrame (bare tail-call in the asm).
    CVideoFrameAnalyst* analysisFrame(u8* pY, u8* pU, u8* pV, u8* pSourceId);

    // Decides whether to apply differential quantisation to this picture: builds the
    // per-MB IDquant map, rejects too-sparse / too-dense maps, and when applying writes
    // the delta-QP (table value for the picture quantiser minus that quantiser) to
    // *piOutDeltaQP and returns 1. Returns 0 otherwise.
    int decideApplyDquan(int* piOutDeltaQP);

    // Per-MB IDquant decision over the reference frames: 0 (do not apply), 4 (low
    // activity) or 8 (flat), requiring the block average to be temporally stable.
    int decideApplyIDquanBlocks(int iMBRow, int iMBCol, int bCountFlat);

    // Fetches the IDquant decision byte for macro-block (iRow, iCol) from mpIDquanMap.
    int isApplyIDquanBlocks(int iRow, int iCol);

private:
    CWMVideoObjectEncoder* mpVideoInfo;        // +0x38  owning encoder picture context
    u8*                    mpIDquanMap;        // +0x3C  per-MB IDquant decision map
    u32                    miIDquanStride;     // +0x40  bytes per MB row in mpIDquanMap
    s32                    miField44;          // +0x44  ctor = 1 (bit0: texture-band probe)
    s32                    miTextureThreshold; // +0x48  texture-activity threshold (ctor = 14)
    s32                    miField4C;          // +0x4C  ctor = 0
};
