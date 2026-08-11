#pragma once

// ============================================================================
// The thirteen VehicleManager PerfMonCpu monitor handles UpdateVehiclePhysics
// brackets its stages with (dword_82F2A14C..dword_82F2A19C).
//
// On the console ALL of these are file-scope statics of the ONE BrnVehicleManager.cpp
// translation unit; this tree splits that TU into slices, so the handles that more than
// one slice reads are hoisted to external linkage here. They are DEFINED in
// BrnVehicleManager_Construct.cpp (which registers them, in the console's order, with
// the console's page/name/budget/tag arguments) and READ by
// BrnVehicleManager_UpdateVehiclePhysics.cpp's Start/StopMonitor brackets.
//
// The gs_ spelling is kept verbatim from the Construct TU (the identifiers pre-date the
// hoist; renaming them would churn thirty call sites for zero information).
//
// The sixteen handles NOT listed here (stunt offences, the traction sub-stages, the
// seven guarded VPhys sub-monitors) stay file-static in the Construct TU -- nothing
// else reads them yet. Hoist ADDITIVELY when a slice needs one; do not re-declare
// locally (see the shadowing-redeclaration postmortems).
// ============================================================================

#include "types.hpp"

namespace BrnPhysics
{
namespace Vehicle
{
    extern s32 gs_iUpdateVehicleImpactsPM;    // dword_82F2A14C
    extern s32 gs_iProcessAboveGroundLTsPM;   // dword_82F2A150
    extern s32 gs_iTractionLTsPM;             // dword_82F2A154
    extern s32 gs_iCrashFatalPM;              // dword_82F2A178
    extern s32 gs_iUpdateRaceCarsPM;          // dword_82F2A17C
    extern s32 gs_iUpdateDriversPM;           // dword_82F2A180
    extern s32 gs_iUpdateVehiclesPM;          // dword_82F2A184
    extern s32 gs_iRBChangePM;                // dword_82F2A188
    extern s32 gs_iAfterTouchPM;              // dword_82F2A18C
    extern s32 gs_iUpdateTrafficPM;           // dword_82F2A190
    extern s32 gs_iUpdateAggressiveDrivingPM; // dword_82F2A194
    extern s32 gs_iUpdateCrashesPM;           // dword_82F2A198
    extern s32 gs_iUpdatePassBysPM;           // dword_82F2A19C

    // ⭐ ADDITIVE HOIST 2026-08-07 (orchestrator wave), per this header's own rule: the seven
    // GUARDED VPhys sub-monitors stop being Construct-TU statics because VehiclePhysics::Update
    // @0x826412C0 brackets its stages with exactly these ids (the console reads the same seven
    // file-scope slots: dword_82F2A278..dword_82F2A290).
    extern s32 gs_iVPhysUpdatePM;             // dword_82F2A278
    extern s32 gs_iVPhysSwitchAttribsPM;      // dword_82F2A27C
    extern s32 gs_iVPhysUpdateCrashingPM;     // dword_82F2A280
    extern s32 gs_iVPhysUpdateAirRamsPM;      // dword_82F2A284
    extern s32 gs_iVPhysUpdateSpinPM;         // dword_82F2A288
    extern s32 gs_iVPhysUpdateDrivingPM;      // dword_82F2A28C
    extern s32 gs_iVPhysUpdateLVPM;           // dword_82F2A290

    // ⭐ ADDITIVE HOIST 2026-08-10 (ground wave), per this header's own rule: two of the traction
    // sub-stage handles gain a second reader because VehicleManager::RunTractionLineTestJobs
    // @0x825B5168 brackets its two stages with exactly these ids (the console reads the same two
    // file-scope slots). The rest of the traction sub-stages stay Construct-TU statics.
    extern s32 gs_iLineTestsBeginPM;          // dword_82F2A168  ("           Begin")
    extern s32 gs_iLineTestsRunStreamPM;      // dword_82F2A16C  ("           RunStream")

    // ⭐ ADDITIVE HOIST 2026-08-11 (lifetime wave), per this header's own rule: the last five
    // traction sub-stage handles gain a second reader now that StartVehicleTractionLineTests
    // @0x82629CE0 and EndVehicleTractionLineTests @0x82633CD8 are real bodies in
    // BrnVehicleManager_TractionLineTests.cpp -- they bracket their stages with exactly these
    // ids (the console reads the same five file-scope slots). With these five, every traction
    // sub-stage handle is now hoisted.
    extern s32 gs_iTractionGetLinesPM;        // dword_82F2A158  (Start: the three Add* legs)
    extern s32 gs_iTractionLineTestsPM;       // dword_82F2A15C  (Start: RunTractionLineTestJobs)
    extern s32 gs_iTractionProcessResultsPM;  // dword_82F2A160  (End: the three harvests)
    extern s32 gs_iLineTestsFinishPM;         // dword_82F2A170  (End: WaitOn the job)
    extern s32 gs_iLineTestsEndPM;            // dword_82F2A174  (End: close the stream)
}
}
