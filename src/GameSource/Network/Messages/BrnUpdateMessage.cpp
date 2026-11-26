#include "GameSource/Network/Messages/BrnUpdateMessage.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT (UpdateMessage validity guards)
#include "rw/math/vpu/vector3_operation.h"          // Magnitude / Normalize / Min / Max

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::UpdateData::operator=   @ 0x82579CD0
//   called by: BrnNetwork::UpdateMessage::PrepareForSend, BrnNetwork::UpdateMessage::operator=
//
// Memberwise copy assignment of a per-frame car update payload. The X360 emits the copy
// store-for-store in declared-member order: the two u16 frame counters (lhz/sth), then the
// 64-byte Matrix44Affine + the two 16-byte Vector3 velocities as six vector loads/stores
// (lvx128/stvx128 over base+0x10 with strides 0/0x10/0x20/0x30, then +0x50, +0x60), then the
// three control floats (lfs/stfs), the two status words (lwz/stw), and the eight trailing
// bool flags (lbz/stb). Reproduced here as plain named-member assignments; the aggregate
// vector/matrix members copy as 16-byte units exactly as the SIMD load/store pairs do.

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Messages/BrnUpdateMessage.h".

namespace BrnNetwork
{
    namespace
    {
        // The update message type id.
        const s32 KI_UPDATE_MESSAGE_TYPE = 11;

        // A velocity at or above these speeds is pulled back to just inside the packing
        // range (135 linear, 63 angular), scaled by 0.99.
        const f32 KF_UPDATE_LINEAR_VELOCITY_CLAMP_START  = 133.649994f;
        const f32 KF_UPDATE_ANGULAR_VELOCITY_CLAMP_START = 62.3699989f;
        const f32 KF_UPDATE_MAX_LINEAR_VELOCITY          = 135.0f;
        const f32 KF_UPDATE_MAX_ANGULAR_VELOCITY         = 63.0f;
        const f32 KF_UPDATE_VELOCITY_CLAMP_SCALE         = 0.99000001f;

        // The packed translation row's bounds (x, y, z; the w lane clamps to 0).
        const f32 KF_UPDATE_POSITION_MAX_XZ = 20000.0f;
        const f32 KF_UPDATE_POSITION_MAX_Y  = 2000.0f;

        // Rotation as roll / pitch / yaw, then 20 bits per translation axis.
        const s32 KI_UPDATE_ROLL_BITS     = 10;
        const s32 KI_UPDATE_PITCH_BITS    = 11;
        const s32 KI_UPDATE_YAW_BITS      = 10;
        const s32 KI_UPDATE_POSITION_BITS = 20;

        // Velocities: 8 bits per angle and for the magnitude, bounded by the two maximum speeds.
        const s32 KI_UPDATE_VELOCITY_BITS = 8;

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

    // BrnNetwork::UpdateMessage::Construct (identical code to AggressiveDrivingMessage's):
    // only the message base fields are reset.
    void UpdateMessage::Construct()
    {
        CgsNetwork::Message::Construct();
    }

    // Take a new update unless the previous one has not been sent yet: copy the payload,
    // pull either velocity that would not pack back inside its range, clamp the position
    // to the packed bounds, then stamp the message for the payload's send frame.
    void UpdateMessage::PrepareForSend(UpdateData* lpUpdateData)
    {
        if (IsMessageValid())
        {
            return;
        }

        mUpdateData = *lpUpdateData;

        if (rw::math::vpu::Magnitude(mUpdateData.mLinearVelocity) >= KF_UPDATE_LINEAR_VELOCITY_CLAMP_START)
        {
            const Vector3 lUnit = rw::math::vpu::Normalize(mUpdateData.mLinearVelocity);
            {
                const Vector3 lAggregateValue =
                    (lUnit * KF_UPDATE_MAX_LINEAR_VELOCITY) * KF_UPDATE_VELOCITY_CLAMP_SCALE;
                mUpdateData.mLinearVelocity = lAggregateValue;
            }
        }

        if (rw::math::vpu::Magnitude(mUpdateData.mAngularVelocity) >= KF_UPDATE_ANGULAR_VELOCITY_CLAMP_START)
        {
            const Vector3 lUnit = rw::math::vpu::Normalize(mUpdateData.mAngularVelocity);
            {
                const Vector3 lAggregateValue =
                    (lUnit * KF_UPDATE_MAX_ANGULAR_VELOCITY) * KF_UPDATE_VELOCITY_CLAMP_SCALE;
                mUpdateData.mAngularVelocity = lAggregateValue;
            }
        }

        const Vector3 lPositionMax(KF_UPDATE_POSITION_MAX_XZ, KF_UPDATE_POSITION_MAX_Y,
                                   KF_UPDATE_POSITION_MAX_XZ);
        const Vector3 lPositionMin(-KF_UPDATE_POSITION_MAX_XZ, -KF_UPDATE_POSITION_MAX_Y,
                                   -KF_UPDATE_POSITION_MAX_XZ);
        mUpdateData.mMatrix.wAxis = rw::math::vpu::Max(rw::math::vpu::Min(mUpdateData.mMatrix.wAxis, lPositionMax),
                                                       lPositionMin);

        CgsNetwork::Message::PrepareForSend(KI_UPDATE_MESSAGE_TYPE, mUpdateData.mu16SentFrame);
        CGS_ASSERT_AT(!IsReliable(),
            "!IsReliable()",
            __FILE__, 151 /* ARTIST 0x8257D96C */);
    }

