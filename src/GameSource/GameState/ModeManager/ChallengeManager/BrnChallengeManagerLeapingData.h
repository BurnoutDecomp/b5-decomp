#pragma once

#include "types.hpp"

// Provisional element home for the ObjectPool<CarLeapingData,7,s32> instantiation. CarLeapingData
// is a nested record of BrnGameState::ChallengeManager (real fields land with that TU); sized
// to the X360 element stride (32 bytes). Single owner -- grow in place.
namespace BrnGameState
{
class ChallengeManager
{
public:
    struct CarLeapingData { u8 maBlob[32]; };
    struct StoredLeapingData { u8 maBlob[32]; };   // 32-byte X360 element stride (EActiveRaceCarIndex + Vector3); blob-style to match CarLeapingData
};
}
