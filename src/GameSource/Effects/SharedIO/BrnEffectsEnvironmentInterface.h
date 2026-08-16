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

    // ---- SetWindVelocity, DEFINED (post-fx step 9, group envblend) ----------------
    // The only producer in the image is EnvironmentSettings::EnvironmentManager::Update
    // @0x827D6060, and the X360 compiler INLINED the setter there: right after
    // `bl BrnWorldIO__UpdateOutputBuffer__GetEffectsEnvironmentIn` it does a bare
    // `stvx128 v0, r0, r3` (0x827D6354 on the live arm, 0x827D6398 on the paused arm) --
    // ONE 16-byte store at offset 0 of the returned interface, which is exactly
    // `mWindVelocity = lWindVelocity`. Spelled out here rather than left as an unresolved
    // external (`grep -rn "SetWindVelocity" b5-decomp/src` returned ONLY the declaration
    // above, and this type has no .cpp) so the producer reaches the member BY NAME.
    inline void EffectsEnvironmentInterface::SetWindVelocity(Vector2 lWindVelocity)
    {
        mWindVelocity = lWindVelocity;
    }
}
