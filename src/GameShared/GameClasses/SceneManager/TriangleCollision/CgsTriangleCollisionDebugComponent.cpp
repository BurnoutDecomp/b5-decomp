#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h"

#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox4.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupList.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManager.h"
#include "SDKs/XboxMath/XMVectorACos.h"

#include <cmath>
#include <cstring>

namespace
{
    using rw::math::vpu::Vector3;

    f32 FloatFromTriangleDebugBits(u32 luBits)
    {
        f32 lfValue;
        std::memcpy(&lfValue, &luBits, sizeof(lfValue));
        return lfValue;
    }

    // C++ form of the packed-vertex expansion performed by ARTIST sub_828AF0D0.
    // The PC world-support porter stores these 16-bit lanes in host byte order;
    // the named GetVertex accessor supplies the record and its original bounds assert.
    Vector3* UnpackSub828AF0D0(Vector3* lpOut,
                               const CgsGeometric::PolygonSoup& lrSoup,
                               u8 lu8VertexIndex)
    {
        const u8* lpPackedVertex = lrSoup.GetVertex(lu8VertexIndex);
        u16 lau16Packed[3];
        std::memcpy(lau16Packed, lpPackedVertex, sizeof(lau16Packed));

        const u32 luX = static_cast<u32>(lrSoup.miPosX) + static_cast<u32>(lau16Packed[0]);
        const u32 luY = static_cast<u32>(lrSoup.miPosY) + static_cast<u32>(lau16Packed[1]);
        const u32 luZ = static_cast<u32>(lrSoup.miPosZ) + static_cast<u32>(lau16Packed[2]);
        lpOut->Set(static_cast<f32>(static_cast<s32>(luX)) * lrSoup.mfScale,
                   static_cast<f32>(static_cast<s32>(luY)) * lrSoup.mfScale,
                   static_cast<f32>(static_cast<s32>(luZ)) * lrSoup.mfScale);
        lpOut->mV.mafLane[3] = 0.0f;
        return lpOut;
    }

    void SinCosTriangleDebugAngle(f32 lfAngle, f32& rfSine, f32& rfCosine)
    {
        // The coefficients are the original 0x82000BD0/0x82000C00 Taylor tables
        // used by DrawPolySoupPoly's inline axis-angle transform.
        static const u32 KAU_SIN_COEFFICIENT_BITS[12] =
        {
            0x3F800000u, 0xBE2AAAABu, 0x3C088889u, 0xB9500D01u,
            0x3638EF1Du, 0xB2D7322Bu, 0x2F309231u, 0xAB573F9Fu,
            0x274A963Cu, 0xA317A4DAu, 0x1EB8DC78u, 0x9A3B0DA1u
        };
        static const u32 KAU_COS_COEFFICIENT_BITS[12] =
        {
            0x3F800000u, 0xBF000000u, 0x3D2AAAABu, 0xBAB60B61u,
            0x37D00D01u, 0xB493F27Eu, 0x310F76C8u, 0xAD49CBA5u,
            0x29573F9Fu, 0xA53413C3u, 0x20F2A15Du, 0x9C8671CBu
        };

        const f32 lfAngleSquared = lfAngle * lfAngle;
        f32 lfSinPolynomial = FloatFromTriangleDebugBits(KAU_SIN_COEFFICIENT_BITS[11]);
        f32 lfCosPolynomial = FloatFromTriangleDebugBits(KAU_COS_COEFFICIENT_BITS[11]);
        for (s32 liCoefficient = 10; liCoefficient >= 0; --liCoefficient)
        {
            lfSinPolynomial = std::fma(
                lfSinPolynomial, lfAngleSquared,
                FloatFromTriangleDebugBits(KAU_SIN_COEFFICIENT_BITS[liCoefficient]));
            lfCosPolynomial = std::fma(
                lfCosPolynomial, lfAngleSquared,
                FloatFromTriangleDebugBits(KAU_COS_COEFFICIENT_BITS[liCoefficient]));
        }

        rfSine = lfAngle * lfSinPolynomial;
        rfCosine = lfCosPolynomial;
    }

