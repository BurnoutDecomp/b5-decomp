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
}
}
