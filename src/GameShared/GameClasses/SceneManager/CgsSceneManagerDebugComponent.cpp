#include "GameShared/GameClasses/SceneManager/CgsSceneManagerDebugComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                           // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"  // the 3D draws
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModule.h"                       // entity + volume managers
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstance.h"                           // VolumeInstance
#include "SDKs/EATech/rwcollision/volume_debug_access.h"                                     // rw::collision::Volume
#include "rw/rwcore_structs.h"                                                               // rw::RGBA
#include "rw/math/vpu/matrix44affine_operation.h"                                            // TransformPoint / operator*

// CgsSceneManager::SceneManagerDebugComponent -- GetName, DrawPrimitives and the collision-volume
// draw it drives.

namespace CgsSceneManager
{
    const char* SceneManagerDebugComponent::GetName() const
    {
        return "Scene manager";
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