    Vector3 RotateTriangleDebugNormal(const Vector3& lrNormal,
                                      const Vector3& lrAxis,
                                      f32 lfSine,
                                      f32 lfCosine)
    {
        const f32 lfAxisDotNormal = lrAxis.x * lrNormal.x
                                  + lrAxis.y * lrNormal.y
                                  + lrAxis.z * lrNormal.z;
        const Vector3 lvAxisCrossNormal(
            std::fma(lrAxis.y, lrNormal.z, -lrAxis.z * lrNormal.y),
            std::fma(lrAxis.z, lrNormal.x, -lrAxis.x * lrNormal.z),
            std::fma(lrAxis.x, lrNormal.y, -lrAxis.y * lrNormal.x));
        const f32 lfAxisTerm = lfAxisDotNormal * (1.0f - lfCosine);
        const Vector3 lvRotated(
            std::fma(lrNormal.x, lfCosine,
                     std::fma(lvAxisCrossNormal.x, lfSine, lrAxis.x * lfAxisTerm)),
            std::fma(lrNormal.y, lfCosine,
                     std::fma(lvAxisCrossNormal.y, lfSine, lrAxis.y * lfAxisTerm)),
            std::fma(lrNormal.z, lfCosine,
                     std::fma(lvAxisCrossNormal.z, lfSine, lrAxis.z * lfAxisTerm)));
        Vector3 lvResult(lvRotated);
        lvResult.mV.mafLane[3] = 0.0f;
        return lvResult;
    }
}

namespace CgsSceneManager
{
    DrawCollisionPolyCallbackData
        TriangleCollisionDebugComponent::saDrawPolyCallbacks[KI_MAX_NUM_COLLISION_POLY_CALLBACKS] = {};
    s32 TriangleCollisionDebugComponent::siNumDrawPolyCallbacks = 0;

    void DrawCollisionPolyCallbackData::Construct(DrawCollisionPolyCallback* lpfCallback,
                                                  void* lpUserData)
    {
        mpCallbackFunc = lpfCallback;
        mpUserData = lpUserData;
    }

    DrawCollisionPolyCallbackData::DrawCollisionPolyCallback*
    DrawCollisionPolyCallbackData::GetCallback()
    {
        return mpCallbackFunc;
    }

    void* DrawCollisionPolyCallbackData::GetUserData()
    {
        return mpUserData;
    }

    // ARTIST 0x828AAF18. A full four-entry registry returns false; otherwise the current
    // count selects a slot and is incremented before its callback/user-data pair is stored.
    bool TriangleCollisionDebugComponent::RegisterDrawCollisionPolyCallback(
        DrawCollisionPolyCallbackData::DrawCollisionPolyCallback* lpfCallback,
        void* lpUserData)
    {
        CGS_ASSERT(lpfCallback != 0, "lpHandlerFunc != NULL");
        if (siNumDrawPolyCallbacks >= KI_MAX_NUM_COLLISION_POLY_CALLBACKS)
            return false;

        DrawCollisionPolyCallbackData& lrSlot =
            GetDrawCollisionPolyCallback(siNumDrawPolyCallbacks++);
        CGS_ASSERT(lpfCallback != 0, "lpCallbackFunc != NULL");
        lrSlot.Construct(lpfCallback, lpUserData);
        return true;
    }

