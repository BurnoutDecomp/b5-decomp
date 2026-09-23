#include "GameSource/Network/Messages/BrnUpdateMessage.h"

// BrnNetwork::UpdateMessage::PackOrUnpack. Kept apart from the mounted BrnUpdateMessage.cpp
// because it calls the matrix and vector field primitives, which have no bodies yet: mount
// this file together with CgsNetwork::PackOrUnpackMatrix / PackOrUnpackVector.

namespace BrnNetwork
{
    namespace
    {
        // Position bounds of the packed matrix translation row (x, y, z; the w lane is 0).
        const f32 KF_UPDATE_POSITION_MAX_XZ = 20000.0f;
        const f32 KF_UPDATE_POSITION_MAX_Y  = 2000.0f;

        // Rotation as roll / pitch / yaw, then 20 bits per translation axis.
        const s32 KI_UPDATE_ROLL_BITS     = 10;
        const s32 KI_UPDATE_PITCH_BITS    = 11;
        const s32 KI_UPDATE_YAW_BITS      = 10;
        const s32 KI_UPDATE_POSITION_BITS = 20;

        // Velocities: 8 bits per angle and for the magnitude, bounded by these speeds.
        const s32 KI_UPDATE_VELOCITY_BITS          = 8;
        const f32 KF_UPDATE_MAX_LINEAR_VELOCITY    = 135.0f;
        const f32 KF_UPDATE_MAX_ANGULAR_VELOCITY   = 63.0f;

        // Driver controls: steering in [-1, 1] over 5 bits, acceleration over 4 and
        // braking over 3, both in [0, 1].
        const f32 KF_UPDATE_STEERING_MIN     = -1.0f;
        const f32 KF_UPDATE_CONTROL_MIN      = 0.0f;
        const f32 KF_UPDATE_CONTROL_MAX      = 1.0f;
        const s32 KI_UPDATE_STEERING_BITS    = 5;
        const s32 KI_UPDATE_ACCELERATION_BITS = 4;
        const s32 KI_UPDATE_BRAKING_BITS     = 3;

        // Frame counters travel in [0, 65534]; the camera status in [0, 4] and the headset
        // status in [0, 3].
        const s32 KI_UPDATE_MAX_FRAME          = 0xFFFE;
        const s32 KI_UPDATE_MAX_CAMERA_STATUS  = 4;
        const s32 KI_UPDATE_MAX_HEADSET_STATUS = 3;
    }

    // The base status first, then the matrix, both velocities, the three controls, the two
    // frame counters, the two statuses (through int copies written back at the end) and
    // the eight flags, in the console's order; every field status is OR-accumulated.
    CgsNetwork::PackOrUnpackResult UpdateMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::Message::PackOrUnpack();

        s32 liCameraStatus  = static_cast<s32>(mUpdateData.meCameraStatus);
        s32 liHeadsetStatus = mUpdateData.meHeadsetStatus;

        rw::math::vpu::Vector3 lPositionMin;
        lPositionMin.x = -KF_UPDATE_POSITION_MAX_XZ;
        lPositionMin.y = -KF_UPDATE_POSITION_MAX_Y;
        lPositionMin.z = -KF_UPDATE_POSITION_MAX_XZ;
        lPositionMin.w = 0.0f;

        rw::math::vpu::Vector3 lPositionMax;
        lPositionMax.x = KF_UPDATE_POSITION_MAX_XZ;
        lPositionMax.y = KF_UPDATE_POSITION_MAX_Y;
        lPositionMax.z = KF_UPDATE_POSITION_MAX_XZ;
        lPositionMax.w = 0.0f;

        lxResult = CgsNetwork::PackOrUnpackMatrix(this, &mUpdateData.mMatrix,
                                                  KI_UPDATE_ROLL_BITS, KI_UPDATE_PITCH_BITS, KI_UPDATE_YAW_BITS,
                                                  lPositionMin, lPositionMax,
                                                  KI_UPDATE_POSITION_BITS, KI_UPDATE_POSITION_BITS,
                                                  KI_UPDATE_POSITION_BITS) | lxResult;

        lxResult = CgsNetwork::PackOrUnpackVector(this, &mUpdateData.mLinearVelocity,
                                                  KI_UPDATE_VELOCITY_BITS, KI_UPDATE_VELOCITY_BITS,
                                                  KF_UPDATE_MAX_LINEAR_VELOCITY,
                                                  KI_UPDATE_VELOCITY_BITS) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackVector(this, &mUpdateData.mAngularVelocity,
                                                  KI_UPDATE_VELOCITY_BITS, KI_UPDATE_VELOCITY_BITS,
                                                  KF_UPDATE_MAX_ANGULAR_VELOCITY,
                                                  KI_UPDATE_VELOCITY_BITS) | lxResult;

        lxResult = CgsNetwork::PackOrUnpackFloat(this, &mUpdateData.mfSteering,
                                                 KF_UPDATE_STEERING_MIN, KF_UPDATE_CONTROL_MAX,
                                                 KI_UPDATE_STEERING_BITS) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackFloat(this, &mUpdateData.mfAcceleration,
                                                 KF_UPDATE_CONTROL_MIN, KF_UPDATE_CONTROL_MAX,
                                                 KI_UPDATE_ACCELERATION_BITS) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackFloat(this, &mUpdateData.mfBraking,
                                                 KF_UPDATE_CONTROL_MIN, KF_UPDATE_CONTROL_MAX,
                                                 KI_UPDATE_BRAKING_BITS) | lxResult;

        lxResult = CgsNetwork::PackOrUnpackU16(this, &mUpdateData.mu16SentFrame, 0, KI_UPDATE_MAX_FRAME) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackU16(this, &mUpdateData.mu16FramesSinceStart, 0, KI_UPDATE_MAX_FRAME) | lxResult;

        lxResult = CgsNetwork::PackOrUnpackInt(this, &liCameraStatus, 0, KI_UPDATE_MAX_CAMERA_STATUS) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackInt(this, &liHeadsetStatus, 0, KI_UPDATE_MAX_HEADSET_STATUS) | lxResult;

        lxResult = CgsNetwork::PackOrUnpackBool(this, &mUpdateData.mbIsBoosting) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackBool(this, &mUpdateData.mbIsCrashing) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackBool(this, &mUpdateData.mbSnap) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackBool(this, &mUpdateData.mbIsRoundNumberOdd) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackBool(this, &mUpdateData.mbIsFreeBurnLobby) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackBool(this, &mUpdateData.mbIsEliminated) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackBool(this, &mUpdateData.mbIsInCarSelect) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackBool(this, &mUpdateData.mbReceivingPlayerCrashedUs) | lxResult;

        mUpdateData.meCameraStatus  = static_cast<ECameraStatus>(liCameraStatus);
        mUpdateData.meHeadsetStatus = liHeadsetStatus;
        return lxResult;
    }
}
