#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h"

#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox4.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupList.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupSpacialNode.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleBox.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManager.h"
#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"
#include "SDKs/XboxMath/XMVectorACos.h"
#include "SDKs/XboxMath/XMVectorSinCos.h"

#include <vectorintrinsics.h>
#include <cmath>
#include <cstring>

namespace
{
    using rw::math::vpu::Vector3;

    rw::math::vpu::Vector4 MakeVector4(f32 x, f32 y, f32 z, f32 w)
    {
        const rw::math::vpu::Vector4 v(x, y, z, w);
        return v;
    }

    rw::math::vpu::Vector3Plus MakeVector3Plus(f32 x, f32 y, f32 z, f32 w)
    {
        const rw::math::vpu::Vector3Plus v(x, y, z, w);
        return v;
    }

    f32 VmxMadd(f32 a, f32 b, f32 c)
    {
        const __vector4 va = { a, a, a, a };
        const __vector4 vb = { b, b, b, b };
        const __vector4 vc = { c, c, c, c };
        return __vmaddfp(va, vb, vc).x;
    }

    Vector3 OffsetTrianglePoint(const Vector3& normal, const Vector3& point, f32 offset)
    {
        const __vector4 lvNormal = __lvx(&normal, 0);
        const __vector4 lvPoint = __lvx(&point, 0);
        const __vector4 lvOffset = { offset, offset, offset, offset };
        Vector3 result;
        __stvx(__vmaddfp(lvNormal, lvOffset, lvPoint), &result, 0);
        return result;
    }

    // ARTIST 0x828B361C..0x828B36E4 uses scalar fmuls/fmadds.
    f32 ScalarMadd(f32 a, f32 b, f32 c) { return a * b + c; }

    f32 FloatFromTriangleDebugBits(u32 luBits)
    {
        f32 lfValue;
        std::memcpy(&lfValue, &luBits, sizeof(lfValue));
        return lfValue;
    }

    // Preserve the original packed word while constructing the canonical rw::RGBA.
    // RwRGBA.cpp packs (alpha << 24) | (blue << 16) | (green << 8) | red.
    rw::RGBA RwRGBAFromPackedWord(u32 luPackedColour)
    {
        return rw::RGBA(
            static_cast<u8>(luPackedColour & 0xFFu),
            static_cast<u8>((luPackedColour >> 8u) & 0xFFu),
            static_cast<u8>((luPackedColour >> 16u) & 0xFFu),
            static_cast<u8>((luPackedColour >> 24u) & 0xFFu));
    }

    void SinCosTriangleDebugAngle(f32 lfAngle, f32& rfSine, f32& rfCosine)
    {
        // ARTIST 0x828B40B0/0x828B4404: range reduction, rounded powers,
        // then the original ascending FMA chain from the shared XDK helper.
        XboxMath::XMVectorSinCos(&rfSine, &rfCosine, lfAngle);
    }

    Vector3 RotateTriangleDebugNormal(const Vector3& lrNormal,
                                      const Vector3& lrAxis,
                                      f32 lfSine,
                                      f32 lfCosine)
    {
        const f32 lfAxisDotNormal = lrAxis.mV.x * lrNormal.mV.x
                                  + lrAxis.mV.y * lrNormal.mV.y
                                  + lrAxis.mV.z * lrNormal.mV.z;
        Vector3 lvAxisCrossNormal;
        lvAxisCrossNormal.mV.x =
            VmxMadd(lrAxis.mV.y, lrNormal.mV.z, -lrAxis.mV.z * lrNormal.mV.y);
        lvAxisCrossNormal.mV.y =
            VmxMadd(lrAxis.mV.z, lrNormal.mV.x, -lrAxis.mV.x * lrNormal.mV.z);
        lvAxisCrossNormal.mV.z =
            VmxMadd(lrAxis.mV.x, lrNormal.mV.y, -lrAxis.mV.y * lrNormal.mV.x);
        const f32 lfAxisTerm = lfAxisDotNormal * (1.0f - lfCosine);
        Vector3 lvRotated;
        lvRotated.mV.x = VmxMadd(lrNormal.mV.x, lfCosine,
                               VmxMadd(lvAxisCrossNormal.mV.x, lfSine,
                                        lrAxis.mV.x * lfAxisTerm));
        lvRotated.mV.y = VmxMadd(lrNormal.mV.y, lfCosine,
                               VmxMadd(lvAxisCrossNormal.mV.y, lfSine,
                                        lrAxis.mV.y * lfAxisTerm));
        lvRotated.mV.z = VmxMadd(lrNormal.mV.z, lfCosine,
                               VmxMadd(lvAxisCrossNormal.mV.z, lfSine,
                                        lrAxis.mV.z * lfAxisTerm));
        Vector3 lvResult;
        lvResult.mV.x = lvRotated.mV.x;
        lvResult.mV.y = lvRotated.mV.y;
        lvResult.mV.z = lvRotated.mV.z;
        lvResult.mV.w = 0.0f;
        return lvResult;
    }

    // Matrix44AffineFromEulerXYZ, as the X360 inlines it at
    // TestPolySoupBoxCollision @0x828BFE80: Rx * Ry * Rz for row vectors.
    // Its three SinCos calls and the row products are also present in ARTIST.
    rw::math::vpu::Matrix44Affine Matrix44AffineFromEulerXYZ(const Vector3& lrAngles)
    {
        f32 lfSinX, lfCosX, lfSinY, lfCosY, lfSinZ, lfCosZ;
        XboxMath::XMVectorSinCos(&lfSinX, &lfCosX, lrAngles.mV.x);
        XboxMath::XMVectorSinCos(&lfSinY, &lfCosY, lrAngles.mV.y);
        XboxMath::XMVectorSinCos(&lfSinZ, &lfCosZ, lrAngles.mV.z);

        const f32 lfCosXSinZ = lfCosX * lfSinZ;
        const f32 lfSinXSinZ = lfSinX * lfSinZ;
        const f32 lfSinXCosZ = lfSinX * lfCosZ;
        const f32 lfCosXCosZ = lfCosX * lfCosZ;

        rw::math::vpu::Matrix44Affine lResult;
        lResult.xAxis.mV.x = lfCosY * lfCosZ;
        lResult.xAxis.mV.y = lfCosY * lfSinZ;
        lResult.xAxis.mV.z = -lfSinY;
        lResult.xAxis.mV.w = 0.0f;

        lResult.yAxis.mV.x = lfSinY * lfSinXCosZ - lfCosXSinZ;
        lResult.yAxis.mV.y = VmxMadd(lfSinY, lfSinXSinZ, lfCosXCosZ);
        lResult.yAxis.mV.z = lfCosY * lfSinX;
        lResult.yAxis.mV.w = 0.0f;

        lResult.zAxis.mV.x = VmxMadd(lfSinY, lfCosXCosZ, lfSinXSinZ);
        lResult.zAxis.mV.y = lfSinY * lfCosXSinZ - lfSinXCosZ;
        lResult.zAxis.mV.z = lfCosY * lfCosX;
        lResult.zAxis.mV.w = 0.0f;
        lResult.wAxis.SetZero();
        return lResult;
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
        mbNeedToInitLinePosition = false;

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
            TestPolySoupSweptSphereCollision(lpRenderer, 0);
    }

