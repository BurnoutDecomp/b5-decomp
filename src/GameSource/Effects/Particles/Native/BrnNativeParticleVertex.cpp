// ============================================================================
// GameSource/Effects/Particles/Native/BrnNativeParticleVertex.cpp
//
// BrnParticle::NativeParticleVertex::VertexIterator::Write @ 0x8291E478
//
// Writes one native particle vertex into the iterator's current cursor. Store-for-
// store off the X360 asm: the vertex slot spans cursor+4 .. cursor+24 --
//   pos.x @ cur+4, pos.y @ cur+8, pos.z @ cur+12 (stvewx of splatted v1 lanes 0/1/2),
//   colour @ cur+16, uv.u @ cur+20, uv.v @ cur+24,
// and mpCurrentAddress (base +0x00) is advanced +12, then +4, +4, +4 as the asm does.
//
// The concrete engine iterator is renderengine::VertexIterator3<Float4,PS3Color,
// Float4> (Write<Vector4,RGBA8,Vector4>). It is modelled here as a NativeParticle-
// Vertex-owned VertexIterator : VertexIteratorBaseClass, mirroring the committed
// BrnGraphics::SkidVertex::VertexIterator sibling.
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnNativeParticleVertex.h"
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

namespace BrnParticle {

// X360 @ 0x8291E478. Called from BrnSimpleParticleArray::CB4ParticleBank::Render
// and SparkArray::RenderBank.
//
// lv3Position -- vertex position (arrives in vector register v1 on X360)
// lpColour    -- packed colour word (a2, r27)
// lpUv        -- 2-float texture coord (a3, r30)
void NativeParticleVertex::VertexIterator::Write(
        const rw::math::vpu::Vector4& lv3Position,
        const int* lpColour,
        const float* lpUv)
{
    CGS_ASSERT( GetStride() == NativeParticleVertex::GetStride(),
                "GetStride() == NativeParticleVertex::GetStride()" );
    CGS_ASSERT( GetVerticesFree() != 0, "GetVerticesFree() != 0" );

    // Position xyz -> cursor+4/+8/+12 (asm: stvewx of splatted lanes 0/1/2; cursor
    // itself is NOT advanced by these stores).
    u8* lpCur = mpCurrentAddress;
    reinterpret_cast<float*>(lpCur + 4)[0] = lv3Position.x;
    reinterpret_cast<float*>(lpCur + 4)[1] = lv3Position.y; // +8
    reinterpret_cast<float*>(lpCur + 4)[2] = lv3Position.z; // +12

    // Colour: asm computes cur+12 and cur+16, advances current to +12 then +16, and
    // stores the colour word at (cur+12)+4 == cur+16.
    lpCur = mpCurrentAddress + 0xC;      // r11 = current + 12
    mpCurrentAddress = lpCur + 4;        // current := current + 16
    *reinterpret_cast<int*>(lpCur + 4) = *lpColour; // colour @ current+16

    // uv.u: current += 4, store.
    lpCur = mpCurrentAddress;
    mpCurrentAddress = lpCur + 4;
    *reinterpret_cast<float*>(lpCur) = lpUv[0];

    // uv.v: current += 4, store.
    lpCur = mpCurrentAddress;
    mpCurrentAddress = lpCur + 4;
    *reinterpret_cast<float*>(lpCur) = lpUv[1];
}

} // namespace BrnParticle
