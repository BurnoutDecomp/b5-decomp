#include "types.hpp"
#include "GameSource/Network/Messages/BrnCrashingTrafficMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/types.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::CrashingTrafficMessage::GetPackedMessageSize @ 0x8257A7C8
//   BrnNetwork::CrashingTrafficMessage::PackOrUnpack         @ 0x8257A650
//   BrnNetwork::CrashingTrafficMessage::PrepareForSend       @ 0x8257E180
//   BrnNetwork::CrashingTrafficMessage::Retrieve             @ 0x8257E220
// (GetName @ 0x827DE088 is inline in the header.)
//
// A CgsNetwork::Message subclass carrying up to KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE
// crashing-traffic records (vehicle id + affine transform). See the header for the layout.

namespace BrnNetwork
{
    // File-scope constants (DWARF BrnCrashingTrafficMessage.cpp:28-40).
    const s32 KI_CRASHING_TRAFFIC_MATRIX_ROLL_BITS  = 10;
    const s32 KI_CRASHING_TRAFFIC_MATRIX_PITCH_BITS = 11;
    const s32 KI_CRASHING_TRAFFIC_MATRIX_YAW_BITS   = 10;
    const s32 KI_CRASHING_TRAFFIC_POSITIONX_BITS    = 20;
    const s32 KI_CRASHING_TRAFFIC_POSITIONY_BITS    = 20;
    const s32 KI_CRASHING_TRAFFIC_POSITIONZ_BITS    = 20;

    const u16 KU16_CRASHING_TRAFFIC_MIN_VEHICLE_INDEX    = 0;
    const u16 KU16_CRASHING_TRAFFIC_MAX_VEHICLE_INDEX    = 1024;   // li r6,0x400
    const u16 KU16_CRASHING_TRAFFIC_MIN_FRAMES_SINCE_SRT = 0;
    const u16 KU16_CRASHING_TRAFFIC_MAX_FRAMES_SINCE_SRT = 65535;  // 0xFFFF

    static const s32 KI_CRASHING_TRAFFIC_MESSAGE_TYPE = 16;        // li r4,0x10 in PrepareForSend

    static const rw::math::vpu::Vector3 KV_CRASHING_TRAFFIC_POS_MIN = { -20000, -2000, -20000, 0 };
    static const rw::math::vpu::Vector3 KV_CRASHING_TRAFFIC_POS_MAX = {  20000,  2000,  20000, 0 };

