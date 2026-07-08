#include "types.hpp"

#include "GameSource/Network/Messages/BrnAggressiveDrivingMessage.h"
#include "GameSource/Network/BrnNetworkManager.h"          // BrnNetworkManager::PackOrUnpack (NetworkPlayerID field primitive)
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h" // CgsNetwork::Message + field primitives
#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT

#include <cstring>                                          // memcpy (the X360 XMemCpy payload copy)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::AggressiveDrivingMessage::Construct            @ 0x82579CC8
//   BrnNetwork::AggressiveDrivingMessage::PackOrUnpack         @ 0x8257A8C0
//   BrnNetwork::AggressiveDrivingMessage::GetPackedMessageSize @ 0x8257AB20
//   BrnNetwork::AggressiveDrivingMessage::PrepareForSend       @ 0x8257E2D8
//   BrnNetwork::AggressiveDrivingMessage::Retrieve             @ 0x8257E430
// (GetName @ 0x827DE0A8 is inline in the header.)
//
// A CgsNetwork::Message subclass carrying up to KI_MAX_AGGRESSIVE_MOVES aggressive-move
// records (takedown/slam/shunt/etc.). Each record is 80 bytes (the mDirection Vector3 is
// 16-byte aligned), starting at +0x30 right after the miNumberOfAggressiveMoves count at
// +0x20. See the header for the record + message layout.

namespace BrnNetwork
{
    // Wire type id stamped onto the send (CgsNetwork::Message::PrepareForSend leType arg;
    // li r10, 0x11 -> mi8Type in the X360 PrepareForSend body).
    static const s32 KI_AGGRESSIVE_DRIVING_MESSAGE_TYPE = 17;

    // Record-count / per-field wire ranges (all inlined immediates in the X360 PackOrUnpack).
    static const s32 KI_MIN_AGGRESSIVE_MOVES      = 0;    // count range [0, KI_MAX_AGGRESSIVE_MOVES]
    static const s32 KI_MIN_AGGRESSIVE_MOVE_TYPE  = 0;    // move-type range [0, KI_MAX_AGGRESSIVE_MOVE_TYPE]

    // The direction vector quantises to 8 bits/axis with a magnitude bound of 1.01 (it is a
    // near-unit impact direction). Both the per-axis widths and the magnitude are the
    // sub_8288E390 immediates (r5/r6/r8 == 8, f1 == flt_8201FDA0 == 1.01f).
    static const s32 KI_AGGRESSIVE_DIRECTION_BITS       = 8;
    static const f32 KF_AGGRESSIVE_DIRECTION_MAGNITUDE  = 1.01f;

    // Per-record float field wire ranges (min, max, resolution) -- the sub_8288DF50 immediates.
    static const f32 KF_MAGNITUDE_MIN = 0.0f,          KF_MAGNITUDE_MAX = 400000.0f, KF_MAGNITUDE_RES = 10.0f;
    static const f32 KF_DURATION_MIN  = 0.0f,          KF_DURATION_MAX  = 15.0f,     KF_DURATION_RES  = 0.05f;
    static const f32 KF_STEERING_MIN  = -1.0f,         KF_STEERING_MAX  = 1.0f,      KF_STEERING_RES  = 0.1f;
    static const f32 KF_RECOVERY_MIN  = 0.0f,          KF_RECOVERY_MAX  = 10.0f,     KF_RECOVERY_RES  = 0.1f;

    // muImpactScore quantises over the full byte range.
    static const s32 KI_MIN_IMPACT_SCORE = 0;
    static const s32 KI_MAX_IMPACT_SCORE = 255;

    void AggressiveDrivingMessage::Construct()
    {
        // Pure tail-call to the base initialiser (the X360 body is a single `b` -- no member
        // seeding of its own).
        CgsNetwork::Message::Construct();
    }

