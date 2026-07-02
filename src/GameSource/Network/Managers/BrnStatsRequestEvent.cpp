#include "GameSource/Network/Managers/BrnStatsRequestEvent.h"

#include <cstring>   // std::strlen / std::strncpy

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnNetwork::StatsRequestEvent -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameSource/Network/Managers/BrnStatsRequestEvent.cpp):
//   StatsRequestEvent::Construct @0x825487D8

namespace BrnNetwork
{

// @ 0x825487D8
void StatsRequestEvent::Construct(const char* lpcName, s32 liPlayerID)
{
    // Non-gating tripwire (cpp:47; the X360 streams the message, folded static):
    // the source name must fit the 16-byte key buffer.
    CGS_ASSERT(std::strlen(lpcName) < sizeof(macName),
               "Memory trample, name bigger than buffer");

    std::strncpy(macName, lpcName, sizeof(macName));
    mPlayerID = liPlayerID;
}

}
