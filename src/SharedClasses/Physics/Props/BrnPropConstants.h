#pragma once

// ============================================================================
// b5-decomp/src/SharedClasses/Physics/Props/BrnPropConstants.h
//
// ⭐ NEW 2026-08-18 (wave Q round 2, world-side prop IO-buffer pass).
//
// The DWARF-canonical home for the BrnPhysics::Props shared constants
// (references/DecFIGS/dwarfdump/SharedClasses/Physics/Props/BrnPropConstants.h). Several
// committed headers already carry constants marked "MOVE to BrnPropConstants.h when that
// file lands" -- BrnPropInputInterface.h:52 (KU_MAX_PHYSICAL_PROPS) and :61
// (KU_MAX_PHYSICAL_PROP_PARTS) are the two live squatters.
//
// ⚠️ THIS IS A MINIMAL SLICE, NOT A BULK DWARF IMPORT. The DWARF file declares ~40
// constants; per AGENTS.md ("DWARF supplies names/types; the X360 ledger decides what
// exists") only the one this pass MEASURED in the X360 image is homed here. Grow it
// additively, one measured constant at a time. In particular the squatters above are
// deliberately NOT moved here yet: relocating them is a cross-header change with its own
// blast radius (every consumer of BrnPropInputInterface.h), and that is the owner of those
// headers' call, not this pass's.
// ============================================================================

#include "types.hpp"

namespace BrnPhysics
{
namespace Props
{
    // DWARF BrnPropConstants.h:176 -- `const VecFloat KVF_PROP_FLOOR;`
    //
    // The world-floor Y a prop must stay above. Three consumers read it, and ⚠️ THEY DO NOT
    // ALL SPELL THE SAME PREDICATE -- do NOT "normalise" them. Each line below was re-read
    // instruction-by-instruction from the X360 asm on 2026-08-18 (round-3 fix pass); the
    // earlier blanket claim that all three used `vcmpgtfp.` with the operands in the same
    // order was WRONG for two of the three:
    //
    //   * PropEntityModule::ProcessPotentialContactWithPart -- floor loaded @0x822EEE48,
    //     compared @0x822EEE50 `vcmpgefp. v0, v0, v13` with v0 = splat(KVF_PROP_FLOOR) and
    //     v13 = vspltw(pos,1) = Y, then `mfocrf r11,2 ; extrwi r11,r11,1,24 ; bne` (CR6 bit 0,
    //     "all lanes true") branching to loc_822EEF98 == early return. Predicate:
    //         KVF_PROP_FLOOR >= pos.y   ->  the prop is at/below the floor, bail out.
    //     ORDERED >=, so NaN takes neither the early-out nor a negated `<`.
    //
    //   * PropEntityModule::ChangePropState -- floor loaded @0x822EF5E8, compared
    //     @0x822EF5F4 `vcmpgtfp. v0, v13, v0` (OPERANDS REVERSED: v13 = Y is the LEFT
    //     operand), CR6 bit 0 `bne -> loc_822EF628` skipping the assert body. Predicate:
    //         pos.y > KVF_PROP_FLOOR    ->  above the floor, i.e. the ASSERTED condition.
    //     Corroborated by the console's own baked assert string at 0x8201DFAC/0x822EF618:
    //     "lpProp->GetWorldTransform().Pos().Y() > ..." (line 0x621).
    //
    //   * PropZoneManager::UpdateInstance -- floor loaded @0x822F0B90, compared @0x822F0B9C
    //     `vcmpgtfp. v0, v0, v13` (v0 = floor on the LEFT). Predicate:
    //         KVF_PROP_FLOOR > pos.y    ->  below the floor.
    //     This is the one the old comment described, and BrnPropZoneManager.cpp:1405's
    //     `(KVF_PROP_FLOOR > lfPositionY)` is correct as committed.
    //
    // ⚠️ THE >= SITE AND THE > SITES DIFFER AT EXACTLY pos.y == -1000.0f. A prop sitting on
    // the floor plane is "at/below" for ProcessPotentialContactWithPart and "not above" for
    // ChangePropState's assert, but is NOT "below" for UpdateInstance. Rewriting any of the
    // three into the other's form is a behaviour change at the boundary, not a cleanup.
    // (All three are ORDERED VMX compares, so a NaN Y is false at every site.)
    //
    // VALUE MEASURED (headless idat over IDA Files/BURNOUT_X360_ARTIST.XEX.i64, this pass;
    // independently re-measured by two round-1 verifiers before that):
    //   * unk_82FAD840 is SIXTEEN ZERO BYTES in the image -- it is .bss, which is why a
    //     rodata read of it yields nothing and why an earlier comment concluded "the exact
    //     float is NOT in .ida-exports". That conclusion was wrong about RECOVERABILITY.
    //   * It is filled by the unnamed dynamic initialiser at 0x82C4B478:
    //         lis  r11, flt_8200D4F8@ha
    //         lfs  f0,  flt_8200D4F8@l(r11)
    //         stfs f0, -0x10(r1) ; lvlx v0 ; vspltw v0,v0,0 ; stvx128 v0 -> unk_82FAD840
    //   * flt_8200D4F8 is the big-endian word 0xC47A0000 == -1000.0f.
    // So all four lanes hold -1000.0f.
    //
    // ⚠️ TYPE DEVIATION, deliberate and marked: the DWARF type is VecFloat (the 16-byte
    // broadcast lane), because the console keeps it splatted so `vcmpgtfp.` can consume it
    // directly. Every consumer uses it as a SCALAR compared against one lane of a position,
    // and all four lanes are identical by construction, so the host models it as f32 while
    // keeping the DWARF's KVF_ name so the two are greppable as the same entity. (Modelling
    // it as a real VecFloat would need a runtime initialiser here for a value that is a
    // compile-time constant on the host -- strictly worse.)
    static const f32 KVF_PROP_FLOOR = -1000.0f;
}
}
