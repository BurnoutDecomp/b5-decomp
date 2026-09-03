#pragma once

// ============================================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/RenderedParticle.h
//
// RenderedParticle -- the per-particle RENDER record: what the simulation produces and what
// the draw path consumes. It sat in the tree as `struct RenderedParticle;` (a bare forward
// declaration, marked "opaque" in ParticleRender.h) while being the OUTPUT type of
// cParticleEmitter::ParticleBuild @0x82910118 and the INPUT type of all three
// LionBlendRenderer draw halves. Nothing on either side could be written by name until it
// had a layout; this is that layout.
//
// LAYOUT AUTHORITY: the DecFIGS DWARF (RenderedParticle.h:50-58) declares seven members --
// six Vector3Plus and one Vector2, in this order. 7 * 16 == 112 == 0x70, which is EXACTLY the
// array stride LionBlendRenderer::RenderSprites @0x82282608 walks the particle run with
// (asm word 327, 0x82282B08 `addi r27, r27, 0x70`). The member list and the measured stride
// are two independent derivations of the same number.
//
// EVERY MEMBER IS WITNESSED BY AN INSTRUCTION, none is placed to make the arithmetic close:
//   +0x00  mPos                     ParticleBuild tail `lvx128/stvx128 v, r0, r28`
//                                   (0x8291120C..0x82911228) and RenderSprites word 251
//                                   (0x822829D8 `addi r5, r27, -0x40` == &particle->mPos,
//                                   handed to the vertex writer)
//   +0x10  mPos1                    ParticleBuild `addi r11, r28, 0x10` -- six separate sites
//                                   (0x8291048C, 0x82910ED0, 0x82910FDC, 0x8291106C,
//                                    0x829110FC, 0x82911170, 0x82911230)
//   +0x20  mLocatorVel              ParticleBuild word 92 (0x8291026C `addi r11, r28, 0x20`)
//   +0x30  mvRotPlusFrame           ParticleBuild tail 0x829112B0 `addi r11, r28, 0x30`;
//                                   RenderSprites word 80 (0x8228272C `addi r11, r27, -0x10`)
//   +0x40  mvSizePlusNextFrame      ParticleBuild tail 0x829112B4 `addi r10, r28, 0x40`;
//                                   RenderSprites word 84 (0x8228273C `lvx128 v13, r0, r27`,
//                                   r27 == apParticle + 0x40)
//   +0x50  mvColour                 BaseColourWithVarianceBehaviour::Process @0x8290D720
//                                   `stvx128 v0, r29, r10` with r10 == 0x50 (twice), and
//                                   AlphaFadeBehaviour::Process @0x8290D7D8 0x8290D8C4
//                                   `addi r11, r30, 0x50` (the alpha lane)
//   +0x60  mvTimeScaleAndLifeScale  AlphaFadeBehaviour::Process 0x8290D800
//                                   `addi r11, r30, 0x60`; ParticleBuild 0x829103C8
//                                   `lfs f2, 0x64(r28)` reads its .y lane
//   ============ sizeof == 0x70 (112), 16-aligned, ZERO slack ============
//
// ⭐ THE "PLUS" IN Vector3Plus IS THE W LANE, AND THE ASM SAYS SO. ParticleBuild's tail sets
// exactly the w lane of four of these with `vrlimi128 vD, vS, 1, 0` -- mask 1 selects word 3
// -- at +0x00, +0x10 (0x82911214/0x82911224/0x82911234/0x82911250) and, when the multi-frame
// behaviour is OFF, at +0x30 and +0x40 (0x829112BC/0x829112C8, inserting the zero vector).
// When it is ON, MultiFrameBehaviour::Process @0x8290FC48 writes those two w lanes instead --
// which is precisely what the DWARF names say they hold: mvRotPlusFrame == rotation.xyz plus
// the current frame, mvSizePlusNextFrame == size.xyz plus the next frame. The names and the
// branch agree without anyone choosing them to.
//
// This record contains NO pointers -- only 16-byte lane registers -- so its console offsets
// ARE its host offsets, and they are pinned by static_assert at the foot of this file (same
// rule and same reason as sParticleNucleus in sParticle.h).
// ============================================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h"   // cVector -- the lane type

