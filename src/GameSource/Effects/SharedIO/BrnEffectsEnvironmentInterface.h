#pragma once

// ============================================================================
// b5-decomp/src/GameSource/Effects/SharedIO/BrnEffectsEnvironmentInterface.h
//
// Canonical DWARF home of BrnEffects::EffectsEnvironmentInterface
// (DWARF: BrnEffectsEnvironmentInterface.h:40) -- the per-frame environment
// payload (wind velocity) the world publishes for the effects system.
//
// The world Update OUTPUT buffer embeds it BY VALUE
// (BrnWorldIO::UpdateOutputBuffer::mEffectsEnvironmentInterface, X360 +169072,
// span 169072..169088 == 16 bytes == one 16-aligned Vector2) and only returns
// &member from its accessors (GetEffectsEnvironmentInterface const @ 0x823B64A0 /
// mutable @ 0x827BCC30).
//
// Method declarations follow the DWARF list; bodies belong to this type's own TU.

#include "BrnCommonTypes.h"   // Vector2 (rw::math::vpu::Vector2, 16-byte aligned)

namespace BrnEffects
{
    // DWARF: BrnEffectsEnvironmentInterface.h:40
    struct EffectsEnvironmentInterface
    {
        // ---- methods (DWARF :43-:53; bodies belong to this type's own TU) ----
        void Clear();                                        // :43
        void SetWindVelocity(Vector2);                       // :48
        const rw::math::vpu::Vector2& GetWindVelocity() const; // :53

    private:
        // ---- FROZEN LAYOUT (DWARF :60) ----
        Vector2 mWindVelocity;   // :60
    };
}