    // ARTIST 0x828B3340. Seed the query controls, collision-test state, map defaults and
    // six CPU monitors exactly as stored by the X360 constructor body.
    void TriangleCollisionDebugComponent::Construct(
        const TriangleCollisionManager* lpTriCollisionManager)
    {
        CGS_ASSERT(lpTriCollisionManager != 0, "lpTriCollisionManager != NULL");
        mpTriCollisionManager = lpTriCollisionManager;

        mfSoupDrawDistance = 27.5f;
        mbUpdateViewFrustum = true;
        mbDrawPolySoups = false;
        mbDrawSoupAabbs = false;
        // The X360 does not store mbDrawPolyNormals in this body.
        mbDrawEdgeCosines = false;

        mbTestSphere = false;
        mSpherePos.SetZero();
        mfSphereRad = 0.5f;
        mbNeedToInitSpherePos = true;
        muSphereTestCycles = 0;
        mfSphereTestTimeSecs = 0.0f;
        mfSphereTestTimePercentFrame = 0.0f;
        miSphereTestResultCount = 0;

        mbTestLine = false;
        mbNeedToInitLinePosition = true;
        mLineStartPosition.SetZero();
        mLineEndPosition.SetZero();

        mbTestBox = false;
        mbNeedToInitBoxPos = true;
        mBoxPos.SetZero();
        mBoxSize.Set(1.0f, 1.0f, 1.0f);
        mBoxOrientation.SetZero();
        muBoxTestCycles = 0;
        miNumBoxTests = 0;
        muBoxTestCyclesPerTriangle = 0;
        mfBoxTestTime = 0.0f;
        mfBoxTestPercent = 0.0f;

        mbTestSweptSphere = false;
        mbNeedToInitSweptSpherePos = true;
        mSweptSpherePos.SetZero();
        mSweptSphereSize = 1.0f;
        mSweptSphereVelocity.SetZero();

        miMaxLevelToDraw = KI_MAX_NUM_LEVELS;
        miMinLevelToDraw = 0;
        mpcSpacialBuffer = 0;
        miNumLevels = 1;
        miLastNumLevels = 0;
        miMemUse = 0;
        miNumListsSearched = 0;

        const CgsDev::PerfMonCpuPage lePage = CgsDev::E_PMP_4;
        miSphereTestPerfMon = CgsDev::PerfMonCpu::AddMonitor(
            "Sphere test perf mon", lePage, false, 10.0f, true);
        miSphereTestGetListsPerfMon = CgsDev::PerfMonCpu::AddMonitor(
            "  ST: Get lists", lePage, false, 10.0f, true);
        miSphereTestProcessSoupsPerfMon = CgsDev::PerfMonCpu::AddMonitor(
            "  ST: Process soups", lePage, false, 10.0f, true);
        miLineTestPerfMon = CgsDev::PerfMonCpu::AddMonitor(
            "Line test perf mon", lePage, false, 10.0f, true);
        miLineTestGetListsPerfMon = CgsDev::PerfMonCpu::AddMonitor(
            "  LT: Get lists", lePage, false, 10.0f, true);
        miLineTestProcessSoupsPerfMon = CgsDev::PerfMonCpu::AddMonitor(
            "  LT: Process soups", lePage, false, 10.0f, true);
    }

    // ARTIST 0x828CAE88. Preserve the per-frame order: update the current frustum,
    // draw the previous spatial map, rebuild it if the level count changed, then run
    // each enabled collision query. The swept-sphere pass receives a null 2D renderer
    // exactly as the X360 caller's r5=0 argument does.
    void TriangleCollisionDebugComponent::RenderWorld(
        CgsDev::Debug3DImmediateRender* lpRenderer)
    {
        if (mbUpdateViewFrustum)
            UpdateViewFrustum(lpRenderer);

        if (mbDrawPolySoups)
            DrawPolySoups(lpRenderer);

        BuildSpacialMap();

        if (mbTestSphere)
            TestPolySoupSphereCollision(lpRenderer);

        if (mbTestBox)
            TestPolySoupBoxCollision(lpRenderer);

        if (mbTestSweptSphere)
            TestPolySoupSweptSphereCollision(lpRenderer, nullptr);
    }

    // ARTIST 0x828C0E20. Extract the six inward clip planes from the renderer's
    // row-vector view-projection matrix. rw::collision::Frustum uses the native
    // RenderWare plane order: near, far, left, right, top, bottom.
    void TriangleCollisionDebugComponent::UpdateViewFrustum(
        CgsDev::Debug3DImmediateRender* lpRenderer)
    {
        CGS_ASSERT(lpRenderer != 0, "lpDisplay != NULL");

        const rw::math::vpu::Matrix44 lViewProjection =
            lpRenderer->GetViewProjectionMatrix();
        const rw::math::vpu::Vector4 lColumn0(
            lViewProjection.xAxis.x, lViewProjection.yAxis.x,
            lViewProjection.zAxis.x, lViewProjection.wAxis.x);
        const rw::math::vpu::Vector4 lColumn1(
            lViewProjection.xAxis.y, lViewProjection.yAxis.y,
            lViewProjection.zAxis.y, lViewProjection.wAxis.y);
        const rw::math::vpu::Vector4 lColumn2(
            lViewProjection.xAxis.z, lViewProjection.yAxis.z,
            lViewProjection.zAxis.z, lViewProjection.wAxis.z);
        const rw::math::vpu::Vector4 lColumn3(
            lViewProjection.xAxis.w, lViewProjection.yAxis.w,
            lViewProjection.zAxis.w, lViewProjection.wAxis.w);

        // Clip-space inequalities are converted to dot(N, point) == D, the
        // Plane convention consumed by rw::collision::Frustum. The stored D is
        // therefore the negated homogeneous constant.
        const rw::math::vpu::Vector4 laClipPlanes[6] =
        {
            lColumn2,
            lColumn3 - lColumn2,
            lColumn3 + lColumn0,
            lColumn3 - lColumn0,
            lColumn3 - lColumn1,
            lColumn3 + lColumn1
        };

        for (u32 luPlane = 0; luPlane < 6; ++luPlane)
        {
            const rw::math::vpu::Vector4& lrClipPlane = laClipPlanes[luPlane];
            const rw::math::vpu::Vector4 lPlane(
                lrClipPlane.x, lrClipPlane.y, lrClipPlane.z, -lrClipPlane.w);
            mViewFrustum.SetPlane(luPlane, rw::collision::Plane(lPlane));
        }
    }

