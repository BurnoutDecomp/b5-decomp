#include "GameSource/Director/BrnReplayDirector.h"

namespace BrnDirector
{
// The real member constructors emit ARTIST827E2020's behaviour/collision-policy
// vptrs, two ICETake constructors, AllVehicleData array sentinel, CarScoreData
// constructor and DebugLog index sentinel. Remaining storage stays untouched.
ReplayDirector::ReplayDirector()
{
}
}
