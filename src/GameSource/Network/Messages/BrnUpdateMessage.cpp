#include "GameSource/Network/Messages/BrnUpdateMessage.h"

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
} // namespace BrnNetwork
