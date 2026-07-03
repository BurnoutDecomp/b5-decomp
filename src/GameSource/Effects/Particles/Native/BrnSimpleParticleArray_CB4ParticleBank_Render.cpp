// ============================================================================
// GameSource/Effects/Particles/Native/BrnSimpleParticleArray_CB4ParticleBank_Render.cpp
//
// Reconstructed-home (HONEST STUB) for BURNOUT_X360_ARTIST.XEX
//   BrnParticle::Native::BrnSimpleParticleArray::CB4ParticleBank::Render @ 0x8291E600
//
// VMX KEYSTONE -- DELIBERATELY NOT BODIED (mirrors the committed
// ShadedRotatingRenderMethod::BuildQuad stub precedent).
//
// Render walks this bank's live CB4 particles four-at-a-time and, for each particle that
// survives the near/far clip+fade test, builds one shaded, rotated, screen-space quad into
// the locked vertex buffer (via the iterator) using ShadedRotatingRenderMethod::BuildQuad.
// It first builds a normalised tile-UV lookup table (rows x cols, <= 16 tiles) into a
// 512-byte stack scratch, then streams per-corner UVs from it.
//
// The X360 body @0x8291E600 is a ~1000-instruction hand-vectorised VMX/AltiVec pipeline
// (Hex-Rays reports "local variable allocation has failed, the output may be wrong!"):
// lvx128 strided gathers from rodata vector-constant tables (@0x82CDA350 / 0x82CDADB0.. /
// 0x8327F110 / 0x8307xxxx), a vrefp/vnmsubfp128 Newton-Raphson reciprocal, vperm/vsldoi/
// vrlimi128 lane shuffles assembling the four rotated quad corners, vmaddfp polynomial
// clip/fade evaluation, vmulfp/vmaxfp/vminfp colour clamp, and vctuxs->byte packing of the
// RGBA dwords. This does NOT lower faithfully to scalar C++ without inventing a per-lane
// formula, so per project policy the keystone is FLAGGED and given an honest stub. A
// fabricated per-lane formula would be guessed, not X360 fact.
//
// Two assert strings the scalar prologue/epilogue carry (X360 rodata,
// BrnSimpleParticleRenderer.cpp), recorded for the eventual VMX-decode pass:
//   CGS_ASSERT(luNumFreeVertices >= (muNumParticles * 4),
//              "luNumFreeVertices >= ( muNumParticles * 4 )");   // src line 487
//   CGS_ASSERT(luNumberOfTiles <= 16, "luNumberOfTiles <= 16");  // src line 608
//
// SIGNATURE (X360 register contract at the caller @0x8291F948-58): this = the CB4 bank
// (r3 = r31+4); lpIterator = r4 (var_210); lpArray = r5 (r31, the owning
// BrnSimpleParticleArray); lpCamera = r6 (var_200, the local Camera copy). The 2-arg
// signature that DROPPED the camera (r6) argument was corrected to this 3-arg form.
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

namespace BrnParticle
{
namespace Native
{
    void BrnSimpleParticleArray::CB4ParticleBank::Render(void* /*lpIterator*/,
                                                         void* /*lpArray*/,
                                                         void* /*lpCamera*/)
    {
        // HONEST FLOOR: the VMX quad-build pipeline (@0x8291E600) is not reconstructed in
        // this pass. Do not fabricate output vertices.
        CGS_ASSERT(false, "BrnParticle::Native::BrnSimpleParticleArray::CB4ParticleBank::Render: VMX pipeline not yet reconstructed");
    }
}
}
