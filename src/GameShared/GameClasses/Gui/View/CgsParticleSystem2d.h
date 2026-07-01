#ifndef CGS_PARTICLE_SYSTEM_2D_H
#define CGS_PARTICLE_SYSTEM_2D_H

#include "types.hpp"
#include "BrnCommonTypes.h"     // Vector4
#include "rw/rwcore_structs.h"  // rw::RGBA

// CgsGui::ParticleSystem2d - a 2D (screen-space) particle system the GUI custom
// renderers embed to drive HUD particle effects. The full type has many more members and
// methods per the DecFIGS DWARF (CgsParticleSystem2d.h) -- Construct/Prepare/Release/
// Destruct/Render/the Set* setters/etc -- but only the two functions the X360 ledger
// attests to THIS translation unit are reconstructed here; everything else (e.g.
// ::Release, its own separate TU) is out of scope and intentionally omitted.
//
// Reconstructed surface from BURNOUT_X360_ARTIST.XEX:
//   CgsGui::ParticleSystem2d::ParticleSystem2d @ 0x827DE758  (default ctor)
//   CgsGui::ParticleSystem2d::GetRGBA          @ 0x824507B8
//
// The guest object is large (stride 0x21A0 == 8608 bytes when arrayed). Its full internal
// layout is out of scope (most members are unattested here), so it stays modelled as a
// fixed-size byte span of the attested guest size so that embedding owners (e.g.
// BrnGui::MainMapRenderer) can hold an array of instances by value and construct them --
// with the specific offsets the ctor writes promoted to named members (by-name access,
// no raw offset casts in the .cpp) and the untouched bytes kept as explicit padding.
// FLAG: partial-layout boundary type sized to the X360 stride; the remaining members are
// reconstructed when their owning functions are (this TU's own ctor/GetRGBA only).
namespace CgsGui
{
    class ParticleSystem2d
    {
    public:
        // The X360 guest stride between arrayed instances (8608 bytes).
        static const u32 KU_GUEST_SIZE = 8608;

        // 0x827DE758 -- default constructor. Body links from the ParticleSystem2d TU.
        ParticleSystem2d();

        // 0x824507B8 -- clamp the float colour to [0,1], scale to 0..255 and pack to
        // one rw::RGBA. DWARF (CgsParticleSystem2d.h:63): `RGBA GetRGBA(const
        // rw::math::vpu::Vector4&)` (a member, no explicit `static`). But the ASM proves
        // it takes NO `this`: the callee only reads r3 (hidden return-value pointer) and
        // r4 (the colour Vector4*) -- and its only caller (BoostBarRenderer::RenderComponent)
        // sets up exactly those two registers before `bl`, with no instance pointer loaded
        // anywhere. Per AGENTS.md the asm is authoritative for calling convention over the
        // DWARF declaration site, so this is reconstructed as `static` (matches the observed
        // 2-register ABI exactly; a non-static const method would need a 3rd register for
        // `this`).
        static rw::RGBA GetRGBA(const Vector4& lrColour);

    private:
        // guest +0x9C (156): a float sentinel the ctor primes to -FLT_MAX. FLAG: DWARF
        // gives PS3 32-bit member offsets only up to ~148 bytes before the first
        // unsized sub-object (BillboardRenderer); offset 156 on the X360/x64 layout falls
        // past that point, so this field cannot yet be attributed to a specific named
        // DWARF member with confidence -- modelled as an anonymous named slot instead of
        // guessing a DWARF name onto the wrong offset.
        u8 maPad0[156];
        f32 mfExtentSentinel;   // guest +0x9C, ctor writes -FLT_MAX (-3.4028235e38f)
        u8 maPad1[8256];        // guest +0xA0 .. +0x20DF (untouched by this TU's ctor)

        // guest +0x20E0 .. +0x2108 (8416..8456): two zero-filled 5-dword groups with a
        // 1-dword gap, matching the ctor's do-while unroll (same gap-aware pattern as
        // BrnGui::BoostBarRenderer's maZeroGroups / BrnGui::MainMapRenderer's maZeroGroups).
        u32 maZeroGroupA[5];    // guest +0x20E0..+0x20F0 (8416,8420,8424,8428,8432)
        u32 muGapA;             // guest +0x20F4 (8436) -- never written by the ctor
        u32 maZeroGroupB[5];    // guest +0x20F8..+0x2108 (8440,8444,8448,8452,8456)

        u8 maPad2[68];          // guest +0x210C .. +0x2150 (8460..8527, untouched)

        u32 muZeroTrailing;     // guest +0x2150 (8528)
        u32 maZeroGroupC[5];    // guest +0x2154..+0x2164 (8532,8536,8540,8544,8548)
        u32 muGapB;             // guest +0x2168 (8552) -- never written
        u32 maZeroGroupD[5];    // guest +0x216C..+0x217C (8556,8560,8564,8568,8572)
        u32 muGapC;             // guest +0x2180 (8576) -- never written
        u32 maZeroGroupE[5];    // guest +0x2184..+0x2194 (8580,8584,8588,8592,8596)

        u8 maPad3[8];           // guest +0x2198 .. +0x21A0 (8600..8607, untouched)
    };

    static_assert(sizeof(ParticleSystem2d) == ParticleSystem2d::KU_GUEST_SIZE,
                  "CgsGui::ParticleSystem2d must stay sized to the attested X360 guest "
                  "stride so array-embedding owners (e.g. BrnGui::MainMapRenderer) match "
                  "the guest layout.");
}

#endif