    s32 AggressiveDrivingMessage::GetPackedMessageSize()
    {
        // The X360 build sizes for the worst case: it pins the count to the array capacity and
        // clears each record's leading fields (move type -> TAKE_DOWN, takedown -> NONE(-1),
        // both player ids -> 0, both flags -> false), then defers to the base packer. Only the
        // six leading fields are written (the asm stores exactly rec+0x00/+0x04/+0x08/+0x0C and
        // the two bytes +0x10/+0x11); the remaining fields are not touched here.
        miNumberOfAggressiveMoves = KI_MAX_AGGRESSIVE_MOVES;

        for (s32 liIndex = 0; liIndex < miNumberOfAggressiveMoves; ++liIndex)
        {
            AggressiveMoveData& lrRecord = maAggressiveMoveDataArray[liIndex];
            lrRecord.meAggressiveMoveType      = E_AGGRESSIVE_MOVE_TAKE_DOWN;   // 0
            lrRecord.meTakedownType            = BrnGameState::E_TAKEDOWN_NONE; // -1
            lrRecord.mAggressorNetworkPlayerID = 0;
            lrRecord.mVictimNetworkPlayerID    = 0;
            lrRecord.mbMarkedMan               = false;
            lrRecord.mbSettledScore            = false;
        }

        return CgsNetwork::Message::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult AggressiveDrivingMessage::PackOrUnpack()
    {
        // The X360 body opens with a bl to a trivial `return 0` leaf the linker identical-code-
        // folded onto BrnWorld::PVSDebugComponent::IsSimple's address (the same ICF artifact
        // seen in BrnCrashingTrafficMessage::PackOrUnpack). It is not a real relationship: the
        // observable result is 0, seeded into the OR accumulator.
        const u8 lbIsSimple = 0;

        // Record count [0, KI_MAX_AGGRESSIVE_MOVES]; each per-field status is OR-accumulated
        // (0 == KX_PACK_OR_UNPACK_SUCCESS == all succeeded).
        CgsNetwork::PackOrUnpackResult lxResult =
            CgsNetwork::PackOrUnpackInt(this, &miNumberOfAggressiveMoves,
                                        KI_MIN_AGGRESSIVE_MOVES, KI_MAX_AGGRESSIVE_MOVES) | lbIsSimple;

        for (s32 liIndex = 0; liIndex < miNumberOfAggressiveMoves; ++liIndex)
        {
            AggressiveMoveData& lrRecord = maAggressiveMoveDataArray[liIndex];

            // The two enum fields go through the int primitive via a temporary (the X360 copies
            // the enum to a stack slot, packs, and writes it back).
            s32 liMoveType = lrRecord.meAggressiveMoveType;
            const CgsNetwork::PackOrUnpackResult lxMoveType =
                CgsNetwork::PackOrUnpackInt(this, &liMoveType,
                                            KI_MIN_AGGRESSIVE_MOVE_TYPE, KI_MAX_AGGRESSIVE_MOVE_TYPE);
            lrRecord.meAggressiveMoveType = static_cast<EAggressiveMoveType>(liMoveType);

            s32 liTakedownType = lrRecord.meTakedownType;
            lxResult =
                CgsNetwork::PackOrUnpackInt(this, &liTakedownType,
                                            BrnGameState::E_TAKEDOWN_NONE,
                                            BrnGameState::E_TAKEDOWN_INTO_BUS) | lxMoveType | lxResult;
            lrRecord.meTakedownType = static_cast<BrnGameState::ETakedownType>(liTakedownType);

            // Aggressor / victim player ids through the NetworkPlayerID primitive.
            lxResult = BrnNetworkManager::PackOrUnpack(this, &lrRecord.mAggressorNetworkPlayerID) | lxResult;
            lxResult = BrnNetworkManager::PackOrUnpack(this, &lrRecord.mVictimNetworkPlayerID)    | lxResult;

            // Two boolean flags.
            lxResult = CgsNetwork::PackOrUnpackBool(this, &lrRecord.mbMarkedMan)    | lxResult;
            lxResult = CgsNetwork::PackOrUnpackBool(this, &lrRecord.mbSettledScore) | lxResult;

            // Impact direction (8 bits/axis, magnitude bound 1.01).
            lxResult = CgsNetwork::PackOrUnpackVector(this, &lrRecord.mDirection,
                                                      KI_AGGRESSIVE_DIRECTION_BITS,
                                                      KI_AGGRESSIVE_DIRECTION_BITS,
                                                      KF_AGGRESSIVE_DIRECTION_MAGNITUDE,
                                                      KI_AGGRESSIVE_DIRECTION_BITS) | lxResult;

            // Quantised scalar fields.
            lxResult = CgsNetwork::PackOrUnpackFloat(this, &lrRecord.mfMagnitude,
                                                     KF_MAGNITUDE_MIN, KF_MAGNITUDE_MAX, KF_MAGNITUDE_RES) | lxResult;
            lxResult = CgsNetwork::PackOrUnpackFloat(this, &lrRecord.mfDuration,
                                                     KF_DURATION_MIN, KF_DURATION_MAX, KF_DURATION_RES) | lxResult;
            lxResult = CgsNetwork::PackOrUnpackFloat(this, &lrRecord.mfSteeringDirection,
                                                     KF_STEERING_MIN, KF_STEERING_MAX, KF_STEERING_RES) | lxResult;
            lxResult = CgsNetwork::PackOrUnpackFloat(this, &lrRecord.mfRecoveryTime,
                                                     KF_RECOVERY_MIN, KF_RECOVERY_MAX, KF_RECOVERY_RES) | lxResult;

            // Impact score (full byte range).
            lxResult = CgsNetwork::PackOrUnpackU8(this, &lrRecord.muImpactScore,
                                                  KI_MIN_IMPACT_SCORE, KI_MAX_IMPACT_SCORE) | lxResult;
        }

        return lxResult;
    }

    void AggressiveDrivingMessage::PrepareForSend(u16 lu16Frame, s32 liNumberOfAggressiveMoves,
                                                  AggressiveMoveData* lpMoves)
    {
        // Only stamp an unclaimed slot (the send pump re-queues by clearing VALID first).
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            return;
        }

        CGS_ASSERT(liNumberOfAggressiveMoves >= 0, "liNumberOfAggressiveMoves >= 0");
        CGS_ASSERT(liNumberOfAggressiveMoves <= KI_MAX_AGGRESSIVE_MOVES,
                   "liNumberOfAggressiveMoves <= KI_MAX_AGGRESSIVE_MOVES");

        miNumberOfAggressiveMoves = liNumberOfAggressiveMoves;   // stw +0x20

        // Copy in exactly the supplied records (X360: the 10 x 8-byte inner move per record).
        for (s32 liIndex = 0; liIndex < miNumberOfAggressiveMoves; ++liIndex)
        {
            maAggressiveMoveDataArray[liIndex] = lpMoves[liIndex];
        }

        // Stamp the message type (17) + send frame + mark valid through the base (the base
        // PrepareForSend body -- inlined here in the X360 image -- fires the KU16_INVALID_FRAME
        // assert at CgsMessage.h:594).
        CgsNetwork::Message::PrepareForSend(KI_AGGRESSIVE_DRIVING_MESSAGE_TYPE, lu16Frame);

        // This is an unreliable message: the X360 build asserts it never went reliable.
        CGS_ASSERT(!IsReliable(), "!IsReliable()");
    }

    bool AggressiveDrivingMessage::Retrieve(s32* lpiNumberOfAggressiveMoves, AggressiveMoveData* lpMoves)
    {
        // Nothing to hand back unless a received message is pending in this slot.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            return false;
        }

        *lpiNumberOfAggressiveMoves = miNumberOfAggressiveMoves;   // *a2 = +0x20

        // Copy out exactly the received records (count * 80 bytes; the X360 computes count*80
        // as (count + count*4) << 4).
        memcpy(lpMoves, maAggressiveMoveDataArray,
               static_cast<u32>(miNumberOfAggressiveMoves) * sizeof(AggressiveMoveData));

        // Consume the slot: clear VALID, then assert it is now invalid.
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        CGS_ASSERT(!IsMessageValid(), "!IsMessageValid()");

        return true;
    }
} // namespace BrnNetwork