    // ARTIST 0x828BF8C0. The world pass supplies a null 2D renderer; this
    // separate HUD route supplies the live renderer for swept-sphere text.
    void TriangleCollisionDebugComponent::RenderHUD(
        CgsDev::Debug2DImmediateRender* lpRenderer)
    {
        if (mbTestSweptSphere)
            TestPolySoupSweptSphereCollision(0, lpRenderer);
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
        rw::math::vpu::Vector4 lColumn0;
        lColumn0.mV.x = lViewProjection.xAxis.mV.x;
        lColumn0.mV.y = lViewProjection.yAxis.mV.x;
        lColumn0.mV.z = lViewProjection.zAxis.mV.x;
        lColumn0.mV.w = lViewProjection.wAxis.mV.x;
        rw::math::vpu::Vector4 lColumn1;
        lColumn1.mV.x = lViewProjection.xAxis.mV.y;
        lColumn1.mV.y = lViewProjection.yAxis.mV.y;
        lColumn1.mV.z = lViewProjection.zAxis.mV.y;
        lColumn1.mV.w = lViewProjection.wAxis.mV.y;
        rw::math::vpu::Vector4 lColumn2;
        lColumn2.mV.x = lViewProjection.xAxis.mV.z;
        lColumn2.mV.y = lViewProjection.yAxis.mV.z;
        lColumn2.mV.z = lViewProjection.zAxis.mV.z;
        lColumn2.mV.w = lViewProjection.wAxis.mV.z;
        rw::math::vpu::Vector4 lColumn3;
        lColumn3.mV.x = lViewProjection.xAxis.mV.w;
        lColumn3.mV.y = lViewProjection.yAxis.mV.w;
        lColumn3.mV.z = lViewProjection.zAxis.mV.w;
        lColumn3.mV.w = lViewProjection.wAxis.mV.w;

        // ARTIST stores each extracted xyzw column combination unchanged,
        // including its homogeneous W lane; it does not negate the offset.
        rw::math::vpu::Vector4 laClipPlanes[6];
        laClipPlanes[0] = lColumn2;
        laClipPlanes[1].mV.x = lColumn3.mV.x - lColumn2.mV.x;
        laClipPlanes[1].mV.y = lColumn3.mV.y - lColumn2.mV.y;
        laClipPlanes[1].mV.z = lColumn3.mV.z - lColumn2.mV.z;
        laClipPlanes[1].mV.w = lColumn3.mV.w - lColumn2.mV.w;
        laClipPlanes[2].mV.x = lColumn3.mV.x + lColumn0.mV.x;
        laClipPlanes[2].mV.y = lColumn3.mV.y + lColumn0.mV.y;
        laClipPlanes[2].mV.z = lColumn3.mV.z + lColumn0.mV.z;
        laClipPlanes[2].mV.w = lColumn3.mV.w + lColumn0.mV.w;
        laClipPlanes[3].mV.x = lColumn3.mV.x - lColumn0.mV.x;
        laClipPlanes[3].mV.y = lColumn3.mV.y - lColumn0.mV.y;
        laClipPlanes[3].mV.z = lColumn3.mV.z - lColumn0.mV.z;
        laClipPlanes[3].mV.w = lColumn3.mV.w - lColumn0.mV.w;
        laClipPlanes[4].mV.x = lColumn3.mV.x - lColumn1.mV.x;
        laClipPlanes[4].mV.y = lColumn3.mV.y - lColumn1.mV.y;
        laClipPlanes[4].mV.z = lColumn3.mV.z - lColumn1.mV.z;
        laClipPlanes[4].mV.w = lColumn3.mV.w - lColumn1.mV.w;
        laClipPlanes[5].mV.x = lColumn3.mV.x + lColumn1.mV.x;
        laClipPlanes[5].mV.y = lColumn3.mV.y + lColumn1.mV.y;
        laClipPlanes[5].mV.z = lColumn3.mV.z + lColumn1.mV.z;
        laClipPlanes[5].mV.w = lColumn3.mV.w + lColumn1.mV.w;

        for (u32 luPlane = 0; luPlane < 6; ++luPlane)
        {
            mViewFrustum.SetPlane(luPlane, rw::collision::Plane(laClipPlanes[luPlane]));
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
        rw::math::vpu::Vector3 lvCameraPoint;
        lvCameraPoint.mV.x = lvCameraPosition.mV.x;
        lvCameraPoint.mV.y = lvCameraPosition.mV.y;
        lvCameraPoint.mV.z = lvCameraPosition.mV.z;
        lvCameraPoint.mV.w = lvCameraPosition.mV.w;

        for (s32 liSoupList = 0; liSoupList < liNumPolySoupLists; ++liSoupList)
        {
            const CgsGeometric::PolygonSoupList* lpSoupList =
                lpSpatialMap->GetPolySoupList(liSoupList);

            CgsGeometric::AxisAlignedBox lSoupListBounds;
            lSoupListBounds.mMin.mV.x = lpSoupList->mOverallAabb[0];
            lSoupListBounds.mMin.mV.y = lpSoupList->mOverallAabb[1];
            lSoupListBounds.mMin.mV.z = lpSoupList->mOverallAabb[2];
            lSoupListBounds.mMin.mV.w = lpSoupList->mOverallAabb[3];
            lSoupListBounds.mMax.mV.x = lpSoupList->mOverallAabb[4];
            lSoupListBounds.mMax.mV.y = lpSoupList->mOverallAabb[5];
            lSoupListBounds.mMax.mV.z = lpSoupList->mOverallAabb[6];
            lSoupListBounds.mMax.mV.w = lpSoupList->mOverallAabb[7];
            if (!lSoupListBounds.ContainsPoint(lvCameraPoint))
                continue;

            for (s32 liSoup = 0; liSoup < lpSoupList->GetNumPolygonSoups(); ++liSoup)
            {
                const CgsGeometric::PolygonSoup* lpPolySoup =
                    lpSoupList->GetPolygonSoup(liSoup);
                const CgsGeometric::AxisAlignedBox lSoupBounds =
                    lpSoupList->Get4Boxes(liSoup / 4)->GetAxisAlignedBox(
                        static_cast<u32>(liSoup % 4));
                const rw::math::vpu::Vector3& lrMin = lSoupBounds.mMin;
                const rw::math::vpu::Vector3& lrMax = lSoupBounds.mMax;
                const f32 lfDx = lvCameraPosition.mV.x - (lrMin.mV.x + lrMax.mV.x) * 0.5f;
                const f32 lfDy = lvCameraPosition.mV.y - (lrMin.mV.y + lrMax.mV.y) * 0.5f;
                const f32 lfDz = lvCameraPosition.mV.z - (lrMin.mV.z + lrMax.mV.z) * 0.5f;
                const f32 lfSqDistToSoup = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;

                if (!lSoupBounds.ContainsPoint(lvCameraPoint) && !(lfSqDistToSoup < 10000.0f))
                    continue;

                for (u32 lu8Poly = 0; lu8Poly < lpPolySoup->GetNumPolygons(); ++lu8Poly)
                {
                    DrawPolySoupPoly(lpRenderer, lpPolySoup, liSoup,
                                     static_cast<u8>(lu8Poly), true);
                }

                if (mbDrawSoupAabbs)
                {
                    rw::math::vpu::Vector3 lvMin;
                    lvMin.mV.x = lSoupBounds.mMin.mV.x;
                    lvMin.mV.y = lSoupBounds.mMin.mV.y;
                    lvMin.mV.z = lSoupBounds.mMin.mV.z;
                    lvMin.mV.w = lSoupBounds.mMin.mV.w;
                    rw::math::vpu::Vector3 lvMax;
                    lvMax.mV.x = lSoupBounds.mMax.mV.x;
                    lvMax.mV.y = lSoupBounds.mMax.mV.y;
                    lvMax.mV.z = lSoupBounds.mMax.mV.z;
                    lvMax.mV.w = lSoupBounds.mMax.mV.w;
                    const u32 luColour = KAU_SOUP_COLOURS[static_cast<u32>(liSoup) % 16u]
                                       | 0xFF000000u;
                    lpRenderer->DrawBox(lvMin, lvMax, RwRGBAFromPackedWord(luColour));
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
        const bool lbIsQuad = lu8Poly < lpPolySoup->GetNumQuads();
        const s32 liNumVertices = lbIsQuad ? 4 : 3;

        rw::math::vpu::Vector3 laPolyPoints[4];
        for (s32 liVertex = 0; liVertex < liNumVertices; ++liVertex)
        {
            // ARTIST sub_828AF0D0: shared packed-vertex expansion (native halfwords).
            laPolyPoints[liVertex] =
                lpPolySoup->GetVertexPosition(lpPoly->mau8VertexIndex[liVertex]);
        }

        const rw::math::vpu::Vector3& lrVertex0 = laPolyPoints[0];
        const rw::math::vpu::Vector3& lrVertex1 = laPolyPoints[1];
        const rw::math::vpu::Vector3& lrVertex2 = laPolyPoints[2];
        // ARTIST 0x828B3C9C..0x828B3D04: rounded cross-product terms,
        // followed by vmsum3fp and two reciprocal-square-root refinements.
        const __vector4 lvVertex0 = __lvx(&lrVertex0, 0);
        const __vector4 lvVertex1 = __lvx(&lrVertex1, 0);
        const __vector4 lvVertex2 = __lvx(&lrVertex2, 0);
        const __vector4 lvEdge01 = __vsubfp(lvVertex0, lvVertex1);
        const __vector4 lvEdge12 = __vsubfp(lvVertex2, lvVertex1);
        const __vector4 lvA = { lvEdge01.y, lvEdge01.z, lvEdge01.x, lvEdge01.w };
        const __vector4 lvB = { lvEdge12.z, lvEdge12.x, lvEdge12.y, lvEdge12.w };
        const __vector4 lvC = { lvEdge01.z, lvEdge01.x, lvEdge01.y, lvEdge01.w };
        const __vector4 lvD = { lvEdge12.y, lvEdge12.z, lvEdge12.x, lvEdge12.w };
        const __vector4 lvCross = __vnmsubfp(lvC, lvD, __vmulfp(lvA, lvB));
        const __vector4 lvSquaredLength = __vmsum3fp(lvCross, lvCross);
        const __vector4 lvHalf = { 0.5f, 0.5f, 0.5f, 0.5f };
        const __vector4 lvOne = { 1.0f, 1.0f, 1.0f, 1.0f };
        __vector4 lvInverse = __vrsqrtefp(lvSquaredLength);
        for (int liStep = 0; liStep < 2; ++liStep)
        {
            const __vector4 lvSquare = __vmulfp(lvInverse, lvInverse);
            const __vector4 lvHalfInverse = __vmulfp(lvInverse, lvHalf);
            const __vector4 lvResidual = __vnmsubfp(lvSquaredLength, lvSquare, lvOne);
            lvInverse = __vmaddfp(lvHalfInverse, lvResidual, lvInverse);
        }
        rw::math::vpu::Vector3 lPolyNormal;
        __stvx(__vmulfp(lvCross, lvInverse), &lPolyNormal, 0);

        rw::math::vpu::Vector3 lAveragePoint;
        lAveragePoint.mV.x = 0.0f;
        lAveragePoint.mV.y = 0.0f;
        lAveragePoint.mV.z = 0.0f;
        for (s32 liVertex = 0; liVertex < liNumVertices; ++liVertex)
        {
            lAveragePoint.mV.x += laPolyPoints[liVertex].mV.x;
            lAveragePoint.mV.y += laPolyPoints[liVertex].mV.y;
            lAveragePoint.mV.z += laPolyPoints[liVertex].mV.z;
        }
        const f32 lfInverseVertexCount = 1.0f / static_cast<f32>(liNumVertices);
        lAveragePoint.mV.x *= lfInverseVertexCount;
        lAveragePoint.mV.y *= lfInverseVertexCount;
        lAveragePoint.mV.z *= lfInverseVertexCount;
        lAveragePoint.mV.w = 0.0f;

        const rw::math::vpu::Vector3 lvCameraPosition = lpRenderer->GetCameraPosition();
        const f32 lfCameraDx = lvCameraPosition.mV.x - lAveragePoint.mV.x;
        const f32 lfCameraDy = lvCameraPosition.mV.y - lAveragePoint.mV.y;
        const f32 lfCameraDz = lvCameraPosition.mV.z - lAveragePoint.mV.z;
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
            0xFFFFFFFFu, 0xFF00FF00u, 0xFFFF0000u, 0xFF000000u
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
            rw::math::vpu::Vector3 lvEdge;
            lvEdge.mV.x = lrPointB.mV.x - lrPointA.mV.x;
            lvEdge.mV.y = lrPointB.mV.y - lrPointA.mV.y;
            lvEdge.mV.z = lrPointB.mV.z - lrPointA.mV.z;
            const f32 lfEdgeLength = std::sqrt(
                lvEdge.mV.x * lvEdge.mV.x + lvEdge.mV.y * lvEdge.mV.y + lvEdge.mV.z * lvEdge.mV.z);
            const f32 lfInverseEdgeLength = 1.0f / lfEdgeLength;
            rw::math::vpu::Vector3 lAxis;
            lAxis.mV.x = lvEdge.mV.x * lfInverseEdgeLength;
            lAxis.mV.y = lvEdge.mV.y * lfInverseEdgeLength;
            lAxis.mV.z = lvEdge.mV.z * lfInverseEdgeLength;
            lAxis.mV.w = 0.0f;

            rw::math::vpu::Vector3 lEdgeCentre;
            lEdgeCentre.mV.x = (lrPointA.mV.x + lrPointB.mV.x) * 0.5f;
            lEdgeCentre.mV.y = (lrPointA.mV.y + lrPointB.mV.y) * 0.5f;
            lEdgeCentre.mV.z = (lrPointA.mV.z + lrPointB.mV.z) * 0.5f;
            lEdgeCentre.mV.w = 0.0f;
            const f32 lfEdgeCosine = static_cast<f32>(
                lpPoly->GetEdgeCosineUncompressed(static_cast<u32>(liEdge)));
            const f32 lfEdgeAngle = XboxMath::XMVectorACos(lfEdgeCosine);
            f32 lfSine;
            f32 lfCosine;
            SinCosTriangleDebugAngle(lfEdgeAngle, lfSine, lfCosine);
            const rw::math::vpu::Vector3 lRenderEdgeCos =
                RotateTriangleDebugNormal(lPolyNormal, lAxis, lfSine, lfCosine);
            const rw::math::vpu::Vector3 lPointA = lEdgeCentre;
            rw::math::vpu::Vector3 lPointB;
            lPointB.mV.x = lEdgeCentre.mV.x + lRenderEdgeCos.mV.x;
            lPointB.mV.y = lEdgeCentre.mV.y + lRenderEdgeCos.mV.y;
            lPointB.mV.z = lEdgeCentre.mV.z + lRenderEdgeCos.mV.z;
            lPointB.mV.w = 0.0f;
            const u32 luColour = lbIsQuad ? KAU_QUAD_EDGE_COLOURS[liEdge]
                                           : 0xFF0000FFu;
            lpRenderer->DrawLine(lPointA, lPointB, RwRGBAFromPackedWord(luColour));
        }
    }

    // ARTIST 0x828BF8E0. Initialize and draw the selected sphere, reject soup lists
    // whose whole-list AABB does not strictly contain its center, then run the
    // original four-lane sphere contact and valid-triangle debug passes.
    void TriangleCollisionDebugComponent::TestPolySoupSphereCollision(
        CgsDev::Debug3DImmediateRender* lpRenderer)
    {
        if (mbNeedToInitSpherePos)
        {
            const rw::math::vpu::Vector3 lvCameraPosition =
                lpRenderer->GetCameraPosition();
            mSpherePos.X() = lvCameraPosition.mV.x;
            mSpherePos.Y() = lvCameraPosition.mV.y;
            mSpherePos.Z() = lvCameraPosition.mV.z;
            mbNeedToInitSpherePos = false;
            mSpherePos.Z() += 10.0f; // flt_82004A20
        }

        const rw::math::vpu::Vector3 lvSphereCentre(MakeVector4(
            mSpherePos.X(), mSpherePos.Y(), mSpherePos.Z(), mfSphereRad
        ).mV);
        const CgsGeometric::Sphere lSphere = {
            MakeVector4(
                mSpherePos.X(), mSpherePos.Y(), mSpherePos.Z(), mfSphereRad
            )
        };
        lpRenderer->DrawSphere(lvSphereCentre, mfSphereRad,
                               RwRGBAFromPackedWord(0xFFFFFFFFu));

        const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap =
            mpTriCollisionManager->GetPolySoupListSpacialMap();
        const s32 liNumPolySoupLists = lpSpatialMap->GetNumPolySoupLists();
        const f32 KF_EXTRACTIONRADIUS = 5.0f;    // flt_820F2718
        const f32 KF_SPHERECONTACTPADDING = 1.0f; // 0x828BFB08..0x828BFB1C
        const f32 KF_TRIANGLEOFFSET = 0.02f;       // flt_82005574
        const VecFloat lvSphereContactPadding(KF_SPHERECONTACTPADDING);

        for (s32 liSoupList = 0; liSoupList < liNumPolySoupLists; ++liSoupList)
        {
            const CgsGeometric::PolygonSoupList* lpPolySoupList =
                lpSpatialMap->GetPolySoupList(liSoupList);

            // ARTIST calls AxisAlignedBox::ContainsPoint on the resource's first
            // eight floats: a strict center-in-whole-list test, not a radius overlap.
            const f32* const lapOverallAabb = lpPolySoupList->mOverallAabb;
            CgsGeometric::AxisAlignedBox lOverallAabb;
            lOverallAabb.Set(
                rw::math::vpu::Vector3(MakeVector4(
                    lapOverallAabb[0], lapOverallAabb[1], lapOverallAabb[2], lapOverallAabb[3]
                ).mV),
                rw::math::vpu::Vector3(MakeVector4(
                    lapOverallAabb[4], lapOverallAabb[5], lapOverallAabb[6], lapOverallAabb[7]
                ).mV));
            if (!lOverallAabb.ContainsPoint(lSphere.mData.GetVector3()))
                continue;

            const CgsGeometric::PolygonSoup* const* lpapPolySoups =
                reinterpret_cast<const CgsGeometric::PolygonSoup* const*>(
                    lpPolySoupList->mpapPolySoups);
            for (s32 liPolySoup = 0; liPolySoup < lpPolySoupList->miNumPolySoups;
                 ++liPolySoup)
            {
                CGS_ASSERT(liPolySoup >= 0 && liPolySoup < lpPolySoupList->miNumPolySoups,
                           "liPolySoupIndex >= 0 && liPolySoupIndex < GetNumPolygonSoups()");
                const CgsGeometric::PolygonSoup& lrPolySoup = *lpapPolySoups[liPolySoup];

                CgsGeometric::Triangle4 laTriangle4s[1024];
                // ARTIST temporarily writes 5.0 into sphere W for extraction,
                // then restores the selected radius before the contact tests.
                CgsGeometric::Sphere lExtractionSphere = lSphere;
                lExtractionSphere.mData.mV.w = KF_EXTRACTIONRADIUS;
                const s32 liNumTriangle4s =
                    CgsGeometric::ExtractTriangle4ListIntersectingSphere(
                        lrPolySoup, lExtractionSphere, laTriangle4s, 0x400, 0);
                for (s32 liTriangle4 = 0; liTriangle4 < liNumTriangle4s; ++liTriangle4)
                {
                    rw::math::vpu::Vector3 laContactNormals[4];
                    rw::math::vpu::Vector3 laTriangleNormals[4];
                    rw::math::vpu::Vector3Plus laSphereContactPoints[4];
                    rw::math::vpu::Vector3Plus laTriangleContactPoints[4];
                    const CgsGeometric::Triangle4::Mask4 lContactMask =
                        CgsGeometric::IntersectTriangle4Sphere_HackyBurnoutVersion(
                            lSphere, laTriangle4s[liTriangle4], lvSphereContactPadding,
                            laContactNormals[0], laTriangleNormals[0],
                            laSphereContactPoints[0], laTriangleContactPoints[0],
                            laContactNormals[1], laTriangleNormals[1],
                            laSphereContactPoints[1], laTriangleContactPoints[1],
                            laContactNormals[2], laTriangleNormals[2],
                            laSphereContactPoints[2], laTriangleContactPoints[2],
                            laContactNormals[3], laTriangleNormals[3],
                            laSphereContactPoints[3], laTriangleContactPoints[3]);

                    const f32 lafContactMask[4] = {
                        lContactMask.mV.x, lContactMask.mV.y, lContactMask.mV.z, lContactMask.mV.w
                    };
                    for (s32 liLane = 0; liLane < 4; ++liLane)
                    {
                        // IntersectTriangle4Sphere returns full-word masks for hits.
                        if (lafContactMask[liLane] != 0.0f)
                        {
                            // The X360 vector call orders triangle contact before sphere contact.
                            lpRenderer->DrawLine(
                                laTriangleContactPoints[liLane].GetVector3(),
                                laSphereContactPoints[liLane].GetVector3(),
                                RwRGBAFromPackedWord(0xFF0000FFu));
                        }
                    }

                    const f32 lafValidMask[4] = {
                        laTriangle4s[liTriangle4].mValidMasks.mV.x,
                        laTriangle4s[liTriangle4].mValidMasks.mV.y,
                        laTriangle4s[liTriangle4].mValidMasks.mV.z,
                        laTriangle4s[liTriangle4].mValidMasks.mV.w
                    };
                    for (s32 liLane = 0; liLane < 4; ++liLane)
                    {
                        // The extractor marks unused tail lanes with an all-zero valid mask.
                        if (lafValidMask[liLane] == 0.0f)
                            continue;

                        CgsGeometric::Triangle4::AOSTriangle lAOSTriangle;
                        laTriangle4s[liTriangle4].GetAOSTriangle(liLane, lAOSTriangle);
                        // VMX vmaddfp computes normal*0.02 + vertex before the solid draw.
                        const rw::math::vpu::Vector3 lvV0 = OffsetTrianglePoint(lAOSTriangle.mNormal, lAOSTriangle.mVertex0, KF_TRIANGLEOFFSET);
                        const rw::math::vpu::Vector3 lvV1 = OffsetTrianglePoint(lAOSTriangle.mNormal, lAOSTriangle.mVertex1, KF_TRIANGLEOFFSET);
                        const rw::math::vpu::Vector3 lvV2 = OffsetTrianglePoint(lAOSTriangle.mNormal, lAOSTriangle.mVertex2, KF_TRIANGLEOFFSET);
                        lpRenderer->DrawSolidTriangle(
                            lvV0, lvV1, lvV2, RwRGBAFromPackedWord(0x64FFFFFFu));
                    }
                }
            }
        }
    }

    // ARTIST 0x828BFE80. Initialize the box from the debug camera when requested,
    // draw its oriented wire/fill/axis, gather candidate Triangle4 batches through
    // the map's typed box query and sphere extractor, then run the four-lane
    // IntersectTriangleBox contact pass.
    void TriangleCollisionDebugComponent::TestPolySoupBoxCollision(
        CgsDev::Debug3DImmediateRender* lpRenderer)
    {
        if (mbNeedToInitBoxPos)
        {
            const rw::math::vpu::Vector3 lvCameraPosition =
                lpRenderer->GetCameraPosition();
            mBoxPos.Set(lvCameraPosition.mV.x, lvCameraPosition.mV.y, lvCameraPosition.mV.z);
            mbNeedToInitBoxPos = false;
            mBoxPos.Z() += 10.0f; // flt_82004A20
        }

        rw::math::vpu::Vector3 lvBoxPosition;
        lvBoxPosition.mV.x = mBoxPos.X();
        lvBoxPosition.mV.y = mBoxPos.Y();
        lvBoxPosition.mV.z = mBoxPos.Z();
        lvBoxPosition.mV.w = 0.0f;
        rw::math::vpu::Vector3 lvBoxAngles;
        lvBoxAngles.mV.x = mBoxOrientation.X();
        lvBoxAngles.mV.y = mBoxOrientation.Y();
        lvBoxAngles.mV.z = mBoxOrientation.Z();
        lvBoxAngles.mV.w = 0.0f;
        rw::math::vpu::Matrix44Affine lTransform =
            Matrix44AffineFromEulerXYZ(lvBoxAngles);
        lTransform.wAxis = lvBoxPosition;

        rw::math::vpu::Vector3 lvBoxHalfSize;
        lvBoxHalfSize.mV.x = mBoxSize.X() * 0.5f;
        lvBoxHalfSize.mV.y = mBoxSize.Y() * 0.5f;
        lvBoxHalfSize.mV.z = mBoxSize.Z() * 0.5f;
        lvBoxHalfSize.mV.w = 0.0f;
        CgsGeometric::Box lBox;
        lBox.Set(lTransform, lvBoxHalfSize,
                 VecFloat(0.0f));

        const rw::math::vpu::Vector3 lvLocalMin(MakeVector4(
            -lvBoxHalfSize.mV.x, -lvBoxHalfSize.mV.y, -lvBoxHalfSize.mV.z, 0.0f
        ).mV);
        const rw::math::vpu::Vector3 lvLocalMax(MakeVector4(
            lvBoxHalfSize.mV.x, lvBoxHalfSize.mV.y, lvBoxHalfSize.mV.z, 0.0f
        ).mV);
        lpRenderer->DrawBox(lvLocalMin, lvLocalMax, lTransform,
                            RwRGBAFromPackedWord(0xFFFFFFFFu));
        lpRenderer->DrawSolidBox(lvLocalMin, lvLocalMax, lTransform,
                                 RwRGBAFromPackedWord(0x1EFFFFFFu));
        lpRenderer->DrawAxis(lTransform);

        // The console uses half of the largest authored dimension for both the
        // query sphere radius and its AABB, including the w lane initialized from
        // a zero Vector3 center. Keep that original broad-phase shape.
        const f32 lfSizeXY = (mBoxSize.X() - mBoxSize.Y() >= 0.0f)
                           ? mBoxSize.X() : mBoxSize.Y();
        const f32 lfMaxSize = (lfSizeXY - mBoxSize.Z() >= 0.0f)
                            ? lfSizeXY : mBoxSize.Z();
        const f32 lfRadius = lfMaxSize * 0.5f;

        CgsGeometric::Sphere lBoundingSphere;
        lBoundingSphere.mData.mV.x = lvBoxPosition.mV.x;
        lBoundingSphere.mData.mV.y = lvBoxPosition.mV.y;
        lBoundingSphere.mData.mV.z = lvBoxPosition.mV.z;
        lBoundingSphere.mData.mV.w = lfRadius;

        CgsGeometric::AxisAlignedBox lBoundingBox;
        {
            const rw::math::vpu::Vector4 lAggregateValue(
            lvBoxPosition.mV.x - lfRadius,
            lvBoxPosition.mV.y - lfRadius,
            lvBoxPosition.mV.z - lfRadius,
            -lfRadius);
            lBoundingBox.mMin = lAggregateValue.mV;
        }
        {
            const rw::math::vpu::Vector4 lAggregateValue(
            lvBoxPosition.mV.x + lfRadius,
            lvBoxPosition.mV.y + lfRadius,
            lvBoxPosition.mV.z + lfRadius,
            lfRadius);
            lBoundingBox.mMax = lAggregateValue.mV;
        }

        // RunQuery publishes its leaf-index list into the mutable spatial map.
        // The console stores this manager as const but calls that mutator directly.
        TriangleCollisionManager* lpManager =
            const_cast<TriangleCollisionManager*>(mpTriCollisionManager);
        CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap =
            lpManager->GetPolySoupListSpacialMap();
        const s32 liNumSoupLists = lpSpatialMap->RunQuery(lBoundingBox);

        CgsGeometric::Triangle4 laTriangles[1024];
        s32 liNumTriangle4s = 0;
        if (liNumSoupLists > 0)
        {
            const u16* lpaLeafIndices = lpSpatialMap->GetOutputQueryBuffer();
            const CgsGeometric::PolygonSoupLeafNode* lpaLeafNodes =
                lpSpatialMap->GetLeafNodes();

            for (s32 liSoupList = 0; liSoupList < liNumSoupLists; ++liSoupList)
            {
                const CgsGeometric::PolygonSoupLeafNode& lrLeaf =
                    lpaLeafNodes[lpaLeafIndices[liSoupList]];
                const CgsGeometric::AxisAlignedBox& lrLeafBox = lrLeaf.mBox;

                // The X360 SIMD reduction is inclusive on all four vector lanes.
                const bool lbLeafOverlapsBox =
                    (lrLeafBox.mMax.mV.x >= lBoundingBox.mMin.mV.x) &&
                    (lrLeafBox.mMax.mV.y >= lBoundingBox.mMin.mV.y) &&
                    (lrLeafBox.mMax.mV.z >= lBoundingBox.mMin.mV.z) &&
                    (lrLeafBox.mMax.mV.w >= lBoundingBox.mMin.mV.w) &&
                    (lBoundingBox.mMax.mV.x >= lrLeafBox.mMin.mV.x) &&
                    (lBoundingBox.mMax.mV.y >= lrLeafBox.mMin.mV.y) &&
                    (lBoundingBox.mMax.mV.z >= lrLeafBox.mMin.mV.z) &&
                    (lBoundingBox.mMax.mV.w >= lrLeafBox.mMin.mV.w);
                if (!lbLeafOverlapsBox)
                    continue;

                const s32 liRemainingTriangle4s = 1024 - liNumTriangle4s;
                liNumTriangle4s += CgsGeometric::ExtractTriangle4ListIntersectingSphere(
                    *lrLeaf.mpPolygonSoup,
                    lBoundingSphere,
                    laTriangles + liNumTriangle4s,
                    liRemainingTriangle4s,
                    0);
            }
        }

        miNumBoxTests = 0;
        u64 luTotalIntersectionTicks = 0;
        rw::math::vpu::Vector3 laTriangleNormals[32];
        rw::math::vpu::Vector3 laContactNormals[32];
        rw::math::vpu::Vector3 laTrianglePoints[32];
        rw::math::vpu::Vector3 laContactPoints[32];
        for (s32 liTriangle4 = 0; liTriangle4 < liNumTriangle4s; ++liTriangle4)
        {
            const CgsGeometric::Triangle4& lrTriangle4 = laTriangles[liTriangle4];
            const f32* lapValidMaskLanes = &lrTriangle4.mValidMasks.mV.x;
            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                CGS_ASSERT(liLane < 4, "liIndex < 4");
                if (lapValidMaskLanes[liLane] == 0.0f)
                    continue;

                CgsGeometric::Triangle4::AOSTriangle lAOSTriangle;
                lrTriangle4.GetAOSTriangle(liLane, lAOSTriangle);
                lpRenderer->DrawSolidTriangle(
                    lAOSTriangle.mVertex0,
                    lAOSTriangle.mVertex1,
                    lAOSTriangle.mVertex2,
                    RwRGBAFromPackedWord(0x320000FFu));
                lpRenderer->DrawHollowTriangle(
                    lAOSTriangle.mVertex0,
                    lAOSTriangle.mVertex1,
                    lAOSTriangle.mVertex2,
                    RwRGBAFromPackedWord(0xFF000000u));

                CgsGeometric::Triangle lTriangle;
                lTriangle.Set(lAOSTriangle.mVertex0,
                              lAOSTriangle.mVertex1,
                              lAOSTriangle.mVertex2);
                s32 liNumContacts = 0;
                const VecFloat lvPadding(0.1f); // flt_82004014
                const rw::math::vpu::MaskScalar lvClampPenetration(false); // unk_8327F240+0x10, raw initial zero

                const u64 luIntersectionStart = CgsSystem::GetSystemTimerBaseTime();
                (void)CgsGeometric::IntersectTriangleBox(
                    lBox,
                    lTriangle,
                    lvPadding,
                    lvClampPenetration,
                    laTriangleNormals,
                    laContactNormals,
                    laTrianglePoints,
                    laContactPoints,
                    &liNumContacts,
                    0x20);
                luTotalIntersectionTicks +=
                    CgsSystem::GetSystemTimerBaseTime() - luIntersectionStart;
                ++miNumBoxTests;

                for (s32 liContact = 0; liContact < liNumContacts; ++liContact)
                {
                    lpRenderer->DrawPoint(
                        laTrianglePoints[liContact],
                        RwRGBAFromPackedWord(0xFF00FFFFu));
                    lpRenderer->DrawPoint(
                        laContactPoints[liContact],
                        RwRGBAFromPackedWord(0xFFFF00FFu));
                    lpRenderer->DrawLine(
                        laTrianglePoints[liContact],
                        laContactPoints[liContact],
                        RwRGBAFromPackedWord(0xFFFFFFFFu));

                    rw::math::vpu::Vector3 lvNormalEnd;
                    lvNormalEnd.mV.x =
                        laTrianglePoints[liContact].mV.x + laTriangleNormals[liContact].mV.x;
                    lvNormalEnd.mV.y =
                        laTrianglePoints[liContact].mV.y + laTriangleNormals[liContact].mV.y;
                    lvNormalEnd.mV.z =
                        laTrianglePoints[liContact].mV.z + laTriangleNormals[liContact].mV.z;
                    lvNormalEnd.mV.w =
                        laTrianglePoints[liContact].mV.w + laTriangleNormals[liContact].mV.w;
                    lpRenderer->DrawArrow(
                        laTrianglePoints[liContact],
                        lvNormalEnd,
                        RwRGBAFromPackedWord(0xFF0000FFu));
                }
            }
        }

        const u64 luTimerFrequency = CgsSystem::GetSystemTimerFrequency();
        muBoxTestCycles = static_cast<u32>(
            (luTotalIntersectionTicks * 0xCCCCCCCCu) / luTimerFrequency);
        mfBoxTestTime = static_cast<f32>(
            static_cast<f64>(luTotalIntersectionTicks * 1000u) /
            static_cast<f64>(luTimerFrequency));
        muBoxTestCyclesPerTriangle = muBoxTestCycles /
            static_cast<u32>(miNumBoxTests);
        mfBoxTestPercent = static_cast<f32>(
            static_cast<f64>(luTotalIntersectionTicks * 6000u) /
            static_cast<f64>(luTimerFrequency));
    }

    // ARTIST 0x828B3580. Build the swept sphere from the menu's start/velocity,
    // gather Triangle4 candidates in the original padded query box, and draw the
    // valid triangles plus the single-triangle swept-contact results.
    void TriangleCollisionDebugComponent::TestPolySoupSweptSphereCollision(
        CgsDev::Debug3DImmediateRender* lp3DRenderer,
        CgsDev::Debug2DImmediateRender* lp2DRenderer)
    {
        if (mbNeedToInitSweptSpherePos)
        {
            mSweptSpherePos.X() = 3027.28f;       // flt_820F47AC
            mSweptSpherePos.Y() = -2.5220001f;    // flt_820F47A8
            mSweptSpherePos.Z() = -1945.551f;     // flt_820F47A4
            mbNeedToInitSweptSpherePos = false;
        }

        if (lp2DRenderer)
        {
            lp2DRenderer->DrawText("Testing polygon soup swept sphere collision",
                                   50.0f, 50.0f, 12.0f,
                                   0xFFFFFFFFu);
        }

        const rw::math::vpu::Vector3 lvStartPosition(MakeVector4(
            mSweptSpherePos.X(), mSweptSpherePos.Y(), mSweptSpherePos.Z(), 0.0f
        ).mV);
        const rw::math::vpu::Vector3 lvVelocity(MakeVector4(
            mSweptSphereVelocity.X(), mSweptSphereVelocity.Y(),
            mSweptSphereVelocity.Z(), 0.0f
        ).mV);

        // ARTIST computes length from the three velocity lanes. Its scalar broad-phase
        // path uses fused multiply-adds for the squared-length accumulation.
        const f32 lfVelocityXYLengthSquared =
            ScalarMadd(lvVelocity.mV.x, lvVelocity.mV.x, lvVelocity.mV.y * lvVelocity.mV.y);
        const f32 lfVelocityLengthSquared =
            ScalarMadd(lvVelocity.mV.z, lvVelocity.mV.z, lfVelocityXYLengthSquared);
        const f32 lfVelocityLength = std::sqrt(lfVelocityLengthSquared);

        // The broad-phase sphere/AABB uses (length + 2*radius) * 1.05 (raw
        // flt_82005448); the narrow-phase SweptSphere retains the authored radius.
        const f32 lfBroadPhaseRadius =
            ScalarMadd(mSweptSphereSize, 2.0f, lfVelocityLength) * 1.05f;

        CgsGeometric::Sphere lBroadPhaseSphere;
        {
            const rw::math::vpu::Vector4 lAggregateValue(
            lvStartPosition.mV.x, lvStartPosition.mV.y, lvStartPosition.mV.z, lfBroadPhaseRadius
        );
            lBroadPhaseSphere.mData.mV = lAggregateValue.mV;
        }

        CgsGeometric::AxisAlignedBox lQueryBox;
        lQueryBox.Set(
            rw::math::vpu::Vector3(MakeVector4(
                lvStartPosition.mV.x - lfBroadPhaseRadius,
                lvStartPosition.mV.y - lfBroadPhaseRadius,
                lvStartPosition.mV.z - lfBroadPhaseRadius,
                -lfBroadPhaseRadius
            ).mV),
            rw::math::vpu::Vector3(MakeVector4(
                lvStartPosition.mV.x + lfBroadPhaseRadius,
                lvStartPosition.mV.y + lfBroadPhaseRadius,
                lvStartPosition.mV.z + lfBroadPhaseRadius,
                lfBroadPhaseRadius
            ).mV));

        rw::math::vpu::Vector3 lvSweepDirection;
        if (lfVelocityLength <= 0.00001f) // flt_82004884
        {
            // The console selects rw::math::vpu::detail::gIVector in this arm.
            lvSweepDirection = rw::math::vpu::GetVector3_XAxis();
        }
        else
        {
            const f32 lfInverseLength = 1.0f / lfVelocityLength;
            {
                const rw::math::vpu::Vector3 lAggregateValue(MakeVector4(
                lvVelocity.mV.x * lfInverseLength,
                lvVelocity.mV.y * lfInverseLength,
                lvVelocity.mV.z * lfInverseLength,
                0.0f
            ).mV);
                lvSweepDirection = lAggregateValue;
            }
        }

        CgsGeometric::SweptSphere lSweptSphere;
        lSweptSphere.Set(
            MakeVector3Plus(
                lvStartPosition.mV.x, lvStartPosition.mV.y, lvStartPosition.mV.z, mSweptSphereSize
            ),
            MakeVector3Plus(
                lvSweepDirection.mV.x, lvSweepDirection.mV.y, lvSweepDirection.mV.z, lfVelocityLength
            ));

        if (lp3DRenderer)
        {
            lp3DRenderer->DrawSphere(lvStartPosition, mSweptSphereSize,
                                     RwRGBAFromPackedWord(0xFFFF0000u));
            const rw::math::vpu::Vector3 lvSweepEnd(MakeVector4(
                lvStartPosition.mV.x + lvVelocity.mV.x,
                lvStartPosition.mV.y + lvVelocity.mV.y,
                lvStartPosition.mV.z + lvVelocity.mV.z,
                0.0f
            ).mV);
            lp3DRenderer->DrawArrow(lvStartPosition, lvSweepEnd,
                                    RwRGBAFromPackedWord(0xFFFF00FFu));
        }

        // RunQuery mutates the map's result buffer; the X360 stores this manager as
        // const but calls the map's non-const query directly through its first member.
        TriangleCollisionManager* lpManager =
            const_cast<TriangleCollisionManager*>(mpTriCollisionManager);
        CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap =
            lpManager->GetPolySoupListSpacialMap();
        const s32 liNumLeaves = lpSpatialMap->RunQuery(lQueryBox);

        CgsGeometric::Triangle4 laTriangle4s[1024];
        s32 liNumTriangle4s = 0;
        if (liNumLeaves > 0)
        {
            const u16* lpaLeafIndices = lpSpatialMap->GetOutputQueryBuffer();
            const CgsGeometric::PolygonSoupLeafNode* lpaLeafNodes =
                lpSpatialMap->GetLeafNodes();

            for (s32 liQueryResult = 0; liQueryResult < liNumLeaves; ++liQueryResult)
            {
                const CgsGeometric::PolygonSoupLeafNode& lrLeaf =
                    lpaLeafNodes[lpaLeafIndices[liQueryResult]];
                const CgsGeometric::AxisAlignedBox& lrLeafBox = lrLeaf.mBox;

                // The console's VMX reduction is inclusive on all four lanes.
                const bool lbLeafOverlapsQuery =
                    (lrLeafBox.mMax.mV.x >= lQueryBox.mMin.mV.x) &&
                    (lrLeafBox.mMax.mV.y >= lQueryBox.mMin.mV.y) &&
                    (lrLeafBox.mMax.mV.z >= lQueryBox.mMin.mV.z) &&
                    (lrLeafBox.mMax.mV.w >= lQueryBox.mMin.mV.w) &&
                    (lQueryBox.mMax.mV.x >= lrLeafBox.mMin.mV.x) &&
                    (lQueryBox.mMax.mV.y >= lrLeafBox.mMin.mV.y) &&
                    (lQueryBox.mMax.mV.z >= lrLeafBox.mMin.mV.z) &&
                    (lQueryBox.mMax.mV.w >= lrLeafBox.mMin.mV.w);
                if (!lbLeafOverlapsQuery)
                    continue;

                const s32 liRemainingTriangle4s = 1024 - liNumTriangle4s;
                liNumTriangle4s += CgsGeometric::ExtractTriangle4ListIntersectingSphere(
                    *lrLeaf.mpPolygonSoup, lBroadPhaseSphere,
                    laTriangle4s + liNumTriangle4s, liRemainingTriangle4s, 0);
            }
        }

        for (s32 liTriangle4 = 0; liTriangle4 < liNumTriangle4s; ++liTriangle4)
        {
            const CgsGeometric::Triangle4& lrTriangle4 = laTriangle4s[liTriangle4];
            const f32 lafValidMask[4] = {
                lrTriangle4.mValidMasks.mV.x, lrTriangle4.mValidMasks.mV.y,
                lrTriangle4.mValidMasks.mV.z, lrTriangle4.mValidMasks.mV.w
            };
            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                CGS_ASSERT(liLane < 4, "liIndex < 4");
                if (lafValidMask[liLane] == 0.0f)
                    continue;

                CgsGeometric::Triangle4::AOSTriangle lAOSTriangle;
                lrTriangle4.GetAOSTriangle(liLane, lAOSTriangle);
                if (lp3DRenderer)
                {
                    lp3DRenderer->DrawSolidTriangle(
                        lAOSTriangle.mVertex0, lAOSTriangle.mVertex1, lAOSTriangle.mVertex2,
                        RwRGBAFromPackedWord(0x320000FFu));
                    lp3DRenderer->DrawHollowTriangle(
                        lAOSTriangle.mVertex0, lAOSTriangle.mVertex1, lAOSTriangle.mVertex2,
                        RwRGBAFromPackedWord(0xFF000000u));
                }

                CgsGeometric::Triangle lTriangle;
                lTriangle.Set(lAOSTriangle.mVertex0, lAOSTriangle.mVertex1,
                              lAOSTriangle.mVertex2);
                rw::math::vpu::Vector3 lContactNormal;
                rw::math::vpu::Vector3 lTriangleNormal;
                rw::math::vpu::Vector3Plus lSphereContactPoint;
                rw::math::vpu::Vector3Plus lTriangleContactPoint;
                const rw::math::vpu::MaskScalar lHitMask =
                    CgsGeometric::IntersectTriangleSweptSphere(
                        lSweptSphere, lTriangle, lContactNormal, lTriangleNormal,
                        lSphereContactPoint, lTriangleContactPoint);
                if (lHitMask.mV.x == 0.0f || !lp3DRenderer)
                    continue;

                const rw::math::vpu::Vector3 lvTriangleContactPoint(MakeVector4(
                    lTriangleContactPoint.mV.x, lTriangleContactPoint.mV.y,
                    lTriangleContactPoint.mV.z, lTriangleContactPoint.mV.w
                ).mV);
                const rw::math::vpu::Vector3 lvSphereContactPoint(MakeVector4(
                    lSphereContactPoint.mV.x, lSphereContactPoint.mV.y,
                    lSphereContactPoint.mV.z, lSphereContactPoint.mV.w
                ).mV);
                lp3DRenderer->DrawPoint(
                    lvTriangleContactPoint, RwRGBAFromPackedWord(0xFF00FFFFu));
                lp3DRenderer->DrawPoint(
                    lvSphereContactPoint, RwRGBAFromPackedWord(0xFFFF00FFu));
                lp3DRenderer->DrawLine(
                    lvTriangleContactPoint, lvSphereContactPoint,
                    RwRGBAFromPackedWord(0xFFFFFFFFu));

                const rw::math::vpu::Vector3 lvArrowEnd(MakeVector4(
                    lvTriangleContactPoint.mV.x + lContactNormal.mV.x,
                    lvTriangleContactPoint.mV.y + lContactNormal.mV.y,
                    lvTriangleContactPoint.mV.z + lContactNormal.mV.z,
                    lvTriangleContactPoint.mV.w + lContactNormal.mV.w
                ).mV);
                lp3DRenderer->DrawArrow(
                    lvTriangleContactPoint, lvArrowEnd,
                    RwRGBAFromPackedWord(0xFF0000FFu));
            }
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

    // ARTIST 0x828AAE08 repeats the first stored plane six times; the plane
    // address never advances. Preserve that original behavior and its exactly
    // three-negative-vertices rejection, including for four-vertex polygons.
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
            const rw::collision::Plane& lrPlane = lrFrustum.GetPlane(0);
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
                    (lrVertex.mV.x - lvCameraPosition.mV.x) * lfNormalX +
                    (lrVertex.mV.y - lvCameraPosition.mV.y) * lfNormalY +
                    (lrVertex.mV.z - lvCameraPosition.mV.z) * lfNormalZ;
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
        const s32 liNumLevels =
            mpTriCollisionManager->GetPolySoupListSpacialMap()->GetNumLevels();
        miLastNumLevels = liNumLevels;
        miNumLevels = liNumLevels;
    }
}
