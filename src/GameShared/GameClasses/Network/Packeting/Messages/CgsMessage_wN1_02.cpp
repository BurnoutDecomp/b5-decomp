#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsNetwork::PackOrUnpackMatrix: the affine field primitive. Kept apart from the mounted
// CgsMessage.cpp because packing reads the rotation through
// Message::GetEulerAnglesFromMatrix, which has no body yet: mount this file together with
// that function.

namespace CgsNetwork
{
    namespace
    {
        const f32 KF_PI = 3.14159265f;
    }

    // The rotation travels as roll, pitch and yaw, each in [-pi, pi] over its own bit
    // count (a pack stops at the first angle that does not fit); then the translation
    // row, one axis at a time, in [lPosMin, lPosMax] over the per-axis bit counts, each
    // through a copy written back into the row. An unknown lifecycle state asserts and
    // still runs the translation part.
    PackOrUnpackResult PackOrUnpackMatrix(Message* lpMessage, rw::math::vpu::Matrix44Affine* lpMatrix,
                                          s32 liRollBits, s32 liPitchBits, s32 liYawBits,
                                          rw::math::vpu::Vector3 lPosMin, rw::math::vpu::Vector3 lPosMax,
                                          s32 liPosXBits, s32 liPosYBits, s32 liPosZBits)
    {
        f32 lfRoll  = 0.0f;
        f32 lfPitch = 0.0f;
        f32 lfYaw   = 0.0f;

        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            Message::GetEulerAnglesFromMatrix(*lpMatrix, &lfRoll, &lfPitch, &lfYaw);

            u32 luPacked = 0;
            FloatQuantiser::Pack(lfRoll, -KF_PI, KF_PI, liRollBits, &luPacked);
            if (!lpMessage->mBitstream.AddBits(luPacked, liRollBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            FloatQuantiser::Pack(lfPitch, -KF_PI, KF_PI, liPitchBits, &luPacked);
            if (!lpMessage->mBitstream.AddBits(luPacked, liPitchBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            FloatQuantiser::Pack(lfYaw, -KF_PI, KF_PI, liYawBits, &luPacked);
            if (!lpMessage->mBitstream.AddBits(luPacked, liYawBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }
        }
        else if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            lpMessage->mBitstream.GetQuantisedFloat(&lfRoll, -KF_PI, KF_PI, liRollBits);
            lpMessage->mBitstream.GetQuantisedFloat(&lfPitch, -KF_PI, KF_PI, liPitchBits);
            lpMessage->mBitstream.GetQuantisedFloat(&lfYaw, -KF_PI, KF_PI, liYawBits);
            Message::SetMatrixFromEulerAngles(lpMatrix, lfRoll, lfPitch, lfYaw);
        }
        else
        {
            CGS_ASSERT(false,
                       "CgsNetwork::Message::PackOrUnpack called without telling "
                       "it which it is doing\n");
        }

        f32 lfPosition = lpMatrix->wAxis.x;
        PackOrUnpackResult lxResult = PackOrUnpackFloat(lpMessage, &lfPosition, lPosMin.x, lPosMax.x, liPosXBits);
        lpMatrix->wAxis.x = lfPosition;

        lfPosition = lpMatrix->wAxis.y;
        lxResult   = PackOrUnpackFloat(lpMessage, &lfPosition, lPosMin.y, lPosMax.y, liPosYBits) | lxResult;
        lpMatrix->wAxis.y = lfPosition;

        lfPosition = lpMatrix->wAxis.z;
        lxResult   = PackOrUnpackFloat(lpMessage, &lfPosition, lPosMin.z, lPosMax.z, liPosZBits) | lxResult;
        lpMatrix->wAxis.z = lfPosition;

        return lxResult;
    }
}
