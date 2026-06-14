#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGraphics::EnvironmentMap::Construct @ 0x827B40D0
//   BrnGraphics::EnvironmentMap::Release   @ 0x827B4218
//   BrnGraphics::EnvironmentMap::Update    @ 0x827B4268

namespace rw
{
namespace math
{
namespace vpu
{
    struct Vector3
    {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };

    static Vector3 Add(const Vector3& lLhs, const Vector3& lRhs)
    {
        Vector3 lResult = { lLhs.x + lRhs.x, lLhs.y + lRhs.y, lLhs.z + lRhs.z, 0.0f };
        return lResult;
    }
}
}
}

namespace CgsGraphics
{
    struct Camera
    {
        void Construct();
        void Release();
        void LookAt(rw::math::vpu::Vector3 lEyePosition, rw::math::vpu::Vector3 lUpVector, rw::math::vpu::Vector3 lTargetPosition);
        void SetPerspectiveProjectionMatrixRightHanded();

        u8 maStorage[368];
    };
}

namespace BrnGraphics
{
    enum EEnvironmentMapFace
    {
        E_FACE_POSITIVE_X = 0,
        E_FACE_NEGATIVE_X = 1,
        E_FACE_POSITIVE_Y = 2,
        E_FACE_NEGATIVE_Y = 3,
        E_FACE_POSITIVE_Z = 4,
        E_FACE_NEGATIVE_Z = 5,
        E_FACE_NUM = 6
    };

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

    struct EnvironmentMap
    {
        void Construct();
        bool Release();
        void Update(rw::math::vpu::Vector3 lCameraPosition);

        CgsGraphics::Camera maEnvMapCameras[E_FACE_NUM];
        rw::math::vpu::Vector3 mCameraPosition;
        f32 mfFOV;
        f32 mfAspectRatio;
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
