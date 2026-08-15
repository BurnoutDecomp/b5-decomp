#ifndef BRN_POST_FX_BLOOM_DATA_H
#define BRN_POST_FX_BLOOM_DATA_H

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector4

// ==================================================================================================
// THIS FILE IS THE SOLE HOME OF BrnPostFxBloomData. NO OTHER HEADER MAY DEFINE IT.
//
// The DecFIGS DWARF gives the type its own translation-unit-level header at exactly this path --
// references/DecFIGS/dwarfdump/GameSource/Graphics/PostFx/BrnPostFxBloomData.h:31-35 -- so this is
// the original's own home, not a convenience split. Two files in this wave want the type:
//   * GameSource/Graphics/PostFx/BrnPostFx.h        -- owns one by value (BrnPostFx.h:249)
//   * GameSource/Graphics/PostFx/BrnPostFxShader.h  -- takes one by const& (BrnPostFxShader.h:113)
// BOTH must `#include "GameSource/Graphics/PostFx/BrnPostFxBloomData.h"`. A second definition in
// BrnPostFxShader.h is a hard C2011 the moment one TU sees both headers, which BrnPostFx.cpp does --
// and it cascades into C2079 on BrnPostFx::mBloomData, C2660 on BrnPostFxBloom::Render and C2664 on
// BrnPostFxShader::Render. This header is self-contained and include-guarded precisely so that both
// includers can name it without ordering constraints.
// ==================================================================================================
//
// BrnPostFxBloomData -- the per-frame bloom CONSTANT block (not the bloom effect object, which is
// BrnPostFxBloom). BrnPostFx owns one by value and hands it to BrnPostFxShader::Render, which turns
// it into the composite's `BloomColour` shader constant; BrnPostFx::PrepareDownSampleBuffers reads
// mfThreshold out of it and hands it to BrnPostFxBloom::Render.
//
// SHAPE authoritative from the DecFIGS DWARF (the path above):
//     Vector4   mColour;
//     float32_t mfThreshold;
//     float32_t mfLuminance;
// It is a `struct` there, and it is a `struct` here -- declaring it `class` elsewhere is a C4099 in
// addition to the redefinition.
//
// THE THREE MEMBERS ARE CONFIRMED BY THREE INDEPENDENT X360 SITES, all reaching this object at the
// BrnPostFx member the driver passes as `this + 0x950`:
//   * BrnRendererModule::Render @0x8240BFA8 writes all three each frame --
//       `_R11 = 2384; stvx128 v0, r20, r11`      -> 2384 == 0x950 == mColour
//       `_R20[600] = v312 * gfBloomThresholdScale;`  600*4 == 0x960 == mfThreshold
//       `_R20[601] = v311 * gfBloomLuminanceScale;`  601*4 == 0x964 == mfLuminance
//   * BrnPostFx::PrepareDownSampleBuffers @0x82408E88 reads `lfs f1, 0x960(r31)` (== mfThreshold)
//     and passes it to BrnPostFxBloom::Render as its threshold argument.
//   * BrnPostFxShader::Render @0x82408F08 reads `lvx128 v0, r0, r5` (mColour) and
//     `lfs f13, 0x14(r5)` (0x950 + 0x14 == 0x964 == mfLuminance) to build BloomColour.
//
// The X360 offsets above are the guest 4-byte-pointer image and are DOCUMENTATION ONLY: this type
// holds no pointers, so nothing here widens, but every reader still reaches it by name.
struct BrnPostFxBloomData
{
    rw::math::vpu::Vector4 mColour;      // BrnPostFxBloomData.h:33  (X360 +0x00)
    f32                    mfThreshold;  // BrnPostFxBloomData.h:34  (X360 +0x10)
    f32                    mfLuminance;  // BrnPostFxBloomData.h:35  (X360 +0x14)
};

#endif // BRN_POST_FX_BLOOM_DATA_H
