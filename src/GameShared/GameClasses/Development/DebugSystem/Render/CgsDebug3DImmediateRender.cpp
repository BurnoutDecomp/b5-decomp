#if !defined(D_PLATFORM_X360)
// The RenderWare headers define NOUSER before including Windows headers.  D3D9 still
// requires winuser's LPMSG, so establish the complete Win32/D3D declarations first.
#include <Windows.h>
#include <d3d9.h>
#undef GetNextWindow
#undef DrawText
#endif

#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "rw/math/vpu/matrix44affine_operation.h"

#include <cmath>
#include <cstring>

#if !defined(D_PLATFORM_X360)
#include "pc/gcm/renderengine/device.h"
#endif

// CgsDev::Debug3DImmediateRender. Primitive construction follows the ARTIST functions in
// CgsDebug3DImmediateRender.cpp. The shipping renderer submits those vertices through
// CgsGraphics::Im3dRenderBuffer; on PC that console buffer is not a host renderer, so
// DispatchVertices performs the equivalent fixed-function D3D9 submission after applying the
// frame's view-projection matrix.

namespace
{
    using rw::math::vpu::Matrix44;
    using rw::math::vpu::Matrix44Affine;
    using rw::math::vpu::Vector3;

    const f32 KF_PI = 3.14159265358979323846f;
    const f32 KF_TWO_PI = KF_PI * 2.0f;

    Vector3 Add3(Vector3 lA, Vector3 lB)
    {
        return { lA.x + lB.x, lA.y + lB.y, lA.z + lB.z, 0.0f };
    }

    Vector3 Subtract3(Vector3 lA, Vector3 lB)
    {
        return { lA.x - lB.x, lA.y - lB.y, lA.z - lB.z, 0.0f };
    }

    Vector3 Scale3(Vector3 lValue, f32 lfScale)
    {
        return { lValue.x * lfScale, lValue.y * lfScale, lValue.z * lfScale, 0.0f };
    }

    f32 Dot3(Vector3 lA, Vector3 lB)
    {
        return lA.x * lB.x + lA.y * lB.y + lA.z * lB.z;
    }

    Vector3 Cross3(Vector3 lA, Vector3 lB)
    {
        return {
            lA.y * lB.z - lA.z * lB.y,
            lA.z * lB.x - lA.x * lB.z,
            lA.x * lB.y - lA.y * lB.x,
            0.0f,
        };
    }

    f32 Length3(Vector3 lValue)
    {
        return std::sqrt(Dot3(lValue, lValue));
    }

    Vector3 Normalise(Vector3 lValue)
    {
        const f32 lfLength = Length3(lValue);
        return lfLength > 0.000001f ? Scale3(lValue, 1.0f / lfLength)
                                    : Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
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
            ? Vector3{ 0.0f, 1.0f, 0.0f, 0.0f }
            : Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
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
        return {
            lrMatrix.xAxis.x * x + lrMatrix.yAxis.x * y + lrMatrix.zAxis.x * z + lrMatrix.wAxis.x,
            lrMatrix.xAxis.y * x + lrMatrix.yAxis.y * y + lrMatrix.zAxis.y * z + lrMatrix.wAxis.y,
            lrMatrix.xAxis.z * x + lrMatrix.yAxis.z * y + lrMatrix.zAxis.z * z + lrMatrix.wAxis.z,
            lrMatrix.xAxis.w * x + lrMatrix.yAxis.w * y + lrMatrix.zAxis.w * z + lrMatrix.wAxis.w,
        };
    }
}

namespace CgsDev
{
    void Debug3DImmediateRender::Construct(rw::IResourceAllocator* /*lpAllocator*/,
                                           f32 lfVirtualScreenWidth, f32 lfVirtualScreenHeight)
    {
        meProjectionMode = E_PROJECTION_3D;
        meDrawingMode = E_DRAWING_COUNT;
        meZTestEnable = E_ZTEST_ON;
        mPointBoxRadius = { 0.05f, 0.05f, 0.05f, 0.0f };
        mfVirtualScreenWidth = lfVirtualScreenWidth;
        mfVirtualScreenHeight = lfVirtualScreenHeight;
        mpRenderBuffer = nullptr;
        mpFont.Clear();
        mVectorFont.Construct();
        miIm3dVertsHead = 0;
        mViewProjectionMatrix.SetIdentity();
        mCameraPosition.SetZero();
        mePrimitiveType = 0;
        std::memset(maSphereIndices, 0, sizeof(maSphereIndices));
    }

