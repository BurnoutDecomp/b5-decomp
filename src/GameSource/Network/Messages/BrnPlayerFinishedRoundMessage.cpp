#include "types.hpp"

#include "GameSource/Network/Messages/BrnPlayerFinishedRoundMessage.h"
#include "GameSource/Network/BrnNetworkManager.h"                                       // BrnNetworkManager::PackOrUnpack (NetworkPlayerID field primitive)
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"               // CgsNetwork::PackOrUnpack* field primitives
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsTestConnectionMessage.h" // base-size delegate (ICF-folded reliable size)
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::PlayerFinishedRoundMessage::Construct            @ 0x8257A188
//   BrnNetwork::PlayerFinishedRoundMessage::PackOrUnpack         @ 0x8257A1E8
//   BrnNetwork::PlayerFinishedRoundMessage::GetPackedMessageSize @ 0x8257A348
//   BrnNetwork::PlayerFinishedRoundMessage::PrepareForSend       @ 0x8257D9A0
//   BrnNetwork::PlayerFinishedRoundMessage::Retrieve             @ 0x8257DB78
//   (GetName is bodied inline in the header @ 0x827DFCD0; Destruct lives in its own
//    not-yet-reconstructed TU.)
//
// A CgsNetwork::ReliableMessage subclass announcing that a player has finished an
// elimination round. Payload (X360 byte offsets, after the size-0x28 reliable base):
//   +0x28 mFinishTime (Time)            +0x30 mEliminatorNetworkPlayerID (NetworkPlayerID)
//   +0x34 miEliminations (s32)          +0x38 mfDistanceFromFinish (f32)
//   +0x3C mu8RoundNumber (u8)           +0x3D mbTimedOut (bool)
//   +0x3E mbWonRound (bool)             <- X360-attested seventh scalar (DWARF omits it).
//
// PrepareForSend re-arms the reliable slot, range-asserts the distance, stores every
// field, then stamps the reliable message with type 12. Retrieve copies the seven fields
// out only when the VALID flag is set (clearing it). PackOrUnpack ORs every per-field
// status with the reliable base status (0 == all succeeded), routing the finish-time and
// distance through quantised float primitives and bracketing the distance pack/unpack
// with an unset<->invalid sentinel swap.
//
// FLAGGED constants -- exact .rdata float bits were not dumpable from the work packet, so
// the values below are the semantically-justified best estimates (the raw X360 addresses
// are documented per constant). A reviewer should pin them against a .rdata dump:
//   KF_INVALID_NETWORK_DISTANCE (flt_82F29A48), KF_UNSET_DISTANCE (flt_82F299C8),
//   KF_MIN_RACE_DISTANCE (-1.0), KF_MAX_RACE_DISTANCE (200000.0), the distance resolution
//   (flt_82085510) and the finish-time resolution (flt_8208550C == 1/600).

namespace BrnNetwork
{
    // ---- DWARF-named module constants (BrnPlayerFinishedRoundMessage.cpp:27-37) --------
    // Round-number quantiser range (asm: li r5,0 ; li r6,0xA -> [0, 10]).
    static const u8  KU8_MIN_NUM_ROUNDS         = 0;
    static const u8  KU8_MAX_NUM_ROUNDS         = 10;
    // Eliminations quantiser range (asm: li r5,0 ; li r6,7 -> [0, 7]). The DWARF spells the
    // bracket constants KI_MIN/MAX_ELIMINATOR_INDEX (= -1 / 7); the X360 PackOrUnpack passes
    // [0, 7] for the +0x34 field, so the runtime min is 0 (FLAGGED: DWARF min is -1).
    static const s32 KI_MIN_ELIMINATIONS        = 0;
    static const s32 KI_MAX_ELIMINATIONS        = 7;

    // Distance quantiser range + resolution (asm: f1=flt_82085620, f2=flt_82085640,
    // f3=flt_82085510 -> PackOrUnpackFloat(this, &dist, -1.0, 200000.0, 0.005)).
    static const f32 KF_MIN_RACE_DISTANCE       = -1.0f;       // flt_82085620
    static const f32 KF_MAX_RACE_DISTANCE       = 200000.0f;   // flt_82085640
    static const f32 KF_RACE_DISTANCE_RESOLUTION = 0.005f;     // flt_82085510 (pseudocode 0.0049999999)

    // Finish-time quantiser resolution (asm: f1=flt_8208550C -> ~1/600 s per tick).
    static const f32 KF_FINISH_TIME_RESOLUTION  = 1.0f / 600.0f; // flt_8208550C (pseudocode 0.0016666667)

    // Distance sentinels. KF_INVALID_NETWORK_DISTANCE is the wire/in-range "no valid
    // distance" marker the assert guards against (== flt_82F29A48). KF_UNSET_DISTANCE is the
    // in-RAM default Construct seeds (== flt_82F299C8); PackOrUnpack maps unset<->invalid
    // across the wire so the default survives a quantise round-trip. FLAGGED values.
    static const f32 KF_INVALID_NETWORK_DISTANCE = -1.0f;     // flt_82F29A48 (FLAGGED bits)
    static const f32 KF_UNSET_DISTANCE           = -2.0f;     // flt_82F299C8 (FLAGGED bits)

