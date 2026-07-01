#include "GameShared/GameClasses/Gui/View/CgsParticleSystem2d.h"

#include "rw/math/vpu/vector4_operation.h"  // Clamp/Max/Min/operator*/GetVector4_One

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsGui::ParticleSystem2d::ParticleSystem2d @ 0x827DE758  (default ctor; EXECUTED in the boot trace)
//   CgsGui::ParticleSystem2d::GetRGBA          @ 0x824507B8
//
// Only these two functions are in scope for this TU (X360 ledger). The default ctor
// primes one float sentinel to -FLT_MAX and zero-fills five small dword groups (a
// compiler clear-loop unroll, gap-aware like the sibling BrnGui::BoostBarRenderer /
// BrnGui::MainMapRenderer constructors); the rest of the large guest object is untouched
// by this function and stays as explicit padding (see the header FLAG). GetRGBA clamps a
// float RGBA colour to [0,1], scales by 255 and packs it to one rw::RGBA -- the same
// clamp-then-scale-then-truncate pattern as BrnGui::ProgressBarRenderer::RenderQuadUntex's
// PackChannel, confirmed against rw::RGBA::RGBA's byte-order ((a<<24)|(b<<16)|(g<<8)|r).

namespace CgsGui
{
namespace
{
    // -FLT_MAX, as written by the ctor into the offset-0x160 sentinel.
    const f32 KF_EXTENT_SENTINEL = -3.4028235e38f;

    // GetRGBA's clamp-to-[0,1]-then-scale-by-255 constant. DWARF (CgsParticleSystem2d.h:194):
    // `extern const VecFloat KF_COLOURSCALE;` -- the .rdata vector constant the ARTIST asm
    // loads as unk_8305A950 and multiplies the clamped colour by (vmulfp128). Matches the
    // identical pattern already named KF_COLOUR_SCALE in BrnProgressBarRenderer.cpp.
    const f32 KF_COLOURSCALE = 255.0f;

    // Pack one already-clamped-and-scaled (0..255) float channel to a u8 via truncation,
    // matching the X360 fctidz (round-toward-zero float->int) + low-byte extract.
    inline u8 PackChannel(f32 lfScaled0To255)
    {
        return static_cast<u8>(static_cast<s32>(lfScaled0To255));
    }
}

// 0x827DE758 -- prime the offset-0x160 float sentinel to -FLT_MAX and zero-fill the five
// small dword groups the ctor's unrolled clear loops touch. Every other member of this
// (much larger) object is left as-is by this function -- faithful to the asm, which
// writes nothing else.
ParticleSystem2d::ParticleSystem2d()
{
    mfExtentSentinel = KF_EXTENT_SENTINEL;  // guest +0x160

    // guest +0x20E0..+0x20F0 (5 dwords) and +0x20F8..+0x2108 (5 dwords); the 1-dword gap
    // at +0x20F4 is never written by the guest.
    for (int i = 0; i < 5; ++i)
    {
        maZeroGroupA[i] = 0;
        maZeroGroupB[i] = 0;
    }

    // guest +0x2150 (single trailing dword), then three more 5-dword groups on the same
    // gap-aware pattern (+0x2154.., +0x216C.., +0x2184..).
    muZeroTrailing = 0;
    for (int i = 0; i < 5; ++i)
    {
        maZeroGroupC[i] = 0;
        maZeroGroupD[i] = 0;
        maZeroGroupE[i] = 0;
    }
}

// 0x824507B8 -- clamp the float colour to [0,1], scale to 0..255 and pack to one rw::RGBA.
// The asm: lvx128-load the colour, vmaxfp with 0, vminfp with 1 (clamp to [0,1]), vmulfp128
// by the KF_COLOURSCALE vector, fctidz-truncate each lane to an integer and pack the four
// low bytes as (a<<24)|(b<<16)|(g<<8)|r -- exactly rw::RGBA::RGBA's byte order.
rw::RGBA ParticleSystem2d::GetRGBA(const Vector4& lrColour)
{
    using namespace rw::math::vpu;

    const Vector4 lZero  = Vector4{ 0.0f, 0.0f, 0.0f, 0.0f };
    const Vector4 lOne   = GetVector4_One();
    const Vector4 lScaled = Clamp(lrColour, lZero, lOne) * Splat(KF_COLOURSCALE);

    return rw::RGBA(PackChannel(lScaled.x),   // r
                    PackChannel(lScaled.y),   // g
                    PackChannel(lScaled.z),   // b
                    PackChannel(lScaled.w));  // a
}
}
