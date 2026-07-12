// ============================================================================
// CSeqDissolveDetector.h  --  sequence dissolve (cross-fade) detector
//
//   Embedded BY VALUE inside CArrayLookahead at +0x24 (see CArrayLookahead.h).
//   This header is a LAYOUT-ONLY reconstruction: it provides the byte-exact
//   storage CArrayLookahead needs to contain the detector and to seed
//   `miMaxDissolveLength` in CArrayLookahead::Initialize.
//
//   Its engine methods live in a separate, not-yet-reconstructed TU. The X360
//   call sites (in CArrayLookahead::DetectDissolve @0x82A05648 and
//   CArrayLookahead::DetectEventInArray @0x82A05710) show these shapes:
//       CSeqDissolveDetector::Reset(this)                          -> this
//       CSeqDissolveDetector::Detect(this, f32,f32,f32,f32)        -> this
//       CSeqDissolveDetector::ComputeLocalFeature(this)            -> int
//   Those two driver functions are therefore BLOCKED until this class's engine
//   TU is recovered; they are intentionally absent from CArrayLookahead.cpp.
//
//   Size 0x88 (136 bytes). The default (all-zero) construction is folded inline
//   into CArrayLookahead's constructor in the X360 asm (every field written 0 /
//   0.0f); reconstructed there as value-initialisation of this member.
//
//   Class keeps its native `C`-prefixed name (third-party-style video helper),
//   matching the sibling CHistogram / CArrayLookahead; no Cgs/Brn namespace.
// ============================================================================
#pragma once

#include <cstddef>  // offsetof
#include "types.hpp"

class CSeqDissolveDetector
{
public:
    // Grounded fields (offsets confirmed from CArrayLookahead's asm):
    s32 miDissolveDetected;    // +0x00  non-zero once a dissolve is flagged
    s32 miDissolveLength;      // +0x04  detected run length (local-feature clamp)
    s32 miMaxDissolveLength;   // +0x08  seeded to 100 by CArrayLookahead::Initialize

    // +0x0C .. +0x87 : detector working state (per-frame feature accumulators,
    // reset floats at +0x0C/+0x10/+0x80 and counters). Not yet individually
    // recovered -- retained as an opaque, zero-initialised block so the
    // containing CArrayLookahead layout stays byte-exact. Named members will be
    // filled in when the CSeqDissolveDetector engine TU is reconstructed.
    u8  maState[0x7C];         // +0x0C

private:
    friend void _CSeqDissolveDetector_AssertLayout();
};

inline void _CSeqDissolveDetector_AssertLayout()
{
    static_assert(offsetof(CSeqDissolveDetector, miDissolveDetected)  == 0x00, "CSeqDissolveDetector::miDissolveDetected");
    static_assert(offsetof(CSeqDissolveDetector, miDissolveLength)    == 0x04, "CSeqDissolveDetector::miDissolveLength");
    static_assert(offsetof(CSeqDissolveDetector, miMaxDissolveLength) == 0x08, "CSeqDissolveDetector::miMaxDissolveLength");
    static_assert(offsetof(CSeqDissolveDetector, maState)             == 0x0C, "CSeqDissolveDetector::maState");
    static_assert(sizeof(CSeqDissolveDetector) == 0x88, "CSeqDissolveDetector size");
}
