#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations

#include "GameSource/Network/Messages/BrnRestartTrafficMessage.h"
#include "GameSource/Network/BrnNetworkManager.h"        // BrnNetworkManager::PackOrUnpack (player-id field helper)
#include "GameSource/BurnoutConstants.h"                 // EActiveRaceCarIndex + range-guarded operator++
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

#include <xtl.h>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::RestartTrafficMessage::Construct           @ 0x8257BB90
//   BrnNetwork::RestartTrafficMessage::PrepareForSend      @ 0x8257BBD0
//   BrnNetwork::RestartTrafficMessage::GetPackedMessageSize @ 0x8257BCA0
//   BrnNetwork::RestartTrafficMessage::PackOrUnpack        @ 0x8257BCF8
//   BrnNetwork::RestartTrafficMessage::Retrieve            @ 0x8257EBB0
//
// A RELIABLE message broadcasting a traffic restart: the eight active traffic hulls,
// the eight player ids owning those hulls, and the game-logic frame at which traffic
// should reset (mu16FrameSinceStartToResetTraffic, +0x58). The two payload arrays sit
// right after the 0x28-byte ReliableMessage base:
//   +0x28  maPlayerIDsForActiveHulls[8]   (8 x s32 = 32 bytes)
//   +0x48  mau16ActiveTrafficHulls[8]     (8 x u16 = 16 bytes)
//   +0x58  mu16FrameSinceStartToResetTraffic (u16)
//
// Construct seeds the two inherited player-id fields to -1, chains to the Message base,
// then clears the reset frame. PrepareForSend asserts the message is not already valid,
// stores the reset frame, memcpys the two payload arrays in (the X360 XMemCpy moves of
// 16 / 32 bytes), stamps the reliable send frame/type (id 28) through the base, then
// asserts IsReliable(). PackOrUnpack ORs the base reliable status with the reset frame
// and, for each active-race-car slot, the hull value and the player id -- exactly the
// X360 OR-accumulation (0 == success). The per-iteration index guard is the inlined
// EActiveRaceCarIndex operator++ range assert.

#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Messages/BrnRestartTrafficMessage.cpp"

namespace BrnNetwork
{
    // DecFIGS DWARF (BrnRestartTrafficMessage.cpp:24-27). The X360 build inlines these as
    // the [0, 0xFFFF] wire ranges the two u16 fields quantise to.
    const u16 KU16_MIN_RESTART_FRAME = 0;
    const u16 KU16_MAX_RESTART_FRAME = 65535;
    const u16 KU16_MIN_HULL_VALUE    = 0;
    const u16 KU16_MAX_HULL_VALUE    = 65535;

    // Wire type id stamped onto the reliable send (CgsNetwork::ReliableMessage::
    // PrepareForSend leType arg; li r4, 0x1C in the X360 body).
    static const s32 KI_RESTART_TRAFFIC_MESSAGE_TYPE = 28;

    void RestartTrafficMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (the two player ids -> -1, stored before
        // the base Construct in the X360 body).
        mSendingPlayerID = KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = KI_INVALID_PLAYER_ID;
        CgsNetwork::Message::Construct();
        mu16FrameSinceStartToResetTraffic = 0;      // sth 0, +0x58
    }

    void RestartTrafficMessage::PrepareForSend(u16 lu16CurrentFrame,
                                               u16 lu16FrameSinceStartToResetTraffic,
                                               const u16* lpau16ActiveTrafficHulls,
                                               const NetworkPlayerID* lpaPlayerIDsForActiveHulls)
    {
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0, CGS_ASSERT_MESSAGE_AT(
            "!CgsNetwork::ReliableMessage::IsMessageValid()",
            CGS_ARTIST_FILE, 85 /* ARTIST 0x8257BC1C */));

        mu16FrameSinceStartToResetTraffic = lu16FrameSinceStartToResetTraffic;   // +0x58
        XMemCpy(mau16ActiveTrafficHulls,   lpau16ActiveTrafficHulls,   sizeof(mau16ActiveTrafficHulls));        // +0x48, 8 x u16
        XMemCpy(maPlayerIDsForActiveHulls, lpaPlayerIDsForActiveHulls, sizeof(maPlayerIDsForActiveHulls));        // +0x28, 8 x s32

