#include "GameShared/GameClasses/SceneManager/CgsSceneManagerDebugComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                           // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"  // the 3D draws
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                     // triangle-test monitor
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModule.h"                       // entity + volume managers
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"         // cached slots and triangle batches
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"                        // four-lane cached triangle batches
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstance.h"                           // VolumeInstance
#include "SDKs/EATech/rwcollision/volume_debug_access.h"                                     // rw::collision::Volume
#include "rw/rwcore_structs.h"                                                               // rw::RGBA
#include "rw/math/vpu/matrix44affine_operation.h"                                            // TransformPoint / operator*

#include <cmath>

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

    // @ X360 0x828D04E8. Preserve the target dispatch order and trailing camera-position store.
    // The three named callees below are still in the pending consumer recovery set; their
    // declarations are kept so the calls are not silently omitted from the source path.
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
            DrawTriangleTest(lpRender);

        mInitialCameraPosition = lpRender->GetCameraPosition();
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
        const f32 lfEdge0X = lVertex0.x - lVertex1.x;
        const f32 lfEdge0Y = lVertex0.y - lVertex1.y;
        const f32 lfEdge0Z = lVertex0.z - lVertex1.z;
        const f32 lfEdge1X = lVertex0.x - lVertex2.x;
        const f32 lfEdge1Y = lVertex0.y - lVertex2.y;
        const f32 lfEdge1Z = lVertex0.z - lVertex2.z;

        f32 lfNormalX = lfEdge0Y * lfEdge1Z - lfEdge0Z * lfEdge1Y;
        f32 lfNormalY = lfEdge0Z * lfEdge1X - lfEdge0X * lfEdge1Z;
        f32 lfNormalZ = lfEdge0X * lfEdge1Y - lfEdge0Y * lfEdge1X;
        const f32 lfInverseLength = 1.0f / std::sqrt(
            lfNormalX * lfNormalX + lfNormalY * lfNormalY + lfNormalZ * lfNormalZ);
        lfNormalX *= lfInverseLength;
        lfNormalY *= lfInverseLength;
        lfNormalZ *= lfInverseLength;

        const f32 lfOffset = 0.04f;
        lVertex0.x += lfNormalX * lfOffset;
        lVertex0.y += lfNormalY * lfOffset;
        lVertex0.z += lfNormalZ * lfOffset;
        lVertex1.x += lfNormalX * lfOffset;
        lVertex1.y += lfNormalY * lfOffset;
        lVertex1.z += lfNormalZ * lfOffset;
        lVertex2.x += lfNormalX * lfOffset;
        lVertex2.y += lfNormalY * lfOffset;
        lVertex2.z += lfNormalZ * lfOffset;

        lpRender->DrawHollowTriangle(lVertex0, lVertex1, lVertex2,
            rw::RGBA(0xFFu, 0xFFu, 0xFFu, 0xFFu));
        lpRender->DrawSolidTriangle(lVertex0, lVertex1, lVertex2, lColour);
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
