#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnLionBlendVertex.h
//
// BrnGraphics::LionBlendVertex -- the 36-byte Lion blend particle vertex
// (position Vector4 + packed RGBA8 colour + uv Vector4) and its writing iterator.
//
// DWARF AUTHORITY (DecFIGS dumps -- the dossier's 'DWARF none found' is a FALSE
// NEGATIVE; the real dumps exist at
//   references/DecFIGS/dwarfdump/GameSource/Effects/Particles/Native/BrnLionBlendVertex.h
//   references/DecFIGS/dwarfdump/GameSource/Effects/Particles/EffectsVertexBuffer.h):
//     BrnLionBlendVertex.h:49  struct LionBlendVertex {
//                                  void FillVertexDescriptorParameters(...);   // :61
//                                  uint32_t GetStride();                        // :92 (== 36)
//                                  struct VertexIterator : EffectsVertexBufferIterator { // :101
//                                      void Write(Vector4, const RGBA8&, Vector4); // :114
//                                  }
//                               }
//     EffectsVertexBuffer.h:20/66  struct EffectsVertexBufferIterator : public
//                                  VertexIteratorBaseClass { uint32_t GetVerticesFree();
//                                  uint32_t GetStride() const; ... }
//   The concrete engine iterator is renderengine::VertexIterator3<VertexTypeFloat4,
//   VertexTypePS3Color(RGBA8), VertexTypeFloat4> (Write<Vector4,RGBA8,Vector4>).
//   VertexIteratorBaseClass is the committed shared base (BrnNativeParticleVertex.h);
//   only the surface Write touches (GetStride / GetVerticesFree) is modelled here.
//   Mirrors the committed BrnParticle::NativeParticleVertex::VertexIterator sibling.
//
// STRIDE = 36 (0x24): asserted by Write @ 0x8227E568 (cmplwi r11, 0x24), and equals
//   sizeof(Vector4) + sizeof(RGBA8) + sizeof(Vector4) = 16 + 4 + 16.
//
// Write's asm uses the VertexIteratorBaseClass base fields: current@+0x00 (lwz/subf
// 0(r30)), top@+0x08 (lwz 8(r30)), stride@+0x0C (lwz 0xC(r30)==0x24). X360 pointers
// are 32-bit and widen on the 64-bit host, so absolute offsets are NOT assertable;
// members are pinned BY NAME/ORDER. GROW additively.
//
// FLAG: EffectsVertexBufferIterator's canonical home is EffectsVertexBuffer.h (DWARF);
// it is not committed yet, so a minimal compilable stand-in is defined here. Committed
// users (LionParticleRender.h, BrnLionBlendRenderer.h, ParticleRender.h) only forward-
// declare / reference it, so this is the sole definition -- no ODR clash today.
// ============================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h" // rw::math::vpu::Vector4
// VertexIteratorBaseClass (cursor/base/top/stride bookkeeping) -- committed home is the
// NativeParticle sibling header.
#include "GameSource/Effects/Particles/Native/BrnNativeParticleVertex.h"
// renderengine::VertexDescriptor::Parameters is named (by reference) in the declaration
// of FillVertexDescriptorParameters below, so the full descriptor type must be visible.
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "GameSource/Effects/Particles/EffectsVertexBuffer.h"  // EffectsVertexBufferIterator -- the ONE definition

namespace renderengine { struct RGBA8; }

// EffectsVertexBufferIterator comes from its committed home, NOT from a local stand-in.
// This header used to define a second one (`: public VertexIteratorBaseClass`, no members)
// labelled "additive stand-in until EffectsVertexBuffer.h is committed". That header is
// committed, and its definition is deliberately a FLAT four-member struct -- it does not
// inherit VertexIteratorBaseClass, because the committed stand-in for that base already
// absorbs the same four members and inheriting would double them and break the attested
// offsets (current @+0x00, start @+0x04, top @+0x08, stride @+0x0C). Two structurally
// different types under one name is an ODR violation the linker resolves in silence; it
// only surfaced when ParticleModule.h began including the Lion blend renderer and finally
// put both definitions in one TU.

namespace BrnGraphics {

// BrnLionBlendVertex.h:49 (DWARF).
struct LionBlendVertex
{
    // BrnLionBlendVertex.h:61 -- describe the vertex layout to the render engine.
    // Declaration-only (mirrors committed CgsPositionOnlyVertex.h); the nested
    // renderengine::VertexDescriptor::Parameters is resolved at the definition site.
    void FillVertexDescriptorParameters(renderengine::VertexDescriptor::Parameters& arParameters);

    // BrnLionBlendVertex.h:92 -- on-disk vertex stride (asserted 36 in Write).
    static u32 GetStride() { return 36u; }

    // BrnLionBlendVertex.h:101 -- concrete VertexIterator3<Float4,PS3Color,Float4> writer.
    struct VertexIterator : public EffectsVertexBufferIterator
    {
        // X360 @ 0x8227E568. Called from LionBlendRenderer QuadDraw. Writes one 36-byte
        // vertex (position Vector4 @ cur+0, colour RGBA8 @ cur+16, uv Vector4 @ cur+20)
        // and advances mpCurrentAddress by the stride. The body delegates to the generic
        // renderengine::VertexIterator3::Write; only the two GetStride/GetVerticesFree
        // asserts are attested inline in the X360 asm.
        void Write(rw::math::vpu::Vector4 lv4Position,
                   const renderengine::RGBA8& lrColour,
                   rw::math::vpu::Vector4 lv4Uv);
    };
};

} // namespace BrnGraphics
