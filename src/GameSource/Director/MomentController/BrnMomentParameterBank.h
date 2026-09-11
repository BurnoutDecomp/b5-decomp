#pragma once

// Home for BrnDirector::MomentParameterBank -- the fixed bank of pre-authored per-moment
// tuning records the MomentController hands to a freshly-allocated moment.
//
// MomentController embeds one of these by value (MomentController::mMomentParameterBank)
// and NewMoment calls GetParameters(leMomentParamID)
// on it to fetch the Moment::Parameters* it then feeds to the new moment's SetParameters.
//
// Member layout is the declared member list: one
// hard-stop record, two bystander records, and seven tumbling records, each held by value.
// The per-record field layouts are the real subclass Parameters (Moments/BrnMomentHardStop.h,
// Moments/BrnMomentTumbling.h, BrnMoment.h), so the bank layout is faithful (not offset-pinned
// -- x64 host).
//
// NOTE: GetParameters/Construct bodies are already homed in the standalone
// BrnMomentParameterBank.cpp ledger TU (which models the bank with its own local field
// naming). This header is the shared declaration the MomentController family compiles
// against; it is declaration-only for the methods (the per-TU `cl /c` gate does not link).

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"             // Moment::Parameters, MomentBystanderSeesAction::Parameters
#include "GameSource/Director/MomentController/Moments/BrnMomentTumbling.h"   // MomentTumbling::Parameters (held by value) -- THE REAL HOME
#include "GameSource/Director/MomentController/Moments/BrnMomentHardStop.h"   // MomentHardStop::Parameters (held by value) -- THE REAL HOME
//
// ⛔ KEEP THIS HEADER LIGHT. It is on the include path of BrnMainDirector.cpp. The umbrella
// over the other nine real moment homes deliberately lives in BrnMomentControllerNewMoment.cpp
// (its only consumer), NOT here.

namespace BrnDirector
{

class MomentParameterBank
{
public:
    enum EMomentParamID
    {
        E_PARAM_NONE                                 = 0,
        E_PARAM_HARD_STOP_DEFAULT                    = 1,
        E_PARAM_BYSTANDER_CLOSE_TAKEDOWN_ONLY        = 2,
        E_PARAM_BYSTANDER_FAR_CRASH_ONLY             = 3,
        E_PARAM_TUMBLING_TRUCKING_SIDE_CRASH_ONLY    = 4,
        E_PARAM_TUMBLING_TRUCKING_SIDE_TAKEDOWN_ONLY = 5,
        E_PARAM_TUMBLING_TRUCKING_FRONT_CRASH_ONLY   = 6,
        E_PARAM_TUMBLING_FOLLOW_CRASH_ONLY           = 7,
        E_PARAM_TUMBLING_LEAD_CRASH_ONLY             = 8,
        E_PARAM_TUMBLING_LEAD_TAKEDOWN_ONLY          = 9,
        E_PARAM_TUMBLING_SIDE_CRASH_ONLY             = 10
    };

    // Lifecycle (declared-only here;
    // bodies live in BrnMomentParameterBank.cpp).
    void Construct();
    bool Prepare();
    void Update();
    bool Release();
    void Destruct();

    // Returns the stored record for an id (NULL for
    // E_PARAM_NONE). Bodied in BrnMomentParameterBank.cpp.
    Moment::Parameters* GetParameters(EMomentParamID leMomentParamID);

private:
    // Member list; held by value.
    MomentHardStop::Parameters            mParamsHardStopDefault;
    MomentBystanderSeesAction::Parameters mParamsBystanderCloseTakedownOnly;
    MomentBystanderSeesAction::Parameters mParamsBystanderFarCrashOnly;
    MomentTumbling::Parameters            mParamsTumblingTruckingSideCrashOnly;
    MomentTumbling::Parameters            mParamsTumblingTruckingSideTakedownOnly;
    MomentTumbling::Parameters            mParamsTumblingTruckingFrontCrashOnly;
    MomentTumbling::Parameters            mParamsTumblingFollowCrashOnly;
    MomentTumbling::Parameters            mParamsTumblingLeadCrashOnly;
    MomentTumbling::Parameters            mParamsTumblingLeadTakedownOnly;
    MomentTumbling::Parameters            mParamsTumblingSideCrashOnly;
};

} // namespace BrnDirector
