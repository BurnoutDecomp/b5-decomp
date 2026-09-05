// ============================================================================
// GameSource/Effects/Particles/Native/BrnLionBlendVertex.cpp
//
// BrnGraphics::LionBlendVertex::VertexIterator::Write @ 0x8227E568
//
// Writes one 36-byte Lion blend particle vertex into the iterator's current cursor.
// The X360 asm body is the two bookkeeping asserts followed by a TAIL-CALL to the
// generic renderengine::VertexIterator3<VertexTypeFloat4,VertexTypePS3Color(RGBA8),
// VertexTypeFloat4>::Write<Vector4,RGBA8,Vector4> writer, which lays the vertex out as:
//   position Vector4 @ cur+0  (16 bytes)
//   colour   RGBA8   @ cur+16 ( 4 bytes)
//   uv       Vector4 @ cur+20 (16 bytes)   -> stride 36 (0x24)
// and advances mpCurrentAddress by the stride. Only the two GetStride/GetVerticesFree
// asserts are attested inline in THIS function's asm; the per-element store layout is
// MODELLED here (store-for-store per element) so the TU compiles standalone -- the
// concrete lane order comes from the generic template writer + the DWARF element types
// (Float4/PS3Color/Float4) and the Write(Vector4,const RGBA8&,Vector4) arg order, NOT
// from this body's asm. Mirrors the committed BrnParticle::NativeParticleVertex sibling.
//
// Called from BrnGraphics::LionBlendRenderer QuadDraw.
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnLionBlendVertex.h"
#include "SDKs/EATech/include/ps3/gcm/renderengine/stateparams.h" // renderengine::RGBA8
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT

namespace BrnGraphics {

// X360 @ 0x8227E568.
//   lv4Position -- vertex position (arrives in vector register v1 on X360)
//   lrColour    -- packed RGBA8 colour (const RGBA8&, passed as a2/r29)
//   lv4Uv       -- texture coordinate (arrives in vector register v2 on X360)
void LionBlendVertex::VertexIterator::Write(
        rw::math::vpu::Vector4 lv4Position,
        const renderengine::RGBA8& lrColour,
        rw::math::vpu::Vector4 lv4Uv)
{
    // cmplwi r11(=muStride), 0x24 -> assert stride == LionBlendVertex::GetStride() (36).
    CGS_ASSERT( GetStride() == LionBlendVertex::GetStride(),
                "GetStride() == LionBlendVertex::GetStride()" );
    // (top - current) / stride != 0.
    CGS_ASSERT( GetVerticesFree() != 0, "GetVerticesFree() != 0" );

    // Delegated element writes (renderengine::VertexIterator3::Write) -- MODELLED.
    // The canonical EffectsVertexBufferIterator (EffectsVertexBuffer.h) keeps its four
    // fields private and hands out accessors; the cursor is stored as `const u8*` because
    // every other reader of it only reads. This is the one writer, so it casts once here
    // rather than widening the shared struct's interface for a single call site.
    u8* lpCur = const_cast<u8*>(GetCurrentAddress());

    // position Vector4 @ cur+0
    reinterpret_cast<float*>(lpCur)[0] = lv4Position.x;
    reinterpret_cast<float*>(lpCur)[1] = lv4Position.y;
    reinterpret_cast<float*>(lpCur)[2] = lv4Position.z;
    reinterpret_cast<float*>(lpCur)[3] = lv4Position.w;

    // colour RGBA8 @ cur+16
    *reinterpret_cast<renderengine::RGBA8*>(lpCur + 16) = lrColour;

    // uv Vector4 @ cur+20
    reinterpret_cast<float*>(lpCur + 20)[0] = lv4Uv.x;
    reinterpret_cast<float*>(lpCur + 20)[1] = lv4Uv.y;
    reinterpret_cast<float*>(lpCur + 20)[2] = lv4Uv.z;
    reinterpret_cast<float*>(lpCur + 20)[3] = lv4Uv.w;

    // Advance the write cursor by one stride (36 bytes).
    SetCurrentAddress(lpCur + LionBlendVertex::GetStride());
}

} // namespace BrnGraphics