    // ARTIST 0x828C09D8. Cull each soup list against the camera, then cull each
    // soup by its typed AxisAlignedBox4 bounds or the original strict 100-unit
    // squared-distance limit. Polygon and optional AABB drawing retain soup-list
    // order, and the callback-facing liSoup remains the soup index within its list.
    void TriangleCollisionDebugComponent::DrawPolySoups(
        CgsDev::Debug3DImmediateRender* lpRenderer)
    {
        CGS_ASSERT(lpRenderer != 0, "lpDisplay != NULL");

        static const u32 KAU_SOUP_COLOURS[16] =
        {
            0x608080FFu, 0x6000FFFFu, 0x6080FF80u, 0x60FF0000u,
            0x60400080u, 0x60808040u, 0x60FFFF00u, 0x600080FFu,
            0x60FF0080u, 0x60C0C0C0u, 0x60008000u, 0x600000FFu,
            0x60800040u, 0x60000000u, 0x60FFFFFFu, 0x60C08000u
        };

        const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap =
            mpTriCollisionManager->GetPolySoupListSpacialMap();
        const s32 liNumPolySoupLists = lpSpatialMap->GetNumPolySoupLists();
        const rw::math::vpu::Vector3 lvCameraPosition = lpRenderer->GetCameraPosition();
        const rw::math::vpu::Vector4 lvCameraPoint(lvCameraPosition.GetVector());

        for (s32 liSoupList = 0; liSoupList < liNumPolySoupLists; ++liSoupList)
        {
            const CgsGeometric::PolygonSoupList* lpSoupList =
                lpSpatialMap->GetPolySoupList(liSoupList);

            CgsGeometric::AxisAlignedBox lSoupListBounds;
            lSoupListBounds.mMin.Set(lpSoupList->mOverallAabb[0],
                                     lpSoupList->mOverallAabb[1],
                                     lpSoupList->mOverallAabb[2],
                                     lpSoupList->mOverallAabb[3]);
            lSoupListBounds.mMax.Set(lpSoupList->mOverallAabb[4],
                                     lpSoupList->mOverallAabb[5],
                                     lpSoupList->mOverallAabb[6],
                                     lpSoupList->mOverallAabb[7]);
            if (!lSoupListBounds.ContainsPoint(lvCameraPoint))
                continue;

            const CgsGeometric::PolygonSoup* const* lppaPolySoups =
                reinterpret_cast<const CgsGeometric::PolygonSoup* const*>(
                    lpSoupList->mpapPolySoups);
            const CgsGeometric::AxisAlignedBox4* lpaSoupBounds =
                reinterpret_cast<const CgsGeometric::AxisAlignedBox4*>(
                    lpSoupList->mpaPolySoupBoxes);

            for (s32 liSoup = 0; liSoup < lpSoupList->miNumPolySoups; ++liSoup)
            {
                CGS_ASSERT(liSoup >= 0 && liSoup < lpSoupList->miNumPolySoups,
                           "liPolySoupIndex >= 0 && liPolySoupIndex < GetNumPolygonSoups()");
                const s32 li4BoxIndex = liSoup / 4;
                const s32 liNum4Boxes = (lpSoupList->miNumPolySoups + 3) & ~3;
                CGS_ASSERT(li4BoxIndex >= 0 && li4BoxIndex < liNum4Boxes,
                           "li4BoxIndex >= 0 && li4BoxIndex < GetNum4Boxes()");

                const CgsGeometric::PolygonSoup* lpPolySoup = lppaPolySoups[liSoup];
                const CgsGeometric::AxisAlignedBox lSoupBounds =
                    lpaSoupBounds[li4BoxIndex].GetAxisAlignedBox(
                        static_cast<u32>(liSoup % 4));
                const rw::math::vpu::Vector4& lrMin = lSoupBounds.mMin;
                const rw::math::vpu::Vector4& lrMax = lSoupBounds.mMax;
                const f32 lfDx = lvCameraPosition.x - (lrMin.x + lrMax.x) * 0.5f;
                const f32 lfDy = lvCameraPosition.y - (lrMin.y + lrMax.y) * 0.5f;
                const f32 lfDz = lvCameraPosition.z - (lrMin.z + lrMax.z) * 0.5f;
                const f32 lfSqDistToSoup = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;

                if (!lSoupBounds.ContainsPoint(lvCameraPoint) && !(lfSqDistToSoup < 10000.0f))
                    continue;

                for (u32 lu8Poly = 0; lu8Poly < lpPolySoup->mu8NumPolygons; ++lu8Poly)
                {
                    DrawPolySoupPoly(lpRenderer, lpPolySoup, liSoup,
                                     static_cast<u8>(lu8Poly), true);
                }

                if (mbDrawSoupAabbs)
                {
                    const rw::math::vpu::Vector3 lvMin(lSoupBounds.mMin.GetVector());
                    const rw::math::vpu::Vector3 lvMax(lSoupBounds.mMax.GetVector());
                    const u32 luColour = KAU_SOUP_COLOURS[static_cast<u32>(liSoup) % 16u]
                                       | 0xFF000000u;
                    lpRenderer->DrawBox(lvMin, lvMax, static_cast<rw::RGBA>(luColour));
                }
            }
        }
    }

