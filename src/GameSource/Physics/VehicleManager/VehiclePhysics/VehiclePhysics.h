#pragma once

// BrnPhysics::Vehicle::VehiclePhysics -- MINIMAL reconstruction.
//
// The full VehiclePhysics class (a SimpleVehiclePhysics subclass with engine, drift,
// boost, slam/shunt/airram/spin state and several hundred methods) is a separate future
// TU. Here we reconstruct ONLY the two nested effect structs that RaceCarState embeds by
// value: SlamEffect (offset 880) and ShuntEffect (offset 928). Both are nested in
// `struct VehiclePhysics` per references/DecFIGS/dwarfdump/.../VehiclePhysics.h, giving the
// fully-qualified names BrnPhysics::Vehicle::VehiclePhysics::SlamEffect / ::ShuntEffect that
// RaceCarState's member declarations use.
//
// ShuntEffect is its own ledger TU (ShuntEffect::IsActive @0x8236AFC0); SlamEffect methods
// are likewise owned elsewhere. Every method on both structs is therefore declared-only so
// those TUs can define the bodies later without an ODR clash. GetDesiredSpeed/GetLife/etc.
// return VecFloat in the SDK; per the project de-SIMD convention they are spelled f32 here.
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus, Vector4
#include "types.hpp"          // f32, s8

namespace BrnPhysics
{
namespace Vehicle
{
    // Minimal VehiclePhysics: only the nested SlamEffect / ShuntEffect are reconstructed.
    struct VehiclePhysics
    {
        // A directional "slam" impulse with decay/steering and a slam-index counter.
        struct SlamEffect
        {
            static const s8 KI8_MAX_NUM_SLAMS = 3;

            Vector3 mForce;
            f32     mfDecay;
            f32     mfSteering;
            f32     mfOriginalSteering;
            f32     mfSlamLife;
            f32     mfTotalSlamTime;
            f32     mfRecoveryTime;
            s8      mi8SlamNumber;

            // Owned by the VehiclePhysics/ShuntEffect ledger TUs -- declare only.
            bool IsActive() const;
            void Clear();
            void StopCurrentSlam();
        };

        // A "shunt" (sustained push toward a desired speed); state packed into two SIMD
        // registers (direction+desired-speed, life+speed-increase-to-quit).
        struct ShuntEffect
        {
            Vector3Plus mDirectionPlusDesiredSpeed;
            Vector4     mv4_Life_SpeedIncreaseToQuit;

            // ShuntEffect is its own ledger TU (IsActive @0x8236AFC0) -- declare only ALL.
            // SDK VecFloat returns are de-SIMD'd to f32 to match the project convention.
            bool IsActive() const;
            void Clear();
            f32  GetDesiredSpeed() const;
            bool ForceSlip() const;
            f32  GetLife() const;
            f32  GetSpeedIncreaseToQuit() const;
            void SetLife(f32);
            void SetSpeedIncreaseToQuit(f32);
        };
    };
}
}
