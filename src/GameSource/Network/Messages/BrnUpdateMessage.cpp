#include "GameSource/Network/Messages/BrnUpdateMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (UpdateMessage validity guards)

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

namespace BrnNetwork
{
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

    // BrnNetwork::UpdateMessage::GetUpdateData  @ 0x82580BA0
    //   (BrnNetwork::BrnNetworkPlayer::CheckForNewMessages, ::Update)
    // Asserts the message is valid (mx8Flags VALID bit @ +0x19), then returns &mUpdateData
    // (the embedded payload at +0x20, i.e. `this + 0x20`).
    const UpdateData* UpdateMessage::GetUpdateData()
    {
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0, "IsMessageValid()");
        return &mUpdateData;
    }

    // BrnNetwork::UpdateMessage::GetU16FramesSinceStart  @ 0x82580C00
    //   (BrnNetwork::BrnNetworkPlayer::CheckForNewMessages)
    // Asserts valid, then returns the payload's mu16FramesSinceStart (lhz +0x22 == mUpdateData+2).
    u16 UpdateMessage::GetU16FramesSinceStart()
    {
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0, "IsMessageValid()");
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
        muBitstreamCursor0 = lOther.muBitstreamCursor0;   // +0x08 (lwz/stw)
        muBitstreamCursor1 = lOther.muBitstreamCursor1;   // +0x0C (lwz/stw)
        muBitstreamCursor2 = lOther.muBitstreamCursor2;   // +0x10 (lwz/stw)
        muBitstreamCursor3 = lOther.muBitstreamCursor3;   // +0x14 (lwz/stw)
        mu8GameID          = lOther.mu8GameID;            // +0x18 (lbz/stb)
        mx8Flags           = lOther.mx8Flags;             // +0x19 (lbz/stb)
        mi8Type            = lOther.mi8Type;              // +0x1A (lbz/stb)
        mu16Frame          = lOther.mu16Frame;            // +0x1C (lhz/sth)

        mUpdateData        = lOther.mUpdateData;          // +0x20 (UpdateData::operator=)
        return *this;
    }
} // namespace BrnNetwork
