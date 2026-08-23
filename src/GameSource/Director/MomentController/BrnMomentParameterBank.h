#pragma once

// Home for BrnDirector::MomentParameterBank -- the fixed bank of pre-authored per-moment
// tuning records the MomentController hands to a freshly-allocated moment.
// DWARF home: GameSource/Director/MomentController/BrnMomentParameterBank.h:50.
//
// MomentController embeds one of these by value (DWARF: MomentController::mMomentParameterBank
// at BrnMomentController.h:83) and NewMoment (@0x82255850) calls GetParameters(leMomentParamID)
// on it to fetch the Moment::Parameters* it then feeds to the new moment's SetParameters.
//
// Member layout is the DecFIGS DWARF member list (BrnMomentParameterBank.h:90-101): one
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
#include "GameSource/Director/MomentController/BrnMomentSubclasses.h"          // MomentHardStop::Parameters -- the LAST remaining slice, see that file
// ⚠️ 2026-08-23: these two used to BOTH come from BrnMomentSubclasses.h, which carried its own
// layout-stubbed MomentHardStop / MomentTumbling (plus nine more stub moment classes). Ten of
// the eleven stubs are retired; MomentTumbling now comes from its real home. MomentHardStop
// could NOT follow -- its real home drags BrnDirectorVehicleTracker.h, whose
// BrnDirector::DirectorIO::InputBuffer slice is a SECOND definition of the one in
// BrnDirectorModuleIO.h, so any TU that sees both dies with C2011 -- and this header reaches
// BrnMainDirector.cpp (MomentController holds the bank by value; MainDirector reaches
// MomentController). Read BrnMomentSubclasses.h for the full note and the DELETE-WHEN.
//
// ⛔ KEEP THIS HEADER LIGHT. It is on the include path of BrnMainDirector.cpp. The umbrella
// over the other nine real moment homes deliberately lives in BrnMomentControllerNewMoment.cpp
// (its only consumer), NOT here.

namespace BrnDirector
{

class MomentParameterBank
{
public:
    // DWARF: BrnMomentParameterBank.h:53.
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

    // DWARF: BrnMomentParameterBank.h:69/72/76/79/82. Lifecycle (declared-only here;
    // bodies live in BrnMomentParameterBank.cpp).
    void Construct();
    bool Prepare();
    void Update();
    bool Release();
    void Destruct();

    // DWARF: BrnMomentParameterBank.h:86. Returns the stored record for an id (NULL for
    // E_PARAM_NONE). Bodied in BrnMomentParameterBank.cpp.
    Moment::Parameters* GetParameters(EMomentParamID leMomentParamID);

private:
    // DWARF member list (BrnMomentParameterBank.h:90-101); held by value.
    MomentHardStop::Parameters            mParamsHardStopDefault;                  // :90
    MomentBystanderSeesAction::Parameters mParamsBystanderCloseTakedownOnly;       // :92
    MomentBystanderSeesAction::Parameters mParamsBystanderFarCrashOnly;            // :93
    MomentTumbling::Parameters            mParamsTumblingTruckingSideCrashOnly;    // :95
    MomentTumbling::Parameters            mParamsTumblingTruckingSideTakedownOnly; // :96
    MomentTumbling::Parameters            mParamsTumblingTruckingFrontCrashOnly;   // :97
    MomentTumbling::Parameters            mParamsTumblingFollowCrashOnly;          // :98
    MomentTumbling::Parameters            mParamsTumblingLeadCrashOnly;            // :99
    MomentTumbling::Parameters            mParamsTumblingLeadTakedownOnly;         // :100
    MomentTumbling::Parameters            mParamsTumblingSideCrashOnly;            // :101
};

} // namespace BrnDirector
