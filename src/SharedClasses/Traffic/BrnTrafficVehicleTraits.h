#pragma once

// =============================================================================
// BrnTraffic::VehicleTraits -- the per-personality driving traits a traffic vehicle
// type is assigned. TrafficData::mpaVehicleTraits is an array of these, and
// TrafficData::GetVehicleTraitsForVehicleType @0x82705DF0 hands one out by the type's
// muTraitsId.
//
// Layout, names and accessibility are DWARF-authoritative
// (dwarfdump/.../BrnTrafficVehicleTraits.h @ :45): the seven data members are private
// behind the two inline getters (:49 / :50), plus the tuning constant at :86.
//
// Stride 16 is X360-attested: GetVehicleTraitsForVehicleType indexes the traits table
// with `slwi r11,r24,4`. The DWARF member list sums to 13, so the record carries three
// bytes of trailing pad. Pointer-free, so console and host footprints agree.
// =============================================================================

#include "types.hpp"

namespace BrnTraffic
{
    // BrnTrafficVehicleTraits.h:45
    struct VehicleTraits
    {
        // :49 / :50 -- console-inlined accessors (no standalone ARTIST symbol).
        f32 GetSwervingModifier() const { return mfSwervingAmountModifier; }
        f32 GetAccelerationModifier() const { return mfAcceleration; }

        // :55 DECLARED-ONLY -- no ARTIST symbol, no rw::EndianSwap in this tree, and
        // the shipped PC payload is already little-endian (BrnTrafficStaticTraffic.cpp).
        void EndianSwap();

        // Never called. A member function so offsetof reaches the private members.
        static void _AssertLayout();

    private:
        f32 mfSwervingAmountModifier; // :74  +0x00
        f32 mfAcceleration;           // :75  +0x04
        u8  muCuttingUpChance;        // :77  +0x08
        u8  muTailgatingChance;       // :78  +0x09
        u8  muPatience;               // :80  +0x0A
        u8  muTantrumAttackCumProb;   // :81  +0x0B
        u8  muTantrumStopCumProb;     // :82  +0x0C  (+3 trailing pad -> 16)

        // :86 -- how many altercations a driver tolerates before throwing a tantrum.
        // A compile-time constant, not a stored field: it does not occupy record space.
        static const s32 KI_MAX_ALTERCATIONS_BEFORE_TANTRUM = 5;
    };

    static_assert(sizeof(VehicleTraits) == 16,
                  "VehicleTraits stride (GetVehicleTraitsForVehicleType @0x82705DF0 uses slwi 4)");
}
