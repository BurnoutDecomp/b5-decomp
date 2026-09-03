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
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // [diag] CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Development/BrnDiagFilmLatch.h"       // [diag] BrnDiag::gFilmLatch (frames.csv NDC columns)

#include <cstdio>   // [diag] snprintf (the [trailpass] transform probe)
#include "rw/math/vpu/vector3_operation.h"                             // rw::math::vpu::{operator+,operator*}

// The immediate-mode state library's three trail states, built by CgsGraphics::ImRendererBase::
// ConstructOnceOnly @0x827F1C20 (read out of its asm -- the ImmediateModePCLeaf.cpp note that
// dword_83010F4C's initialiser "was never recovered" is stale):
//   dword_83010F4C = ConstructDepthStencilState(alloc, test=1, write=0, func=3 LESSEQUAL)
//   dword_83010F20 = ConstructBlendState(alloc, 6, 7, 0)   -- the standard alpha blend
//   dword_83010F3C = ConstructRasteriserState(alloc, 0)     -- cull none
// The X360 binds them through the renderer's three ImRendererBase::SetState overloads
// (sub_82276DA8 depth/stencil, 0x82276D08 blend, sub_82276E48 rasteriser). On this build the
// same PC objects the sky dome binds (gpSkyDomeDepthStencilState IS dword_83010F4C,
// gpSkyDomeRasterizerState IS dword_83010F3C) plus the standard alpha blend
// (gpImStandardAlphaBlendState IS dword_83010F20) -- all three defined by
// ImmediateModePCLeaf.cpp and declared `extern void*` here exactly as BrnSkyDomeManager.cpp
// does, bound through the same three PC leaf appliers.
extern void* gpSkyDomeDepthStencilState;   // dword_83010F4C
extern void* gpImStandardAlphaBlendState;  // dword_83010F20
extern void* gpSkyDomeRasterizerState;     // dword_83010F3C
void ImDeviceSetDepthStencilState(void* lpState);
void ImDeviceSetBlendState(void* lpState);
void ImDeviceSetRasterizerState(void* lpState);

namespace BrnParticle
{
namespace Native
{
    namespace
    {
        // ---- BrnTrailRender.cpp:31-34 (DWARF) -----------------------------------------------
        // kvHalfTrailSize / kvMinusHalfTrailSize / krTrailBaseLife / kvOneOverTrailBaseLife. The
        // three vectors are dynamically-initialised X360 .data objects (unk_82FAB920 / 930 / 940
        // all read 0.0 in the image; their CRT-init thunk is an export hole, not located this
        // wave). Values are the DecFIGS static initialiser's (__static_initialization_and_
        // destruction_0 @0xE2B5C: the 0x3E000000 == 0.125 lanes, their sign-flipped copy, and
        // the 0.1 splat), corroborated by the X360's own EndOfFrame life of 10.0 s.
        //
        // Which .data word is which is pinned by USE in Render @0x82295930: v123 (unk_82FAB920)
        // multiplies the (now - timeLaid) age (vmulfp128 @0x82295B28) so it is the 1/life
        // splat; v122 (unk_82FAB940) and v121 (unk_82FAB930) scale the tangent for the two edge
        // vertices. The .data order is therefore the DWARF declaration order REVERSED
        // (1/life, minus-half, half), i.e. the first vertex of a pair (uv.y == 0) is the
        // +half edge. Cull is none for this pass, so the winding cannot change the picture
        // either way. FLAG: the +/- assignment rests on that reversed-order inference.
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
        const Vector4 K_UV_SECOND_EDGE = { 0.0f, 1.0f, 0.0f, 0.0f };

        inline Vector4 MakeUvTimeAlpha(f32 lfU, f32 lfV, f32 lfAge, f32 lfStrength)
        {
            const Vector4 lResult = { lfU, lfV, lfAge, lfStrength };
            return lResult;
        }
        inline Vector4 Add(Vector4 lLhs, Vector4 lRhs)
        {
            const Vector4 lResult = { lLhs.x + lRhs.x, lLhs.y + lRhs.y, lLhs.z + lRhs.z, lLhs.w + lRhs.w };
            return lResult;
        }
        inline Vector4 ScaleRgb(const RwRGBAReal& lColour, f32 lfWhiteLevel)
        {
            // v55 = {wl, wl, wl, 1.0}; colour * v55 (vmulfp128 @0x822959E4 / @0x82295A14).
            const Vector4 lResult = { lColour.red * lfWhiteLevel, lColour.green * lfWhiteLevel,
                                      lColour.blue * lfWhiteLevel, lColour.alpha * 1.0f };
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
        ImDeviceSetDepthStencilState(gpSkyDomeDepthStencilState);
        ImDeviceSetBlendState(gpImStandardAlphaBlendState);
        ImDeviceSetRasterizerState(gpSkyDomeRasterizerState);

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
                guProbeVertices += static_cast<u32>(lnVertexCount);   // [diag]
                ++guProbeDraws;                                        // [diag]
                lnVertexCount = 0;
            }

