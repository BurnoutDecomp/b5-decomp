#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"
#if defined(_XBOX)
#include <vectorintrinsics.h>
#endif

#include <cmath>
#include <cstring>

// CgsDev::Debug3DImmediateRender records world-space debug primitives into the
// original Im3d command bank. BrnRendererModule dispatches that bank after world
// immediate mode, before scene resolve; the producer never calls the GPU directly.

namespace
{

    renderengine::BlendMaterialState* sgBlend;
    renderengine::DepthStencilState* sgDepth2D;
    renderengine::DepthStencilState* sgDepth3D;
    renderengine::RasterizerState* sgRasterizer;
    Vector3 Make3(f32 x, f32 y, f32 z, f32 w)
    { const Vector3 value = {x, y, z, w}; return value; }
    rw::math::vpu::Vector2 Make2(f32 x, f32 y, f32 z, f32 w)
    { const rw::math::vpu::Vector2 value = {x, y, z, w}; return value; }

    const f32 KF_PI = 3.14159265358979323846f;
    const f32 KF_TWO_PI = KF_PI * 2.0f;

    Vector3 Add3(Vector3 lA, Vector3 lB)
    {
        const Vector3 value = { lA.x + lB.x, lA.y + lB.y, lA.z + lB.z, 0.0f }; return value;
    }

    Vector3 Subtract3(Vector3 lA, Vector3 lB)
    {
        const Vector3 value = { lA.x - lB.x, lA.y - lB.y, lA.z - lB.z, 0.0f }; return value;
    }

    Vector3 Scale3(Vector3 lValue, f32 lfScale)
    {
        const Vector3 value = { lValue.x * lfScale, lValue.y * lfScale, lValue.z * lfScale, 0.0f }; return value;
    }

    f32 Dot3(Vector3 lA, Vector3 lB)
    {
        return lA.x * lB.x + lA.y * lB.y + lA.z * lB.z;
    }

    Vector3 Cross3(Vector3 lA, Vector3 lB)
    {
        const Vector3 value = {
            lA.y * lB.z - lA.z * lB.y,
            lA.z * lB.x - lA.x * lB.z,
            lA.x * lB.y - lA.y * lB.x,
            0.0f,
        }; return value;
    }

    f32 Length3(Vector3 lValue)
    {
        return std::sqrt(Dot3(lValue, lValue));
    }

    Vector3 Normalise(Vector3 lValue)
    {
        const f32 lfLength = Length3(lValue);
        return lfLength > 0.000001f ? Scale3(lValue, 1.0f / lfLength)
                                    : Make3( 0.0f, 1.0f, 0.0f, 0.0f );
    }

    Matrix44Affine IdentityAffine()
    {
        Matrix44Affine lResult;
        lResult.SetIdentity();
        return lResult;
    }

    void MakePerpendicularBasis(Vector3 lNormal, Vector3& lrRight, Vector3& lrUp)
    {
        lNormal = Normalise(lNormal);
        const Vector3 lReference = std::fabs(lNormal.y) < 0.75f
            ? Make3( 0.0f, 1.0f, 0.0f, 0.0f )
            : Make3( 1.0f, 0.0f, 0.0f, 0.0f );
        lrRight = Normalise(Cross3(lReference, lNormal));
        lrUp = Normalise(Cross3(lNormal, lrRight));
    }

    struct ClipPoint
    {
        f32 x, y, z, w;
    };

    ClipPoint TransformToClip(const Matrix44& lrMatrix,
                              const CgsGraphics::BasicColouredTexturedVertex& lrVertex)
    {
        const f32 x = lrVertex.mv3Pos.x;
        const f32 y = lrVertex.mv3Pos.y;
        const f32 z = lrVertex.mv3Pos.z;
        const ClipPoint value = {
            lrMatrix.xAxis.x * x + lrMatrix.yAxis.x * y + lrMatrix.zAxis.x * z + lrMatrix.wAxis.x,
            lrMatrix.xAxis.y * x + lrMatrix.yAxis.y * y + lrMatrix.zAxis.y * z + lrMatrix.wAxis.y,
            lrMatrix.xAxis.z * x + lrMatrix.yAxis.z * y + lrMatrix.zAxis.z * z + lrMatrix.wAxis.z,
            lrMatrix.xAxis.w * x + lrMatrix.yAxis.w * y + lrMatrix.zAxis.w * z + lrMatrix.wAxis.w,
        }; return value;
    }
}

namespace CgsDev
{
    void Debug3DImmediateRender::Construct(rw::IResourceAllocator* lpAllocator,
                                           f32 lfVirtualScreenWidth, f32 lfVirtualScreenHeight)
    {
        meProjectionMode = E_PROJECTION_3D;
        meDrawingMode = E_DRAWING_COUNT;
        meZTestEnable = E_ZTEST_ON;
        mPointBoxRadius = Make3(0.05f, 0.05f, 0.05f, 0.0f);
        mfVirtualScreenWidth = lfVirtualScreenWidth;
        mfVirtualScreenHeight = lfVirtualScreenHeight;
        mpRenderBuffer = 0;
        CreateDebugRenderStates(lpAllocator);
        mpFont.Clear();
        mVectorFont.Construct();
        mVectorFont.SetSize(Make2(10.0f, 10.0f, 0.0f, 0.0f));
        mVectorFont.SetRenderer(this);
        mTextRenderer.Construct();
        miIm3dVertsHead = 0;
        mViewProjectionMatrix.SetIdentity();
        mCameraPosition.SetZero();
        mePrimitiveType = 0;
        BuildSphereIndices();
    }