    void Debug3DImmediateRender::SetDebugFont(
        const CgsResource::SafeResourceHandle<CgsResource::Font>& lrFont)
    {
        mpFont = lrFont;
    }

    void Debug3DImmediateRender::Begin(const rw::math::vpu::Matrix44& lrViewProjection,
                                       rw::math::vpu::Vector3 lCameraPosition)
    {
        CGS_ASSERT(mpRenderBuffer != nullptr, "mpRenderBuffer");
        mViewProjectionMatrix = lrViewProjection;
        mCameraPosition = lCameraPosition;
        miIm3dVertsHead = 0;
        meDrawingMode = E_DRAWING_COUNT;
        meProjectionMode = E_PROJECTION_3D;
        meZTestEnable = E_ZTEST_ON;
    }

    void Debug3DImmediateRender::End()
    {
        DispatchVertices();
        miIm3dVertsHead = 0;
    }

    rw::math::vpu::Vector2 Debug3DImmediateRender::GetVirtualScreenSize() const
    {
        return { mfVirtualScreenWidth, mfVirtualScreenHeight, 0.0f, 0.0f };
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
            DispatchVertices();
            meProjectionMode = leMode;
        }
    }

    void Debug3DImmediateRender::SetZTestEnable(ZTestEnable leEnable)
    {
        if (meZTestEnable != leEnable)
        {
            DispatchVertices();
            meZTestEnable = leEnable;
        }
    }

    void Debug3DImmediateRender::SetDrawingMode(DrawingMode leMode)
    {
        if (meDrawingMode != leMode)
        {
            DispatchVertices();
            meDrawingMode = leMode;
        }
    }

    void Debug3DImmediateRender::AddVertex(rw::math::vpu::Vector3 lPosition, rw::RGBA lColour)
    {
        if (miIm3dVertsHead >= KI_VERTEX_BUFFER_SIZE)
            DispatchVertices();

        CgsGraphics::BasicColouredTexturedVertex& lrVertex = maIm3dVertsArray[miIm3dVertsHead++];
        lrVertex.mv3Pos = { lPosition.x, lPosition.y, lPosition.z };
        lrVertex.mv4Colour.r = static_cast<u8>(lColour.m_rgba);
        lrVertex.mv4Colour.g = static_cast<u8>(lColour.m_rgba >> 8);
        lrVertex.mv4Colour.b = static_cast<u8>(lColour.m_rgba >> 16);
        lrVertex.mv4Colour.a = static_cast<u8>(lColour.m_rgba >> 24);
        lrVertex.mv2Tex0UV = { 0.5f, 0.5f };
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
        if (miIm3dVertsHead <= 0)
            return;

#if !defined(D_PLATFORM_X360)
        // FLAG PC-platform leaf: ARTIST submits the same world vertices through the X360
        // Im3dRenderBuffer. The host uses pre-transformed D3D9 vertices because RendererModule
        // currently publishes an opaque console buffer object.
        IDirect3DDevice9* lpDevice = renderengine::gDevice;
        if (lpDevice)
        {
            struct ScreenVertex { f32 x, y, z, rhw; DWORD colour; };
            static ScreenVertex saVertices[KI_VERTEX_BUFFER_SIZE];

            for (s32 liIndex = 0; liIndex < miIm3dVertsHead; ++liIndex)
            {
                const CgsGraphics::BasicColouredTexturedVertex& lrSource = maIm3dVertsArray[liIndex];
                const ClipPoint lClip = TransformToClip(mViewProjectionMatrix, lrSource);
                const f32 lfInvW = std::fabs(lClip.w) > 0.000001f ? 1.0f / lClip.w : 0.0f;
                ScreenVertex& lrDest = saVertices[liIndex];
                lrDest.x = (lClip.x * lfInvW * 0.5f + 0.5f) * static_cast<f32>(renderengine::gDisplayWidth);
                lrDest.y = (0.5f - lClip.y * lfInvW * 0.5f) * static_cast<f32>(renderengine::gDisplayHeight);
                lrDest.z = lClip.z * lfInvW;
                lrDest.rhw = 1.0f;
                lrDest.colour = D3DCOLOR_ARGB(lrSource.mv4Colour.a, lrSource.mv4Colour.r,
                                               lrSource.mv4Colour.g, lrSource.mv4Colour.b);
            }

            lpDevice->SetVertexShader(nullptr);
            lpDevice->SetPixelShader(nullptr);
            lpDevice->SetTexture(0, nullptr);
            lpDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
            lpDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
            lpDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            lpDevice->SetRenderState(D3DRS_ZENABLE, meZTestEnable == E_ZTEST_ON ? TRUE : FALSE);
            lpDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            lpDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            lpDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            lpDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
            lpDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
            lpDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
            lpDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

            if (meDrawingMode == E_DRAWING_LINES || meDrawingMode == E_DRAWING_TRISTRIP_LINES)
                lpDevice->DrawPrimitiveUP(D3DPT_LINELIST, static_cast<UINT>(miIm3dVertsHead / 2),
                                          saVertices, sizeof(ScreenVertex));
            else
                lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(miIm3dVertsHead / 3),
                                          saVertices, sizeof(ScreenVertex));
        }