    // ARTIST 0x828B3B98. Unpack the polygon, compute its plane/centre, run the
    // original visibility and optional draw-distance gates, then dispatch its
    // callback. The optional edge-cosine pass draws the axis-angle normal marker
    // once per stored perimeter edge.
    void TriangleCollisionDebugComponent::DrawPolySoupPoly(
        CgsDev::Debug3DImmediateRender* lpRenderer,
        const CgsGeometric::PolygonSoup* lpPolySoup,
        s32 liSoup,
        u8 lu8Poly,
        bool lbCullPolys)
    {
        CGS_ASSERT(lpRenderer != 0, "lpDisplay != NULL");

        const CgsGeometric::PolygonSoupPoly* lpPoly =
            reinterpret_cast<const CgsGeometric::PolygonSoupPoly*>(
                lpPolySoup->GetPolygon(lu8Poly));
        const bool lbIsQuad = lu8Poly < lpPolySoup->mu8NumQuads;
        const s32 liNumVertices = lbIsQuad ? 4 : 3;

        rw::math::vpu::Vector3 laPolyPoints[4];
        for (s32 liVertex = 0; liVertex < liNumVertices; ++liVertex)
        {
            UnpackSub828AF0D0(&laPolyPoints[liVertex], *lpPolySoup,
                              lpPoly->mau8VertexIndex[liVertex]);
        }

        const rw::math::vpu::Vector3& lrVertex0 = laPolyPoints[0];
        const rw::math::vpu::Vector3& lrVertex1 = laPolyPoints[1];
        const rw::math::vpu::Vector3& lrVertex2 = laPolyPoints[2];
        const f32 lfEdge01X = lrVertex1.x - lrVertex0.x;
        const f32 lfEdge01Y = lrVertex1.y - lrVertex0.y;
        const f32 lfEdge01Z = lrVertex1.z - lrVertex0.z;
        const f32 lfEdge12X = lrVertex2.x - lrVertex1.x;
        const f32 lfEdge12Y = lrVertex2.y - lrVertex1.y;
        const f32 lfEdge12Z = lrVertex2.z - lrVertex1.z;
        rw::math::vpu::Vector3 lPolyNormal(
            std::fma(lfEdge01Y, lfEdge12Z, -lfEdge01Z * lfEdge12Y),
            std::fma(lfEdge01Z, lfEdge12X, -lfEdge01X * lfEdge12Z),
            std::fma(lfEdge01X, lfEdge12Y, -lfEdge01Y * lfEdge12X));
        const f32 lfNormalLength = std::sqrt(
            lPolyNormal.x * lPolyNormal.x +
            lPolyNormal.y * lPolyNormal.y +
            lPolyNormal.z * lPolyNormal.z);
        const f32 lfInvNormalLength = 1.0f / lfNormalLength;
        lPolyNormal.x *= lfInvNormalLength;
        lPolyNormal.y *= lfInvNormalLength;
        lPolyNormal.z *= lfInvNormalLength;
        lPolyNormal.mV.mafLane[3] = 0.0f;

        rw::math::vpu::Vector3 lAveragePoint(0.0f, 0.0f, 0.0f);
        for (s32 liVertex = 0; liVertex < liNumVertices; ++liVertex)
        {
            lAveragePoint.x += laPolyPoints[liVertex].x;
            lAveragePoint.y += laPolyPoints[liVertex].y;
            lAveragePoint.z += laPolyPoints[liVertex].z;
        }
        const f32 lfInverseVertexCount = 1.0f / static_cast<f32>(liNumVertices);
        lAveragePoint.x *= lfInverseVertexCount;
        lAveragePoint.y *= lfInverseVertexCount;
        lAveragePoint.z *= lfInverseVertexCount;
        lAveragePoint.mV.mafLane[3] = 0.0f;

        const rw::math::vpu::Vector3 lvCameraPosition = lpRenderer->GetCameraPosition();
        const f32 lfCameraDx = lvCameraPosition.x - lAveragePoint.x;
        const f32 lfCameraDy = lvCameraPosition.y - lAveragePoint.y;
        const f32 lfCameraDz = lvCameraPosition.z - lAveragePoint.z;
        const f32 lfSqDistToPoly =
            lfCameraDx * lfCameraDx + lfCameraDy * lfCameraDy + lfCameraDz * lfCameraDz;

        if (IsPolyVisible(laPolyPoints, liNumVertices, lpRenderer) &&
            (!lbCullPolys || lfSqDistToPoly < mfSoupDrawDistance * mfSoupDrawDistance))
        {
            DrawCollisionPoly(lpRenderer, laPolyPoints, liNumVertices, lPolyNormal,
                              lpPoly->muSurfaceTag, liSoup);
        }

        if (!mbDrawEdgeCosines)
            return;

        static const u8 KAU_QUAD_EDGE_START[4] = { 0, 1, 2, 3 };
        static const u8 KAU_QUAD_EDGE_END[4]   = { 1, 3, 0, 2 };
        static const u8 KAU_TRI_EDGE_START[3]  = { 0, 1, 2 };
        static const u8 KAU_TRI_EDGE_END[3]    = { 1, 2, 0 };
        static const u32 KAU_QUAD_EDGE_COLOURS[4] =
        {
            0xFFFFFFFFu, 0xFF000000u, 0xFFFF0000u, 0xFF00FF00u
        };
        const s32 liNumEdges = lbIsQuad ? 4 : 3;
        for (s32 liEdge = 0; liEdge < liNumEdges; ++liEdge)
        {
            const u8 luStart = lbIsQuad ? KAU_QUAD_EDGE_START[liEdge]
                                         : KAU_TRI_EDGE_START[liEdge];
            const u8 luEnd = lbIsQuad ? KAU_QUAD_EDGE_END[liEdge]
                                       : KAU_TRI_EDGE_END[liEdge];
            const rw::math::vpu::Vector3& lrPointA = laPolyPoints[luStart];
            const rw::math::vpu::Vector3& lrPointB = laPolyPoints[luEnd];
            const rw::math::vpu::Vector3 lvEdge(lrPointB.x - lrPointA.x,
                                                 lrPointB.y - lrPointA.y,
                                                 lrPointB.z - lrPointA.z);
            const f32 lfEdgeLength = std::sqrt(
                lvEdge.x * lvEdge.x + lvEdge.y * lvEdge.y + lvEdge.z * lvEdge.z);
            const f32 lfInverseEdgeLength = 1.0f / lfEdgeLength;
            rw::math::vpu::Vector3 lAxis(lvEdge.x * lfInverseEdgeLength,
                                         lvEdge.y * lfInverseEdgeLength,
                                         lvEdge.z * lfInverseEdgeLength);
            lAxis.mV.mafLane[3] = 0.0f;

            rw::math::vpu::Vector3 lEdgeCentre(
                (lrPointA.x + lrPointB.x) * 0.5f,
                (lrPointA.y + lrPointB.y) * 0.5f,
                (lrPointA.z + lrPointB.z) * 0.5f);
            lEdgeCentre.mV.mafLane[3] = 0.0f;
            const f32 lfEdgeCosine = static_cast<f32>(
                lpPoly->GetEdgeCosineUncompressed(static_cast<u32>(liEdge)));
            const f32 lfEdgeAngle = XboxMath::XMVectorACos(lfEdgeCosine);
            f32 lfSine;
            f32 lfCosine;
            SinCosTriangleDebugAngle(lfEdgeAngle, lfSine, lfCosine);
            const rw::math::vpu::Vector3 lRenderEdgeCos =
                RotateTriangleDebugNormal(lPolyNormal, lAxis, lfSine, lfCosine);
            const rw::math::vpu::Vector3 lPointA = lEdgeCentre;
            rw::math::vpu::Vector3 lPointB(
                lEdgeCentre.x + lRenderEdgeCos.x,
                lEdgeCentre.y + lRenderEdgeCos.y,
                lEdgeCentre.z + lRenderEdgeCos.z);
            lPointB.mV.mafLane[3] = 0.0f;
            const u32 luColour = lbIsQuad ? KAU_QUAD_EDGE_COLOURS[liEdge]
                                           : 0xFF0000FFu;
            lpRenderer->DrawLine(lPointA, lPointB, static_cast<rw::RGBA>(luColour));
        }
    }