        CgsNetwork::ReliableMessage::PrepareForSend(KI_RESTART_TRAFFIC_MESSAGE_TYPE, lu16CurrentFrame);
        CGS_ASSERT(IsReliable(), CGS_ASSERT_MESSAGE_AT(
            "IsReliable()",
            CGS_ARTIST_FILE, 94 /* ARTIST 0x8257BC8C */));
    }

    s32 RestartTrafficMessage::GetPackedMessageSize()
    {
        // Clear the payload back to its empty default (frame -> 0, then both arrays
        // zeroed -- the sth @+0x58 followed by the two XMemSet moves), then return the
        // base reliable size. The base call folds (ICF) with TestConnectionMessage::
        // GetPackedMessageSize in the X360 image; the source calls the ReliableMessage
        // base per the DWARF (RestartTrafficMessage : ReliableMessage).
        mu16FrameSinceStartToResetTraffic = 0;                             // sth 0, +0x58
        XMemSet(mau16ActiveTrafficHulls,   0, sizeof(mau16ActiveTrafficHulls));                          // +0x48, 8 x u16
        XMemSet(maPlayerIDsForActiveHulls, 0, sizeof(maPlayerIDsForActiveHulls));                          // +0x28, 8 x s32
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    bool RestartTrafficMessage::Retrieve(u16* lpu16FrameSinceStartToResetTraffic,
                                         u16* lpau16ActiveTrafficHulls,
                                         NetworkPlayerID* lpaPlayerIDsForActiveHulls)
    {
        // ARTIST 0x8257EBE0 skips the payload copy and returns false when invalid.
        if (IsMessageValid())
        {
            *lpu16FrameSinceStartToResetTraffic = mu16FrameSinceStartToResetTraffic;   // lhz +0x58
            XMemCpy(lpau16ActiveTrafficHulls,   mau16ActiveTrafficHulls,   sizeof(mau16ActiveTrafficHulls));
            XMemCpy(lpaPlayerIDsForActiveHulls, maPlayerIDsForActiveHulls, sizeof(maPlayerIDsForActiveHulls));

            // Consume the message: clear VALID, then assert it is no longer valid (the
            // X360 re-reads +0x19 and fires the IsMessageValid assert -- always passes here).
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
            CGS_ASSERT(!IsMessageValid(), CGS_ASSERT_MESSAGE_AT(
                "!CgsNetwork::ReliableMessage::IsMessageValid()",
                CGS_ARTIST_FILE, 126 /* ARTIST 0x8257EC38 */));
            return true;
        }
        return false;
    }

    CgsNetwork::PackOrUnpackResult RestartTrafficMessage::PackOrUnpack()
    {
        // Base reliable (de)serialise (the wrapped 16-bit reliable id).
        const CgsNetwork::PackOrUnpackResult lxBase = CgsNetwork::ReliableMessage::PackOrUnpack();

        // The reset frame, then each slot's hull value + owning player id. Every field's
        // per-field status is OR-ed into the running result (0 == all succeeded).
        CgsNetwork::PackOrUnpackResult lxResult =
            CgsNetwork::Message::PackOrUnpack(&mu16FrameSinceStartToResetTraffic,
                                        KU16_MIN_RESTART_FRAME, KU16_MAX_RESTART_FRAME) | lxBase;

        for (s32 leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
             leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
             ++leActiveRaceCarIndex)
        {
            const CgsNetwork::PackOrUnpackResult lxHull =
                CgsNetwork::Message::PackOrUnpack(&mau16ActiveTrafficHulls[leActiveRaceCarIndex],
                                            KU16_MIN_HULL_VALUE, KU16_MAX_HULL_VALUE) | lxResult;
            lxResult = BrnNetworkManager::PackOrUnpack(this,
                           &maPlayerIDsForActiveHulls[leActiveRaceCarIndex]) | lxHull;

            // Inlined EActiveRaceCarIndex operator++ range guard (X360 fires only if the
            // incremented index exceeds the slot count; non-fatal).
            CGS_ASSERT(leActiveRaceCarIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
        }

        return lxResult;
    }
} // namespace BrnNetwork

#undef CGS_ARTIST_FILE
