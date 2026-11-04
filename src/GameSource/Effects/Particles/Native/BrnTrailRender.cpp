// ============================================================================
// GameSource/Effects/Particles/Native/BrnTrailRender.cpp
//
// BrnParticle::Native::TrailRenderer -- draws the active skid TrailEmitters of
// one trail type as one immediate-mode triangle strip of BrnGraphics::SkidVertex.
// Reconstructed store-for-store from the X360 ARTIST asm:
//
//   TrailRenderer::BeginRender @0x82284468
//   TrailRenderer::Render      @0x82295930
//   Construct / Update / EndRender -- inlined into ParticleModule::Prepare
//     @0x8229BEA0, ParticleModule::BuildLionVertexBuffers @0x8228AC20 and
//     TrailSystem::Render @0x82295C58 respectively.
//
// HOW A MARK IS DRAWN, as the asm has it:
//   * each segment becomes TWO SkidVertex: pos + tangent * half and
//     pos - tangent * half (kvHalfTrailSize == 0.125 m, so marks are 0.25 m
//     wide), uv = (segment index, 0) and (segment index, 1) -- the texture
//     repeats once per segment along the strip;
//   * the vertex TIME lane is the segment's age fraction
//     min((now - timeLaid) * kvOneOverTrailBaseLife, 1) with the base life
//     10 s (the same 10 s TrailSystem::EndOfFrame releases at) -- the skid
//     vertex program lerps gStartColour -> gEndColour by it;
//   * the vertex ALPHA lane is the segment's skid strength -- the program
//     multiplies the lerped alpha by it;
//   * emitters are batched into one strip of up to 256 vertices: the first
//     vertex of every emitter is written TWICE and its last vertex is
//     repeated after the loop, so consecutive emitters join through
//     degenerate triangles (the 2 * (segments + 1) vertex budget check);
//   * gStartColour / gEndColour are the type's TrailParams with RGB scaled by
//     the frame's white level (alpha untouched).
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnTrailRender.h"
#include "GameSource/Effects/Particles/Native/BrnTrailSystem.h"        // TrailEmitter / TrailParams
#include "GameSource/Effects/Particles/Native/BrnIm3dSkidsRenderer.h"  // BrnGraphics::Im3dSkidsRenderer
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                             // rw::math::vpu::{operator+,operator*}

namespace BrnParticle
{
namespace Native
{
    namespace
    {
        // ---- BrnTrailRender.cpp:31-34 (DWARF) -----------------------------------------------
        // kvHalfTrailSize / kvMinusHalfTrailSize / krTrailBaseLife / kvOneOverTrailBaseLife.
        //
        // ⭐ THE FLAG THAT STOOD HERE IS DISCHARGED (2026-09-15, issue #21). The three vectors
        // are dynamically-initialised .data objects that read 0.0 straight out of the image, and
        // this banner used to carry the DecFIGS values plus an INFERRED +/- assignment ("the
        // .data order is the DWARF declaration order REVERSED ... FLAG: the +/- assignment rests
        // on that inference"). The CRT-init thunks are now HOMED in the X360 image
        // (tools/re/findinit.py + ppcdis.py) and they settle it -- the inference was BACKWARDS:
        //
        //   0x82C4AEA8  lfs f0,[0x82004014] (== 0.1)   ; vspltw ; stvx -> unk_82FAB920
        //   0x82C4AE48  lfs f0,[0x82004010] (== 0.125) ; x/y/z lanes, w = 0 ; stvx -> unk_82FAB930
        //   0x82C4AE80  vspltisw v0,-1 ; vslw v0,v0,v0 (== 0x80000000 splat) ;
        //               lvx v13,[unk_82FAB930] ; vxor v0,v13,v0 ; stvx -> unk_82FAB940
        //
        // so unk_82FAB920 == 0.1 (1/life), unk_82FAB930 == +0.125 and unk_82FAB940 == -0.125
        // (the sign-flipped COPY of 930, which is why it is the one the DWARF calls
        // kvMinusHalfTrailSize). Render @0x82295930 loads v123 <- 920, v122 <- 940, v121 <- 930
        // (0x82295A30/0x82295A3C/0x82295A48), and BOTH extrude blocks pair the MINUS edge with
        // uv.y == 0:
        //   segment 0   @0x82295B38  vmr128 v10,v122 ; vmaddcfp128 v10 = tangent*v10 + pos   (-half)
        //               @0x82295B40  vmaddfp128 v12 = tangent*v121 + pos                     (+half)
        //               @0x82295B4C  stvx v10 -> vertex[i].pos with uv v0   (uv.y == 0)
        //               @0x82295B68  stvx v12 -> vertex[i+2].pos with uv v0 + {0,1,0,0}
        //   segments 1+ @0x82295BC4/0x82295BC8 the same two products, then
        //               @0x82295BD8 stvx v10 (-half) with uv.y == 0 and
        //               @0x82295BE8 stvx v12 (+half) with uv.y == 1.
        // The tree had the two swapped, which mirrors the tread texture's V across the strip and
        // reverses the winding. Cull is none for this pass so the silhouette was unaffected --
        // which is exactly why only the image could settle it.
        //
        // The console vectors are xyz splats of +/-0.125 (w == 0); the tree's Vector3 * f32 is the
        // same lane-wise product, so they are carried as the scalar they splat.
        const f32     KR_HALF_TRAIL_SIZE          =  0.125f;   // kvHalfTrailSize
        const f32     KR_MINUS_HALF_TRAIL_SIZE    = -0.125f;   // kvMinusHalfTrailSize
        const f32     KR_TRAIL_BASE_LIFE          = 10.0f;     // krTrailBaseLife
        const f32     KF_ONE_OVER_TRAIL_BASE_LIFE = 0.1f;      // kvOneOverTrailBaseLife