    // ARTIST 0x828AAD50. Rebuild only when the menu-selected level count changes.
    // The 2 MiB backing store is retained for the component's lifetime; subsequent
    // rebuilds rewind the same LinearMalloc arena before carving the map.
    void TriangleCollisionDebugComponent::BuildSpacialMap()
    {
        if (miNumLevels == miLastNumLevels)
            return;

        miLastNumLevels = miNumLevels;
        if (mpcSpacialBuffer == 0)
        {
            mpcSpacialBuffer = new char[KI_SPACIAL_BUFFER_SIZE];
            mMalloc.Construct();
            mMalloc.Create(mpcSpacialBuffer, KI_SPACIAL_BUFFER_SIZE);
            mMalloc.SetAlignment(16);
        }
        else
        {
            mMalloc.FreeAll();
        }

        // The original member is const-qualified in DecFIGS, while ARTIST calls the
        // map's mutating BuildSpacialPartition through that stored manager pointer.
        // Remove only that qualification to reach the existing typed mutator.
        TriangleCollisionManager* lpManager =
            const_cast<TriangleCollisionManager*>(mpTriCollisionManager);
        lpManager->GetPolySoupListSpacialMap()->BuildSpacialPartition(
            &mMalloc, miNumLevels, 0x800);
        miMemUse = static_cast<s32>(mMalloc.GetUsage());
    }

