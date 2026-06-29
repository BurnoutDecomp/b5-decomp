// BrnGuiRaceCarInfoEvent.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnGui::GuiRaceCarInfoEvent::Construct
// (@0x823A7658) zero-initialises the fixed set of 8 per-active-race-car entries (the
// 16-byte position lanes, the identity qwords, the entry count, and the three per-entry
// flag-byte arrays). The 16-byte VMX zero-store is reproduced as a scalar Vector4 zero
// per the project's established VMX-as-scalar reconstruction (endian-independent;
// see BrnGuiEventTypeDefs.cpp DoWorstCase precedent).
//
// DoWorstCase (@0x823A6BB8) recomputes every entry's "worst case" screen position via a
// per-entry VMX vmaddfp128 fused-multiply-add transform; it is reconstructed below by
// scalarizing the lane math, following the committed GuiEventUpdateSatNav::DoWorstCase
// precedent in BrnGuiEventTypeDefs.cpp. The X360 inlined the bounds-checked
// operator++(EActiveRaceCarIndex&, int) (BurnoutConstants.h, the asm's FireAssert points at
// BurnoutConstants.h:39 with "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT"); the loop is
// restored to drive that real operator so the guard lands exactly where the operator defines
// it.

#include "GameSource/Gui/BrnGuiRaceCarInfoEvent.h"

namespace BrnGui
{

// @ 0x823A7658
//   for (i=0; i<8; ++i) {
//       stvx128 0   @ maPosition[i]   (16-byte zero)
//       std     0   @ maIdentity[i]   (8-byte zero)
//       stb     0   @ maFlagA[i]      (byte zero)
//       stb     0   @ maFlagB[i]      (byte zero)
//       stb     0   @ maFlagC[i]      (byte zero)
//   }
//   stw 0 @ miNumEntries
GuiRaceCarInfoEvent* GuiRaceCarInfoEvent::Construct()
{
    for ( s32 liIndex = 0; liIndex < KI_NUM_ENTRIES; ++liIndex )
    {
        maPosition[liIndex].x = 0.0f;
        maPosition[liIndex].y = 0.0f;
        maPosition[liIndex].z = 0.0f;
        maPosition[liIndex].w = 0.0f;
        maIdentity[liIndex] = 0;
        maFlagA[liIndex] = 0;
        maFlagB[liIndex] = 0;
        maFlagC[liIndex] = 0;
    }

    miNumEntries = 0;
    return this;
}

// @ 0x823A6BB8
// Per-entry position lanes are baked from a function-local float pool:
//   weight = {0.0, 0.1, 0.25, 0.0}   (flt_82001CC0 / flt_82004014 / flt_82003F40 / flt_82001CC0)
//   input scale = 2.0                (flt_82001D9C)
// For each entry index li in [0,8) other than liNumActive:
//   base = maPosition[liNumActive]                     ; lvx128 v0,(liNumActive*16),this
//   step = lvInput * (2.0 * li) + base                 ; vmaddfp128 v0, v127, splat(li*2), v0
//   pos  = weight * step + (f32)li                     ; vmaddfp   v0, weight, v0, splat(li)
//   maPosition[li] = pos                               ; stvx128 v0,(li*16),this
//   maFlagA[li]=1; maFlagD[li]=1; maFlagE[li]=0; maFlagC[li]=0; maFlagB[li]=0  (store order)
//   maIdentity[li] = maIdentity[0]                      ; ld 0(this+0x80) -> std 0(this+0x80+li*8)
// Then miNumEntries = 8.
GuiRaceCarInfoEvent* GuiRaceCarInfoEvent::DoWorstCase(Vector4 lvInput, s32 liNumActive)
{
    // X360 float-pool immediates (named for clarity; values from the asm float pool).
    static const f32 KAF_WEIGHT[4] = { 0.0f, 0.1f, 0.25f, 0.0f }; // {flt_82001CC0, flt_82004014, flt_82003F40, flt_82001CC0}
    static const f32 KF_INPUT_SCALE = 2.0f;                       // flt_82001D9C

    // The source/template entry the others are derived from. FLAGGED: the X360 reads
    // maPosition[liNumActive] unconditionally; if liNumActive == KI_NUM_ENTRIES (8) this is a
    // one-past-end read. In practice liNumActive is the active-car count (< 8) and that entry
    // is the one skipped by the loop below, so it is the preserved source.
    const Vector4& lSource = maPosition[liNumActive];

    for ( EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_0;
          leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
          leIndex++ )                       // inlined operator++(EActiveRaceCarIndex&,int): asserts <= COUNT
    {
        const s32 liIndex = static_cast<s32>( leIndex );
        if ( liIndex == liNumActive )       // X360 `beq` skips the source entry's body
        {
            continue;
        }

        const f32 lfIndex = static_cast<f32>( liIndex );
        const f32 lfScale = KF_INPUT_SCALE * lfIndex; // splat(li*2)

        // step = lvInput * (2*li) + source  (vmaddfp128, lane-by-lane)
        Vector4 lStep;
        lStep.x = lvInput.x * lfScale + lSource.x;
        lStep.y = lvInput.y * lfScale + lSource.y;
        lStep.z = lvInput.z * lfScale + lSource.z;
        lStep.w = lvInput.w * lfScale + lSource.w;

        // pos = weight * step + li  (vmaddfp, lane-by-lane). Lanes 0/3 weight==0 -> just li.
        Vector4& lPos = maPosition[liIndex];
        lPos.x = KAF_WEIGHT[0] * lStep.x + lfIndex;
        lPos.y = KAF_WEIGHT[1] * lStep.y + lfIndex;
        lPos.z = KAF_WEIGHT[2] * lStep.z + lfIndex;
        lPos.w = KAF_WEIGHT[3] * lStep.w + lfIndex;

        maFlagA[liIndex] = 1;
        maFlagD[liIndex] = 1;
        maFlagE[liIndex] = 0;
        maFlagC[liIndex] = 0;
        maFlagB[liIndex] = 0;

        maIdentity[liIndex] = maIdentity[0];
    }

    miNumEntries = KI_NUM_ENTRIES; // 8
    return this;
}

} // namespace BrnGui