        // The strip scratch the X360 fills on the stack (var_20F0, 0x2000 bytes == 256 vertices)
        // and the flush threshold (`cmpwi 0x100` @0x82295A94).
        const s32 KN_MAX_STRIP_VERTICES = 256;

        // renderengine::PrimitiveType 6 -- the triangle strip both Render calls submit
        // (`li r4, 6` @0x82295AA4 / @0x82295C3C). The enum is external (CgsImRenderer.h:18).
        const s32 KI_PRIMITIVE_TRIANGLE_STRIP = 6;

        // .rdata unk_82181510 == {0, 1, 0, 0}: the uv.y step to the second edge vertex.
        const ::Vector4 K_UV_SECOND_EDGE(0.0f, 1.0f, 0.0f, 0.0f);

        inline ::Vector4 MakeUvTimeAlpha(f32 lfU, f32 lfV, f32 lfAge, f32 lfStrength)
        {
            const ::Vector4 lResult(lfU, lfV, lfAge, lfStrength);
            return lResult;
        }
        inline ::Vector4 AddTrailVectors(::Vector4 lLhs, ::Vector4 lRhs)
        {
            const ::Vector4 lResult(lLhs.mV.v[0] + lRhs.mV.v[0], lLhs.mV.v[1] + lRhs.mV.v[1], lLhs.mV.v[2] + lRhs.mV.v[2], lLhs.mV.v[3] + lRhs.mV.v[3]);
            return lResult;
        }
        inline ::Vector4 ScaleRgb(const RwRGBAReal& lColour, f32 lfWhiteLevel)
        {
            // v55 = {wl, wl, wl, 1.0}; colour * v55 (vmulfp128 @0x822959E4 / @0x82295A14).
            const ::Vector4 lResult(lColour.red * lfWhiteLevel, lColour.green * lfWhiteLevel,
                                      lColour.blue * lfWhiteLevel, lColour.alpha * 1.0f);
            return lResult;
        }

    }

    // =========================================================================================
    void TrailRenderer::Construct(CgsMemory::HeapMalloc* lpHeapMalloc, BrnGraphics::Im3dSkidsRenderer* lpRenderer)
    {
        (void)lpHeapMalloc;   // not read by the X360 body
        mpRenderer = lpRenderer;
    }

