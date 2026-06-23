#pragma once

// ===================================================================================
// BrnNetwork::StatsUpdateMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnStatsUpdateMessage.h
//
// SHAPE from DecFIGS DWARF (BrnStatsUpdateMessage.h:44) gated against the X360 binary. An
// UNRELIABLE per-player message carrying four running stat counters (challenges, rivals,
// achievements, takedowns).
//
// LAYOUT (X360-AUTHORITATIVE offsets; Construct @ 0x8257CF90, PrepareForSend @ 0x8257FE60,
//          Retrieve @ 0x8257FF00):
//   +0x00  (CgsNetwork::Message base, size 0x20)
//   +0x20  s32 miNumberOfChallenges     (PackOrUnpack int [0, 2000])
//   +0x24  s32 miNumberOfRivals         (PackOrUnpack int [0, 250])
//   +0x28  s32 miNumberOfAchievements   (PackOrUnpack int [0, 50])
//   +0x2C  s32 miNumberOfTakedowns      (PackOrUnpack int [0, 0x7FFFFFFF])
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"

namespace BrnNetwork
{
    struct StatsUpdateMessage : CgsNetwork::Message
    {
        s32 miNumberOfChallenges;     // +0x20
        s32 miNumberOfRivals;         // +0x24
        s32 miNumberOfAchievements;   // +0x28
        s32 miNumberOfTakedowns;      // +0x2C

        void                          Construct();
        void                          Destruct();
        void                          PrepareForSend(u16 lu16Frame,
                                                     s32 liNumberOfChallenges,
                                                     s32 liNumberOfRivals,
                                                     s32 liNumberOfAchievements,
                                                     s32 liNumberOfTakedowns);
        bool                          Retrieve(s32* lpiNumberOfChallenges,
                                              s32* lpiNumberOfRivals,
                                              s32* lpiNumberOfAchievements,
                                              s32* lpiNumberOfTakedowns);
        s32                           GetPackedMessageSize();
        CgsNetwork::PackOrUnpackResult PackOrUnpack();
        const char*                   GetName() const;
    };
} // namespace BrnNetwork
