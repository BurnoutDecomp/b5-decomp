#include "GameShared/GameClasses/Gui/View/CgsParticleSystem2d.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "rw/math/vpu/vector4_operation.h"  // Clamp/Max/Min/operator*/GetVector4_One

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsGui::ParticleSystem2d::ParticleSystem2d @ 0x827DE758  (default ctor; EXECUTED in the boot trace)
//   CgsGui::ParticleSystem2d::GetRGBA          @ 0x824507B8
//   CgsGui::ParticleSystem2d::Release          @ 0x8284F6B8  (ledger TU GameShared/GameClasses/
//     Gui/View/ParticleSystem2d/CgsParticleSystem2d.cpp -- bodied here in the class home file)
//
// The default ctor primes mfLastTime to -FLT_MAX, zero-fills two small dword groups inside
// the still-unattributed mBillboardRenderer/mRandom span (a compiler clear-loop unroll, gap-
// aware like the sibling BrnGui::BoostBarRenderer / BrnGui::MainMapRenderer constructors),
// nulls mpAllocator and zero-fills the three render-state rw::Resources; the rest of the
// large guest object is untouched by it and stays as explicit padding (see the header).
// GetRGBA clamps a float RGBA colour to [0,1], scales by 255 and packs it to one rw::RGBA --
// the same clamp-then-scale-then-truncate pattern as BrnGui::ProgressBarRenderer::
// RenderQuadUntex's PackChannel, confirmed against rw::RGBA::RGBA's byte-order
// ((a<<24)|(b<<16)|(g<<8)|r). Release hands the three per-particle arrays back to
// mpAllocator and re-primes mfLastTime.

namespace CgsGui
{
namespace
{
    // -FLT_MAX -- the "never updated" time sentinel the ctor and Release write into
    // mfLastTime (X360 rodata flt_82035570).
    const f32 KF_TIME_NEVER = -3.4028235e38f;

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

// 0x827DE758 -- prime the mfLastTime sentinel (guest +0x9C) to -FLT_MAX and zero-fill what
// the ctor's unrolled clear loops touch: two 5-dword groups in the unattributed span, then
// mpAllocator (guest +0x2150) and the three render-state Resources (each a 5-dword
// BaseResources<5> on the X360 == a whole rw::Resource zero-fill here). The state POINTERS
// between the Resources are the clear pattern's gaps -- the guest ctor leaves them
// uninitialised, so this ctor does too. Every other member of this (much larger) object is
// left as-is by this function -- faithful to the asm, which writes nothing else.
ParticleSystem2d::ParticleSystem2d()
{
    mfLastTime = KF_TIME_NEVER;  // guest +0x9C

    // guest +0x20E0..+0x20F0 (5 dwords) and +0x20F8..+0x2108 (5 dwords); the 1-dword gap
    // at +0x20F4 is never written by the guest.
    for (int i = 0; i < 5; ++i)
    {
        maZeroGroupA[i] = 0;
        maZeroGroupB[i] = 0;
    }

    // guest +0x2150 (mpAllocator), then the three render-state Resources on the same
    // gap-aware pattern (+0x2154.., +0x216C.., +0x2184..).
    mpAllocator = NULL;
    mTextureStateResource      = rw::Resource();
    mBlendStateResource        = rw::Resource();
    mDepthStencilStateResource = rw::Resource();
}

// 0x8284F6B8 -- release the system's dynamic storage. Asserts the allocator is still bound
// (CgsParticleSystem2d.cpp:160), then for each of the three per-particle arrays
// (mabIsActive/maParticles/maBillboards, in that order) that is non-null: wrap the raw
// allocation in a zeroed temporary rw::Resource whose first slot is the pointer (the asm
// builds each 5-dword X360 Resource on the stack -- handle dword + 4 zeroes), hand it to
// the allocator's virtual Free (X360 vtbl +0x14, the resource-block Free slot -- same slot
// the BrnCoronaManager::SetTextureAtlas site dispatches), and null the member. Finally
// re-prime mfLastTime to -FLT_MAX, marking the system unprepared.
void ParticleSystem2d::Release()
{
    CGS_ASSERT(mpAllocator != NULL, "mpAllocator!=NULL");

    if (mabIsActive)
    {
        rw::Resource lResource = rw::Resource();
        lResource.m_baseResources[0] = mabIsActive;
        mpAllocator->Free(&lResource);
        mabIsActive = NULL;
    }

    if (maParticles)
    {
        rw::Resource lResource = rw::Resource();
        lResource.m_baseResources[0] = maParticles;
        mpAllocator->Free(&lResource);
        maParticles = NULL;
    }

    if (maBillboards)
    {
        rw::Resource lResource = rw::Resource();
        lResource.m_baseResources[0] = maBillboards;
        mpAllocator->Free(&lResource);
        maBillboards = NULL;
    }

    mfLastTime = KF_TIME_NEVER;
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