    // ARTIST 0x828AAE08. For each of the six planes, count vertices whose camera-relative
    // signed distance is strictly negative. The console returns false when that count is
    // exactly three (its callers pass three- and four-vertex polygons); an empty input returns true.
    bool TriangleCollisionDebugComponent::IsPolyVisible(
        const rw::math::vpu::Vector3* lapVertices,
        s32 liNumVertices,
        CgsDev::Debug3DImmediateRender* lpRenderer)
    {
        if (liNumVertices <= 0)
            return true;

        const rw::math::vpu::Vector3 lvCameraPosition = lpRenderer->GetCameraPosition();
        const rw::collision::Frustum& lrFrustum = mViewFrustum;
        for (u32 luPlane = 0; luPlane < 6; ++luPlane)
        {
            const rw::collision::Plane& lrPlane = lrFrustum.GetPlane(luPlane);
            const f32 lfNormalLength = std::sqrt(
                lrPlane.x * lrPlane.x + lrPlane.y * lrPlane.y + lrPlane.z * lrPlane.z);
            const f32 lfInverseNormalLength = 1.0f / lfNormalLength;
            const f32 lfNormalX = lrPlane.x * lfInverseNormalLength;
            const f32 lfNormalY = lrPlane.y * lfInverseNormalLength;
            const f32 lfNormalZ = lrPlane.z * lfInverseNormalLength;

            s32 liVerticesBehindPlane = 0;
            for (s32 liVertex = 0; liVertex < liNumVertices; ++liVertex)
            {
                const rw::math::vpu::Vector3& lrVertex = lapVertices[liVertex];
                const f32 lfSignedDistance =
                    (lrVertex.x - lvCameraPosition.x) * lfNormalX +
                    (lrVertex.y - lvCameraPosition.y) * lfNormalY +
                    (lrVertex.z - lvCameraPosition.z) * lfNormalZ;
                if (lfSignedDistance < 0.0f)
                    ++liVerticesBehindPlane;
            }

            if (liVerticesBehindPlane == 3)
                return false;
        }

        return true;
    }

