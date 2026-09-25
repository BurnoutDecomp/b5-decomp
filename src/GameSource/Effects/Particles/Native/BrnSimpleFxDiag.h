#ifndef GAMESOURCE_EFFECTS_PARTICLES_NATIVE_BRNSIMPLEFXDIAG_H
#define GAMESOURCE_EFFECTS_PARTICLES_NATIVE_BRNSIMPLEFXDIAG_H

// =============================================================================================
// [DIAG] BRN_SIMPLEFX_DIAG -- NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
//
// The ONE switch every [simplefx] / [racecar-contact] witness line in the effects and particle
// modules answers to (the crash-parity campaign's rule: diagnostics are default-OFF). Armed when
// the variable is set to anything that does not start with '0'; read once per process.
// FX-CRASHVFX 2026-09-24 (reviewer G item 6): the load-time and first-draw one-shot lines used to
// print unconditionally; they now wait for this like the per-frame ladder always did.
// =============================================================================================

#include <cstdlib>   // std::getenv

namespace BrnParticle
{
namespace Native
{
    inline bool SimpleFxDiagArmed()
    {
        static const bool sbArmed = []()
        {
            const char* const lpcValue = std::getenv("BRN_SIMPLEFX_DIAG");
            return lpcValue != 0 && lpcValue[0] != 0 && lpcValue[0] != '0';
        }();
        return sbArmed;
    }
}
}

#endif