    s32 CrashingTrafficMessage::GetPackedMessageSize()
    {
        // The X360 build sizes for the worst case: every record slot gets an identity affine
        // (rows {1,0,0,0}/{0,1,0,0}/{0,0,1,0}/{0,0,0,0}) + zero vehicle id, with the count
        // pinned to the array capacity, before deferring to the base packer.
        miCrashingTrafficDataCount = KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE;   // stw 24, +0x20

        for (s32 liIndex = 0; liIndex < miCrashingTrafficDataCount; ++liIndex)
        {
            maCrashingTrafficData[liIndex].mMatrix.SetIdentity();   // rows written via 4 vector stores
            maCrashingTrafficData[liIndex].mu16VehicleID = 0;       // sth 0, rec+0x00
        }

        return CgsNetwork::Message::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult CrashingTrafficMessage::PackOrUnpack()
    {
        // The X360 body opens with a bl to a trivial `return 0` leaf that the linker
        // identical-code-folded onto BrnWorld::PVSDebugComponent::IsSimple's address (the
        // same ICF artifact seen in BrnCameraStatusMessage::PackOrUnpack). It is not a real
        // relationship: the observable result is 0, seeded into the OR accumulator.
        const u8 lbIsSimple = 0;

        const rw::math::vpu::Vector3 lPosMin = KV_CRASHING_TRAFFIC_POS_MIN;
        const rw::math::vpu::Vector3 lPosMax = KV_CRASHING_TRAFFIC_POS_MAX;

        // Record count [KI_MIN, KI_MAX], then the frame counter [0, 0xFFFF]. Each per-field
        // status is OR-accumulated (0 == KX_PACK_OR_UNPACK_SUCCESS == all succeeded).
        CgsNetwork::PackOrUnpackResult lxResult =
            CgsNetwork::PackOrUnpackInt(this, &miCrashingTrafficDataCount,
                                        KI_MIN_CRASHING_TRAFFIC_IN_MESSAGE,
                                        KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE) | lbIsSimple;
        lxResult =
            CgsNetwork::PackOrUnpackU16(this, &mu16FramesSinceRoundStart,
                                        KU16_CRASHING_TRAFFIC_MIN_FRAMES_SINCE_SRT,
                                        KU16_CRASHING_TRAFFIC_MAX_FRAMES_SINCE_SRT) | lxResult;

        for (s32 liIndex = 0; liIndex < miCrashingTrafficDataCount; ++liIndex)
        {
            CrashingTrafficData& lrRecord = maCrashingTrafficData[liIndex];

            // Clamp the translation row into the wire AABB (asm: vminfp against max then
            // vmaxfp against min, stored back in place = Max(Min(pos, max), min)).
            rw::math::vpu::Vector3& lrPos = lrRecord.mMatrix.wAxis;
            lrPos = rw::math::vpu::Max(rw::math::vpu::Min(lrPos, lPosMax), lPosMin);

            // Quantised affine (roll 10 / pitch 11 / yaw 10 bits; position 20 bits/axis).
            const CgsNetwork::PackOrUnpackResult lxMatrix =
                CgsNetwork::PackOrUnpackMatrix(this, &lrRecord.mMatrix,
                                               KI_CRASHING_TRAFFIC_MATRIX_ROLL_BITS,
                                               KI_CRASHING_TRAFFIC_MATRIX_PITCH_BITS,
                                               KI_CRASHING_TRAFFIC_MATRIX_YAW_BITS,
                                               KI_CRASHING_TRAFFIC_POSITIONX_BITS,
                                               KI_CRASHING_TRAFFIC_POSITIONY_BITS,
                                               KI_CRASHING_TRAFFIC_POSITIONZ_BITS,
                                               lPosMin, lPosMax) | lxResult;

            // Vehicle id in [0, 1024].
            lxResult =
                CgsNetwork::PackOrUnpackU16(this, &lrRecord.mu16VehicleID,
                                            KU16_CRASHING_TRAFFIC_MIN_VEHICLE_INDEX,
                                            KU16_CRASHING_TRAFFIC_MAX_VEHICLE_INDEX) | lxMatrix;
        }

        return lxResult;
    }

    void CrashingTrafficMessage::PrepareForSend(u16 lu16Frame, u16 lu16FramesSinceRoundStart,
                                                s32 liCount, CrashingTrafficData* lpData)
    {
        // Only stamp an unclaimed slot (the send pump re-queues by clearing VALID first).
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            return;
        }

        // Whole fixed-capacity record array copied in (X360 XMemCpy of 24 * 80 = 1920 bytes).
        memcpy(maCrashingTrafficData, lpData, sizeof(maCrashingTrafficData));   // +0x30, 1920
        miCrashingTrafficDataCount = liCount;                                   // stw +0x20
        mu16FramesSinceRoundStart  = lu16FramesSinceRoundStart;                 // sth +0x24

        // Stamp the message type (16) + send frame + mark valid through the base.
        CgsNetwork::Message::PrepareForSend(KI_CRASHING_TRAFFIC_MESSAGE_TYPE, lu16Frame);

        // This is an unreliable message: the X360 build asserts it never went reliable.
        CGS_ASSERT(!IsReliable(), "!IsReliable()");
    }

    bool CrashingTrafficMessage::Retrieve(s32* lpiCount, CrashingTrafficData* lpData)
    {
        // Nothing to hand back unless a received message is pending in this slot.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            return false;
        }

        *lpiCount = miCrashingTrafficDataCount;   // *a2 = +0x20

        // Copy out exactly the received records (count * 80 bytes; the X360 computes
        // count*80 as (count + count*4) << 4).
        memcpy(lpData, maCrashingTrafficData,
               static_cast<u32>(miCrashingTrafficDataCount) * sizeof(CrashingTrafficData));

        // Consume the slot: clear VALID, then assert it is now invalid.
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        CGS_ASSERT(!IsMessageValid(), "!CgsNetwork::Message::IsMessageValid()");

        return true;
    }
} // namespace BrnNetwork