#include <cstddef>   // offsetof

// DWARF RenderedParticle.h:50-58. The DWARF types six members `Vector3Plus` and the last
// `Vector2`; all seven are the same 16-byte, 16-aligned lane register the rest of the Lion
// runtime spells `cVector`, and every access in the asm is a whole-register load/store (or a
// single `lfs` on a named lane), so the one Lion vector home is used throughout rather than
// pulling a second vector vocabulary into this header. Same call sParticle.h made.
struct RenderedParticle
{
    cVector mPos;                     // +0x00  RenderedParticle.h:50
    cVector mPos1;                    // +0x10  RenderedParticle.h:51
    cVector mLocatorVel;              // +0x20  RenderedParticle.h:52
    cVector mvRotPlusFrame;           // +0x30  RenderedParticle.h:53
    cVector mvSizePlusNextFrame;      // +0x40  RenderedParticle.h:54
    cVector mvColour;                 // +0x50  RenderedParticle.h:56
    cVector mvTimeScaleAndLifeScale;  // +0x60  RenderedParticle.h:58 (DWARF Vector2 -- only
                                      //        the x/y lanes are ever named; see below)

    // ---- named lanes, so no body has to index a packed vector by number ---------------------
    // The three packed members carry a scalar in a lane the member name spells out.
    f32  Frame()      const { return mvRotPlusFrame.w; }
    f32  NextFrame()  const { return mvSizePlusNextFrame.w; }
    f32  TimeScale()  const { return mvTimeScaleAndLifeScale.x; }
    // ParticleBuild reads this one directly: `lfs f2, 0x64(r28)` @0x829103C8 == +0x60 lane 1.
    f32  LifeScale()  const { return mvTimeScaleAndLifeScale.y; }
};

// The array stride LionBlendRenderer::RenderSprites @0x82282608 walks the particle run with
// (`addi r27, r27, 0x70`), and the sum of the DWARF's seven 16-byte members.
static_assert(sizeof(RenderedParticle) == 0x70,
              "RenderedParticle is the 112-byte render record (RenderSprites @0x82282608 "
              "asm word 327: addi r27, r27, 0x70)");
static_assert(alignof(RenderedParticle) == 16,
              "every member is a VMX lane register (lvx128/stvx128 throughout)");

// Each offset below names the instruction that forms it. Break one and the gate fails here
// rather than the game drawing a particle with its size in the colour slot.
static_assert(offsetof(RenderedParticle, mPos)        == 0x00,
              "RenderSprites 0x822829D8 addi r5, r27, -0x40");
static_assert(offsetof(RenderedParticle, mPos1)       == 0x10,
              "ParticleBuild 0x82911230 addi r11, r28, 0x10");
static_assert(offsetof(RenderedParticle, mLocatorVel) == 0x20,
              "ParticleBuild 0x8291026C addi r11, r28, 0x20");
static_assert(offsetof(RenderedParticle, mvRotPlusFrame) == 0x30,
              "ParticleBuild 0x829112B0 addi r11, r28, 0x30");
static_assert(offsetof(RenderedParticle, mvSizePlusNextFrame) == 0x40,
              "ParticleBuild 0x829112B4 addi r10, r28, 0x40");
static_assert(offsetof(RenderedParticle, mvColour) == 0x50,
              "BaseColourWithVarianceBehaviour::Process 0x8290D774 stvx128 v0, r29, r10 (r10==0x50)");
static_assert(offsetof(RenderedParticle, mvTimeScaleAndLifeScale) == 0x60,
              "AlphaFadeBehaviour::Process 0x8290D800 addi r11, r30, 0x60");
