#include "types.hpp"

#include "GameSource/Network/Messages/BrnStatsUpdateMessage.h"
#include "GameSource/World/DebugComponents/BrnPVSDebugComponent.h"   // base pack/unpack stub (COMDAT-folded)
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::StatsUpdateMessage::Construct             @ 0x8257CF90
//   BrnNetwork::StatsUpdateMessage::Destruct              @ 0x8257CFD0
//   BrnNetwork::StatsUpdateMessage::GetName              @ 0x8257D0C0
//   BrnNetwork::StatsUpdateMessage::GetPackedMessageSize  @ 0x8257CFE8
//   BrnNetwork::StatsUpdateMessage::PackOrUnpack          @ 0x8257D000
//   BrnNetwork::StatsUpdateMessage::PrepareForSend        @ 0x8257FE60
//   BrnNetwork::StatsUpdateMessage::Retrieve              @ 0x8257FF00
//
// An UNRELIABLE per-player message carrying four running stat counters. Construct /
// Destruct / GetPackedMessageSize all zero the four counters; GetPackedMessageSize then
// defers to the base Message size.
//
// PackOrUnpack ORs the base pack/unpack status (the X360 build resolves the base-stub call
// to a COMDAT-folded copy reported as BrnWorld::PVSDebugComponent::IsSimple, which returns
// false == 0 == success) with the four quantised int counters in their wire ranges
// ([0,2000] / [0,250] / [0,50] / [0,0x7FFFFFFF]).
//
// PrepareForSend inlines the base Message frame-stamp (the X360 build does not call
// Message::PrepareForSend here -- it interleaves the four field stores between clearing and
// re-setting the VALID flag, then stamps frame/type/VALID by hand). Type id 40 (0x28).
// Reproduced store-for-store against the named base fields. The frame-validity assert keeps
// the X360 message-base wording.

namespace BrnNetwork
{
    static const s8 KI8_STATS_UPDATE_MESSAGE_TYPE = 40;   // 0x28

    void StatsUpdateMessage::Construct()
    {
        CgsNetwork::Message::Construct();
        miNumberOfChallenges   = 0;
        miNumberOfRivals       = 0;
        miNumberOfAchievements = 0;
        miNumberOfTakedowns    = 0;
    }

    void StatsUpdateMessage::Destruct()
    {
        miNumberOfChallenges   = 0;
        miNumberOfRivals       = 0;
        miNumberOfAchievements = 0;
        miNumberOfTakedowns    = 0;
    }

    const char* StatsUpdateMessage::GetName() const
    {
        return "Stats Update Message";
    }

    s32 StatsUpdateMessage::GetPackedMessageSize()
    {
        miNumberOfChallenges   = 0;
        miNumberOfRivals       = 0;
        miNumberOfAchievements = 0;
        miNumberOfTakedowns    = 0;
        return CgsNetwork::Message::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult StatsUpdateMessage::PackOrUnpack()
    {
        const CgsNetwork::PackOrUnpackResult lxBase =
            reinterpret_cast<BrnWorld::PVSDebugComponent*>(this)->IsSimple() ? 1 : 0;
        const CgsNetwork::PackOrUnpackResult lxChallenges =
            CgsNetwork::PackOrUnpackInt(this, &miNumberOfChallenges, 0, 2000) | lxBase;
        const CgsNetwork::PackOrUnpackResult lxRivals =
            CgsNetwork::PackOrUnpackInt(this, &miNumberOfRivals, 0, 250) | lxChallenges;
        const CgsNetwork::PackOrUnpackResult lxAchievements =
            CgsNetwork::PackOrUnpackInt(this, &miNumberOfAchievements, 0, 50);
        return CgsNetwork::PackOrUnpackInt(this, &miNumberOfTakedowns, 0, 0x7FFFFFFF)
               | (lxAchievements | lxRivals);
    }

    void StatsUpdateMessage::PrepareForSend(u16 lu16Frame,
                                            s32 liNumberOfChallenges,
                                            s32 liNumberOfRivals,
                                            s32 liNumberOfAchievements,
                                            s32 liNumberOfTakedowns)
    {
        // Clear any pending VALID flag before re-stamping (X360 store order).
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;

        miNumberOfChallenges   = liNumberOfChallenges;
        miNumberOfRivals       = liNumberOfRivals;
        miNumberOfAchievements = liNumberOfAchievements;
        miNumberOfTakedowns    = liNumberOfTakedowns;

        // Inlined Message::PrepareForSend frame-stamp.
        CGS_ASSERT(lu16Frame != CgsNetwork::KU16_INVALID_FRAME,
                   "lu16Frame != KU16_INVALID_FRAME");
        mu16Frame = lu16Frame;
        mi8Type   = KI8_STATS_UPDATE_MESSAGE_TYPE;
        mx8Flags  = static_cast<u8>(mx8Flags | CgsNetwork::KX8_FLAGS_VALID);
    }

    bool StatsUpdateMessage::Retrieve(s32* lpiNumberOfChallenges,
                                      s32* lpiNumberOfRivals,
                                      s32* lpiNumberOfAchievements,
                                      s32* lpiNumberOfTakedowns)
    {
        CGS_ASSERT(lpiNumberOfChallenges, "lpiNumberOfChallenges");
        CGS_ASSERT(lpiNumberOfRivals, "lpiNumberOfRivals");
        CGS_ASSERT(lpiNumberOfAchievements, "lpiNumberOfAchievements");
        CGS_ASSERT(lpiNumberOfTakedowns, "lpiNumberOfTakedowns");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            *lpiNumberOfChallenges   = miNumberOfChallenges;
            *lpiNumberOfRivals       = miNumberOfRivals;
            *lpiNumberOfAchievements = miNumberOfAchievements;
            *lpiNumberOfTakedowns    = miNumberOfTakedowns;
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
            return true;
        }

        *lpiNumberOfChallenges   = 0;
        *lpiNumberOfRivals       = 0;
        *lpiNumberOfAchievements = 0;
        *lpiNumberOfTakedowns    = 0;
        return false;
    }
} // namespace BrnNetwork