            const TrailSegmentCollection* const lpSegments = lpEmitter->mpCurrentSegments;

            // ---- segment 0: its +half edge vertex is written twice (the strip join) ----------
            {
                const Vector3 lPosition  = lpSegments->ReadSegmentPosition(0);
                const Vector3 lTangent   = lpSegments->ReadSegmentTangent(0);
                const f32     lfStrength = lpSegments->ReadSegmentStrength(0);
                f32           lfAge      = (lfNow - lpSegments->ReadSegmentTime(0)) * KF_ONE_OVER_TRAIL_BASE_LIFE;
                if (lfAge > 1.0f)
                {
                    lfAge = 1.0f;                                                  // vminfp128 with 1.0
                }
                const Vector4 lUvA = MakeUvTimeAlpha(0.0f, 0.0f, lfAge, lfStrength);   // u == segment index 0
                const Vector4 lUvB = Add(lUvA, K_UV_SECOND_EDGE);
                const Vector3 lEdgeA = lTangent * KR_HALF_TRAIL_SIZE + lPosition;       // vmaddcfp128 v10 = v11 * v122 + v12
                const Vector3 lEdgeB = lTangent * KR_MINUS_HALF_TRAIL_SIZE + lPosition; // vmaddfp128 v12 = v11 * v121 + v12

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
                const Vector3 lPosition  = lpSegments->ReadSegmentPosition(lnSegment);
                const Vector3 lTangent   = lpSegments->ReadSegmentTangent(lnSegment);
                const f32     lfStrength = lpSegments->ReadSegmentStrength(lnSegment);
                f32           lfAge      = (lfNow - lpSegments->ReadSegmentTime(lnSegment)) * KF_ONE_OVER_TRAIL_BASE_LIFE;
                if (lfAge > 1.0f)
                {
                    lfAge = 1.0f;
                }
                const Vector4 lUvA = MakeUvTimeAlpha(lfSegmentU, 0.0f, lfAge, lfStrength);
                const Vector4 lUvB = Add(lUvA, K_UV_SECOND_EDGE);
                const Vector3 lEdgeA = lTangent * KR_HALF_TRAIL_SIZE + lPosition;
                const Vector3 lEdgeB = lTangent * KR_MINUS_HALF_TRAIL_SIZE + lPosition;

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
            guProbeVertices += static_cast<u32>(lnVertexCount);   // [diag]
            ++guProbeDraws;                                        // [diag]

            // [DIAG] RUN THE VERTEX PROGRAM'S OWN TRANSFORM ON ONE REAL VERTEX.
            // 8.4 million submitted vertices and an empty road is the classic
            // valid-call/invalid-data shape: the strips are being drawn somewhere the
            // camera is not. The skid vertex program's whole position math is
            //     oPos = pos.x*c0 + pos.y*c1 + pos.z*c2 + c3        (gWorldViewProj rows)
            // so doing exactly that here, on the first vertex of the batch, says whether
            // the fault is upstream (the matrix) or downstream (states/shader/blend).
            // A zero or identity matrix, a w <= 0, or |x/w| > 1 convicts the matrix;
            // sane NDC inside the frustum clears it and moves the search below the draw.
            // Read the matrix as its 16 floats -- it is being handed to the shader as raw
            // bytes by SetTransform, so this reads it exactly as the program does.
            const f32* const lpM = reinterpret_cast<const f32*>(&mViewProjectionMatrix);
            const Vector3& lrP = laVertices[0].mv3Pos;
            const f32 lfX = lrP.x * lpM[0] + lrP.y * lpM[4] + lrP.z * lpM[8]  + lpM[12];
            const f32 lfY = lrP.x * lpM[1] + lrP.y * lpM[5] + lrP.z * lpM[9]  + lpM[13];
            const f32 lfZ = lrP.x * lpM[2] + lrP.y * lpM[6] + lrP.z * lpM[10] + lpM[14];
            const f32 lfW = lrP.x * lpM[3] + lrP.y * lpM[7] + lrP.z * lpM[11] + lpM[15];

            // Stamp the NDC into the film latch so frames.csv carries the SCREEN position of
            // a submitted mark (columns 15-17). See BrnDiagFilmLatch.h.
            //
            // ⭐ The point transformed here is the MOST RECENTLY LAID segment
            // (gFilmLatch.mfLastSeg*, written by HandleWheels beside AddTrailSegment), NOT the
            // batch's first vertex. Both are real submitted geometry, but the first vertex of
            // the last batch belongs to an arbitrary emitter that may be nine seconds old and
            // therefore faded to nothing -- pointing a marker at it and finding bare tarmac
            // proves only that old marks fade. The freshest segment is the one whose alpha is
            // at its highest, so it is the honest place to aim.
            {
                const f32 lfSX = BrnDiag::gFilmLatch.mfLastSegX;
                const f32 lfSY = BrnDiag::gFilmLatch.mfLastSegY;
                const f32 lfSZ = BrnDiag::gFilmLatch.mfLastSegZ;
                const f32 lfSClipX = lfSX * lpM[0] + lfSY * lpM[4] + lfSZ * lpM[8]  + lpM[12];
                const f32 lfSClipY = lfSX * lpM[1] + lfSY * lpM[5] + lfSZ * lpM[9]  + lpM[13];
                const f32 lfSClipW = lfSX * lpM[3] + lfSY * lpM[7] + lfSZ * lpM[11] + lpM[15];
                BrnDiag::gFilmLatch.mfSegNdcX  = (lfSClipW != 0.0f) ? (lfSClipX / lfSClipW) : 0.0f;
                BrnDiag::gFilmLatch.mfSegNdcY  = (lfSClipW != 0.0f) ? (lfSClipY / lfSClipW) : 0.0f;
                BrnDiag::gFilmLatch.mfSegClipW = lfSClipW;
            }
            (void)lfZ;

            if (guProbeFirstVertex != 0u)
            {
                --guProbeFirstVertex;
                char lacMsg[400];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[trailpass] xform: v0=(%.2f,%.2f,%.2f) clip=(%.3f,%.3f,%.3f,%.3f) "
                    "ndc=(%.3f,%.3f,%.3f) m0=[%.4f %.4f %.4f %.4f] m3=[%.3f %.3f %.3f %.3f] "
                    "start=(%.3f,%.3f,%.3f,%.3f) end=(%.3f,%.3f,%.3f,%.3f) uvta0=(%.2f,%.2f,%.3f,%.3f)\n",
                    static_cast<double>(lrP.x), static_cast<double>(lrP.y), static_cast<double>(lrP.z),
                    static_cast<double>(lfX), static_cast<double>(lfY), static_cast<double>(lfZ), static_cast<double>(lfW),
                    static_cast<double>(lfW != 0.0f ? lfX / lfW : 0.0f),
                    static_cast<double>(lfW != 0.0f ? lfY / lfW : 0.0f),
                    static_cast<double>(lfW != 0.0f ? lfZ / lfW : 0.0f),
                    static_cast<double>(lpM[0]), static_cast<double>(lpM[1]), static_cast<double>(lpM[2]), static_cast<double>(lpM[3]),
                    static_cast<double>(lpM[12]), static_cast<double>(lpM[13]), static_cast<double>(lpM[14]), static_cast<double>(lpM[15]),
                    static_cast<double>(lpParams->mStartColour.red), static_cast<double>(lpParams->mStartColour.green),
                    static_cast<double>(lpParams->mStartColour.blue), static_cast<double>(lpParams->mStartColour.alpha),
                    static_cast<double>(lpParams->mEndColour.red), static_cast<double>(lpParams->mEndColour.green),
                    static_cast<double>(lpParams->mEndColour.blue), static_cast<double>(lpParams->mEndColour.alpha),
                    static_cast<double>(laVertices[0].mv4UvTimeAlpha.x), static_cast<double>(laVertices[0].mv4UvTimeAlpha.y),
                    static_cast<double>(laVertices[0].mv4UvTimeAlpha.z), static_cast<double>(laVertices[0].mv4UvTimeAlpha.w));
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
    }

    // [DIAG] the two running totals the [trailpass] line reads. See BrnTrailRender.h.
    u32 TrailRenderer::guProbeVertices    = 0;
    u32 TrailRenderer::guProbeDraws       = 0;
    u32 TrailRenderer::guProbeFirstVertex = 8;   // print the first EIGHT batches, then stop
}
}