    // ARTIST 82818B68: construct the same blend, depth-on/write-off,
    // depth-off and rasterizer states once, allocated through the resource allocator.
    void Debug3DImmediateRender::CreateDebugRenderStates(rw::IResourceAllocator* lpAllocator)
    {
        static bool sbCreated = false;
        if (sbCreated) return;
        renderengine::BlendStateParameters lBlend = {0};
        for (u32 i = 0; i < 4; ++i) lBlend.maBlendFactor[i] = 0x07060706u;
        lBlend.maBlendFactor[0] = 0x07060706u | (899u << 1);
        lBlend.muState15 = 7;
        lBlend.muState4 = lBlend.muState5 = lBlend.muState6 = lBlend.muState7 = 15;
        lBlend.muState8 = 135;
        lBlend.muState9 = 0xFFFFFFFFu;
        lBlend.mbHasCustomBlendFactors = 1;
        renderengine::ResourceDescriptor5 descriptor;
        renderengine::BlendMaterialState* blendHandles[5] = {0};
        renderengine::BlendState::GetResourceDescriptor(&descriptor, &lBlend);
        CgsGraphics::ResourceAllocatorCreate(lpAllocator, blendHandles, &descriptor);
        sgBlend = static_cast<renderengine::BlendMaterialState*>(renderengine::BlendState::Initialize(blendHandles, &lBlend));

        renderengine::DepthStencilState::Parameters lDepth = {0};
        lDepth.muFunction = 3;
        lDepth.muState4 = lDepth.muState8 = 7;
        lDepth.muStencilReadMask = lDepth.muStencilWriteMask = 0xFFFFFFFFu;
        lDepth.muState14 = lDepth.muState15 = 0xFFFFFFFFu;
        renderengine::DepthStencilState* depth2DHandles[5] = {0};
        renderengine::DepthStencilState::GetResourceDescriptor(&descriptor, &lDepth);
        CgsGraphics::ResourceAllocatorCreate(lpAllocator, depth2DHandles, &descriptor);
        sgDepth2D = renderengine::DepthStencilState::Initialize(depth2DHandles, &lDepth);
        lDepth.mbDepthTestEnable = 1;
        renderengine::DepthStencilState* depth3DHandles[5] = {0};
        renderengine::DepthStencilState::GetResourceDescriptor(&descriptor, &lDepth);
        CgsGraphics::ResourceAllocatorCreate(lpAllocator, depth3DHandles, &descriptor);
        sgDepth3D = renderengine::DepthStencilState::Initialize(depth3DHandles, &lDepth);

        renderengine::RasterizerState::Parameters lRasterizer = {0};
        lRasterizer.muCullMode = 2;
        lRasterizer.muMultisampleEnable = 0xFFFFFFFFu;
        lRasterizer.muAntialiasedLineEnable = 0xFFFFu;
        lRasterizer.mu8DepthClipEnable = lRasterizer.mu8FrontCounterClockwise = 1;
        lRasterizer.mu8PaddingMode = 1;
        renderengine::RasterizerState* rasterHandles[5] = {0};
        renderengine::RasterizerState::GetResourceDescriptor(&descriptor, &lRasterizer);
        CgsGraphics::ResourceAllocatorCreate(lpAllocator, rasterHandles, &descriptor);
        sgRasterizer = renderengine::RasterizerState::Initialize(rasterHandles, &lRasterizer);
        sbCreated = true;
    }

    void Debug3DImmediateRender::SetDebugFont(
        const CgsResource::SafeResourceHandle<CgsResource::Font>& lrFont)
    {
        mpFont = lrFont;
    }

    void Debug3DImmediateRender::Begin(const rw::math::vpu::Matrix44& lrViewProjection,
                                       rw::math::vpu::Vector3 lCameraPosition)
    {
        mViewProjectionMatrix = lrViewProjection;
        mCameraPosition = lCameraPosition;
        miIm3dVertsHead = 0;
        CGS_ASSERT(mpRenderBuffer != 0, "mpRenderBuffer");
        CGS_ASSERT(!mpRenderBuffer->IsInARenderingBlock(), "!mpRenderBuffer->IsInARenderingBlock()");
        mpRenderBuffer->BeginRendering();
        mpRenderBuffer->SetState(reinterpret_cast<const renderengine::BlendState*>(sgBlend));
        mpRenderBuffer->SetState(sgRasterizer);
        meZTestEnable = E_ZTEST_ON;
        meProjectionMode = E_PROJECTION_COUNT;
        meDrawingMode = E_DRAWING_COUNT;
    }

    void Debug3DImmediateRender::End()
    {
        if (miIm3dVertsHead > 0) DispatchVertices();
        miIm3dVertsHead = 0;
        CGS_ASSERT(mpRenderBuffer != 0, "mpRenderBuffer");
        CGS_ASSERT(mpRenderBuffer->IsInARenderingBlock(), "mpRenderBuffer->IsInARenderingBlock()");
        mpRenderBuffer->EndRendering();
    }