    // =========================================================================================
    void TrailRenderer::Update(f32 lfCurrentTime, Matrix44::InParam lViewProjMatrix)
    {
        mfCurrentTime         = lfCurrentTime;
        mViewProjectionMatrix = lViewProjMatrix;
    }

    // =========================================================================================
    // TrailRenderer::BeginRender  @0x82284468
    // =========================================================================================
    void TrailRenderer::BeginRender(renderengine::Texture* lpTexture)
    {
        mpRenderer->BeginRendering();                        // ImRenderer<SkidVertex>::BeginRendering @0x8227C1E8
        mpRenderer->SetTransform(mViewProjectionMatrix);     // BeginShaderStates(this+88) + 4 x stvx128

        // sub_82276DA8(base, dword_83010F4C) / SetState(base, dword_83010F20) / sub_82276E48(base, dword_83010F3C)
        mpRenderer->SetState(CgsGraphics::ImRendererBase::GetStateLibrary().mpDepthStencilState_ZBufferOnWriteOff);
        mpRenderer->SetState(CgsGraphics::ImRendererBase::GetStateLibrary().mpBlendState_Standard);
        mpRenderer->SetState(CgsGraphics::ImRendererBase::GetStateLibrary().mpRasterizerState_CullNone);

        mpRenderer->SetTexture(lpTexture);                   // ImRendererBase::SetTexture @0x82276EE8
    }

    // =========================================================================================
    // TrailRenderer::EndRender -- the ImRenderer<V>::EndRendering fold at the tail of
    // TrailSystem::Render @0x82295C58 (`mgpActiveRenderer == this` at CgsImRenderer.h:645).
    // =========================================================================================
    void TrailRenderer::EndRender()
    {
        CgsGraphics::ImRendererBase* const lpBase =
            (mpRenderer != 0) ? static_cast<CgsGraphics::ImRendererBase*>(mpRenderer) : 0;
        CGS_ASSERT(CgsGraphics::ImRendererBase::mgpActiveRenderer == lpBase, "mgpActiveRenderer == this");
        CgsGraphics::ImRendererBase::mgpActiveRenderer = 0;
    }

