#include "SharedClasses/Progression/BrnRaceEventData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnProgression::EventRacerPersonality::Construct   @ 0x826767B8

namespace BrnProgression
{

// X360 0x826767B8. Resets the four AI tuning values to zero. The X360 build loads a single
// 0.0f constant (flt_82001CC0) and stores it into all four float slots.
void EventRacerPersonality::Construct()
{
    mfMinAggression = 0.0f;
    mfMaxAggression = 0.0f;
    mfSkill         = 0.0f;
    mfSpeed         = 0.0f;
}

}
