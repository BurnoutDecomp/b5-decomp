#pragma once

#include "GameSource/Network/Messages/BrnStuntMultiplierMessage.h"

// ARTIST 0x8231C170 and 0x8234CE48 populate this 16-byte online scoring record.
// Both copy the multiplier-message payload, then record the car slot and multiplier time.
namespace BrnGameState
{
namespace GameStateModuleIO
{
struct ChainableMultiplierInfo
{
    BrnNetwork::StuntMultiplierMessage::MultiplierInfo mMultiplierInfo;
    s32 miRaceCarIndex;
    s32 miMultiplier;
};

CGS_STATIC_ASSERT((sizeof(ChainableMultiplierInfo) == 16), "ChainableMultiplierInfo stride");
}
}
