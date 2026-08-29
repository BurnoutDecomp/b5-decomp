// ===================================================================================
// BrnGui::CrashNavMapSoundData -- the crash-nav map's move/scroll audio debouncer.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCrashNavMapSoundData.cpp
//
// Three bodies, all declared in the owning header
// GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h (DWARF BrnCrashNavMap.h:61/:70/:80):
//   CrashNavMapSoundData::Construct()
//   CrashNavMapSoundData::Prepare(Vector3)
//   CrashNavMapSoundData::Update(Vector3, bool)
//
// WHY A SEPARATE TU. The struct is DWARF-attested as its own type but NONE of the three
// methods has an X360 symbol: the compiler folded all three into their call sites. The
// class they hang off (CrashNavMap) has no base .cpp of its own yet, and its eight
// `_wJ_0*` partfiles are a mount-as-a-set group with a cross-file linkage contract
// (mauComponentHashIds). Landing these three in a small TU beside them keeps that set
// untouched and leaves whoever writes BrnCrashNavMap.cpp free of a merge conflict. This
// is not a fork -- the type and its declarations stay in the one committed home header.
//
// PROVENANCE. Every store below is read out of the ONE X360 body that inlines all three,
// CrashNavMap::UpdateSoundEvents @0x824CB8B0, plus CrashNavMap::OnEnter @0x824CB158 for
// the constructor. mSoundData sits at X360 +24880 == 0x6130, so in that body:
//   +24880 (0x6130) = mPrevCurPos        (16-byte lane, `lvx128 v13, r30, r8(24880)`)
//   +24896 (0x6140) = mbPrevIsScrolling  (word-compared/word-stored)
// The X360 offsets are DOCUMENTATION; the host access is by name.
//
//   Construct()  -- OnEnter zeroes the quad at +0x6130 and the byte at +0x6140.
//   Prepare(p)   -- `vsubfp128 v13, v127(p), v13(mPrevCurPos)` then `vmsum3fp128` (a
//                   THREE-lane dot, not two) and the usual vrsqrtefp + 2x Newton-Raphson
//                   + zero-length `vsel`, compared `vcmpgtfp.` against 0.3f. Returns the
//                   all-lanes-true bit, i.e. "the cursor has moved more than 0.3 world
//                   units since the last latch" -- the map counts as scrolling.
//   Update(p, b) -- the tail latch: `stvx128 v127, r0, r11(+0x6130)` then
//                   `stb/stw` the flag at +0x6140. It runs on EVERY path through
//                   UpdateSoundEvents, which is what makes both audio edges detectable.
//
// CONSTANT. 0.3f is an open-coded literal in the original (the DWARF names no constant
// for it), materialised at the call site as `v18[0] = 0.29999998` and splatted.
// [FLAG consumer-named] -- the value is measured, the name below is ours.
//
// The X360 spells the length as vrsqrtefp + two Newton-Raphson steps + a zero-length
// vsel; rw::math::vpu::Magnitude is the same three-lane sqrt-of-dot and returns the same
// value including the zero case -- the sanctioned scalar de-optimisation, as in the
// committed BrnCrashNavMap_wJ_07.cpp.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::Magnitude / operator-

namespace BrnGui
{
    namespace
    {
        // The distance the cursor must travel between two frames for the map to count as
        // scrolling. X360: the 0.29999998f splatted into the vcmpgtfp inside
        // UpdateSoundEvents @0x824CB8B0. [FLAG consumer-named]
        const f32 KF_MAP_SCROLL_MOVE_THRESHOLD = 0.29999998f;
    }

    // ---------------------------------------------------- Construct (BrnCrashNavMap.h:61)
    // Inlined by the X360 into CrashNavMap::OnEnter @0x824CB158 as a zero quad at
    // this+0x6130 followed by a zero at this+0x6140.
    void CrashNavMapSoundData::Construct()
    {
        mPrevCurPos.SetZero();
        mbPrevIsScrolling = false;
    }

    // ------------------------------------------------------ Prepare (BrnCrashNavMap.h:70)
    // "Has the cursor moved far enough since the last latch to count as scrolling?"
    // Inlined into UpdateSoundEvents @0x824CB8B0's E_CURSORMODE_PANNING arm.
    bool CrashNavMapSoundData::Prepare(Vector3 lv3CursorPos)
    {
        // vmsum3fp128 is a THREE-lane dot product, so the map's height axis participates.
        const f32 lfDistanceMoved =
            rw::math::vpu::Magnitude(lv3CursorPos - mPrevCurPos);

        return lfDistanceMoved > KF_MAP_SCROLL_MOVE_THRESHOLD;
    }

    // ------------------------------------------------------- Update (BrnCrashNavMap.h:80)
    // The tail latch, run unconditionally at the end of UpdateSoundEvents: it is what
    // turns "is scrolling now" into an edge next frame.
    void CrashNavMapSoundData::Update(Vector3 lv3CursorPos, bool lbIsScrolling)
    {
        mPrevCurPos       = lv3CursorPos;    // stvx128 v127, r0, r11 (this+0x6130)
        mbPrevIsScrolling = lbIsScrolling;   // *(this + 24896) = v9
    }
}
