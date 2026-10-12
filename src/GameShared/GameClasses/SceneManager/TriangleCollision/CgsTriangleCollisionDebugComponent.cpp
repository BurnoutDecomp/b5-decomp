#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h"

#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManager.h"

#include <cmath>

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
