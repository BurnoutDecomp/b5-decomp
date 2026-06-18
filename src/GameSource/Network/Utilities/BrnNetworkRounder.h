#pragma once

// ============================================================================
// b5-decomp/src/GameSource/Network/Utilities/BrnNetworkRounder.h
// ============================================================================
// MINIMAL declare-only home for the BrnNetwork::NetworkRounder rounding helpers.
//
// WHY THIS HEADER EXISTS
//   The committed TU GameSource/Network/Utilities/BrnNetworkRounder.cpp already
//   reconstructed RoundFloatToAccuracy and modelled NetworkRounder as a NAMESPACE
//   of free helpers (the X360 build inlines these into their callers; there is no
//   instance state). That .cpp had NO header, so callers could not name the helpers.
//
//   ScoringSystem::StopModeTimer (X360 0x8231F590) calls two of these helpers:
//       BrnNetwork::NetworkRounder::RoundTimeToNetworkAccuracy(&mTimeRemaining)  // a1+0x10
//       BrnNetwork::NetworkRounder::RoundDistanceToNetworkAccuracy(&fDistance)   // v11+0x48
//   and RegisterFinishForCar (0x82340...) also calls RoundTimeToNetworkAccuracy.
//   This header gives those callers a declaration to compile against. The bodies
//   live in BrnNetworkRounder.cpp (its own TU); the gate is `cl /c`, no link.
//
// SHAPE
//   NetworkRounder is a NAMESPACE (matches the committed .cpp shape -- do NOT
//   re-shape it into a struct/class). Signatures are taken from the X360 asm /
//   DecFIGS dwarfdump (BrnNetworkRounder.h:46-65):
//     RoundFloatToAccuracy        -- committed verbatim from the .cpp
//                                    (float* result, double a2) -> float*
//     RoundDistanceToNetworkAccuracy(float*) -> float*   (X360 0x82591EB0; snaps a
//                                    distance to KF_NETWORK_DISTANCE_ACCURACY = 0.01m,
//                                    scale 100.0, half-up)
//     RoundTimeToNetworkAccuracy(CgsSystem::Time*)        (X360 0x82591F20; rounds the
//                                    fraction to KF_NETWORK_TIME_ACCURACY, scale 300.0,
//                                    via CgsSystem::Time::SetFraction). DWARF return is
//                                    void; the asm tail-returns the SetFraction result.
//   DECLARE-ONLY here (except RoundFloatToAccuracy, whose body is the sibling .cpp).
//   NOT byte-verified; no instance layout (free functions).

#include "types.hpp"   // f32

namespace CgsSystem
{
    class Time;        // RoundTimeToNetworkAccuracy operates on a CgsSystem::Time* (pointer only)
}

namespace BrnNetwork
{
    namespace NetworkRounder
    {
        // Committed in BrnNetworkRounder.cpp -- signature reproduced verbatim so callers bind.
        // (Snaps *result to the nearest multiple of a2*2, rounding halves up.)
        float* RoundFloatToAccuracy(float* result, double a2);

        // Declare-only (body in BrnNetworkRounder.cpp). X360 0x82591EB0.
        float* RoundDistanceToNetworkAccuracy(float* lpfDistance);

        // Declare-only (body in BrnNetworkRounder.cpp). X360 0x82591F20.
        void   RoundTimeToNetworkAccuracy(CgsSystem::Time* lpTime);
    }
}
