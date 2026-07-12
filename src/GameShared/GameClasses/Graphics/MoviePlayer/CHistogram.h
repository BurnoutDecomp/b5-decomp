// ============================================================================
// CHistogram.h  --  video/scene histogram + distance metrics
//   X360 methods: CHistogram::CalcHistogram   @ 0x82A04330
//                 CHistogram::CalcHistDist    @ 0x82A048B0
//                 CHistogram::isHistogramChanged @ 0x82A04A60
//   Called by CArrayLookahead::DetectEventInArray (scene-cut / event detection).
//   Class keeps its native `C`-prefixed name (third-party-style video helper);
//   no Cgs/Brn namespace is imposed.
// ============================================================================
#pragma once

#include <cstddef>  // offsetof
#include <cmath>
#include "types.hpp"

class CHistogram
{
public:
    void CalcHistogram(const u8* lpauImage, u32 luWidth, u32 luHeight);

    // Distance between two 256-bin histograms.
    //   liMode == 1: L1 (sum of abs bin differences) / luCount
    //   liMode == 2: (luCount - histogram-intersection) * scale / luCount
    //   otherwise  : KF_HIST_DIST_DEFAULT
    f32 CalcHistDist(const s32* lpaOther, const s32* lpaThis, u32 luCount, s32 liMode);

    bool isHistogramChanged(const CHistogram* lpOther, u32 luImageSize,
                            f32* lpfSecondaryDist, f32* lpfPrimaryDist);

private:
    // Never-called layout pin so any drift in a member offset is a compile error.
    friend void _CHistogram_AssertLayout();

    // Small L1-abs helper matching the PPC xor/srawi/subf sign-flip idiom.
    static s32 Abs(s32 liValue)
    {
        const s32 liSign = liValue >> 31;
        return (liValue ^ liSign) - liSign;
    }

    static const u32 KU_NUM_BINS = 256;

    // Reconstructed rodata float literals (values not attested in any dump
    // available at reconstruction time; derived from statistical/normalizing
    // role -> flagged medium confidence in the function entries).
    static constexpr f32 KF_ONE                     = 1.0f;  // flt_82001C98
    static constexpr f32 KF_HIST_INTERSECTION_SCALE = 1.0f;  // flt_82001D9C
    static constexpr f32 KF_HIST_DIST_DEFAULT       = 0.0f;  // flt_820037C8
    static constexpr f32 KF_HIST_CHANGED_HIGH       = 0.6f;  // dbl_8210EF58
    static constexpr f32 KF_HIST_CHANGED_LOW        = 0.25f; // flt_82003F40

public:
    // --- byte-exact layout (total size 0x81C) ---
    // Public data: the X360 asm accesses these fields directly from the owning
    // CArrayLookahead (per-slot histogram reset / variance-mean IIR filtering),
    // so they are part of the class's public surface, not private state.
    f32  mfVariance;                 // +0x000  (1/N)*Sum(k^2*histA[k]) - mean^2
    f32  mfVarianceDelta;            // +0x004  frame-to-frame variance difference
                                     //         (written by CArrayLookahead::Filter1stOrderDiffLumaVarMean)
    f32  mfMean;                     // +0x008  (1/N)*Sum(k*histA[k])
    f32  mfMeanDelta;                // +0x00C  frame-to-frame mean difference (same filter)
    u32  muPad10;                    // +0x010
    f32  mfHighMoment;               // +0x014  Sum_{k=100..255} k*histA[k]
    s32  maHistogram[KU_NUM_BINS];   // +0x018  primary histogram (256 bins)
    s32  maHistogramSecondary[KU_NUM_BINS]; // +0x418  secondary histogram
    s32  miValid;                    // +0x818  set to 1 after CalcHistogram
};

// Layout pins for the pointer-free prefix (32-bit X360 offsets hold on host).
// Wrapped in a never-called friend helper so offsetof can reach the private members.
inline void _CHistogram_AssertLayout()
{
    static_assert(offsetof(CHistogram, mfVariance)           == 0x000, "CHistogram::mfVariance");
    static_assert(offsetof(CHistogram, mfVarianceDelta)      == 0x004, "CHistogram::mfVarianceDelta");
    static_assert(offsetof(CHistogram, mfMean)               == 0x008, "CHistogram::mfMean");
    static_assert(offsetof(CHistogram, mfMeanDelta)          == 0x00C, "CHistogram::mfMeanDelta");
    static_assert(offsetof(CHistogram, mfHighMoment)         == 0x014, "CHistogram::mfHighMoment");
    static_assert(offsetof(CHistogram, maHistogram)          == 0x018, "CHistogram::maHistogram");
    static_assert(offsetof(CHistogram, maHistogramSecondary) == 0x418, "CHistogram::maHistogramSecondary");
    static_assert(offsetof(CHistogram, miValid)              == 0x818, "CHistogram::miValid");
    static_assert(sizeof(CHistogram) == 0x81C, "CHistogram size");
}