    rw::math::vpu::Vector2 Debug3DImmediateRender::GetVirtualScreenSize() const
    {
        return Make2(mfVirtualScreenWidth, mfVirtualScreenHeight, 0.0f, 0.0f);
    }

    rw::math::vpu::Vector3 Debug3DImmediateRender::GetCameraPosition() const
    {
        return mCameraPosition;
    }

    const rw::math::vpu::Matrix44 Debug3DImmediateRender::GetViewProjectionMatrix() const
    {
        return mViewProjectionMatrix;
    }

    void Debug3DImmediateRender::SetProjectionMode(ProjectionMode leMode)
    {
        if (meProjectionMode != leMode)
        {
            meProjectionMode = leMode;
            DispatchVertices();
            if (mpRenderBuffer)
            {
                if (leMode == E_PROJECTION_3D)
                {
                    mpRenderBuffer->SetState(sgDepth3D);
                    mpRenderBuffer->SetTransform(mViewProjectionMatrix);
                }
                else
                {
                    Matrix44 lProjection;
                    lProjection.SetIdentity();
                    const Vector4 yAxis = {0.0f, -1.0f, 0.0f, 0.0f};
                    lProjection.yAxis = yAxis;
                    mpRenderBuffer->SetState(sgDepth2D);
                    mpRenderBuffer->SetTransform(lProjection);
                }
            }
        }
    }

    void Debug3DImmediateRender::SetZTestEnable(ZTestEnable leEnable)
    {
        if (meZTestEnable != leEnable)
        {
            DispatchVertices();
            if (leEnable != E_ZTEST_OFF)
                CGS_ASSERT(leEnable == E_ZTEST_ON, "leZTestEnable == E_ZTEST_ON");
            meZTestEnable = leEnable;
        }
    }

    void Debug3DImmediateRender::SetDrawingMode(DrawingMode leMode)
    {
        if (meDrawingMode != leMode)
        {
            DispatchVertices();
            meDrawingMode = leMode;
            // ARTIST DrawLine stores 2, DrawSphere stores 3 (a line strip),
            // and DrawSolidSphere stores 4. Native AddQuad expands to triangles.
            mePrimitiveType = leMode == E_DRAWING_LINES ? 2
                : leMode == E_DRAWING_TRISTRIP_LINES ? 3 : 4;
        }
    }

    void Debug3DImmediateRender::AddVertex(rw::math::vpu::Vector3 lPosition, rw::RGBA lColour)
    {
        if (miIm3dVertsHead >= KI_VERTEX_BUFFER_SIZE)
            DispatchVertices();

        CgsGraphics::BasicColouredTexturedVertex& lrVertex = maIm3dVertsArray[miIm3dVertsHead++];
        lrVertex.mv3Pos = lPosition;
        lrVertex.mv4Colour.r = static_cast<u8>(lColour.m_rgba);
        lrVertex.mv4Colour.g = static_cast<u8>(lColour.m_rgba >> 8);
        lrVertex.mv4Colour.b = static_cast<u8>(lColour.m_rgba >> 16);
        lrVertex.mv4Colour.a = static_cast<u8>(lColour.m_rgba >> 24);
        lrVertex.mv2Tex0UV.x = 0.5f;
        lrVertex.mv2Tex0UV.y = 0.5f;
    }

    void Debug3DImmediateRender::AddLine(rw::math::vpu::Vector3 lA,
                                         rw::math::vpu::Vector3 lB, rw::RGBA lColour)
    {
        if (miIm3dVertsHead > KI_VERTEX_BUFFER_SIZE - 2)
            DispatchVertices();
        AddVertex(lA, lColour);
        AddVertex(lB, lColour);
    }

    void Debug3DImmediateRender::AddTriangle(rw::math::vpu::Vector3 lA,
                                             rw::math::vpu::Vector3 lB,
                                             rw::math::vpu::Vector3 lC,
                                             rw::RGBA lColour)
    {
        if (miIm3dVertsHead > KI_VERTEX_BUFFER_SIZE - 3)
            DispatchVertices();
        AddVertex(lA, lColour);
        AddVertex(lB, lColour);
        AddVertex(lC, lColour);
    }

    void Debug3DImmediateRender::AddQuad(rw::math::vpu::Vector3 lA,
                                         rw::math::vpu::Vector3 lB,
                                         rw::math::vpu::Vector3 lC,
                                         rw::math::vpu::Vector3 lD,
                                         rw::RGBA lColour)
    {
        AddTriangle(lA, lB, lC, lColour);
        AddTriangle(lA, lC, lD, lColour);
    }

