#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"   // Magnitude / operator- (the round-trip checks)

// CgsNetwork::PackOrUnpackMatrix and PackOrUnpackVector: the affine and bounded-vector field
// primitives. They read the rotation / direction through Message::GetEulerAnglesFromMatrix
// and Message::GetAnglesFromVector (bodied in CgsMessage.cpp).

namespace CgsNetwork
{
    namespace
    {
        const f32 KF_PI      = 3.14159265f;
        const f32 KF_HALF_PI = 1.57079637f;

        // How far a vector may drift through its angle encoding before it is reported.
        const f32 KF_MAX_VECTOR_MAGNITUDE_ERROR = 0.1f;
        const f32 KF_MAX_VECTOR_ERROR           = 0.05f;
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

    // A vector travels as its two direction angles (angle A in [-pi, pi], angle B in
    // [-pi/2, pi/2]) and its magnitude in [0, lfMagnitudeBound], over the three bit counts
    // (a pack stops at the first value that does not fit). Both directions check the
    // encoding: after packing, the vector rebuilt from the packed angles must be close to
    // the original; after unpacking, the rebuilt vector must survive a second trip.
    PackOrUnpackResult PackOrUnpackVector(Message* lpMessage, rw::math::vpu::Vector3* lpvField,
                                          s32 liXBits, s32 liYBits, f32 lfMagnitudeBound, s32 liZBits)
    {
        f32 lfAngleA    = 0.0f;
        f32 lfAngleB    = 0.0f;
        f32 lfMagnitude = 0.0f;

        if (lpMessage->mePackOrUnpack == Message::E_PACK_INTO_BITSTREAM)
        {
            Message::GetAnglesFromVector(*lpvField, &lfAngleA, &lfAngleB, &lfMagnitude);

            u32 luPacked = 0;
            FloatQuantiser::Pack(lfAngleA, -KF_PI, KF_PI, liXBits, &luPacked);
            if (!lpMessage->mBitstream.AddBits(luPacked, liXBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            FloatQuantiser::Pack(lfAngleB, -KF_HALF_PI, KF_HALF_PI, liYBits, &luPacked);
            if (!lpMessage->mBitstream.AddBits(luPacked, liYBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            FloatQuantiser::Pack(lfMagnitude, 0.0f, lfMagnitudeBound, liZBits, &luPacked);
            if (!lpMessage->mBitstream.AddBits(luPacked, liZBits))
            {
                return KX_PACK_FAILED_NO_SPACE;
            }

            rw::math::vpu::Vector3 lRebuiltVector;
            Message::GetVectorFromAngles(&lRebuiltVector, lfAngleA, lfAngleB, lfMagnitude);
            const rw::math::vpu::Vector3 lDifferenceVector = lRebuiltVector - *lpvField;
            CGS_ASSERT(rw::math::vpu::Magnitude(lDifferenceVector) < KF_MAX_VECTOR_ERROR,
                       "RwMath::Magnitude( lDifferenceVector ) < 0.05f");
        }
        else if (lpMessage->mePackOrUnpack == Message::E_UNPACK_FROM_BITSTREAM)
        {
            lpMessage->mBitstream.GetQuantisedFloat(&lfAngleA, -KF_PI, KF_PI, liXBits);
            lpMessage->mBitstream.GetQuantisedFloat(&lfAngleB, -KF_HALF_PI, KF_HALF_PI, liYBits);
            lpMessage->mBitstream.GetQuantisedFloat(&lfMagnitude, 0.0f, lfMagnitudeBound, liZBits);
            Message::GetVectorFromAngles(lpvField, lfAngleA, lfAngleB, lfMagnitude);

            f32 lfTestMagnitude = 0.0f;
            Message::GetAnglesFromVector(*lpvField, &lfAngleA, &lfAngleB, &lfTestMagnitude);
            CGS_ASSERT(fabsf(lfTestMagnitude - lfMagnitude) < KF_MAX_VECTOR_MAGNITUDE_ERROR,
                       "RwMath::Abs( lfTestMagnitude - lfMagnitude ) < 0.1f");

            rw::math::vpu::Vector3 lRebuiltVector;
            Message::GetVectorFromAngles(&lRebuiltVector, lfAngleA, lfAngleB, lfTestMagnitude);
            const rw::math::vpu::Vector3 lDifferenceVector = lRebuiltVector - *lpvField;
            CGS_ASSERT(rw::math::vpu::Magnitude(lDifferenceVector) < KF_MAX_VECTOR_ERROR,
                       "RwMath::Magnitude( lDifferenceVector ) < 0.05f");
        }
        else
        {
            CGS_ASSERT(false,
                       "CgsNetwork::Message::PackOrUnpack called without telling "
                       "it which it is doing\n");
        }

        return KX_PACK_OR_UNPACK_SUCCESS;
    }
}
