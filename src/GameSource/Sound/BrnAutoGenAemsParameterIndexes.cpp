#include "GameSource/Sound/BrnAutoGenAemsParameterIndexes.h"

// AUTO-GENERATED. BrnAutoGenAemsParameterIndexes.cpp — startup-initialized AEMS
// parameter-name index arrays. The compiler emits one dynamic (static) initializer
// per array; each element is constructed from a string literal via the
// CgsSound::Playback::Name(const char*) ctor, which hashes and interns the name
// (MakeHash -> HashTable::Store).
//
// THIS TU defines ONLY AEMS_Class::gaAEMS_ClassParameterNames[12]. The other 13
// sibling arrays (AEMS_TrafficEngineClass, AEMS_Skids, ...) are each their own
// static-init TU and add their definitions to this same .cpp later.
//
// STATIC-INIT-ORDER SAFETY: the Name ctor reaches into the CgsCommon HashTable pool
// (sapHashNode/saNodes/su32CurrentNode). That pool is statically (zero/constant-)
// initialized before any dynamic initializer runs, so the intern done here during
// this array's dynamic init is safe regardless of TU init order.

namespace BrnSound
{
namespace ParameterIndexes
{
namespace AEMS_Class
{

CgsSound::Playback::Name gaAEMS_ClassParameterNames[12] =
{
    CgsSound::Playback::Name("AEMS_velocity"),                  // [0]
    CgsSound::Playback::Name("AEMS_start_stage_2"),            // [1]
    CgsSound::Playback::Name("AEMS_control"),                  // [2]
    CgsSound::Playback::Name("AEMS_stop_boost"),               // [3]
    CgsSound::Playback::Name("AEMS_boost_remaining"),          // [4]
    CgsSound::Playback::Name("AEMS_car_speed"),                // [5]
    CgsSound::Playback::Name("AEMS_volume"),                   // [6]
    CgsSound::Playback::Name("AEMS_time_since_last_boostin"),  // [7]
    CgsSound::Playback::Name("AEMS_time_since_last_boostout"), // [8]
    CgsSound::Playback::Name("AEMS_time_boosting"),            // [9]
    CgsSound::Playback::Name("AEMS_is_boost_blue"),            // [10]
    CgsSound::Playback::Name("AEMS_skid_intensity")            // [11]
};

}
}
}
