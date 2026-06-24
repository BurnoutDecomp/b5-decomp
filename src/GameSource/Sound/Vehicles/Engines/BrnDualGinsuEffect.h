#ifndef BRN_SOUND_VEHICLES_ENGINES_DUAL_GINSU_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_DUAL_GINSU_EFFECT_H

#include "types.hpp"
#include "SharedClasses/Sound/Engines/BrnSoundLoopModelData.h"

// =============================================================================
// BrnSound::Vehicles::Engines::DualGinsuEffect::LoopOutputs
//   GameSource/Sound/Vehicles/Engines/BrnDualGinsuEffect.h +
//   GameSource/Sound/Vehicles/Engines/BrnDualGinsuEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match;
// store widths authoritative). LoopOutputs is the per-update output accumulator the
// dual-Ginsu engine effect drives from a loop-model Partial: three f32 outputs
// (indexed by the output-axis enum) carrying the per-frame pitch/gain multipliers.
//
// Update reads a Partial (its three Graphs) and runs the gain/pitch graph evaluators
// against the current RPM / accelerator inputs, multiplying the outputs in place.
// Each Graph is the committed serialized BrnSound::Vehicles::Engines::Graph
// (mpaPoints / mu8NumOfPoints / mi8XAxis / mi8YAxis) reused BY NAME from
// BrnSoundLoopModelData.h — the asm walks it at exactly those offsets (graph stride
// 8, point stride 8).
//
// This TU bodies two ledger functions:
//   LoopOutputs::Update          @ 0x826B3DD8
//   LoopOutputs::UpdateGainGraph @ 0x82699F90
//
// DEFERRED (own TU, declared only for the call): LoopOutputs::UpdatePitchGraph
// @ (external) — Update calls it but it is not part of this TU's recon set.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{
namespace DualGinsuEffect
{

// BrnDualGinsuEffect.cpp axis enums (asm-attested via the assert == comparisons).
// The X-axis (input) selector for a Graph.
namespace LoopInputs
{
    enum EAxis
    {
        E_UNKNOWN     = 0, // "mi8XAxis != LoopInputs::E_UNKNOWN"
        E_RPM         = 1, // "mi8XAxis == LoopInputs::E_RPM"
        E_ACCELERATOR = 3, // "mi8XAxis == LoopInputs::E_ACCELERATOR"
    };
}

// The output accumulator: one f32 per output axis. UpdateGainGraph multiplies the
// GAIN slot in place; the pitch evaluator drives the PITCH slot. Indexed by EAxis
// (the asm reads mafOutputs[mi8YAxis] via `4 * mi8YAxis + this`).
struct LoopOutputs
{
    // The Y-axis (output) selector for a Graph; also the index into mafOutputs.
    enum EAxis
    {
        E_UNKNOWN = 0,
        E_GAIN    = 1, // "mi8YAxis == LoopOutputs::E_GAIN" / mafOutputs[+4]
        E_PITCH   = 2, // "mi8YAxis == LoopOutputs::E_PITCH" / mafOutputs[+8]
        E_COUNT   = 3,
    };

    // mafOutputs[0..2], one f32 per output axis (stfs writes at +0/+4/+8).
    f32 mafOutputs[E_COUNT];

    // @ 0x826B3DD8 — reset the outputs to 1.0, validate the Partial's three graphs,
    // then evaluate the gain graphs (graph[1] vs aRpm, graph[2] vs aAccelerator)
    // and the pitch graph (graph[0] vs aRpm). Returns 1 if the first gain graph
    // produced a non-zero gain, else 0 (and leaves the outputs at their reset value).
    s32 Update( f64 aRpm, f64 aAccelerator,
                const BrnSound::Vehicles::Engines::Partial* apPartial );

    // @ 0x82699F90 — evaluate one gain Graph at the clamped input value and multiply
    // the GAIN output in place. Returns true if the interpolated gain is non-zero.
    bool UpdateGainGraph( f64 afInput,
                          const BrnSound::Vehicles::Engines::Graph* apGraph );

    // (external TU) — evaluate the pitch graph and drive the PITCH output. Declared
    // only so Update can call it; defined in its own home.
    void UpdatePitchGraph( f64 afInput, const BrnSound::Vehicles::Engines::Graph* apGraph );
};

} // namespace DualGinsuEffect
} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_DUAL_GINSU_EFFECT_H