    UpdateData& UpdateData::operator=(const UpdateData& lOther)
    {
        mu16SentFrame              = lOther.mu16SentFrame;              // +0x00 (lhz/sth 0)
        mu16FramesSinceStart       = lOther.mu16FramesSinceStart;       // +0x02 (lhz/sth 2)

        mMatrix                    = lOther.mMatrix;                    // +0x10 (4x lvx/stvx, 64B)
        mLinearVelocity            = lOther.mLinearVelocity;           // +0x50 (lvx/stvx)
        mAngularVelocity           = lOther.mAngularVelocity;          // +0x60 (lvx/stvx)

        mfSteering                 = lOther.mfSteering;                // +0x70 (lfs/stfs)
        mfAcceleration             = lOther.mfAcceleration;            // +0x74 (lfs/stfs)
        mfBraking                  = lOther.mfBraking;                 // +0x78 (lfs/stfs)

        meCameraStatus             = lOther.meCameraStatus;            // +0x7C (lwz/stw)
        meHeadsetStatus            = lOther.meHeadsetStatus;           // +0x80 (lwz/stw)

        mbIsBoosting               = lOther.mbIsBoosting;              // +0x84 (lbz/stb)
        mbIsCrashing               = lOther.mbIsCrashing;              // +0x85 (lbz/stb)
        mbIsFreeBurnLobby          = lOther.mbIsFreeBurnLobby;         // +0x86 (lbz/stb)
        mbIsRoundNumberOdd         = lOther.mbIsRoundNumberOdd;        // +0x87 (lbz/stb)
        mbIsEliminated             = lOther.mbIsEliminated;           // +0x88 (lbz/stb)
        mbSnap                     = lOther.mbSnap;                    // +0x89 (lbz/stb)
        mbIsInCarSelect            = lOther.mbIsInCarSelect;          // +0x8A (lbz/stb)
        mbReceivingPlayerCrashedUs = lOther.mbReceivingPlayerCrashedUs; // +0x8B (lbz/stb)

        return *this;
    }

    // The base status first, then the matrix, both velocities, the three controls, the two
    // frame counters, the two statuses (through int copies written back at the end) and
    // the eight flags, in the console's order; every field status is OR-accumulated.
    CgsNetwork::PackOrUnpackResult UpdateMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::Message::PackOrUnpack();

        s32 liCameraStatus  = static_cast<s32>(mUpdateData.meCameraStatus);
        s32 liHeadsetStatus = mUpdateData.meHeadsetStatus;

        const rw::math::vpu::Vector3 lPositionMin(-KF_UPDATE_POSITION_MAX_XZ,
                                                   -KF_UPDATE_POSITION_MAX_Y,
                                                   -KF_UPDATE_POSITION_MAX_XZ);
        const rw::math::vpu::Vector3 lPositionMax(KF_UPDATE_POSITION_MAX_XZ,
                                                   KF_UPDATE_POSITION_MAX_Y,
                                                   KF_UPDATE_POSITION_MAX_XZ);

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

        lxResult = CgsNetwork::Message::PackOrUnpack(&mUpdateData.mu16SentFrame, 0, KI_UPDATE_MAX_FRAME) | lxResult;
        lxResult = CgsNetwork::Message::PackOrUnpack(&mUpdateData.mu16FramesSinceStart, 0, KI_UPDATE_MAX_FRAME) | lxResult;

        lxResult = CgsNetwork::Message::PackOrUnpack(&liCameraStatus, 0, KI_UPDATE_MAX_CAMERA_STATUS) | lxResult;
        lxResult = CgsNetwork::Message::PackOrUnpack(&liHeadsetStatus, 0, KI_UPDATE_MAX_HEADSET_STATUS) | lxResult;

