#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout reconstructed
// by PlayerVehicleControls' own TU (DWARF home GameSource/World/EntityModules/
// RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h). Size 60 (BYTE-VERIFIED from
// the full DWARF member list: 13 float32_t + 8 bool, natural alignment).
//
// Unlike the opaque interface payloads in this group, PlayerVehicleControls is a small
// concrete POD whose complete member list is fully captured by the DecFIGS DWARF, so it is
// reconstructed exactly (not a reserved blob). Namespace is BrnWorld (NOT
// BrnWorld::RaceCarEntityModuleIO and NOT BrnPhysics::Vehicle): the DWARF declares
// `struct BrnWorld::PlayerVehicleControls` and BrnWorld::PlayerVehicleControls::Construct().
// No SIMD/Vector members -> natural alignment.
#include "types.hpp"   // f32

namespace BrnWorld
{
    struct PlayerVehicleControls
    {
        f32  mfXAxis1;
        f32  mfXAxis0;
        f32  mfYAxis1;
        f32  mfYAxis0;
        f32  mfXSensor;
        f32  mfYSensor;
        f32  mfZSensor;
        f32  mfGSensor;
        f32  mfAcceleration;
        f32  mfBraking;
        f32  mfHandBrake;
        f32  mfSteering;
        f32  mfSpin;
        bool mbHorn;
        bool mbChangeView;
        bool mbStart;
        bool mbReset;
        bool mbToggle;
        bool mbBoost;
        bool mbIsWheel;
        bool mbBoostBounce;

        void Construct();   // BrnPlayerVehicleControls.h:63 (declare-only; body in own TU)
    };
}
