#include "GameSource/World/EnvironmentMap/BrnEnvironmentMap.h"
#include "types.hpp"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::Add

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGraphics::EnvironmentMap::Construct @ 0x827B40D0
//   BrnGraphics::EnvironmentMap::Release   @ 0x827B4218
//   BrnGraphics::EnvironmentMap::Update    @ 0x827B4268

// (The former cpp-local rw::math::vpu::Vector3/Add and CgsGraphics::Camera
// stand-ins were retired 2026-07-24: the real homes (rw vpu vendor header via
// BrnEnvironmentMap.h, CgsCamera.h) now provide them.)

namespace BrnGraphics
{
    static const rw::math::vpu::Vector3 KAV_ENV_MAP_LOOK_DIRECTIONS[E_FACE_NUM] =
    {
        {  1.0f,  0.0f,  0.0f, 0.0f },
        { -1.0f,  0.0f,  0.0f, 0.0f },
        {  0.0f,  1.0f,  0.0f, 0.0f },
        {  0.0f, -1.0f,  0.0f, 0.0f },
        {  0.0f,  0.0f,  1.0f, 0.0f },
        {  0.0f,  0.0f, -1.0f, 0.0f }
    };

    static const rw::math::vpu::Vector3 KAV_ENV_MAP_UP_DIRECTIONS[E_FACE_NUM] =
    {
        { 0.0f, 1.0f,  0.0f, 0.0f },
        { 0.0f, 1.0f,  0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f, 0.0f },
        { 0.0f, 0.0f,  1.0f, 0.0f },
        { 0.0f, 1.0f,  0.0f, 0.0f },
        { 0.0f, 1.0f,  0.0f, 0.0f }
    };

    void EnvironmentMap::Construct()
    {
        mCameraPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
        mfFOV = 0.0f;
        mfAspectRatio = 0.0f;

        for (u32 luEnvMapFace = 0; luEnvMapFace < E_FACE_NUM; ++luEnvMapFace)
        {
            maEnvMapCameras[luEnvMapFace].Construct();
        }
    }

    // X360 (WorldModule::Destruct @0x827BD0F0 tail call target): release the
    // face cameras -- same walk as Release, no return.
    void EnvironmentMap::Destruct()
    {
        for (u32 luEnvMapFace = 0; luEnvMapFace < E_FACE_NUM; ++luEnvMapFace)
        {
            maEnvMapCameras[luEnvMapFace].Release();
        }
    }

    bool EnvironmentMap::Release()
    {
        for (u32 luEnvMapFace = 0; luEnvMapFace < E_FACE_NUM; ++luEnvMapFace)
        {
            maEnvMapCameras[luEnvMapFace].Release();
        }

        return true;
    }

    void EnvironmentMap::Update(rw::math::vpu::Vector3 lCameraPosition)
    {
        mCameraPosition = lCameraPosition;

        for (u32 luEnvMapFace = 0; luEnvMapFace < E_FACE_NUM; ++luEnvMapFace)
        {
            const rw::math::vpu::Vector3 lTargetPosition =
                rw::math::vpu::Add(lCameraPosition, KAV_ENV_MAP_LOOK_DIRECTIONS[luEnvMapFace]);

            maEnvMapCameras[luEnvMapFace].LookAt(
                lCameraPosition,
                KAV_ENV_MAP_UP_DIRECTIONS[luEnvMapFace],
                lTargetPosition);
            maEnvMapCameras[luEnvMapFace].SetPerspectiveProjectionMatrixRightHanded();
        }
    }
}
