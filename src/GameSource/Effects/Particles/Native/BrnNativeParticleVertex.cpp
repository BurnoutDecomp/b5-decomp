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
// ⭐⭐ FIXED 2026-09-06 (spark wave). THE TWO UV FLOATS WERE WRITTEN FOUR BYTES EARLY --
// uv.u landed on the colour word and uv.v on uv.u's slot, leaving the vertex's last float
// never written at all. This is the same one-word cursor defect the boost-exhaust wave
// found in LionBlendVertex::Write (BrnLionBlendVertex.cpp), in the other direction, and it
// was latent only because nothing called this function yet: its two console callers are
// SparkArray::RenderBank and BrnSimpleParticleArray::CB4ParticleBank::Render, neither of
// which had a body.
//
// The asm settles it. Let C be mpCurrentAddress on entry:
//     stvewx v0,  C, 4        *(float*)(C+0x04) = pos.x
//     stvewx v13, C, 8        *(float*)(C+0x08) = pos.y
//     stvewx v12, C, 0xC      *(float*)(C+0x0C) = pos.z
//     r11 = C + 0xC ; r10 = r11 + 4 ; stw r11,0(this) ; stw r10,0(this)
//     stw    r9, 4(r11)       *(u32*)(C+0x10)   = colour        (cursor now C+0x10)
//     lwz r11,0(this) ; addi r11,r11,4 ; stw r11,0(this)
//     stfs   f0, 0(r11)       *(float*)(C+0x14) = uv.u          <- at the NEW cursor
//     lwz r11,0(this) ; addi r11,r11,4 ; stw r11,0(this)
//     stfs   f0, 0(r11)       *(float*)(C+0x18) = uv.v          <- at the NEW cursor
// i.e. each UV store is at the cursor AFTER the increment, and the vertex occupies
// [C+4, C+0x1B] -- 24 bytes, the attested stride -- with the cursor ending at C+0x18.
// That is the "cursor is one 32-bit word behind the next write" convention BeginBatch
// (SetCurrentAddress(aligned - 4)) and EndBatch ((current - base) + 4) both state.
void NativeParticleVertex::VertexIterator::Write(
        const rw::math::vpu::Vector4& lv3Position,
        const int* lpColour,
        const float* lpUv)
{
    CGS_ASSERT( GetStride() == NativeParticleVertex::GetStride(),
                "GetStride() == NativeParticleVertex::GetStride()" );
    CGS_ASSERT( GetVerticesFree() != 0, "GetVerticesFree() != 0" );

    // The destination is a GPU VERTEX STREAM, not a C++ object: its layout is fixed by the
    // vertex declaration (position float3, colour u32, uv float2 -- 24 bytes), which is the
    // documented external-serialised-data case for direct byte addressing, and the same shape
    // LionBlendVertex::VertexIterator::Write beside it already uses. The canonical
    // EffectsVertexBufferIterator keeps its cursor as `const u8*` because every other reader
    // of it only reads; this is the writer, so it casts once here.
    u8* lpWrite = const_cast<u8*>( GetCurrentAddress() ) + 4;

    // Position xyz. The three stvewx write the stream WITHOUT advancing the cursor.
    float* const lpPosition = reinterpret_cast<float*>( lpWrite );
    lpPosition[0] = lv3Position.x;
    lpPosition[1] = lv3Position.y;
    lpPosition[2] = lv3Position.z;
    lpWrite += 3 * sizeof( float );

    // Colour: the cursor lands on this slot and the word is stored into it.
    SetCurrentAddress( lpWrite );
    *reinterpret_cast<int*>( lpWrite ) = *lpColour;
    lpWrite += sizeof( int );

    // uv.u then uv.v: advance FIRST, store at the new cursor.
    SetCurrentAddress( lpWrite );
    *reinterpret_cast<float*>( lpWrite ) = lpUv[0];
    lpWrite += sizeof( float );

    SetCurrentAddress( lpWrite );
    *reinterpret_cast<float*>( lpWrite ) = lpUv[1];
}

} // namespace BrnParticle