#endif
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
        DrawCircle(lCentre, { 1.0f, 0.0f, 0.0f, 0.0f }, lfRadius, lColour);
        DrawCircle(lCentre, { 0.0f, 1.0f, 0.0f, 0.0f }, lfRadius, lColour);
        DrawCircle(lCentre, { 0.0f, 0.0f, 1.0f, 0.0f }, lfRadius, lColour);
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
                const Vector3 a = Add3(lCentre, { std::cos(lfLat0) * std::cos(lfLon0) * lfRadius,
                                                std::sin(lfLat0) * lfRadius,
                                                std::cos(lfLat0) * std::sin(lfLon0) * lfRadius, 0.0f });
                const Vector3 b = Add3(lCentre, { std::cos(lfLat0) * std::cos(lfLon1) * lfRadius,
                                                std::sin(lfLat0) * lfRadius,
                                                std::cos(lfLat0) * std::sin(lfLon1) * lfRadius, 0.0f });
                const Vector3 c = Add3(lCentre, { std::cos(lfLat1) * std::cos(lfLon1) * lfRadius,
                                                std::sin(lfLat1) * lfRadius,
                                                std::cos(lfLat1) * std::sin(lfLon1) * lfRadius, 0.0f });
                const Vector3 d = Add3(lCentre, { std::cos(lfLat1) * std::cos(lfLon0) * lfRadius,
                                                std::sin(lfLat1) * lfRadius,
                                                std::cos(lfLat1) * std::sin(lfLon0) * lfRadius, 0.0f });
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
        if (!lpcText)
            return;

#if !defined(D_PLATFORM_X360)
        // FLAG PC-platform leaf: project to the host 2D renderer. ARTIST performs this same divide
        // then sends the glyphs through the 3D buffer's TextRenderer.
        CgsGraphics::BasicColouredTexturedVertex lSource = {};
        lSource.mv3Pos = { lWorldPosition.x, lWorldPosition.y, lWorldPosition.z };
        const ClipPoint lClip = TransformToClip(mViewProjectionMatrix, lSource);
        if (lClip.w <= 0.000001f)
            return;
        const f32 lfInvW = 1.0f / lClip.w;
        const f32 lfX = (lClip.x * lfInvW * 0.5f + 0.5f) * mfVirtualScreenWidth;
        const f32 lfY = (0.5f - lClip.y * lfInvW * 0.5f) * mfVirtualScreenHeight;
        DebugManager::GetInstance()->GetUI().Get2DRenderer()->DrawText(
            lpcText, lfX, lfY, lfScale, lColour.m_rgba);
#endif
    }
}
