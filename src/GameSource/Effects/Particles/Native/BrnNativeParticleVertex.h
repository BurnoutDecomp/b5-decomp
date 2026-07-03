#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnNativeParticleVertex.h
//
// BrnParticle::NativeParticleVertex -- the 24-byte native particle vertex
// (position float3 + packed colour u32 + uv float2) and its writing iterator.
//
// DWARF AUTHORITY (DecFIGS dumps -- the dossier's 'DWARF none found' is a FALSE
// NEGATIVE; the real dumps exist):
//   BrnNativeParticleVertex.h:45  struct NativeParticleVertex {
//                                     VertexDescriptor* mpVertexDescriptor; // :100
//                                     void Construct(rw::IResourceAllocator*); // :62
//                                     VertexDescriptor* GetVertexDescriptor(); // :65
//                                     uint32_t GetStride();                    // :69 (== 24)
//                                  }
//     NOTE: DWARF shows NO nested VertexIterator MEMBER of NativeParticleVertex. The
//     concrete iterator is renderengine::VertexIterator3<VertexTypeFloat4,
//     VertexTypePS3Color,VertexTypeFloat4> (Write<Vector4,RGBA8,Vector4>,
//     cmn/renderengine/vertexiterator.h:83). It is modelled below as a NativeParticle-
//     Vertex-owned VertexIterator : VertexIteratorBaseClass -- a compilable stand-in
//     mirroring the committed BrnGraphics::SkidVertex::VertexIterator sibling
//     (BrnSkidVertex.h:64). FLAG: stand-in shape, not a DWARF member.
//
// VertexIteratorBaseClass layout is attested by VertexIterator::Write @ 0x8291E478:
//   *this   (+0x00) mpCurrentAddress  -- write cursor (lwz/stw 0(r31) throughout)
//   this[2] (+0x08) mpTopAddress      -- GetVerticesFree: (top - current)
//   this[3] (+0x0C) muStride          -- GetStride reads *(this+0xC), asserted == 24
// +0x04 is untouched by Write (the base address; EffectsVertexBufferIterator names it
// mpStartAddress, EffectsVertexBuffer.h:153). X360 pointers are 32-bit and widen on the
// 64-bit host, so absolute offsets are NOT assertable; members pinned BY NAME/ORDER.
// GROW additively.
// ============================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h" // rw::math::vpu::Vector4

namespace renderengine { class VertexDescriptor; }
namespace rw { class IResourceAllocator; }

// The shared vertex-iterator base (EATech cmn/renderengine/vertexiterator.h). Only the
// cursor/top/stride bookkeeping touched by the particle vertex writers is modelled.
struct VertexIteratorBaseClass
{
    u8* mpCurrentAddress; // +0x00 -- write cursor, advanced per element
    u8* mpBaseAddress;    // +0x04 -- batch base address (untouched by Write)
    u8* mpTopAddress;     // +0x08 -- one-past-end of the batch
    u32 muStride;         // +0x0C -- vertex stride in bytes

    u32 GetStride() const { return muStride; }

    // GetVerticesFree() (inlined into Write): (top - current) / stride.
    u32 GetVerticesFree() const
    {
        return static_cast<u32>(mpTopAddress - mpCurrentAddress) / muStride;
    }
};

namespace BrnParticle {

// BrnNativeParticleVertex.h:45 (DWARF).
struct NativeParticleVertex
{
    renderengine::VertexDescriptor* mpVertexDescriptor; // BrnNativeParticleVertex.h:100

    void Construct(rw::IResourceAllocator* lpAllocator);      // :62
    renderengine::VertexDescriptor* GetVertexDescriptor();    // :65

    // BrnNativeParticleVertex.h:69 -- the on-disk vertex stride (asserted 24 in Write).
    static u32 GetStride() { return 24u; }

    // Stand-in for renderengine::VertexIterator3<Float4,PS3Color,Float4>; mirrors the
    // committed BrnGraphics::SkidVertex::VertexIterator (BrnSkidVertex.h:64). FLAG.
    struct VertexIterator : public VertexIteratorBaseClass
    {
        // X360 @ 0x8291E478. Writes one 24-byte vertex: pos.x/y/z @ cur+4/+8/+12,
        // colour @ cur+16, uv.u @ cur+20, uv.v @ cur+24 (store-for-store off the asm).
        void Write(const rw::math::vpu::Vector4& lv3Position,
                   const int* lpColour,
                   const float* lpUv);

        u32 GetStride() { return VertexIteratorBaseClass::GetStride(); }
    };
};

} // namespace BrnParticle
