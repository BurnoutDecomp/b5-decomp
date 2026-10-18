#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManager.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupList.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox4.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdlib>

namespace CgsSceneManager
{
DrawCollisionPolyCallback TriangleCollisionDebugComponent::siDrawPolyCallbacks[KI_MAX_NUM_COLLISION_POLY_CALLBACKS] = {};
s32 TriangleCollisionDebugComponent::siNumDrawPolyCallbacks = 0; // 830848A0; slots 8307A880

// ARTIST 828B3340; query fields retain their original initial values.
void TriangleCollisionDebugComponent::Construct(const TriangleCollisionManager* lpTriCollisionManager)
{
    CGS_ASSERT(lpTriCollisionManager != nullptr, "lpTriCollisionManager != NULL");
    CgsDev::DebugComponent::Construct();
    mpTriCollisionManager = lpTriCollisionManager;
    mfSoupDrawDistance = 27.5f; mbUpdateViewFrustum = true;
    mbDrawPolySoups = mbDrawSoupAabbs = mbDrawEdgeCosines = false;
    mbTestSphere = mbTestLine = mbTestBox = mbTestSweptSphere = false;
    mbNeedToInitSpherePos = mbNeedToInitBoxPos = mbNeedToInitSweptSpherePos = true;
    mbNeedToInitLinePosition = false; mpcSpacialBuffer = nullptr;
    for (s32 li = 0; li < 3; ++li)
    {
        mSpherePos[li] = mBoxPos[li] = mBoxOrientation[li] = mSweptSpherePos[li] = mSweptSphereVelocity[li] = 0.f;
        mBoxSize[li] = 1.f;
    }
    mfSphereRad = .5f; muSphereTestCycles = 0;
    mfSphereTestTimeSecs = mfSphereTestTimePercentFrame = 0.f; miSphereTestResultCount = 0;
    miMaxLevelToDraw = 8; miMinLevelToDraw = 0;
    miNumLevels = 1; miLastNumLevels = miMemUse = miNumListsSearched = 0;
    muBoxTestCycles = muBoxTestCyclesPerTriangle = 0; miNumBoxTests = 0;
    mfBoxTestTime = mfBoxTestPercent = 0.f; mSweptSphereSize = 1.f;
    const char* lapcNames[] = {"Sphere test perf mon", "  ST: Get lists", "  ST: Process soups",
                              "Line test perf mon", "  LT: Get lists", "  LT: Process soups"};
    s32* lapMonitors[] = {&miSphereTestPerfMon, &miSphereTestGetListsPerfMon, &miSphereTestProcessSoupsPerfMon,
                         &miLineTestPerfMon, &miLineTestGetListsPerfMon, &miLineTestProcessSoupsPerfMon};
    for (s32 li = 0; li < 6; ++li)
        *lapMonitors[li] = CgsDev::PerfMonCpu::AddMonitor(lapcNames[li], CgsDev::E_PMP_4, false, 10.f, true);
    mMalloc.Construct();
}

// ARTIST 828AAA68. Native read-only metadata prevents selecting diagnostics
// whose consumers remain unreconstructed. This TU is not a completed query sandbox.
void TriangleCollisionDebugComponent::OnActivate()
{
    RegisterVariable(&mbDrawPolySoups, "Draw collision world");
    RegisterVariable(&mbDrawSoupAabbs, "Draw soup AABBs");
    RegisterVariable(&mfSoupDrawDistance, "Draw distance");
    SetRange(&mfSoupDrawDistance, .5f, 100.f); SetStep(&mfSoupDrawDistance, .5f);
    RegisterVariable(&mbDrawEdgeCosines, "Draw edge cosines");
    SetReadOnly(&mbDrawEdgeCosines, true); // TODO: 828B3EE4..828B4648 edge annotations.
    RegisterVariable(&mbTestSweptSphere, "Test swept sphere collision");
    SetReadOnly(&mbTestSweptSphere, true); // TODO: TestPolySoupSweptSphereCollision 828B3580.
    const char* lapcPos[] = {"S.Sphere X pos", "S.Sphere Y pos", "S.Sphere Z pos"};
    const char* lapcVel[] = {"S.Sphere X vel", "S.Sphere Y vel", "S.Sphere Z vel"};
    for (s32 li = 0; li < 3; ++li)
    {
        RegisterVariable(&mSweptSpherePos[li], lapcPos[li]); SetStep(&mSweptSpherePos[li], .25f);
        SetReadOnly(&mSweptSpherePos[li], true);
    }
    RegisterVariable(&mSweptSphereSize, "S.Sphere Radius"); SetStep(&mSweptSphereSize, .05f);
    SetReadOnly(&mSweptSphereSize, true);
    for (s32 li = 0; li < 3; ++li)
    {
        RegisterVariable(&mSweptSphereVelocity[li], lapcVel[li]); SetStep(&mSweptSphereVelocity[li], .05f);
        SetReadOnly(&mSweptSphereVelocity[li], true);
    }
    RegisterVariable(&miNumListsSearched, "Num lists searched"); SetReadOnly(&miNumListsSearched, true);
    RegisterVariable(&miNumLevels, "Num spacial levels"); SetRange(&miNumLevels, 1, 8);
    RegisterVariable(&miMemUse, "Spacial map memory use"); SetReadOnly(&miMemUse, true);
    RegisterVariable(&miMaxLevelToDraw, "Max Draw Spacial Level"); SetRange(&miMaxLevelToDraw, 0, 8);
    SetReadOnly(&miMaxLevelToDraw, true); // TODO: query-spatial-map drawing.
    RegisterVariable(&miMinLevelToDraw, "Min Draw Spacial Level"); SetRange(&miMinLevelToDraw, 0, 8);
    SetReadOnly(&miMinLevelToDraw, true);
    mbNeedToInitSpherePos = mbNeedToInitLinePosition = true;
    miLastNumLevels = miNumLevels = mpTriCollisionManager->GetPolySoupListSpacialMap()->GetNumLevels();
}

// ARTIST 828AAF18. The callback count advances before the bounds-checked slot lookup.
bool TriangleCollisionDebugComponent::RegisterDrawCollisionPolyCallback(
    DrawCollisionPolyCallbackData::DrawCollisionPolyCallback lpHandlerFunc, void* lpUserData)
{
    CGS_ASSERT(lpHandlerFunc != nullptr, "lpHandlerFunc != NULL");
    if (siNumDrawPolyCallbacks >= KI_MAX_NUM_COLLISION_POLY_CALLBACKS) return false;
    DrawCollisionPolyCallback& lrSlot = GetDrawCollisionPolyCallback(siNumDrawPolyCallbacks++);
    CGS_ASSERT(lpHandlerFunc != nullptr, "lpCallbackFunc != NULL");
    lrSlot = {lpHandlerFunc, lpUserData};
    return true;
}

// ARTIST 828AAFC0: normal in v1, renderer/points/count/tag/index in r3..r7.
void TriangleCollisionDebugComponent::DrawCollisionPoly(CgsDev::Debug3DImmediateRender* lpDisplay,
    const Vector3* lpaPoints, s32 liNumPoints, Vector3 lNormal, u32 luTag, s32 liSoupIndex)
{
    for (s32 li = 0; li < siNumDrawPolyCallbacks; ++li)
        siDrawPolyCallbacks[li].mpCallbackFunc(lpDisplay, siDrawPolyCallbacks[li].mpUserData,
            lpaPoints, liNumPoints, lNormal, luTag, liSoupIndex);
}

// ARTIST 828C0E20: left/right are W+/-X; bottom/top W-/+Y;
// near is Z (D3D's zero near clip), far W-Z. All six store bases are checked.
void TriangleCollisionDebugComponent::UpdateViewFrustum(CgsDev::Debug3DImmediateRender* lpDisplay)
{
    CGS_ASSERT(lpDisplay != nullptr, "lpDisplay != NULL");
    const Matrix44 lM = lpDisplay->GetViewProjectionMatrix();
    const Vector4 laColumns[] = {
        {lM.xAxis.x,lM.yAxis.x,lM.zAxis.x,lM.wAxis.x},
        {lM.xAxis.y,lM.yAxis.y,lM.zAxis.y,lM.wAxis.y},
        {lM.xAxis.z,lM.yAxis.z,lM.zAxis.z,lM.wAxis.z},
        {lM.xAxis.w,lM.yAxis.w,lM.zAxis.w,lM.wAxis.w}};
    const u32 lauPlanes[] = {2,3,5,4};
    for (s32 li = 0; li < 2; ++li)
    {
        const Vector4& lrA = laColumns[3]; const Vector4& lrB = laColumns[li];
        mViewFrustum.SetPlane(lauPlanes[li*2], rw::collision::Plane(Vector4{lrA.x+lrB.x,lrA.y+lrB.y,lrA.z+lrB.z,lrA.w+lrB.w}));
        mViewFrustum.SetPlane(lauPlanes[li*2+1], rw::collision::Plane(Vector4{lrA.x-lrB.x,lrA.y-lrB.y,lrA.z-lrB.z,lrA.w-lrB.w}));
    }
    mViewFrustum.SetPlane(0, rw::collision::Plane(laColumns[2]));
    const Vector4& lrW = laColumns[3]; const Vector4& lrZ = laColumns[2];
    mViewFrustum.SetPlane(1, rw::collision::Plane(Vector4{lrW.x-lrZ.x,lrW.y-lrZ.y,lrW.z-lrZ.z,lrW.w-lrZ.w}));
}

// ARTIST 828AAE08 repeats plane 0 and accumulates outside points across
// all six passes. Preserve that original bug and the literal ==3 for quads.
bool TriangleCollisionDebugComponent::IsPolyVisible(const Vector3* lpaPoints, s32 liNumPoints,
    CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    s32 liOutside = 0;
    const auto& lrPlane = mViewFrustum.GetPlane(0);
    const f32 lfInvLength = 1.f/std::sqrt(lrPlane.x*lrPlane.x+lrPlane.y*lrPlane.y+lrPlane.z*lrPlane.z);
    const Vector3 lNormal{lrPlane.x*lfInvLength,lrPlane.y*lfInvLength,lrPlane.z*lfInvLength,0.f};
    const Vector3 lCamera = lpDisplay->GetCameraPosition();
    for (s32 liPlane = 0; liPlane < 6; ++liPlane)
    {
        for (s32 li = 0; li < liNumPoints; ++li)
        {
            const Vector3 lDelta = lpaPoints[li]-lCamera;
            if (lDelta.x*lNormal.x+lDelta.y*lNormal.y+lDelta.z*lNormal.z < 0.f) ++liOutside;
        }
        if (liOutside == 3) return false;
    }
    return true;
}

// Raw ARTIST 828B3B98..828B3EE0 (export hole). Edge annotations after 3EE4 remain TODO.
void TriangleCollisionDebugComponent::DrawPolySoupPoly(CgsDev::Debug3DImmediateRender* lpDisplay,
    const CgsGeometric::PolygonSoup* lpSoup, s32 liSoupIndex, u8 luPolyIndex, bool lbDistanceCull)
{
    CGS_ASSERT(lpDisplay != nullptr, "lpDisplay != NULL");
    const auto* lpPoly = reinterpret_cast<const CgsGeometric::PolygonSoupPoly*>(lpSoup->GetPolygon(luPolyIndex));
    Vector3 laPoints[4] = {};
    const s32 liCount = luPolyIndex < lpSoup->GetNumQuads() ? 4 : 3;
    for (s32 li = 0; li < liCount; ++li) laPoints[li] = lpSoup->GetVertexPosition(lpPoly->mau8VertexIndex[li]);
    const Vector3 lA = laPoints[1]-laPoints[0], lB = laPoints[2]-laPoints[0];
    Vector3 lNormal{lA.y*lB.z-lA.z*lB.y,lA.z*lB.x-lA.x*lB.z,lA.x*lB.y-lA.y*lB.x,0.f};
    const f32 lfInvLength = 1.f/std::sqrt(lNormal.x*lNormal.x+lNormal.y*lNormal.y+lNormal.z*lNormal.z);
    lNormal = lNormal*lfInvLength;
    Vector3 lCentre{0.f,0.f,0.f,0.f};
    for (s32 li = 0; li < liCount; ++li) lCentre = lCentre+laPoints[li];
    lCentre = lCentre*(1.f/static_cast<f32>(liCount));
    const Vector3 lDelta = lpDisplay->GetCameraPosition()-lCentre;
    if (IsPolyVisible(laPoints,liCount,lpDisplay) &&
        (!lbDistanceCull || lDelta.x*lDelta.x+lDelta.y*lDelta.y+lDelta.z*lDelta.z < mfSoupDrawDistance*mfSoupDrawDistance))
        DrawCollisionPoly(lpDisplay,laPoints,liCount,lNormal,lpPoly->muSurfaceTag,liSoupIndex);
}

// ARTIST 828C09D8: list contains camera; soup contains camera or centre is <100m away.
void TriangleCollisionDebugComponent::DrawPolySoups(CgsDev::Debug3DImmediateRender* lpDisplay)
{
    CGS_ASSERT(lpDisplay != nullptr,"lpDisplay != NULL");
    const auto* lpMap = mpTriCollisionManager->GetPolySoupListSpacialMap();
    const Vector3 lCamera = lpDisplay->GetCameraPosition();
    const Vector4 lCamera4{lCamera.x,lCamera.y,lCamera.z,0.f};
    // unk_82F33F80, read from the decrypted ARTIST image.
    const u32 KAU_COLOURS[] = {0x608080ff,0x6000ffff,0x6080ff80,0x60ff0000,0x60400080,0x60808040,0x60ffff00,0x600080ff,
        0x60ff0080,0x60c0c0c0,0x60008000,0x600000ff,0x60800040,0x60000000,0x60ffffff,0x60c08000};
    for (s32 liList = 0; liList < lpMap->GetNumPolySoupLists(); ++liList)
    {
        const auto* lpList = lpMap->GetPolySoupList(liList);
        CgsGeometric::AxisAlignedBox lOverall;
        lOverall.mMin = {lpList->mOverallAabb[0],lpList->mOverallAabb[1],lpList->mOverallAabb[2],lpList->mOverallAabb[3]};
        lOverall.mMax = {lpList->mOverallAabb[4],lpList->mOverallAabb[5],lpList->mOverallAabb[6],lpList->mOverallAabb[7]};
        if (!lOverall.ContainsPoint(lCamera4)) continue;
        for (s32 liSoup = 0; liSoup < lpList->GetNumPolygonSoups(); ++liSoup)
        {
            const auto* lpSoup = lpList->GetPolygonSoup(liSoup);
            const auto lBox = lpList->Get4Boxes(liSoup/4)->GetAxisAlignedBox(liSoup%4);
            const Vector3 lCentre{(lBox.mMin.x+lBox.mMax.x)*.5f,(lBox.mMin.y+lBox.mMax.y)*.5f,(lBox.mMin.z+lBox.mMax.z)*.5f,0.f};
            const Vector3 lDelta = lCamera-lCentre;
            if (!lBox.ContainsPoint(lCamera4) && !(lDelta.x*lDelta.x+lDelta.y*lDelta.y+lDelta.z*lDelta.z < 10000.f)) continue;
            for (s32 liPoly = 0; liPoly < lpSoup->GetNumPolygons(); ++liPoly)
                DrawPolySoupPoly(lpDisplay,lpSoup,liSoup,static_cast<u8>(liPoly),true);
            if (mbDrawSoupAabbs) lpDisplay->DrawBox(Vector3{lBox.mMin.x,lBox.mMin.y,lBox.mMin.z,0.f},
                Vector3{lBox.mMax.x,lBox.mMax.y,lBox.mMax.z,0.f},rw::RGBA(static_cast<u8>(KAU_COLOURS[liSoup%16]),static_cast<u8>(KAU_COLOURS[liSoup%16]>>8),static_cast<u8>(KAU_COLOURS[liSoup%16]>>16),255));
        }
    }
}

// Raw ARTIST 828AAD50: rebuild on a changed level count with a private 2MiB allocator.
void TriangleCollisionDebugComponent::BuildSpacialMap()
{
    if (miNumLevels == miLastNumLevels) return;
    miLastNumLevels = miNumLevels;
    if (!mpcSpacialBuffer)
    {
        mpcSpacialBuffer = static_cast<char*>(std::malloc(2097152));
        mMalloc.Construct(); mMalloc.Create(mpcSpacialBuffer,2097152); mMalloc.SetAlignment(16);
    }
    else mMalloc.FreeAll();
    const_cast<TriangleCollisionManager*>(mpTriCollisionManager)->GetPolySoupListSpacialMap()->BuildSpacialPartition(&mMalloc,miNumLevels,2048);
    miMemUse = static_cast<s32>(mMalloc.GetUsage());
}

// ARTIST 828CAE88 drawing path. Query dispatch remains TODO and its controls read-only.
void TriangleCollisionDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay)
{
    if (mbUpdateViewFrustum) UpdateViewFrustum(lpDisplay);
    if (mbDrawPolySoups) DrawPolySoups(lpDisplay);
    BuildSpacialMap();
}
}
