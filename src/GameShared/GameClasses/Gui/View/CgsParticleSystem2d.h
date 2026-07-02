#ifndef CGS_PARTICLE_SYSTEM_2D_H
#define CGS_PARTICLE_SYSTEM_2D_H

#include "types.hpp"
#include "BrnCommonTypes.h"     // Vector2 / Vector4 (rw::math::vpu layout types)
#include "rw/rwcore_structs.h"  // rw::RGBA / rw::Resource / rw::IResourceAllocator

namespace renderengine { class TextureState; class BlendState; class DepthStencilState; }

// CgsGui::ParticleSystem2d - a 2D (screen-space) particle system the GUI custom
// renderers embed to drive HUD particle effects. The full type has many more members and
// methods per the DecFIGS DWARF (CgsParticleSystem2d.h) -- Construct/Prepare/Render/
// the Set* setters/etc -- but only the functions the X360 ledger attests to the
// reconstructed TUs are bodied; everything else is out of scope and intentionally omitted.
//
// Reconstructed surface from BURNOUT_X360_ARTIST.XEX:
//   CgsGui::ParticleSystem2d::ParticleSystem2d @ 0x827DE758  (default ctor)
//   CgsGui::ParticleSystem2d::GetRGBA          @ 0x824507B8
//   CgsGui::ParticleSystem2d::Release          @ 0x8284F6B8  (decfigs TU
//     GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsParticleSystem2d.cpp)
//
// LAYOUT POLICY (matches the sibling GUI renderers AboveCarRenderer / SatNavRenderer /
// BoostBarRenderer): the compile gate is a per-TU `cl /c` on a 64-bit host, so guest byte
// offsets are NOT load-bearing (pointers widen 4->8). Attested members are declared BY
// NAME (DWARF names, X360-asm-attested offsets); the unattested guest regions between
// them stay explicit padding, re-carved as their owning functions land. The X360 guest
// stride between arrayed instances is 0x21A0 (8608) bytes -- the x64 sizeof differs and
// is deliberately not pinned.
//
// Attested guest offsets (X360, 32-bit):
//   +0x9C   mfLastTime              ctor + Release prime it to -FLT_MAX. Names per the
//           DWARF member order: (mfSystemTimeBorn, mfLastTime, mv2LastEmitterPos, ...)
//           -- only born@0x98/last@0x9C leaves mv2LastEmitterPos (a 16-byte vpu Vector2)
//           16-aligned at 0xA0; the alternative (born@0x9C) would misplace every later
//           attested member.
//   +0xB0   miMaxParticles          (derived, unattested -- kept as padding)
//   +0xB4   miNumActiveParticles    (derived, unattested -- kept as padding)
//   +0xB8   mabIsActive             Release frees + nulls it   (DWARF h:204 bool*)
//   +0xBC   maParticles             Release frees + nulls it   (DWARF h:205 ParticleInfo*)
//   +0xC0   maBillboards            Release frees + nulls it   (DWARF h:206 BillboardInfo*)
//   +0x2150 mpAllocator             ctor zeroes; Release asserts it (DWARF h:215)
//   +0x2154 mTextureStateResource   ctor zero-fills 5 dwords == the X360 20-byte
//           (BaseResources<5>) rw::Resource form  (DWARF h:218)
//   +0x2168 mpTextureState          NOT written by the ctor (the "gap" dword) (h:219)
//   +0x216C mBlendStateResource     ctor zero-fills (h:221)
//   +0x2180 mpBlendState            gap, not written (h:222)
//   +0x2184 mDepthStencilStateResource  ctor zero-fills (h:224)
//   +0x2198 mpDepthStencilState     gap, not written (h:225); +0x219C tail pad -> 0x21A0
namespace CgsGui
{
    struct BillboardInfo;  // real home: CustomRenderer/CgsGuiBillboardInfo.h (pointer-only here)

    // CgsGui::ParticleInfo (DWARF CgsParticleSystem2d.h:39) -- one live particle's record
    // in the maParticles array. This header is its DWARF home.
    struct ParticleInfo
    {
        Vector2 mv2StartPos;   // h:40
        Vector2 mv2StartVel;   // h:41
        f32     mfStartTime;   // h:42
        f32     mfLifetime;    // h:43
        f32     mfSpin;        // h:44
    };

    class ParticleSystem2d
    {
    public:
        // 0x827DE758 -- default constructor. Body links from the ParticleSystem2d TU.
        ParticleSystem2d();

        // 0x8284F6B8 -- free the three per-particle arrays back through mpAllocator and
        // mark the system unprepared (mfLastTime = -FLT_MAX).
        void Release();

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
        // guest +0x00 .. +0x9B: emitter/particle tunables + mfSystemTimeBorn (see the
        // offset table above); unattested by the reconstructed bodies, kept as padding.
        u8 maPad0[156];
        f32 mfLastTime;         // guest +0x9C; ctor + Release write -FLT_MAX (DWARF h:199)

        // guest +0xA0 .. +0xB7: mv2LastEmitterPos / miMaxParticles / miNumActiveParticles
        // per the DWARF order -- none written by the reconstructed bodies, kept as padding.
        u8 maPad1[24];

        // The three per-particle arrays Release frees back to mpAllocator (guest
        // +0xB8/+0xBC/+0xC0; DWARF h:204-206).
        bool*          mabIsActive;
        ParticleInfo*  maParticles;
        BillboardInfo* maBillboards;

        // guest +0xC4 .. +0x20DF: the bulk of the object (mBillboardRenderer by value,
        // frame counts, mRandom, ...) -- untouched by the reconstructed bodies.
        u8 maPad2[8220];

        // guest +0x20E0 .. +0x2108: two zero-filled 5-dword groups with a 1-dword gap at
        // +0x20F4, matching the ctor's do-while unroll (same gap-aware pattern as
        // BrnGui::BoostBarRenderer's / BrnGui::MainMapRenderer's maZeroGroups). This region
        // sits inside the still-unattributed mBillboardRenderer/mRandom span, so the slots
        // keep neutral names until their owning functions land.
        u32 maZeroGroupA[5];    // guest +0x20E0..+0x20F0
        u32 muGapA;             // guest +0x20F4 -- never written by the ctor
        u32 maZeroGroupB[5];    // guest +0x20F8..+0x2108

        u8 maPad3[68];          // guest +0x210C .. +0x214F (untouched)

        // The render-state tail (guest +0x2150 .. +0x219F; DWARF h:215-225). The ctor
        // zero-fills mpAllocator and the three Resources (on X360 each Resource is the
        // 5-dword BaseResources<5> form == the ctor's 5-dword unrolled clears); the three
        // state POINTERS between them are the clear pattern's untouched gaps -- the guest
        // ctor leaves them uninitialised, and so does ours.
        rw::IResourceAllocator* mpAllocator;                         // guest +0x2150
        rw::Resource mTextureStateResource;                          // guest +0x2154
        const renderengine::TextureState* mpTextureState;            // guest +0x2168 (gap)
        rw::Resource mBlendStateResource;                            // guest +0x216C
        const renderengine::BlendState* mpBlendState;                // guest +0x2180 (gap)
        rw::Resource mDepthStencilStateResource;                     // guest +0x2184
        const renderengine::DepthStencilState* mpDepthStencilState;  // guest +0x2198 (gap)
    };
}

#endif