    // EMessageType id stamped onto the reliable wrapper (asm `li r4, 0xC`).
    static const s32 KI_PLAYER_FINISHED_ROUND_MESSAGE_TYPE = 12;

    // BrnNetwork::PlayerFinishedRoundMessage::Construct @ 0x8257A188
    // Inlines the MessageWithPlayerIDs base init (both player ids -> -1) ahead of the
    // Message ctor, then seeds the leaf fields: eliminator id -1, round number / both bool
    // flags 0, finish time 0.0 (via Time::SetFloatVal), eliminations 0, distance to the
    // unset sentinel. (miEliminations @+0x34 is NOT touched between the Message ctor and the
    // SetFloatVal call -- the asm zeroes it right after, at 0x8257A1CC.)
    void PlayerFinishedRoundMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two player ids -> -1).
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();

        mEliminatorNetworkPlayerID = KI_INVALID_PLAYER_ID;  // stw -1, +0x30
        mu8RoundNumber             = 0;                      // stb  0, +0x3C
        mFinishTime.SetFloatVal(0.0f);                       // SetFloatVal(&+0x28, 0.0)
        mbTimedOut                 = 0;                      // stb  0, +0x3D
        miEliminations             = 0;                      // stw  0, +0x34
        mbWonRound                 = 0;                      // stb  0, +0x3E
        mfDistanceFromFinish       = KF_UNSET_DISTANCE;      // stfs flt_82F299C8, +0x38
    }

    // BrnNetwork::PlayerFinishedRoundMessage::GetPackedMessageSize @ 0x8257A348
    // Re-seeds the same payload fields to their Construct defaults (worst-case-neutral
    // reset) before reporting the bare reliable size. The X360 build tail-calls a
    // COMDAT-folded copy resolved to TestConnectionMessage::GetPackedMessageSize, which is a
    // bare ReliableMessage size probe (== the base size). Note the asm sets only round
    // number / both bools / eliminations / distance / finish time and eliminator id here;
    // it does NOT reset the player ids (those are reliable-base state).
    s32 PlayerFinishedRoundMessage::GetPackedMessageSize()
    {
        mEliminatorNetworkPlayerID = KI_INVALID_PLAYER_ID;  // stw -1, +0x30
        mu8RoundNumber             = 0;                      // stb  0, +0x3C
        mFinishTime.SetFloatVal(0.0f);                       // SetFloatVal(&+0x28, 0.0)
        mbTimedOut                 = 0;                      // stb  0, +0x3D
        miEliminations             = 0;                      // stw  0, +0x34
        mbWonRound                 = 0;                      // stb  0, +0x3E
        mfDistanceFromFinish       = KF_UNSET_DISTANCE;      // stfs flt_82F299C8, +0x38

        // @0x8257A39C: b CgsNetwork__TestConnectionMessage__GetPackedMessageSize -- a bare
        // ReliableMessage probe (no extra payload) measuring the base size with `this`
        // reinterpreted as that sibling (matches the X360 ICF tail call).
        return reinterpret_cast<CgsNetwork::TestConnectionMessage*>(this)->GetPackedMessageSize();
    }

    // BrnNetwork::PlayerFinishedRoundMessage::PackOrUnpack @ 0x8257A1E8
    // (De)serialises, in order: the reliable base id, the round number ([0,10]), the
    // eliminator id (via BrnNetworkManager's NetworkPlayerID primitive), the finish time
    // (quantised at 1/600 s), the distance (quantised in [-1, 200000] at 0.005) bracketed by
    // the unset<->invalid sentinel swap, the eliminations ([0,7]) and finally the two bool
    // flags. Every per-field status is OR-ed into the result (0 == all succeeded).
    CgsNetwork::PackOrUnpackResult PlayerFinishedRoundMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::ReliableMessage::PackOrUnpack();

        lxResult |= CgsNetwork::PackOrUnpackU8(this, &mu8RoundNumber,
                                               KU8_MIN_NUM_ROUNDS, KU8_MAX_NUM_ROUNDS);
        lxResult |= BrnNetwork::BrnNetworkManager::PackOrUnpack(this, &mEliminatorNetworkPlayerID);
        lxResult |= CgsNetwork::PackOrUnpackTime(this, &mFinishTime, KF_FINISH_TIME_RESOLUTION);

        // A valid distance must never equal the in-range invalid sentinel.
        CGS_ASSERT(mfDistanceFromFinish != KF_INVALID_NETWORK_DISTANCE,
                   "mfDistanceFromFinish != KF_INVALID_NETWORK_DISTANCE");

        // Map the in-RAM unset default onto its serialisable wire equivalent before packing.
        if (mfDistanceFromFinish == KF_UNSET_DISTANCE)
            mfDistanceFromFinish = KF_INVALID_NETWORK_DISTANCE;

        lxResult |= CgsNetwork::PackOrUnpackFloat(this, &mfDistanceFromFinish,
                                                  KF_MIN_RACE_DISTANCE, KF_MAX_RACE_DISTANCE,
                                                  KF_RACE_DISTANCE_RESOLUTION);

        // Restore the in-RAM unset default after the (un)pack round-trip.
        if (mfDistanceFromFinish == KF_INVALID_NETWORK_DISTANCE)
            mfDistanceFromFinish = KF_UNSET_DISTANCE;

        lxResult |= CgsNetwork::PackOrUnpackInt(this, &miEliminations,
                                                KI_MIN_ELIMINATIONS, KI_MAX_ELIMINATIONS);
        lxResult |= CgsNetwork::PackOrUnpackBool(this, &mbTimedOut);
        lxResult |= CgsNetwork::PackOrUnpackBool(this, &mbWonRound);

        return lxResult;
    }

    // BrnNetwork::PlayerFinishedRoundMessage::PrepareForSend @ 0x8257D9A0
    // Re-arms the slot (drops a previously-pending VALID flag), range-asserts the distance
    // is within [-1, 200000], stores all seven payload fields, stamps the reliable wrapper
    // with type 12 at frame lu16Frame, then asserts the message came out reliable.
    void PlayerFinishedRoundMessage::PrepareForSend(u16 lu16Frame, u8 lu8RoundNumber,
                                                    Time lFinishTime, f32 lfDistanceFromFinish,
                                                    NetworkPlayerID lEliminatorNetworkPlayerID,
                                                    s32 liEliminations, bool lbTimedOut,
                                                    bool lbWonRound)
    {
        // Re-arm: drop a previously-pending VALID flag before re-stamping the slot.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;

        // Distance range asserts (asm: ble/bge vs flt_82085640 / flt_82085620). The original
        // streamed the offending value into the assert buffer ("Bad distance in message #N!
        // lfDistanceFromFinish=<value>"); the streamed value is a debug-only formatting
        // detail, so the condition is reproduced here without the value interpolation.
        CGS_ASSERT(lfDistanceFromFinish <= KF_MAX_RACE_DISTANCE,
                   "Bad distance in message #1! lfDistanceFromFinish=");
        CGS_ASSERT(lfDistanceFromFinish >= KF_MIN_RACE_DISTANCE,
                   "Bad distance in message #2! lfDistanceFromFinish=");

        mu8RoundNumber             = lu8RoundNumber;             // stb,  +0x3C
        mEliminatorNetworkPlayerID = lEliminatorNetworkPlayerID; // stw,  +0x30
        mFinishTime                = lFinishTime;                // +0x28/+0x2C (copy from r6)
        mbTimedOut                 = lbTimedOut;                 // stb,  +0x3D
        mfDistanceFromFinish       = lfDistanceFromFinish;       // stfs, +0x38
        miEliminations             = liEliminations;             // stw,  +0x34
        mbWonRound                 = lbWonRound;                 // stb,  +0x3E

        CgsNetwork::ReliableMessage::PrepareForSend(KI_PLAYER_FINISHED_ROUND_MESSAGE_TYPE,
                                                    lu16Frame);

        // @0x8257DB30: virtual call through vtable slot 0 -> IsReliable().
        CGS_ASSERT(IsReliable(), "IsReliable()");
    }

    // BrnNetwork::PlayerFinishedRoundMessage::Retrieve @ 0x8257DB78
    // Copies the seven payload fields out into the caller's buffers only when the inherited
    // VALID flag is set, then clears it (and asserts it is now clear). Returns whether
    // anything was retrieved.
    bool PlayerFinishedRoundMessage::Retrieve(u8* lpu8RoundNumber, Time* lpFinishTime,
                                              f32* lpfDistanceFromFinish,
                                              NetworkPlayerID* lpEliminatorNetworkPlayerID,
                                              s32* lpiEliminations, bool* lpbTimedOut,
                                              bool* lpbWonRound)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpu8RoundNumber            = mu8RoundNumber;              // +0x3C -> r4
        *lpEliminatorNetworkPlayerID = mEliminatorNetworkPlayerID; // +0x30 -> r7
        *lpFinishTime               = mFinishTime;                 // +0x28/+0x2C -> r5
        *lpfDistanceFromFinish      = mfDistanceFromFinish;        // +0x38 -> r6
        *lpbTimedOut                = mbTimedOut;                  // +0x3D -> r9
        *lpiEliminations            = miEliminations;              // +0x34 -> r8
        *lpbWonRound                = mbWonRound;                  // +0x3E -> r10

        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                   "!CgsNetwork::ReliableMessage::IsMessageValid()");
        return true;
    }
} // namespace BrnNetwork
