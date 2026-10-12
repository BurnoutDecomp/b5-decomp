#include "GameShared/GameClasses/SceneManager/CgsSceneManagerDebugComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                           // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"  // the 3D draws
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                     // triangle-test monitor
#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.h"                  // swept sphere / Triangle4 test
#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"                      // SweptSphere
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModule.h"                       // entity + volume managers
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"         // cached slots and triangle batches
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"                        // four-lane cached triangle batches
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstance.h"                           // VolumeInstance
#include "SDKs/EATech/rwcollision/volume_debug_access.h"                                     // rw::collision::Volume
#include "rw/rwcore_structs.h"                                                               // rw::RGBA
#include "rw/math/vpu/matrix44affine_operation.h"                                            // TransformPoint / operator*
#include "pc/math/VmxFloat.h"
#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"

#include <cmath>
#include <cstring>

// CgsSceneManager::SceneManagerDebugComponent -- GetName, DrawPrimitives and the collision-volume
// draw it drives.

namespace CgsSceneManager
{
    namespace
    {
        // ARTIST dword_82F33F60: Construct stores the Triangle-Sphere test monitor handle here.
        s32 siTriangleTestPerfMon = -1;

        // The SoA vectors in Triangle4 hold one scalar per lane, matching the target's four
        // explicit lane extracts in DrawCachedObjectTriangles.
        f32 GetTriangleLane(const rw::math::vpu::Vector4& lrValues, s32 liLane)
        {
            return (&lrValues.x)[liLane];
        }

        rw::math::vpu::MaskScalar BroadcastMaskLane(const f32& lrLane)
        {
            u32 luMaskBits = 0;
            std::memcpy(&luMaskBits, &lrLane, sizeof(luMaskBits));

            rw::math::vpu::MaskScalar lMask;
            std::memcpy(&lMask.x, &luMaskBits, sizeof(luMaskBits));
            std::memcpy(&lMask.y, &luMaskBits, sizeof(luMaskBits));
            std::memcpy(&lMask.z, &luMaskBits, sizeof(luMaskBits));
            std::memcpy(&lMask.w, &luMaskBits, sizeof(luMaskBits));
            return lMask;
        }

        Vector3 CopyVector3Plus(const Vector3Plus& lrPoint)
        {
            return { lrPoint.x, lrPoint.y, lrPoint.z, lrPoint.w };
        }

        void DrawContactAxes(CgsDev::Debug3DImmediateRender* lpRender, const Vector3& lrPosition)
        {
            const Vector3 laAxes[3] = {
                { 1.0f, 0.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f, 0.0f },
                { 0.0f, 0.0f, 1.0f, 0.0f },
            };
            const rw::RGBA lWhite(0xFFu, 0xFFu, 0xFFu, 0xFFu);
            const f32 lfHalfLength = 0.1f;

            for (s32 liAxis = 0; liAxis < 3; ++liAxis)
            {
                Vector3 lvStart = lrPosition;
                Vector3 lvEnd = lrPosition;
                lvStart.x -= laAxes[liAxis].x * lfHalfLength;
                lvStart.y -= laAxes[liAxis].y * lfHalfLength;
                lvStart.z -= laAxes[liAxis].z * lfHalfLength;
                lvEnd.x += laAxes[liAxis].x * lfHalfLength;
                lvEnd.y += laAxes[liAxis].y * lfHalfLength;
                lvEnd.z += laAxes[liAxis].z * lfHalfLength;
                lpRender->DrawLine(lvStart, lvEnd, lWhite);
            }
        }
    }

    const char* SceneManagerDebugComponent::GetName() const
    {
        return "Scene manager";
    }

