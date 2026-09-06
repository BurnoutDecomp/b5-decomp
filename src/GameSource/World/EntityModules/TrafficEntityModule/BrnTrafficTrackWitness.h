#ifndef BRN_TRAFFIC_TRACK_WITNESS_H
#define BRN_TRAFFIC_TRACK_WITNESS_H

// ============================================================================
// [FLAG PC witness] BrnTrafficTrackWitness.h -- NOT IN THE X360 BINARY.
//
// The `traffic_weird` bug-test lane's oracle: "traffic disappears, teleports above the road
// and does other weird things" is not an assert, so the case needs a number the bug moves.
// This header is the opt-in switch plus the removal-REASON baton the witness prints.
//
// OFF unless BRN_TRAFFIC_TRACK is set in the environment (the bug case passes it through
// DiagEnv; flow_run clears every BRN_* first, so a normal run pays one getenv per frame).
//
// DELETE-WHEN: the traffic_weird lane's case is green on three consecutive tips and the
// [traffic-track] checks have been folded into a cheaper permanent probe (or deleted).
// ============================================================================

#include <cstdlib>   // getenv

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / DebugPrint

namespace BrnTraffic
{
// One env lookup, latched. Same shape as TrafficDiagEnabled next door.
inline bool TrafficTrackEnabled()
{
    static const bool sbEnabled = (std::getenv("BRN_TRAFFIC_TRACK") != 0);
    return sbEnabled;
}

inline CgsDev::Log::DebugPrint* TrafficTrackStream()
{
    if (!TrafficTrackEnabled() || CgsDev::Log::gpDebugPrint == 0)
    {
        return 0;
    }
    return CgsDev::Log::gpDebugPrint;
}

// The removal-reason baton. TrafficEntityModule::RemoveVehicle is the module's single kill
// entry point and has ELEVEN callers, none of which passes a reason -- so the witness cannot
// say WHY a car vanished without one. Each instrumented call site parks its tag here for the
// duration of the call; RemoveVehicle prints it and resets it. Definition lives in
// BrnTrafficEntityModule_wT5_01.cpp beside RemoveVehicle itself.
// Single-threaded by construction: every RemoveVehicle caller runs inside PostPhysicsUpdate /
// PreSceneUpdate on the module's own thread.
extern const char* gpcTrafficRemoveReason;

// RAII tag so an early `return` inside a caller cannot leave a stale reason behind.
class TrafficRemoveReasonTag
{
public:
    explicit TrafficRemoveReasonTag(const char* lpcReason)
        : mpcPrevious(gpcTrafficRemoveReason)
    {
        gpcTrafficRemoveReason = lpcReason;
    }

    ~TrafficRemoveReasonTag()
    {
        gpcTrafficRemoveReason = mpcPrevious;
    }

private:
    const char* mpcPrevious;

    TrafficRemoveReasonTag(const TrafficRemoveReasonTag&);
    TrafficRemoveReasonTag& operator=(const TrafficRemoveReasonTag&);
};
}

#endif // BRN_TRAFFIC_TRACK_WITNESS_H