    // ARTIST 0x828AAFC0. DrawPolySoupPoly calls this class-scoped routine with the
    // renderer in r3 (no component `this`); each registered callback receives the
    // original geometry, normal, surface tag, and polygon index after its user data.
    void TriangleCollisionDebugComponent::DrawCollisionPoly(
        CgsDev::Debug3DImmediateRender* lpRenderer,
        const rw::math::vpu::Vector3* lapVertices,
        s32 liNumVertices,
        rw::math::vpu::Vector3 lrNormal,
        u32 luSurfaceTag,
        s32 liPolyIndex)
    {
        for (s32 liCallback = 0; liCallback < siNumDrawPolyCallbacks; ++liCallback)
        {
            DrawCollisionPolyCallbackData& lrCallback =
                GetDrawCollisionPolyCallback(liCallback);
            lrCallback.GetCallback()(lpRenderer,
                                     lrCallback.GetUserData(),
                                     lapVertices,
                                     liNumVertices,
                                     lrNormal,
                                     luSurfaceTag,
                                     liPolyIndex);
        }
    }

    // ARTIST 0x828AAA68. Register each original field, its range/step/read-only rule, then
    // seed the two test-position refresh flags and copy the live spatial-map level count.
    void TriangleCollisionDebugComponent::OnActivate()
    {
        RegisterVariable(&mbDrawPolySoups, "Draw collision world");
        RegisterVariable(&mbDrawSoupAabbs, "Draw soup AABBs");

        RegisterVariable(&mfSoupDrawDistance, "Draw distance");
        SetRange(&mfSoupDrawDistance, 0.5f, 100.0f);
        SetStep(&mfSoupDrawDistance, 0.5f);

        RegisterVariable(&mbDrawEdgeCosines, "Draw edge cosines");
        RegisterVariable(&mbTestSweptSphere, "Test swept sphere collision");

        RegisterVariable(&mSweptSpherePos.X(), "S.Sphere X pos");
        SetStep(&mSweptSpherePos.X(), 0.25f);
        RegisterVariable(&mSweptSpherePos.Y(), "S.Sphere Y pos");
        SetStep(&mSweptSpherePos.Y(), 0.25f);
        RegisterVariable(&mSweptSpherePos.Z(), "S.Sphere Z pos");
        SetStep(&mSweptSpherePos.Z(), 0.25f);
        RegisterVariable(&mSweptSphereSize, "S.Sphere Radius");
        SetStep(&mSweptSphereSize, 0.05f);
        RegisterVariable(&mSweptSphereVelocity.X(), "S.Sphere X vel");
        SetStep(&mSweptSphereVelocity.X(), 0.05f);
        RegisterVariable(&mSweptSphereVelocity.Y(), "S.Sphere Y vel");
        SetStep(&mSweptSphereVelocity.Y(), 0.05f);
        RegisterVariable(&mSweptSphereVelocity.Z(), "S.Sphere Z vel");
        SetStep(&mSweptSphereVelocity.Z(), 0.05f);

        RegisterVariable(&miNumListsSearched, "Num lists searched");
        SetReadOnly(&miNumListsSearched, true);
        RegisterVariable(&miNumLevels, "Num spacial levels");
        SetRange(&miNumLevels, 1, KI_MAX_NUM_LEVELS);
        RegisterVariable(&miMemUse, "Spacial map memory use");
        SetReadOnly(&miMemUse, true);
        RegisterVariable(&miMaxLevelToDraw, "Max Draw Spacial Level");
        SetRange(&miMaxLevelToDraw, 0, KI_MAX_NUM_LEVELS);
        RegisterVariable(&miMinLevelToDraw, "Min Draw Spacial Level");
        SetRange(&miMinLevelToDraw, 0, KI_MAX_NUM_LEVELS);

        mbNeedToInitSpherePos = true;
        mbNeedToInitLinePosition = true;
        const s32 liNumLevels = mpTriCollisionManager->mPolySoupListSpacialMap.miNumLevels;
        miLastNumLevels = liNumLevels;
        miNumLevels = liNumLevels;
    }
}