    // @ X360 0x828B6658. BaseCollisionGenerator::Destruct @0x8284CB38 is called here by the
    // console, but that target is an empty `blr`; its call has no state effect and the source
    // type is not a base of this component. The following named stores match its component fields.
    void SceneManagerDebugComponent::Construct(const SceneManagerModule* lpSceneManagerModule)
    {
        mpSceneManagerModule = lpSceneManagerModule;
        mfPadding = 0.3f;
        mbDrawTriangleTest = false;
        mbDrawPadding = false;
        mfTriangleScale = 1.0f;
        mTriangleOffset.x = 0.0f;
        mTriangleOffset.y = 0.0f;
        mTriangleOffset.z = 0.0f;

        siTriangleTestPerfMon = CgsDev::PerfMonCpu::AddMonitor(
            "Triangle-Sphere test", CgsDev::E_PMP_17, false, 80.0f, true);

        mbDrawCachedTriangles = false;
        mbDrawCacheSpheres = false;
        mbDrawNonCachedTriangles = false;
        mbDrawNonCachedSpheres = false;
        mbDrawCollisionVolumes = false;
        miTestsToExecute = 1000;
        miTotalCycles = 1;
        miCyclesPerCall = 1;
        miCyclesPerTriangle = 1;
    }

    // @ X360 0x828AB500. The component resets the same statistics cells that scene-query
    // producers update and the OnActivate page exposes as read-only variables.
    void SceneManagerDebugComponent::Update()
    {
        SceneManagerModule::ResetDebugPageCounters();
    }

