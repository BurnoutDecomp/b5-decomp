#include "types.hpp"

// rw::math::vpu Vector3/Matrix44Affine TYPES and the canonical Vector3 operation
// vocabulary (operator- / operator/(scalar) / Normalize) live in the RenderWare SDK home.
// Previously this TU carried a file-local redeclaration of Vector3/Matrix44Affine plus its
// own divergent inline operator- / operator/ / Normalize; those are retired here so there
// is exactly ONE program-wide definition (ODR-clean). The local Matrix44Affine spelled its
// rows maRows[0..3]; the canonical type names them xAxis/yAxis/zAxis/wAxis -- row 2 (forward)
// is zAxis, row 3 (translation) is wAxis.
#include "rw/math/vpu/types.h"               // rw::math::vpu::Vector3, Matrix44Affine
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::operator- / operator/ / Normalize

namespace Utils
{
template <typename T>
struct DataPoint
{
    T mCurrent;
    T mPrevious;

    void Reset(const T& lValue)
    {
        mCurrent = lValue;
        mPrevious = lValue;
    }

    const T& GetCurrent() const
    {
        return mCurrent;
    }

    const T& GetPrevious() const
    {
        return mPrevious;
    }
};
}

namespace CgsSound
{
namespace Logic
{
class MicrophoneSystem
{
public:
    enum EMicPositions
    {
        E_MIC_CAMERA = 0,
        E_MIC_PLAYER = 1,
        E_MIC_MAX_NUM_POSITIONS = 2
    };

    enum EPlayer
    {
        E_PLAYER_1 = 0,
        E_PLAYER_2 = 1,
        E_MAX_NUM_PLAYERS = 2
    };

    struct Microphone
    {
        Microphone()
        {
            Reset();
        }

        void Reset()
        {
            rw::math::vpu::Matrix44Affine lIdentity;
            lIdentity.SetIdentity();
            mMicrophoneMatrix.Reset(lIdentity);
            mDirection = rw::math::vpu::Vector3();
            mVelocity = rw::math::vpu::Vector3();
        }

        void Update(f32 lfFrameTime)
        {
            const rw::math::vpu::Matrix44Affine& lCurrent = mMicrophoneMatrix.GetCurrent();
            const rw::math::vpu::Matrix44Affine& lPrevious = mMicrophoneMatrix.GetPrevious();

            mDirection = rw::math::vpu::Normalize(lCurrent.zAxis);

            if (lfFrameTime != 0.0f)
                mVelocity = (lCurrent.wAxis - lPrevious.wAxis) / lfFrameTime;
            else
                mVelocity = rw::math::vpu::Vector3();
        }

    private:
        Utils::DataPoint<rw::math::vpu::Matrix44Affine> mMicrophoneMatrix;
        rw::math::vpu::Vector3 mDirection;
        rw::math::vpu::Vector3 mVelocity;
    };

    MicrophoneSystem();
    void UpdateMicrophones(f32 lfTimeStep);

private:
    s32        miNumPlayers;
    u8         mPad4[12];
    Microphone maMicrophones[E_MIC_MAX_NUM_POSITIONS][E_MAX_NUM_PLAYERS];
};

static_assert(sizeof(rw::math::vpu::Vector3) == 16, "Vector3 layout drift");
static_assert(sizeof(rw::math::vpu::Matrix44Affine) == 64, "Matrix44Affine layout drift");
static_assert(sizeof(Utils::DataPoint<rw::math::vpu::Matrix44Affine>) == 128, "DataPoint<Matrix44Affine> layout drift");
static_assert(sizeof(MicrophoneSystem::Microphone) == 0xA0, "Microphone layout drift");
static_assert(sizeof(MicrophoneSystem) == 0x290, "MicrophoneSystem layout drift");

MicrophoneSystem::MicrophoneSystem()
{
    for (s32 liMicPosition = 0; liMicPosition < E_MIC_MAX_NUM_POSITIONS; ++liMicPosition)
    {
        for (s32 liPlayer = 0; liPlayer < E_MAX_NUM_PLAYERS; ++liPlayer)
            maMicrophones[liMicPosition][liPlayer].Reset();
    }

    miNumPlayers = 0;
}

void MicrophoneSystem::UpdateMicrophones(f32 lfTimeStep)
{
    for (s32 liCount = 0; liCount < miNumPlayers; ++liCount)
    {
        maMicrophones[E_MIC_CAMERA][liCount].Update(lfTimeStep);
        maMicrophones[E_MIC_PLAYER][liCount].Update(lfTimeStep);
    }
}
}
}