    void Debug3DImmediateRender::DispatchVertices()
    {
        if (miIm3dVertsHead == 0) return;
        if (mpRenderBuffer)
        {
            // ARTIST 8281B92C..8281B950 writes the default texture state, then
            // both global depth states in this order, before the vertex-copy command.
            mpRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(mePrimitiveType),
                maIm3dVertsArray, static_cast<u32>(miIm3dVertsHead));
        }
        miIm3dVertsHead = 0;
    }

    void Debug3DImmediateRender::DrawLine(rw::math::vpu::Vector3 lFrom,
                                          rw::math::vpu::Vector3 lTo, rw::RGBA lColour)
    {
        SetProjectionMode(E_PROJECTION_3D);
        SetDrawingMode(E_DRAWING_LINES);
        AddLine(lFrom, lTo, lColour);
    }

    void Debug3DImmediateRender::DrawHollowTriangle(rw::math::vpu::Vector3 lA,
                                                    rw::math::vpu::Vector3 lB,
                                                    rw::math::vpu::Vector3 lC,
                                                    rw::RGBA lColour)
    {
        DrawLine(lA, lB, lColour);
        DrawLine(lB, lC, lColour);
        DrawLine(lA, lC, lColour);
    }

    void Debug3DImmediateRender::DrawQuad(rw::math::vpu::Vector3 lA,
                                          rw::math::vpu::Vector3 lB,
                                          rw::math::vpu::Vector3 lC,
                                          rw::math::vpu::Vector3 lD,
                                          rw::RGBA lColour)
    {
        DrawLine(lA, lB, lColour);
        DrawLine(lB, lC, lColour);
        DrawLine(lC, lD, lColour);
        DrawLine(lD, lA, lColour);
    }

    void Debug3DImmediateRender::DrawSolidTriangle(rw::math::vpu::Vector3 lA,
                                                   rw::math::vpu::Vector3 lB,
                                                   rw::math::vpu::Vector3 lC,
                                                   rw::RGBA lColour)
    {
        SetProjectionMode(E_PROJECTION_3D);
        SetDrawingMode(E_DRAWING_TRIANGLES);
        AddTriangle(lA, lB, lC, lColour);
    }

    void Debug3DImmediateRender::DrawSolidQuad(rw::math::vpu::Vector3 lA,
                                               rw::math::vpu::Vector3 lB,
                                               rw::math::vpu::Vector3 lC,
                                               rw::math::vpu::Vector3 lD,
                                               rw::RGBA lColour)
    {
        SetProjectionMode(E_PROJECTION_3D);
        SetDrawingMode(E_DRAWING_QUADS);
        AddQuad(lA, lB, lC, lD, lColour);
    }

    void Debug3DImmediateRender::DrawTriangle(rw::math::vpu::Vector3 lA,
                                              rw::math::vpu::Vector3 lB,
                                              rw::math::vpu::Vector3 lC,
                                              rw::RGBA lColour)
    {
        // ARTIST 0x8281F090 is the direct triangle-list primitive. Wire triangles are the
        // separate DrawHollowTriangle entry point.
        DrawSolidTriangle(lA, lB, lC, lColour);
    }

    void Debug3DImmediateRender::DrawBox(rw::math::vpu::Vector3 lMin,
                                         rw::math::vpu::Vector3 lMax,
                                         rw::math::vpu::Matrix44Affine lTransform,
                                         rw::RGBA lColour)
    {
        Vector3 laCorners[8];
        for (s32 i = 0; i < 8; ++i)
        {
            const Vector3 lLocal = { (i & 1) ? lMax.x : lMin.x,
                                     (i & 2) ? lMax.y : lMin.y,
                                     (i & 4) ? lMax.z : lMin.z, 0.0f };
            laCorners[i] = rw::math::vpu::TransformPoint(lTransform, lLocal);
        }

        static const u8 KAU_EDGE_INDICES[24] = {
            0,1, 1,3, 3,2, 2,0, 4,5, 5,7, 7,6, 6,4, 0,4, 1,5, 2,6, 3,7,
        };
        for (s32 i = 0; i < 24; i += 2)
            DrawLine(laCorners[KAU_EDGE_INDICES[i]], laCorners[KAU_EDGE_INDICES[i + 1]], lColour);
    }

    void Debug3DImmediateRender::DrawBox(rw::math::vpu::Vector3 lMin,
                                         rw::math::vpu::Vector3 lMax, rw::RGBA lColour)
    {
        DrawBox(lMin, lMax, IdentityAffine(), lColour);
    }

    void Debug3DImmediateRender::DrawSolidBox(rw::math::vpu::Vector3 lMin,
                                              rw::math::vpu::Vector3 lMax,
                                              rw::math::vpu::Matrix44Affine lTransform,
                                              rw::RGBA lColour)
    {
        Vector3 c[8];
        for (s32 i = 0; i < 8; ++i)
        {
            const Vector3 lLocal = { (i & 1) ? lMax.x : lMin.x,
                                     (i & 2) ? lMax.y : lMin.y,
                                     (i & 4) ? lMax.z : lMin.z, 0.0f };
            c[i] = rw::math::vpu::TransformPoint(lTransform, lLocal);
        }
        DrawSolidQuad(c[0], c[1], c[3], c[2], lColour);
        DrawSolidQuad(c[4], c[6], c[7], c[5], lColour);
        DrawSolidQuad(c[0], c[4], c[5], c[1], lColour);
        DrawSolidQuad(c[2], c[3], c[7], c[6], lColour);
        DrawSolidQuad(c[0], c[2], c[6], c[4], lColour);
        DrawSolidQuad(c[1], c[5], c[7], c[3], lColour);
    }

    void Debug3DImmediateRender::DrawSolidBox(rw::math::vpu::Vector3 lMin,
                                              rw::math::vpu::Vector3 lMax, rw::RGBA lColour)
    {
        DrawSolidBox(lMin, lMax, IdentityAffine(), lColour);
    }

    void Debug3DImmediateRender::DrawPoint(rw::math::vpu::Vector3 lPosition, rw::RGBA lColour)
    {
        // Dispatch3D case 7 calls the AA solid-box body at 0x8281E548.
        DrawSolidBox(Subtract3(lPosition, mPointBoxRadius), Add3(lPosition, mPointBoxRadius), lColour);
    }

    void Debug3DImmediateRender::DrawAngleDeg(rw::math::vpu::Vector3 lPosition,
                                              f32 lfAngle, rw::RGBA lColour)
    {
        DrawAngleRad(lPosition, lfAngle * (KF_PI / 180.0f), lColour);
    }

    void Debug3DImmediateRender::DrawAngleRad(rw::math::vpu::Vector3 lPosition,
                                              f32 lfAngle, rw::RGBA lColour)
    {
        // ARTIST 0x8281D620: its inlined SinCos is packed into X/Z as {sin, 0, cos}.
        const Vector3 lDirection = { std::sin(lfAngle), 0.0f, std::cos(lfAngle), 0.0f };
        DrawLine(lPosition, Add3(lPosition, lDirection), lColour);
    }

    void Debug3DImmediateRender::DrawAxis(rw::math::vpu::Matrix44Affine lTransform/*, const rw::RGBA&*/ /*lrColour*/)
    {
        const Vector3 lOrigin = lTransform.Pos();
        DrawLine(lOrigin, Add3(lOrigin, lTransform.Right()), rw::RGBA(255, 0, 0, 255));
        DrawLine(lOrigin, Add3(lOrigin, lTransform.Up()), rw::RGBA(0, 255, 0, 255));
        DrawLine(lOrigin, Add3(lOrigin, lTransform.At()), rw::RGBA(0, 0, 255, 255));
    }

    void Debug3DImmediateRender::DrawAxis(rw::math::vpu::Matrix44Affine lTransform,
                                          rw::RGBA /*lColour*/)
    {
        DrawAxis(lTransform);
    }

    void Debug3DImmediateRender::DrawCircle(rw::math::vpu::Vector3 lCentre,
                                            rw::math::vpu::Vector3 lNormal,
                                            f32 lfRadius, rw::RGBA lColour)
    {
        Vector3 lRight, lUp;
        MakePerpendicularBasis(lNormal, lRight, lUp);
        const s32 KI_SEGMENTS = 16;
        Vector3 lPrevious = Add3(lCentre, Scale3(lRight, lfRadius));
        for (s32 i = 1; i <= KI_SEGMENTS; ++i)
        {
            const f32 lfAngle = KF_TWO_PI * static_cast<f32>(i) / static_cast<f32>(KI_SEGMENTS);
            const Vector3 lCurrent = Add3(lCentre,
                Add3(Scale3(lRight, std::cos(lfAngle) * lfRadius),
                    Scale3(lUp, std::sin(lfAngle) * lfRadius)));
            DrawLine(lPrevious, lCurrent, lColour);
            lPrevious = lCurrent;
        }
    }

    void Debug3DImmediateRender::DrawCircle(rw::math::vpu::Matrix44Affine lTransform,
                                            f32 lfRadius, rw::RGBA lColour)
    {
        DrawCircle(lTransform.Pos(), lTransform.At(), lfRadius, lColour);
    }

    void Debug3DImmediateRender::DrawSphere(rw::math::vpu::Vector3 lCentre,
                                            f32 lfRadius, rw::RGBA lColour)
    {
        // ARTIST 8281D930: one 192-vertex line strip through the original mesh.
        SetProjectionMode(E_PROJECTION_3D);
        SetDrawingMode(E_DRAWING_TRISTRIP_LINES);
        Vector3 laVertices[34];
        GetSphereVertices(laVertices, lCentre, lfRadius);
        for (s32 liIndex = 0; liIndex < 192; ++liIndex)
            AddVertex(laVertices[maSphereIndices[liIndex]], lColour);
        DispatchVertices();
    }

    // ARTIST 82816EC0: two pole fans and three bands, with each seam wrapped
    // to its first vertex. DecFIGS declares 34 vertices and 192 byte indices.
    void Debug3DImmediateRender::BuildSphereIndices()
    {
        for (s32 liIndex = 0; liIndex < 8; ++liIndex)
        {
            const s32 liBase = 3 * liIndex;
            maSphereIndices[liBase] = 0;
            maSphereIndices[liBase + 1] = static_cast<u8>(liIndex + 2);
            maSphereIndices[liBase + 2] = static_cast<u8>(liIndex == 7 ? 2 : liIndex + 3);
        }
        for (s32 liBand = 0; liBand < 3; ++liBand)
        {
            const s32 liVertexOffset = 2 + 8 * liBand;
            for (s32 liIndex = 0; liIndex < 8; ++liIndex)
            {
                const s32 liBase = 24 + 48 * liBand + 6 * liIndex;
                const s32 liNext = liIndex == 7 ? 0 : liIndex + 1;
                maSphereIndices[liBase] = static_cast<u8>(liVertexOffset + liIndex);
                maSphereIndices[liBase + 1] = static_cast<u8>(liVertexOffset + 8 + liIndex);
                maSphereIndices[liBase + 2] = static_cast<u8>(liVertexOffset + liNext);
                maSphereIndices[liBase + 3] = static_cast<u8>(liVertexOffset + 8 + liIndex);
                maSphereIndices[liBase + 4] = static_cast<u8>(liVertexOffset + 8 + liNext);
                maSphereIndices[liBase + 5] = static_cast<u8>(liVertexOffset + liNext);
            }
        }
        for (s32 liIndex = 0; liIndex < 8; ++liIndex)
        {
            const s32 liBase = 168 + 3 * liIndex;
            maSphereIndices[liBase] = 1;
            maSphereIndices[liBase + 1] = static_cast<u8>(liIndex == 7 ? 26 : liIndex + 27);
            maSphereIndices[liBase + 2] = static_cast<u8>(liIndex + 26);
        }
    }

    // ARTIST 82817018: four eight-vertex rings, poles first. The latitude
    // and longitude steps are the binary's single-precision constants.
    void Debug3DImmediateRender::GetSphereVertices(Vector3* lpVertexArray,
                                                   Vector3 lPosition, f32 lfRadius)
    {
        lpVertexArray[0] = Make3(lPosition.x, lPosition.y + lfRadius, lPosition.z, lPosition.w);
        lpVertexArray[1] = Make3(lPosition.x, lPosition.y - lfRadius, lPosition.z, lPosition.w);
        for (s32 liRing = 0; liRing < 4; ++liRing)
        {
            const f32 lfLatitude = static_cast<f32>(liRing + 1) * 0.62831855f;
            const f32 lfSinLatitude = std::sin(lfLatitude);
            for (s32 liIndex = 0; liIndex < 8; ++liIndex)
            {
                const f32 lfLongitude = static_cast<f32>(liIndex) * 0.78539819f;
                const f32 lfX = (std::sin(lfLongitude) * lfSinLatitude) * lfRadius;
                const f32 lfY = std::cos(lfLatitude) * lfRadius;
                const f32 lfZ = (lfSinLatitude * std::cos(lfLongitude)) * lfRadius;
                lpVertexArray[2 + 8 * liRing + liIndex] = Make3(
                    lPosition.x + lfX, lPosition.y + lfY, lPosition.z + lfZ, lPosition.w);
            }
        }
    }

    void Debug3DImmediateRender::DrawHollowSphere(rw::math::vpu::Vector3 lCentre,
                                                  f32 lfRadius, rw::RGBA lColour)
    {
        DrawSphere(lCentre, lfRadius, lColour);
    }

    void Debug3DImmediateRender::DrawSolidSphere(rw::math::vpu::Vector3 lCentre,
                                                 f32 lfRadius, rw::RGBA lColour)
    {
        const s32 KI_SLICES = 12;
        const s32 KI_STACKS = 6;
        for (s32 y = 0; y < KI_STACKS; ++y)
        {
            const f32 lfLat0 = -KF_PI * 0.5f + KF_PI * static_cast<f32>(y) / KI_STACKS;
            const f32 lfLat1 = -KF_PI * 0.5f + KF_PI * static_cast<f32>(y + 1) / KI_STACKS;
            for (s32 x = 0; x < KI_SLICES; ++x)
            {
                const f32 lfLon0 = KF_TWO_PI * static_cast<f32>(x) / KI_SLICES;
                const f32 lfLon1 = KF_TWO_PI * static_cast<f32>(x + 1) / KI_SLICES;
                const Vector3 a = Add3(lCentre, Make3( std::cos(lfLat0) * std::cos(lfLon0) * lfRadius,
                                                std::sin(lfLat0) * lfRadius,
                                                std::cos(lfLat0) * std::sin(lfLon0) * lfRadius, 0.0f ));
                const Vector3 b = Add3(lCentre, Make3( std::cos(lfLat0) * std::cos(lfLon1) * lfRadius,
                                                std::sin(lfLat0) * lfRadius,
                                                std::cos(lfLat0) * std::sin(lfLon1) * lfRadius, 0.0f ));
                const Vector3 c = Add3(lCentre, Make3( std::cos(lfLat1) * std::cos(lfLon1) * lfRadius,
                                                std::sin(lfLat1) * lfRadius,
                                                std::cos(lfLat1) * std::sin(lfLon1) * lfRadius, 0.0f ));
                const Vector3 d = Add3(lCentre, Make3( std::cos(lfLat1) * std::cos(lfLon0) * lfRadius,
                                                std::sin(lfLat1) * lfRadius,
                                                std::cos(lfLat1) * std::sin(lfLon0) * lfRadius, 0.0f ));
                DrawSolidQuad(a, b, c, d, lColour);
            }
        }
    }

    void Debug3DImmediateRender::DrawArrow(rw::math::vpu::Vector3 lFrom,
                                           rw::math::vpu::Vector3 lTo, rw::RGBA lColour)
    {
        DrawLine(lFrom, lTo, lColour);
        const Vector3 lDelta = Subtract3(lTo, lFrom);
        const f32 lfLength = Length3(lDelta);
        if (lfLength <= 0.000001f)
            return;

        const Vector3 lDirection = Scale3(lDelta, 1.0f / lfLength);
        const f32 lfHeadSize = (lfLength * 0.2f < 0.5f) ? lfLength * 0.2f : 0.5f;
        const Vector3 lHeadBase = Subtract3(lTo, Scale3(lDirection, lfHeadSize));
        Vector3 lRight, lUp;
        MakePerpendicularBasis(lDirection, lRight, lUp);
        lRight = Scale3(lRight, lfHeadSize * 0.5f);
        lUp = Scale3(lUp, lfHeadSize * 0.5f);
        const Vector3 a = Add3(lHeadBase, lRight);
        const Vector3 b = Subtract3(lHeadBase, lRight);
        const Vector3 c = Add3(lHeadBase, lUp);
        const Vector3 d = Subtract3(lHeadBase, lUp);
        DrawLine(a, lTo, lColour); DrawLine(b, lTo, lColour);
        DrawLine(c, lTo, lColour); DrawLine(d, lTo, lColour);
        DrawLine(a, c, lColour); DrawLine(c, b, lColour);
        DrawLine(b, d, lColour); DrawLine(d, a, lColour);
    }

    void Debug3DImmediateRender::DrawSolidArrow(rw::math::vpu::Vector3 lFrom,
                                                rw::math::vpu::Vector3 lTo,
                                                rw::RGBA lColour)
    {
        // ARTIST 0x8281E968: line shaft, then a capped head whose length is
        // min(totalLength*0.2, 0.5) and half-width is half that length. Each of the two
        // perpendicular fins is emitted in both windings so it remains visible with culling.
        DrawLine(lFrom, lTo, lColour);
        const Vector3 lDelta = Subtract3(lTo, lFrom);
        const f32 lfLength = Length3(lDelta);
        if (lfLength <= 0.000001f)
            return;

        const Vector3 lDirection = Scale3(lDelta, 1.0f / lfLength);
        const f32 lfHeadSize = (lfLength * 0.2f < 0.5f) ? lfLength * 0.2f : 0.5f;
        const Vector3 lHeadBase = Subtract3(lTo, Scale3(lDirection, lfHeadSize));
        Vector3 lRight, lUp;
        MakePerpendicularBasis(lDirection, lRight, lUp);
        lRight = Scale3(lRight, lfHeadSize * 0.5f);
        lUp = Scale3(lUp, lfHeadSize * 0.5f);

        const Vector3 lRightA = Add3(lHeadBase, lRight);
        const Vector3 lRightB = Subtract3(lHeadBase, lRight);
        DrawSolidTriangle(lRightA, lRightB, lTo, lColour);
        DrawSolidTriangle(lRightB, lRightA, lTo, lColour);

        const Vector3 lUpA = Add3(lHeadBase, lUp);
        const Vector3 lUpB = Subtract3(lHeadBase, lUp);
        DrawSolidTriangle(lUpA, lUpB, lTo, lColour);
        DrawSolidTriangle(lUpB, lUpA, lTo, lColour);
    }

    void Debug3DImmediateRender::DrawCylinder(rw::math::vpu::Vector3 lStart,
                                              rw::math::vpu::Vector3 lEnd,
                                              f32 lfRadius, rw::RGBA lColour)
    {
        const Vector3 lAxis = Normalise(Subtract3(lEnd, lStart));
        Vector3 lRight, lUp;
        MakePerpendicularBasis(lAxis, lRight, lUp);
        const s32 KI_SEGMENTS = 16;
        Vector3 lPreviousStart = Add3(lStart, Scale3(lRight, lfRadius));
        Vector3 lPreviousEnd = Add3(lEnd, Scale3(lRight, lfRadius));
        for (s32 i = 1; i <= KI_SEGMENTS; ++i)
        {
            const f32 lfAngle = KF_TWO_PI * static_cast<f32>(i) / KI_SEGMENTS;
            const Vector3 lOffset = Add3(Scale3(lRight, std::cos(lfAngle) * lfRadius),
                                         Scale3(lUp, std::sin(lfAngle) * lfRadius));
            const Vector3 lCurrentStart = Add3(lStart, lOffset);
            const Vector3 lCurrentEnd = Add3(lEnd, lOffset);
            DrawLine(lPreviousStart, lCurrentStart, lColour);
            DrawLine(lPreviousEnd, lCurrentEnd, lColour);
            DrawLine(lPreviousStart, lPreviousEnd, lColour);
            lPreviousStart = lCurrentStart;
            lPreviousEnd = lCurrentEnd;
        }
    }

    void Debug3DImmediateRender::DrawCapsule(rw::math::vpu::Vector3 lStart,
                                             rw::math::vpu::Vector3 lEnd,
                                             f32 lfRadius, rw::RGBA lColour)
    {
        DrawCylinder(lStart, lEnd, lfRadius, lColour);
        DrawSphere(lStart, lfRadius, lColour);
        DrawSphere(lEnd, lfRadius, lColour);
    }

    void Debug3DImmediateRender::DrawText(rw::math::vpu::Vector3 lWorldPosition,
                                          const char* lpcText, f32 lfScale,
                                          rw::RGBA lColour)
    {
        // ARTIST 8281FCB8..8281FE78: project to the vector font's normalised
        // coordinates and record its strokes in this renderer's existing block.
        SetProjectionMode(E_PROJECTION_2D);
#if defined(_XBOX)
        // ARTIST 8281FCDC..8281FDD8: three VMX fused projection steps and
        // vrefp followed by two Newton refinements for each projected lane.
        const __vector4 world = __lvx(&lWorldPosition, 0);
        const __vector4 x = __vspltw(world, 0);
        const __vector4 y = __vspltw(world, 1);
        const __vector4 z = __vspltw(world, 2);
        const __vector4 row0 = __lvx(&mViewProjectionMatrix.xAxis, 0);
        const __vector4 row1 = __lvx(&mViewProjectionMatrix.yAxis, 0);
        const __vector4 row2 = __lvx(&mViewProjectionMatrix.zAxis, 0);
        const __vector4 row3 = __lvx(&mViewProjectionMatrix.wAxis, 0);
        const __vector4 clip = __vmaddfp(row2, z, __vmaddfp(row1, y, __vmaddfp(row0, x, row3)));
        __declspec(align(16)) f32 values[4];
        __stvx(clip, values, 0);
        if (values[2] < 0.001f) return;
        const __vector4 depth = __vspltw(clip, 2);
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        __vector4 inv = __vrefp(depth);
        inv = __vmaddfp(inv, __vnmsubfp(inv, depth, one), inv);
        inv = __vmaddfp(inv, __vnmsubfp(inv, depth, one), inv);
        const __vector4 projected = __vmulfp(clip, inv);
        __stvx(projected, values, 0);
        const f32 lfProjectedX = values[0];
        __declspec(align(16)) f32 negative[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
        __stvx(__vmulfp(projected, __lvx(negative, 0)), values, 0);
        const f32 lfProjectedY = values[1];
        const f32 lfTextSize = lfScale / (mfVirtualScreenHeight * 0.5f);
#else
        const auto Flush = [](f32 lfValue)
        {
            return std::fpclassify(lfValue) == FP_SUBNORMAL
                ? std::copysign(0.0f, lfValue) : lfValue;
        };
        const auto MultiplyAdd = [&](f32 lfA, f32 lfB, f32 lfC)
        {
            return Flush(std::fmaf(Flush(lfA), Flush(lfB), Flush(lfC)));
        };
        // 8281FCEC/FD14/FD20 fuse each row in x, y, z order, starting at row3.
        const auto ProjectLane = [&](f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
        {
            return MultiplyAdd(lfZ, lWorldPosition.z,
                MultiplyAdd(lfY, lWorldPosition.y, MultiplyAdd(lfX, lWorldPosition.x, lfW)));
        };
        const Matrix44& lrMatrix = mViewProjectionMatrix;
        const ClipPoint lClip = {
            ProjectLane(lrMatrix.xAxis.x, lrMatrix.yAxis.x, lrMatrix.zAxis.x, lrMatrix.wAxis.x),
            ProjectLane(lrMatrix.xAxis.y, lrMatrix.yAxis.y, lrMatrix.zAxis.y, lrMatrix.wAxis.y),
            ProjectLane(lrMatrix.xAxis.z, lrMatrix.yAxis.z, lrMatrix.zAxis.z, lrMatrix.wAxis.z),
            ProjectLane(lrMatrix.xAxis.w, lrMatrix.yAxis.w, lrMatrix.zAxis.w, lrMatrix.wAxis.w),
        };
        if (lClip.z < 0.001f)
            return;
        // 8281FD48..FDA8 and FDC4..FDD4 independently refine vrefp twice.
        // Keep the unconditional refinements for zero, infinity and NaN too.
        const auto RefinedReciprocal = [&](f32 lfValue)
        {
            lfValue = Flush(lfValue);
            // FLAG (model): correctly rounded vrefp seed; hardware estimate ULPs
            // and NaN payloads remain unverified. VMX operands/results flush.
            f32 lfEstimate = Flush(static_cast<f32>(1.0 / static_cast<f64>(lfValue)));
            for (s32 liStep = 0; liStep < 2; ++liStep)
            {
                const f32 lfDifference = MultiplyAdd(lfEstimate, lfValue, -1.0f);
                const f32 lfError = lfDifference != lfDifference ? lfDifference : -lfDifference;
                lfEstimate = MultiplyAdd(lfEstimate, lfError, lfEstimate);
            }
            return lfEstimate;
        };
        const f32 lfInvZForX = RefinedReciprocal(lClip.z);
        const f32 lfInvZForY = RefinedReciprocal(lClip.z);
        const f32 lfTextSize = lfScale / (mfVirtualScreenHeight * 0.5f);
        const f32 lfProjectedX = Flush(lClip.x * lfInvZForX);
        const f32 lfProjectedY = -Flush(lClip.y * lfInvZForY);
#endif
        SetDrawingMode(E_DRAWING_LINES);
        mVectorFont.SetSize(Make2(lfTextSize, lfTextSize, 0.0f, 0.0f));
        mVectorFont.Print(lfProjectedX, lfProjectedY,
                          lpcText, lColour.m_rgba);
    }

    void Debug3DImmediateRender::Draw2DLineForVectorFont(rw::math::vpu::Vector3 lFrom,
                                                       rw::math::vpu::Vector3 lTo,
                                                       rw::RGBA lColour)
    {
        SetProjectionMode(E_PROJECTION_2D);
        SetDrawingMode(E_DRAWING_LINES);
        AddLine(lFrom, lTo, lColour);
    }
}