        lxResult = CgsNetwork::Message::PackOrUnpack(&mUpdateData.mbIsBoosting) | lxResult;
        lxResult = CgsNetwork::Message::PackOrUnpack(&mUpdateData.mbIsCrashing) | lxResult;
        lxResult = CgsNetwork::Message::PackOrUnpack(&mUpdateData.mbSnap) | lxResult;
        lxResult = CgsNetwork::Message::PackOrUnpack(&mUpdateData.mbIsRoundNumberOdd) | lxResult;
        lxResult = CgsNetwork::Message::PackOrUnpack(&mUpdateData.mbIsFreeBurnLobby) | lxResult;
        lxResult = CgsNetwork::Message::PackOrUnpack(&mUpdateData.mbIsEliminated) | lxResult;
        lxResult = CgsNetwork::Message::PackOrUnpack(&mUpdateData.mbIsInCarSelect) | lxResult;
        lxResult = CgsNetwork::Message::PackOrUnpack(&mUpdateData.mbReceivingPlayerCrashedUs) | lxResult;

        mUpdateData.meCameraStatus  = static_cast<ECameraStatus>(liCameraStatus);
        mUpdateData.meHeadsetStatus = liHeadsetStatus;
        return lxResult;
    }

    // BrnNetwork::UpdateMessage::GetUpdateData  @ 0x82580BA0
    //   (BrnNetwork::BrnNetworkPlayer::CheckForNewMessages, ::Update)
    // Asserts the message is valid (mx8Flags VALID bit @ +0x19), then returns &mUpdateData
    // (the embedded payload at +0x20, i.e. `this + 0x20`).
    const UpdateData* UpdateMessage::GetUpdateData()
    {
        CGS_ASSERT_AT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0,
            "IsMessageValid()", __FILE__, 131 /* ARTIST 0x82580BCC */);
        return &mUpdateData;
    }

    // BrnNetwork::UpdateMessage::GetU16FramesSinceStart  @ 0x82580C00
    //   (BrnNetwork::BrnNetworkPlayer::CheckForNewMessages)
    // Asserts valid, then returns the payload's mu16FramesSinceStart (lhz +0x22 == mUpdateData+2).
    u16 UpdateMessage::GetU16FramesSinceStart()
    {
        CGS_ASSERT_AT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0,
            "IsMessageValid()", __FILE__, 138 /* ARTIST 0x82580C2C */);
        return mUpdateData.mu16FramesSinceStart;
    }

    // BrnNetwork::UpdateMessage::operator=  @ 0x82588CE8
    //   (BrnNetwork::BrnNetworkPlayer::RemoveBufferedMessage)
    // Memberwise copy of the CgsNetwork::Message base scalar fields (NOT the vptr at +0x00),
    // matching the asm store widths exactly: the lifecycle word + the four bitstream cursor
    // words (lwz/stw +0x04..+0x14), the three status bytes (lbz/stb +0x18/+0x19/+0x1A), the
    // frame halfword (lhz/sth +0x1C), then the embedded UpdateData payload via its own
    // operator= (UpdateData::operator=(this+0x20, lOther+0x20)).
    UpdateMessage& UpdateMessage::operator=(const UpdateMessage& lOther)
    {
        mePackOrUnpack     = lOther.mePackOrUnpack;       // +0x04 (lwz/stw)
        mBitstream         = lOther.mBitstream;           // +0x08..+0x14 (four word copies)
        mu8GameID          = lOther.mu8GameID;            // +0x18 (lbz/stb)
        mx8Flags           = lOther.mx8Flags;             // +0x19 (lbz/stb)
        mi8Type            = lOther.mi8Type;              // +0x1A (lbz/stb)
        mu16Frame          = lOther.mu16Frame;            // +0x1C (lhz/sth)

        mUpdateData        = lOther.mUpdateData;          // +0x20 (UpdateData::operator=)
        return *this;
    }

    // Size a representative message: identity rotation with a zero translation row, zero
    // velocities, controls, statuses and flags, and a zero frames-since-start (the sent
    // frame is left as is), then pack through the base.
    s32 UpdateMessage::GetPackedMessageSize()
    {
        mUpdateData.mMatrix.SetIdentity();
        mUpdateData.mu16FramesSinceStart = 0;
        mUpdateData.mfSteering           = 0.0f;
        mUpdateData.mfAcceleration       = 0.0f;
        mUpdateData.mfBraking            = 0.0f;
        mUpdateData.mLinearVelocity.SetZero();
        mUpdateData.mAngularVelocity.SetZero();
        mUpdateData.mbIsBoosting                = false;
        mUpdateData.mbIsCrashing                = false;
        mUpdateData.mbSnap                      = false;
        mUpdateData.mbIsEliminated              = false;
        mUpdateData.mbIsFreeBurnLobby           = false;
        mUpdateData.mbIsRoundNumberOdd          = false;
        mUpdateData.mbIsInCarSelect             = false;
        mUpdateData.mbReceivingPlayerCrashedUs  = false;
        mUpdateData.meCameraStatus              = E_CAMERA_STATUS_NONE;
        mUpdateData.meHeadsetStatus             = 0;
        return CgsNetwork::Message::GetPackedMessageSize();
    }
} // namespace BrnNetwork
