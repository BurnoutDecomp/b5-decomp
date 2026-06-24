#ifndef BRN_PFX_HOOK_BLENDER_H
#define BRN_PFX_HOOK_BLENDER_H

#include "types.hpp"
#include "SharedClasses/Graphics/BrnEffectsData.h"   // BrnEffects::BloomData / BlurData / TintData2d

// =============================================================================
// BrnPfxHookBlender.h
//
// The per-post-FX weighted blend accumulators driven by BrnGui::PFXHookNodeBlender
// (asserts cite "..\\..\\..\\GameSource\\Gui/PFX/BrnPfxHookBlender.cpp", line 84).
//
// Each accumulator is a small wrapper over one post-FX payload plus two trailing
// f32 bookkeeping fields:
//   mfWeight  - the largest source weight seen so far (clamped <= 1.0f)
//   mfCount   - how many Add() calls have folded into this accumulator
//
// Add(lpSrc, lfWeight) semantics (X360 ARTIST, one body per payload type):
//   if (!lpSrc) return this;                       // null source is a no-op
//   mfCount += 1.0f;
//   if (mfWeight < lfWeight) { ASSERT(lfWeight<=1.0f); mfWeight = lfWeight; }
//   if (mfCount == 1.0f) copy the payload straight from the source;
//   else                 payload = src*lfWeight + payload*(1 - lfWeight)
//                                  (per scalar member and per vector lane).
//
// The blend matches the X360 store widths exactly: scalar members are folded in
// fp registers; the 16-byte vector members are blended lane-for-lane (the same
// vmaddfp chain the X360 emits, including the z/w lanes its 16-byte stores touch).
//
// NOTE: layouts are proven by the Add bodies' load/store offsets, not by the
// post-FX *Data structs (those carry SIMD alignment padding that these tightly
// packed blend payloads do not). Members are accessed by name only.
// =============================================================================

namespace BrnGui
{
    // ---- BloomData accumulator (Add @ 0x824F6810) ----------------------------
    // payload == BrnEffects::BloomData (32B) then weight @0x20, count @0x24.
    struct BloomBlend
    {
        BrnEffects::BloomData mData;   // +0x00 .. +0x1F
        f32                   mfWeight; // +0x20
        f32                   mfCount;  // +0x24

        BloomBlend* Add(const BrnEffects::BloomData* lpSrc, f32 lfWeight);
    };

    // ---- DepthOfFieldData accumulator (Add @ 0x824F6C40) ---------------------
    // Tightly packed 5-float payload (the X360 1-weight copy moves exactly 5
    // dwords and the blend touches the first four lanes); weight @0x14, count
    // @0x18. This payload is NOT the padded BrnEffects::DepthOfFieldData; the
    // five fields are spelled here by name so member access stays honest.
    struct DepthOfFieldBlend
    {
        f32 mfNearPlane;   // +0x00
        f32 mfFocalPlane;  // +0x04
        f32 mfFocalPlane2; // +0x08
        f32 mfFarPlane;    // +0x0C
        f32 mfDofAmount;   // +0x10  (copied at count==1, NOT blended)
        f32 mfWeight;      // +0x14
        f32 mfCount;       // +0x18

        DepthOfFieldBlend* Add(const DepthOfFieldBlend* lpSrc, f32 lfWeight);
    };

    // ---- TintData2d accumulator (Add @ 0x824F6D60) ---------------------------
    // payload == BrnEffects::TintData2d (one Vector4, 16B) then weight @0x10,
    // count @0x14.
    struct TintData2dBlend
    {
        BrnEffects::TintData2d mData;    // +0x00 .. +0x0F
        f32                    mfWeight; // +0x10
        f32                    mfCount;  // +0x14

        TintData2dBlend* Add(const BrnEffects::TintData2d* lpSrc, f32 lfWeight);
    };

    // ---- BlurData accumulator (Add @ 0x824F6B70) -----------------------------
    // payload == BrnEffects::BlurData (96B) then weight @0x60, count @0x64. The
    // blend path delegates to BrnEffects::BlurData::SetToBlend; the copy path is a
    // straight 96-byte memcpy of the payload.
    struct BlurBlend
    {
        BrnEffects::BlurData mData;    // +0x00 .. +0x5F
        f32                  mfWeight; // +0x60
        f32                  mfCount;  // +0x64

        BlurBlend* Add(const BrnEffects::BlurData* lpSrc, f32 lfWeight);
    };
}

#endif