    // =========================================================================================
    // TrailRenderer::Render  @0x82295930
    //   r3 = this, r4 = lppEmitter, r5 = lnEmitterCount, r6 = lpParams, r7 = lu8TrailTypeID
    //   (not read), f1 = lfWhiteLevel.
    // =========================================================================================
    void TrailRenderer::Render(TrailEmitter** lppEmitter, s32 lnEmitterCount, TrailParams* lpParams,
                               s8 lu8TrailTypeID, const f32 lfWhiteLevel)
    {
        using namespace rw::math::vpu;
        (void)lu8TrailTypeID;

        // The two colour constants: RGB scaled by the white level, alpha kept.
        mpRenderer->SetBlendStartColour(ScaleRgb(lpParams->mStartColour, lfWhiteLevel));
        mpRenderer->SetBlendEndColour(ScaleRgb(lpParams->mEndColour, lfWhiteLevel));

        BrnGraphics::SkidVertex laVertices[KN_MAX_STRIP_VERTICES];
        s32                     lnVertexCount = 0;
        const f32               lfNow         = mfCurrentTime;                 // v125

        for (s32 lnEmitter = 0; lnEmitter < lnEmitterCount; ++lnEmitter)
        {
            const TrailEmitter* const lpEmitter    = lppEmitter[lnEmitter];
            const s32                 lnNumSegments = lpEmitter->mn8NumSegments;
            if (lnNumSegments < 2)
            {
                continue;
            }

            // Flush when this emitter's strip (2 per segment + the joining pair) would not fit.
            if (2 * (lnNumSegments + 1) + lnVertexCount > KN_MAX_STRIP_VERTICES)
            {
                mpRenderer->Render(static_cast<renderengine::PrimitiveType>(KI_PRIMITIVE_TRIANGLE_STRIP),
                                   laVertices, static_cast<u32>(lnVertexCount));
                lnVertexCount = 0;
            }

            const TrailSegmentCollection* const lpSegments = lpEmitter->mpCurrentSegments;

            // ---- segment 0: its +half edge vertex is written twice (the strip join) ----------
            {
                ::Vector3       lPosition  = lpSegments->ReadSegmentPosition(0);
                const ::Vector3 lTangent   = lpSegments->ReadSegmentTangent(0);
                const f32     lfStrength = lpSegments->ReadSegmentStrength(0);
                f32           lfAge      = (lfNow - lpSegments->ReadSegmentTime(0)) * KF_ONE_OVER_TRAIL_BASE_LIFE;
                if (lfAge > 1.0f)
                {
                    lfAge = 1.0f;                                                  // vminfp128 with 1.0
                }
                const ::Vector4 lUvA = MakeUvTimeAlpha(0.0f, 0.0f, lfAge, lfStrength);   // u == segment index 0
                const ::Vector4 lUvB = AddTrailVectors(lUvA, K_UV_SECOND_EDGE);
                // uv.y == 0 is the MINUS edge (v122 == unk_82FAB940 == -0.125) -- see the banner.
                const ::Vector3 lEdgeA = lTangent * KR_MINUS_HALF_TRAIL_SIZE + lPosition; // vmaddcfp128 v10 = v11 * v122 + v12
                const ::Vector3 lEdgeB = lTangent * KR_HALF_TRAIL_SIZE + lPosition;       // vmaddfp128  v12 = v11 * v121 + v12

                laVertices[lnVertexCount].mv3Pos         = lEdgeA;
                laVertices[lnVertexCount].mv4UvTimeAlpha = lUvA;
                laVertices[lnVertexCount + 1]            = laVertices[lnVertexCount];
                laVertices[lnVertexCount + 2].mv3Pos         = lEdgeB;
                laVertices[lnVertexCount + 2].mv4UvTimeAlpha = lUvB;
                lnVertexCount += 3;
            }

            // ---- segments 1..n-1: two vertices each, u == the segment index ------------------
            f32 lfSegmentU = 1.0f;                                                        // v13, +1.0 per segment
            for (s32 lnSegment = 1; lnSegment < lnNumSegments; ++lnSegment, lfSegmentU += 1.0f)
            {
                ::Vector3       lPosition  = lpSegments->ReadSegmentPosition(lnSegment);
                const ::Vector3 lTangent   = lpSegments->ReadSegmentTangent(lnSegment);
                const f32     lfStrength = lpSegments->ReadSegmentStrength(lnSegment);
                f32           lfAge      = (lfNow - lpSegments->ReadSegmentTime(lnSegment)) * KF_ONE_OVER_TRAIL_BASE_LIFE;
                if (lfAge > 1.0f)
                {
                    lfAge = 1.0f;
                }
                const ::Vector4 lUvA = MakeUvTimeAlpha(lfSegmentU, 0.0f, lfAge, lfStrength);
                const ::Vector4 lUvB = AddTrailVectors(lUvA, K_UV_SECOND_EDGE);
                // uv.y == 0 is the MINUS edge (0x82295BC8/0x82295BD8) -- see the banner.
                const ::Vector3 lEdgeA = lTangent * KR_MINUS_HALF_TRAIL_SIZE + lPosition;
                const ::Vector3 lEdgeB = lTangent * KR_HALF_TRAIL_SIZE + lPosition;

                laVertices[lnVertexCount].mv3Pos             = lEdgeA;
                laVertices[lnVertexCount].mv4UvTimeAlpha     = lUvA;
                laVertices[lnVertexCount + 1].mv3Pos         = lEdgeB;
                laVertices[lnVertexCount + 1].mv4UvTimeAlpha = lUvB;
                lnVertexCount += 2;


            }

            // ---- the last vertex repeated: closes this emitter's strip with a degenerate ----
            laVertices[lnVertexCount] = laVertices[lnVertexCount - 1];
            ++lnVertexCount;
        }

        if (lnVertexCount > 0)
        {
            mpRenderer->Render(static_cast<renderengine::PrimitiveType>(KI_PRIMITIVE_TRIANGLE_STRIP),
                               laVertices, static_cast<u32>(lnVertexCount));

        }
    }

}
}
