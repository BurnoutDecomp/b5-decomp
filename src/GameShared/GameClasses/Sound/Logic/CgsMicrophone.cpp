#include "types.hpp"

#include <cmath>

namespace rw
{
namespace math
{
namespace vpu
{
struct alignas(16) Vector3
{
    f32 mfX;
    f32 mfY;
    f32 mfZ;
    f32 mfW;

    Vector3()
        : mfX(0.0f)
        , mfY(0.0f)
        , mfZ(0.0f)
        , mfW(0.0f)
    {
    }

    Vector3(f32 lfX, f32 lfY, f32 lfZ, f32 lfW = 0.0f)
        : mfX(lfX)
        , mfY(lfY)
        , mfZ(lfZ)
        , mfW(lfW)
    {
    }

    f32 MagnitudeSquared3() const
    {
        return mfX * mfX + mfY * mfY + mfZ * mfZ;
    }
};

struct alignas(16) Matrix44Affine
{
    Vector3 maRows[4];

    void SetIdentity()
    {
        maRows[0] = Vector3(1.0f, 0.0f, 0.0f, 0.0f);
        maRows[1] = Vector3(0.0f, 1.0f, 0.0f, 0.0f);
        maRows[2] = Vector3(0.0f, 0.0f, 1.0f, 0.0f);
        maRows[3] = Vector3(0.0f, 0.0f, 0.0f, 0.0f);
    }
};

inline Vector3 operator-(const Vector3& lLeft, const Vector3& lRight)
{
    return Vector3(lLeft.mfX - lRight.mfX, lLeft.mfY - lRight.mfY, lLeft.mfZ - lRight.mfZ, 0.0f);
}

inline Vector3 operator/(const Vector3& lVector, f32 lfScalar)
{
    return Vector3(lVector.mfX / lfScalar, lVector.mfY / lfScalar, lVector.mfZ / lfScalar, 0.0f);
}

inline Vector3 Normalize(const Vector3& lVector)
{
    const f32 lfMagnitudeSquared = lVector.MagnitudeSquared3();
    if (lfMagnitudeSquared <= 0.0f)
        return Vector3();

    const f32 lfInverseMagnitude = 1.0f / std::sqrt(lfMagnitudeSquared);
    return Vector3(lVector.mfX * lfInverseMagnitude, lVector.mfY * lfInverseMagnitude, lVector.mfZ * lfInverseMagnitude, 0.0f);
}
}
}
}

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

            mDirection = rw::math::vpu::Normalize(lCurrent.maRows[2]);

            if (lfFrameTime != 0.0f)
                mVelocity = (lCurrent.maRows[3] - lPrevious.maRows[3]) / lfFrameTime;
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
