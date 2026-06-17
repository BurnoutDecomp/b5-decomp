#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout
// reconstructed by RaceCarAIInterface / AIRaceCarInterface's own TU (DWARF home
// GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h). Size 256 (NOMINAL --
// not byte-verified, grown by own TU).
//
// Both types are large opaque sub-interface payloads embedded BY VALUE in the
// RaceCarEntityModule IO buffers (RaceCarAIInterface -> OutputBuffer_PreScene
// :317; AIRaceCarInterface -> InputBuffer_PostPhysics :542). The IO header only
// ever returns &member, so a complete sized blob is sufficient here; the real
// member layout (DWARF BrnRaceCarAIInterfaces.h:193-213 / :269-277, incl.
// Matrix44Affine[8], Vector3[8], a VariableEventQueue<16384,16>, BitArray<8>/<35>,
// Vector2) is reconstructed by these types' own ledger TU.
//
// alignas(16): the DWARF shows Matrix44Affine / Vector3 / Vector2 SIMD members
// (RaceCarAIInterface) plus a VariableEventQueue, and Vector3[35] (AIRaceCarInterface).

#include "types.hpp"   // u8 (reserved blob)

namespace BrnAI
{
    namespace AIModuleIO
    {
        // DWARF: BrnRaceCarAIInterfaces.h:56 (struct BrnAI::AIModuleIO::RaceCarAIInterface).
        // Per-active-car AI snapshot interface (matrices/velocities/speeds/section indices,
        // ten BitArray<8> state flags, a VariableEventQueue<16384,16> management queue, and
        // the player-car data block). Embedded by value in OutputBuffer_PreScene.
        struct alignas(16) RaceCarAIInterface
        {
            unsigned char maReserved[256]; // NOMINAL -- full layout grown by own TU
        };

        // DWARF: BrnRaceCarAIInterfaces.h:229 (struct BrnAI::AIModuleIO::AIRaceCarInterface).
        // Inactive/all race-car data interface (Vector3[35] positions + ats, BitArray<35>
        // set/pass-through flags, player route node Vector2s). Distinct from RaceCarAIInterface.
        // Embedded by value in InputBuffer_PostPhysics.
        struct alignas(16) AIRaceCarInterface
        {
            unsigned char maReserved[256]; // NOMINAL -- full layout grown by own TU
        };
    }
}
