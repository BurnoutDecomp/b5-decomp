#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h"

#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManager.h"

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