    // @ X360 0x828D04E8. Preserve dispatch order. The target returns directly from the
    // triangle-test branch; it stores the current camera position only when that test is off.
    void SceneManagerDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpRender)
    {
        CGS_ASSERT(lpRender != NULL, "lpDisplay != NULL");

        if (mbDrawCachedTriangles)
            DrawCachedObjectTriangles(lpRender);
        if (mbDrawCacheSpheres)
            DrawCachedObjectSpheres(lpRender);
        if (mbDrawCollisionVolumes)
            DrawPrimitives(lpRender);
        if (mbDrawTriangleTest)
        {
            DrawTriangleTest(lpRender);
        }
        else
        {
            mInitialCameraPosition = lpRender->GetCameraPosition();
        }
    }

    // @ X360 0x828C2E70. Test the four identical Triangle4 lanes against the fixed swept sphere,
    // time the requested number of kernel calls, then draw the sphere, optional padding sphere,
    // and each lane's triangle/contact pair in the target order.
    void SceneManagerDebugComponent::DrawTriangleTest(CgsDev::Debug3DImmediateRender* lpRender)
    {
        lpRender->SetZTestEnable(CgsDev::Debug3DImmediateRender::E_ZTEST_OFF);
        lpRender->SetZTestEnable(CgsDev::Debug3DImmediateRender::E_ZTEST_ON);
        lpRender->SetZTestEnable(CgsDev::Debug3DImmediateRender::E_ZTEST_OFF);
        lpRender->SetZTestEnable(CgsDev::Debug3DImmediateRender::E_ZTEST_ON);

        const Vector3Plus lSpherePositionAndRadius = { 0.5f, 1.0f, 0.5f, 0.5f };
        const Vector3Plus lSphereDirectionAndLength = { 0.0f, -1.0f, 0.0f, 1.0f };
        CgsGeometric::SweptSphere lSweptSphere;
        lSweptSphere.Set(lSpherePositionAndRadius, lSphereDirectionAndLength);

        const f32 lfTriX = mTriangleOffset.x;
        const f32 lfTriY = mTriangleOffset.y;
        const f32 lfTriZ = mTriangleOffset.z;
        const f32 lfTriScale = mfTriangleScale;
        const f32 lfTriXPlusScale = lfTriScale + lfTriX;
        const f32 lfTriZPlusScale = lfTriScale + lfTriZ;

        CgsGeometric::Triangle4 lTriangles = {};
        lTriangles.mVertex0X = { lfTriX, lfTriX, lfTriX, lfTriX };
        lTriangles.mVertex0Y = { lfTriY, lfTriY, lfTriY, lfTriY };
        lTriangles.mVertex0Z = { lfTriZ, lfTriZ, lfTriZ, lfTriZ };
        lTriangles.mVertex1X = { lfTriX, lfTriX, lfTriX, lfTriX };
        lTriangles.mVertex1Y = { lfTriY, lfTriY, lfTriY, lfTriY };
        lTriangles.mVertex1Z = { lfTriZPlusScale, lfTriZPlusScale, lfTriZPlusScale, lfTriZPlusScale };
        lTriangles.mVertex2X = { lfTriXPlusScale, lfTriXPlusScale, lfTriXPlusScale, lfTriXPlusScale };
        lTriangles.mVertex2Y = { lfTriY, lfTriY, lfTriY, lfTriY };
        lTriangles.mVertex2Z = { lfTriZ, lfTriZ, lfTriZ, lfTriZ };

        const u32 luValidLane = 0xFFFFFFFFu;
        std::memcpy(&lTriangles.mValidMasks.x, &luValidLane, sizeof(luValidLane));
        std::memcpy(&lTriangles.mValidMasks.y, &luValidLane, sizeof(luValidLane));
        std::memcpy(&lTriangles.mValidMasks.z, &luValidLane, sizeof(luValidLane));
        std::memcpy(&lTriangles.mValidMasks.w, &luValidLane, sizeof(luValidLane));
        lTriangles.mSurfaceTags.SetZero();
        lTriangles.mEdge0Cosigns.SetZero();
        lTriangles.mEdge1Cosigns.SetZero();
        lTriangles.mEdge2Cosigns.SetZero();

        Vector3 laContactNormals[4];
        Vector3 laTriangleNormals[4];
        Vector3Plus laSphereContactPoints[4];
        Vector3Plus laTriangleContactPoints[4];
        CgsGeometric::Triangle4::Mask4 lIntersectMask;

        CgsDev::PerfMonCpu::StartMonitor(siTriangleTestPerfMon);
        const u64 luStartTime = CgsSystem::GetSystemTimerBaseTime64();
        for (s32 liTest = 0; liTest < miTestsToExecute; ++liTest)
        {
            lIntersectMask = CgsGeometric::IntersectTriangle4SweptSphere(
                lSweptSphere, lTriangles,
                laContactNormals[0], laTriangleNormals[0], laSphereContactPoints[0], laTriangleContactPoints[0],
                laContactNormals[1], laTriangleNormals[1], laSphereContactPoints[1], laTriangleContactPoints[1],
                laContactNormals[2], laTriangleNormals[2], laSphereContactPoints[2], laTriangleContactPoints[2],
                laContactNormals[3], laTriangleNormals[3], laSphereContactPoints[3], laTriangleContactPoints[3]);
        }
        const u64 luElapsedTicks = CgsSystem::GetSystemTimerBaseTime64() - luStartTime;
        const u64 luTimerFrequency = CgsSystem::GetSystemTimerFrequency64();
        const u64 luElapsedScaled = luElapsedTicks * 3200000000ULL;
        const u64 luTotalCycles = luElapsedScaled / luTimerFrequency;
        miTotalCycles = static_cast<s32>(static_cast<u32>(luTotalCycles));
        miCyclesPerCall = miTotalCycles / miTestsToExecute;
        miCyclesPerTriangle = miCyclesPerCall / 4;
        CgsDev::PerfMonCpu::StopMonitor(siTriangleTestPerfMon);

        const Vector3 lSpherePosition = CopyVector3Plus(lSpherePositionAndRadius);
        const Vector3 lSphereWorldPosition = {
            lSpherePosition.x + mInitialCameraPosition.x,
            lSpherePosition.y + mInitialCameraPosition.y,
            lSpherePosition.z + mInitialCameraPosition.z,
            lSpherePosition.w + mInitialCameraPosition.w };
        lpRender->DrawSphere(lSphereWorldPosition, lSpherePositionAndRadius.w,
            rw::RGBA(0xFFu, 0x00u, 0x00u, 0x64u)); // ARTIST r5 = 0x640000FF

        if (mbDrawPadding)
        {
            lpRender->DrawSphere(lSphereWorldPosition,
                CgsPC::VmxFloat::Add(lSpherePositionAndRadius.w, mfPadding),
                rw::RGBA(0x00u, 0xFFu, 0xFFu, 0x63u)); // ARTIST r5 = 0x63FFFF00
        }

        const Vector3 lTriangleVertex0 = { lfTriX, lfTriY, lfTriZ, lfTriX };
        const Vector3 lTriangleVertex1 = { lfTriX, lfTriY, lfTriZPlusScale, lfTriX };
        const Vector3 lTriangleVertex2 = {
            lfTriXPlusScale, lfTriY, lfTriZ, lfTriXPlusScale };

        DrawTriangle(lpRender, mInitialCameraPosition, lTriangleVertex0, lTriangleVertex1, lTriangleVertex2,
            BroadcastMaskLane(lIntersectMask.x),
            CopyVector3Plus(laSphereContactPoints[0]), CopyVector3Plus(laTriangleContactPoints[0]));
        DrawTriangle(lpRender, mInitialCameraPosition, lTriangleVertex0, lTriangleVertex1, lTriangleVertex2,
            BroadcastMaskLane(lIntersectMask.y),
            CopyVector3Plus(laSphereContactPoints[1]), CopyVector3Plus(laTriangleContactPoints[1]));
        DrawTriangle(lpRender, mInitialCameraPosition, lTriangleVertex0, lTriangleVertex1, lTriangleVertex2,
            BroadcastMaskLane(lIntersectMask.z),
            CopyVector3Plus(laSphereContactPoints[2]), CopyVector3Plus(laTriangleContactPoints[2]));
        DrawTriangle(lpRender, mInitialCameraPosition, lTriangleVertex0, lTriangleVertex1, lTriangleVertex2,
            BroadcastMaskLane(lIntersectMask.w),
            CopyVector3Plus(laSphereContactPoints[3]), CopyVector3Plus(laTriangleContactPoints[3]));
    }

    // @ X360 0x828C2560. Walk each used cache slot and its cached Triangle4 batches, drawing
    // only lanes whose validity mask is nonzero. GetTrianglesForCachedObject retains the target
    // cache-allocation assert, including for a used slot with no cached batches.
    void SceneManagerDebugComponent::DrawCachedObjectTriangles(CgsDev::Debug3DImmediateRender* lpRender)
    {
        const TriangleCacheManager& lrCache = mpSceneManagerModule->mTriangleCacheManager;
        const rw::RGBA lColour(0x00u, 0x00u, 0xFFu, 0x80u); // ARTIST dword_82F33FC0 = 0x80FF0000

        for (s32 liSlot = 0; liSlot < static_cast<s32>(KU_MAX_CACHED_OBJECTS); ++liSlot)
        {
            if (!lrCache.mUsedCacheSlots.IsBitSet(static_cast<u32>(liSlot)))
                continue;

            const CacheSlot& lrSlot = lrCache.mpaCachedObjectSlots[liSlot];
            const CgsGeometric::Triangle4* lpaBatches = lrCache.GetTrianglesForCachedObject(liSlot);
            for (s32 liBatch = 0; liBatch < lrSlot.miNumCachedTriangleBatches; ++liBatch)
            {
                const CgsGeometric::Triangle4& lrBatch = lpaBatches[liBatch];
                for (s32 liLane = 0; liLane < 4; ++liLane)
                {
                    if (GetTriangleLane(lrBatch.mValidMasks, liLane) == 0.0f)
                        continue;

                    // ARTIST builds each world-space point as {x, y, z, 1}; retain that
                    // homogeneous lane when gathering the three SoA coordinates.
                    const Vector3 lvVertex0 = {
                        GetTriangleLane(lrBatch.mVertex0X, liLane),
                        GetTriangleLane(lrBatch.mVertex0Y, liLane),
                        GetTriangleLane(lrBatch.mVertex0Z, liLane), 1.0f };
                    const Vector3 lvVertex1 = {
                        GetTriangleLane(lrBatch.mVertex1X, liLane),
                        GetTriangleLane(lrBatch.mVertex1Y, liLane),
                        GetTriangleLane(lrBatch.mVertex1Z, liLane), 1.0f };
                    const Vector3 lvVertex2 = {
                        GetTriangleLane(lrBatch.mVertex2X, liLane),
                        GetTriangleLane(lrBatch.mVertex2Y, liLane),
                        GetTriangleLane(lrBatch.mVertex2Z, liLane), 1.0f };
                    DrawCachedTriangle(lpRender, lvVertex0, lvVertex1, lvVertex2, lColour);
                }
            }
        }
    }

    // @ X360 0x828C2B80. Both radii come from the packed 16-byte sphere lanes in each used slot.
    // The target explicitly passes 0xFFFF0000 and 0xFF00FF00; the RGBA constructor's packed
    // representation preserves those exact words.
    void SceneManagerDebugComponent::DrawCachedObjectSpheres(CgsDev::Debug3DImmediateRender* lpRender)
    {
        const TriangleCacheManager& lrCache = mpSceneManagerModule->mTriangleCacheManager;
        const rw::RGBA lCachedSphereColour(0x00u, 0x00u, 0xFFu, 0xFFu); // r5 = 0xFFFF0000
        const rw::RGBA lInnerSphereColour(0x00u, 0xFFu, 0x00u, 0xFFu); // r5 = 0xFF00FF00

        for (s32 liSlot = 0; liSlot < static_cast<s32>(KU_MAX_CACHED_OBJECTS); ++liSlot)
        {
            if (!lrCache.mUsedCacheSlots.IsBitSet(static_cast<u32>(liSlot)))
                continue;

            const CacheSlot& lrSlot = lrCache.mpaCachedObjectSlots[liSlot];
            const CacheSphere& lrCachedSphere = lrSlot.mLastCachedSphere;
            const Vector3 lvCachedCentre = {
                lrCachedSphere.x, lrCachedSphere.y, lrCachedSphere.z, lrCachedSphere.w };
            lpRender->DrawHollowSphere(lvCachedCentre, lrCachedSphere.w, lCachedSphereColour);

            const CacheSphere& lrInnerSphere = lrSlot.mInnerSpherePositionAndRadius;
            const Vector3 lvInnerCentre = {
                lrInnerSphere.x, lrInnerSphere.y, lrInnerSphere.z, lrInnerSphere.w };
            lpRender->DrawHollowSphere(lvInnerCentre, lrInnerSphere.w, lInnerSphereColour);
        }
    }

    // @ X360 0x828AB9D0. Offset the triangle's three vertices 0.04 units along its normalized
    // face normal, then draw the expanded triangle as a white outline and a coloured solid.
    void SceneManagerDebugComponent::DrawCachedTriangle(CgsDev::Debug3DImmediateRender* lpRender,
                                                          Vector3 lVertex0, Vector3 lVertex1,
                                                          Vector3 lVertex2, rw::RGBA lColour)
    {
        namespace vmx = CgsPC::VmxFloat;
        const Vector3 lEdge0 = vmx::Subtract(lVertex0, lVertex1);
        const Vector3 lEdge1 = vmx::Subtract(lVertex0, lVertex2);
        // 828ABA84..94: rounded product, fused negative subtract, then YZXW.
        const Vector3 lCross = {
            vmx::NegativeMultiplySubtract(lEdge0.z, lEdge1.y, vmx::Multiply(lEdge0.y, lEdge1.z)),
            vmx::NegativeMultiplySubtract(lEdge0.x, lEdge1.z, vmx::Multiply(lEdge0.z, lEdge1.x)),
            vmx::NegativeMultiplySubtract(lEdge0.y, lEdge1.x, vmx::Multiply(lEdge0.x, lEdge1.y)),
            vmx::NegativeMultiplySubtract(lEdge0.w, lEdge1.w, vmx::Multiply(lEdge0.w, lEdge1.w)) };
        // This site has ONE unconditional refinement, unlike the two-step magnitudes.
        const Vector3 lNormal = vmx::Normalize3Unconditional(lCross, 1);
        lVertex0 = vmx::MultiplyAdd(lNormal, 0.04f, lVertex0);
        lVertex1 = vmx::MultiplyAdd(lNormal, 0.04f, lVertex1);
        lVertex2 = vmx::MultiplyAdd(lNormal, 0.04f, lVertex2);

        lpRender->DrawHollowTriangle(lVertex0, lVertex1, lVertex2,
            rw::RGBA(0xFFu, 0xFFu, 0xFFu, 0xFFu));
        lpRender->DrawSolidTriangle(lVertex0, lVertex1, lVertex2, lColour);
    }

    // @ X360 0x828ABF48. Translate the three triangle corners by lTranslation. A miss draws a
    // blue filled outline; a hit draws white and marks both contact points with the target's
    // three 0.1-unit axis ticks.
    void SceneManagerDebugComponent::DrawTriangle(CgsDev::Debug3DImmediateRender* lpRender,
                                                    Vector3 lTranslation, Vector3 lVertex0,
                                                    Vector3 lVertex1, Vector3 lVertex2,
                                                    rw::math::vpu::MaskScalar lHitMask,
                                                    Vector3 lSphereContactPoint,
                                                    Vector3 lTriangleContactPoint)
    {
        lVertex0.x += lTranslation.x;
        lVertex0.y += lTranslation.y;
        lVertex0.z += lTranslation.z;
        lVertex0.w += lTranslation.w;
        lVertex1.x += lTranslation.x;
        lVertex1.y += lTranslation.y;
        lVertex1.z += lTranslation.z;
        lVertex1.w += lTranslation.w;
        lVertex2.x += lTranslation.x;
        lVertex2.y += lTranslation.y;
        lVertex2.z += lTranslation.z;
        lVertex2.w += lTranslation.w;

        if (!lHitMask.GetBool())
        {
            // The ARTIST immediate is 0xFFFF0000; RGBA packs (a<<24)|(b<<16)|(g<<8)|r.
            lpRender->DrawQuad(lVertex0, lVertex0, lVertex1, lVertex2,
                rw::RGBA(0x00u, 0x00u, 0xFFu, 0xFFu));
            return;
        }

        const rw::RGBA lWhite(0xFFu, 0xFFu, 0xFFu, 0xFFu);
        lpRender->DrawQuad(lVertex0, lVertex0, lVertex1, lVertex2, lWhite);

        lSphereContactPoint.x += lTranslation.x;
        lSphereContactPoint.y += lTranslation.y;
        lSphereContactPoint.z += lTranslation.z;
        lSphereContactPoint.w += lTranslation.w;
        lTriangleContactPoint.x += lTranslation.x;
        lTriangleContactPoint.y += lTranslation.y;
        lTriangleContactPoint.z += lTranslation.z;
        lTriangleContactPoint.w += lTranslation.w;
        DrawContactAxes(lpRender, lSphereContactPoint);
        DrawContactAxes(lpRender, lTriangleContactPoint);
    }

    // @ X360 0x828AB568. Captions, registration order, ranges, increments and read-only state
    // are taken from the original ARTIST calls. The dirty flag and statistics remain their
    // original owner storage in SceneManagerModule/TriangleCacheManager.
    void SceneManagerDebugComponent::OnActivate()
    {
        RegisterVariable(&mbDrawCollisionVolumes, "Draw Collision Volumes");
        RegisterVariable(&mbDrawTriangleTest, "Draw Triangle Test");
        RegisterVariable(&mbDrawPadding, "Draw Padding");

        RegisterVariable(&mTriangleOffset.x, "TriX");
        RegisterVariable(&mTriangleOffset.y, "TriY");
        RegisterVariable(&mTriangleOffset.z, "TriZ");
        RegisterVariable(&mfTriangleScale, "TriScale");
        RegisterVariable(&mfPadding, "Padding");
        SetStep(&mTriangleOffset.x, 0.025f);
        SetStep(&mTriangleOffset.y, 0.025f);
        SetStep(&mTriangleOffset.z, 0.025f);
        SetStep(&mfTriangleScale, 0.25f);
        SetStep(&mfPadding, 0.25f);

        RegisterVariable(&mbDrawCachedTriangles, "Draw cached triangles");
        RegisterVariable(&mbDrawCacheSpheres, "Draw cache spheres");
        RegisterVariable(&mbDrawNonCachedTriangles, "Draw non-cached triangles");
        RegisterVariable(&mbDrawNonCachedSpheres, "Draw non-cached spheres");

        RegisterVariable(&miTestsToExecute, "Tests To Execute");
        SetRange(&miTestsToExecute, 1, 1000000);
        SetStep(&miTestsToExecute, 1000);
        RegisterVariable(&miTotalCycles, "Total Cycles");
        SetReadOnly(&miTotalCycles, true);
        RegisterVariable(&miCyclesPerCall, "Cycles Per Call");
        SetReadOnly(&miCyclesPerCall, true);
        RegisterVariable(&miCyclesPerTriangle, "Cycles Per Triangle");
        SetReadOnly(&miCyclesPerTriangle, true);

        bool* lpbForceCachesDirty =
            &const_cast<SceneManagerModule*>(mpSceneManagerModule)->mTriangleCacheManager.mbDEBUGForceAllDirty;
        RegisterVariable(lpbForceCachesDirty, "Force caches to be dirty");
        SceneManagerModule::RegisterDebugPageCounters(*this);
    }

    // Every slot of the volume-instance pool; empty slots are skipped. Each instance's volume is
    // looked up by its volume index in the volume manager.
    void SceneManagerDebugComponent::DrawPrimitives(CgsDev::Debug3DImmediateRender* lpRender)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_NUM_VOLUME_INSTANCES; ++liIndex)
        {
            const VolumeInstance* lpVolumeInstance =
                mpSceneManagerModule->mEntityManager.GetVolumeInstance(liIndex);
            if (lpVolumeInstance == 0)
                continue;

            const rw::collision::Volume* lpVolume = reinterpret_cast<const rw::collision::Volume*>(
                mpSceneManagerModule->mVolumeManager.GetRwVolume(lpVolumeInstance->miVolumeIndex));
            DebugRenderCollisionVolume(lpVolume, lpRender, lpVolumeInstance->mWorldSpaceTransform);
        }
    }

    // A sphere is drawn hollow at its transformed centre. A capsule or cylinder runs along the
    // composed transform's z axis between -half-height and +half-height. A box is drawn as its
    // half-extents box under the composed transform. Any other type asserts.
    void SceneManagerDebugComponent::DebugRenderCollisionVolume(const rw::collision::Volume* lpVolume,
                                                                CgsDev::Debug3DImmediateRender* lpRender,
                                                                const Matrix44Affine& lTransform) const
    {
        const rw::RGBA lColour(0, 0, 0, 255);

        switch (lpVolume->GetType())
        {
            case rw::collision::E_VOLUMETYPE_SPHERE:
            {
                const Vector3 lv3Centre =
                    rw::math::vpu::TransformPoint(lTransform, lpVolume->GetRelativeTransform().wAxis);
                lpRender->DrawHollowSphere(lv3Centre, lpVolume->GetRadius(), lColour);
            }
            break;

            case rw::collision::E_VOLUMETYPE_CAPSULE:
            case rw::collision::E_VOLUMETYPE_CYLINDER:
            {
                const Matrix44Affine lWorld = lpVolume->GetRelativeTransform() * lTransform;
                const f32 lfHalfHeight = lpVolume->GetExtent().x;

                Vector3 lv3Start;
                lv3Start.x = 0.0f;
                lv3Start.y = 0.0f;
                lv3Start.z = -lfHalfHeight;
                lv3Start.w = 0.0f;
                Vector3 lv3End;
                lv3End.x = 0.0f;
                lv3End.y = 0.0f;
                lv3End.z = lfHalfHeight;
                lv3End.w = 0.0f;

                const Vector3 lv3WorldStart = rw::math::vpu::TransformPoint(lWorld, lv3Start);
                const Vector3 lv3WorldEnd   = rw::math::vpu::TransformPoint(lWorld, lv3End);
                if (lpVolume->GetType() == rw::collision::E_VOLUMETYPE_CAPSULE)
                    lpRender->DrawCapsule(lv3WorldStart, lv3WorldEnd, lpVolume->GetRadius(), lColour);
                else
                    lpRender->DrawCylinder(lv3WorldStart, lv3WorldEnd, lpVolume->GetRadius(), lColour);
            }
            break;

            case rw::collision::E_VOLUMETYPE_BBOX:
            {
                const Matrix44Affine lWorld = lpVolume->GetRelativeTransform() * lTransform;
                const Vector3& lrv3Extent = lpVolume->GetExtent();

                Vector3 lv3Max;
                lv3Max.x = lrv3Extent.x;
                lv3Max.y = lrv3Extent.y;
                lv3Max.z = lrv3Extent.z;
                lv3Max.w = 0.0f;
                Vector3 lv3Min;
                lv3Min.x = -lv3Max.x;
                lv3Min.y = -lv3Max.y;
                lv3Min.z = -lv3Max.z;
                lv3Min.w = -lv3Max.w;

                lpRender->DrawBox(lv3Min, lv3Max, lWorld, lColour);
            }
            break;

            default:
                CGS_ASSERT(false, "Collision volume very confused about what type it is... corrupted?");
                break;
        }
    }
}
