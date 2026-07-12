// ============================================================================
// CSeqDissolveDetector.h  --  sequence dissolve (cross-fade) detector
//
//   Embedded BY VALUE inside CArrayLookahead at +0x24 (see CArrayLookahead.h).
//   This header provides the byte-exact storage CArrayLookahead needs to contain
//   the detector, to seed `miMaxDissolveLength` in CArrayLookahead::Initialize,
//   and to drive the detector from CArrayLookahead::DetectDissolve /
//   DetectEventInArray.
//
//   The three engine methods (Reset / Detect / ComputeLocalFeature) are DECLARED
//   here from their X360 call sites; their BODIES live in a separate,
//   not-yet-reconstructed engine TU. The call sites (in
//   CArrayLookahead::DetectDissolve @0x82A05648 and
//   CArrayLookahead::DetectEventInArray @0x82A05710) attest these shapes:
//       CSeqDissolveDetector::Reset(this)                   -> this
//       CSeqDissolveDetector::Detect(this, f32,f32,f32,f32) -> this
//       CSeqDissolveDetector::ComputeLocalFeature(this)     -> this
//   (Reset/Detect are tail-called; ComputeLocalFeature's return is reused as the
//   `this` base for the post-detection reset in DetectEventInArray -- i.e. it
//   returns a pointer, not the `int` an earlier read of the pseudocode implied.)
//   Because CArrayLookahead compiles under the per-TU `cl /c` gate, these
//   declarations are all its two driver functions need; the bodies are the
//   engine TU's responsibility.
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
    // --- engine methods (declared from CArrayLookahead's call sites; bodies in
    //     the separate CSeqDissolveDetector engine TU) -----------------------
    CSeqDissolveDetector* Reset();                                      // -> this
    CSeqDissolveDetector* Detect(f32 lfPrevVarDelta, f32 lfCurVarDelta, // -> this
                                 f32 lfPrevMeanDelta, f32 lfCurMeanDelta);
    CSeqDissolveDetector* ComputeLocalFeature();                        // -> this

    // Grounded fields (offsets confirmed from CArrayLookahead's asm):
    s32 miDissolveDetected;    // +0x00  non-zero once a dissolve is flagged
    s32 miDissolveLength;      // +0x04  detected run length (local-feature clamp)
    s32 miMaxDissolveLength;   // +0x08  seeded to 100 by CArrayLookahead::Initialize

    // +0x0C .. +0x27 : detector working state cleared by the post-detection
    // reset inlined in CArrayLookahead::DetectEventInArray @0x82A05A04 (which
    // zeroes exactly +0x00/+0x04/+0x0C/+0x10/+0x14/+0x18/+0x1C/+0x20/+0x24 while
    // deliberately preserving +0x08). Each field's offset and store width
    // (stfs => f32, stw => s32) is asm-attested; the precise per-field semantics
    // await the engine TU, so they are named by offset (not by an inferred role).
    f32 mfReset0C;             // +0x0C  (stfs 0.0f)
    f32 mfReset10;             // +0x10  (stfs 0.0f)
    s32 miReset14;             // +0x14  (stw  0)
    s32 miReset18;             // +0x18  (stw  0)
    s32 miReset1C;             // +0x1C  (stw  0)
    s32 miReset20;             // +0x20  (stw  0)
    s32 miReset24;             // +0x24  (stw  0)

    // +0x28 .. +0x87 : remaining detector working state (per-frame feature
    // accumulators incl. the reset float at +0x80 zeroed in the ctor). Retained
    // as an opaque, zero-initialised block so the containing CArrayLookahead
    // layout stays byte-exact; named members fill in with the engine TU.
    u8  maState[0x88 - 0x28];  // +0x28

private:
    friend void _CSeqDissolveDetector_AssertLayout();
};

inline void _CSeqDissolveDetector_AssertLayout()
{
    static_assert(offsetof(CSeqDissolveDetector, miDissolveDetected)  == 0x00, "CSeqDissolveDetector::miDissolveDetected");
    static_assert(offsetof(CSeqDissolveDetector, miDissolveLength)    == 0x04, "CSeqDissolveDetector::miDissolveLength");
    static_assert(offsetof(CSeqDissolveDetector, miMaxDissolveLength) == 0x08, "CSeqDissolveDetector::miMaxDissolveLength");
    static_assert(offsetof(CSeqDissolveDetector, mfReset0C)           == 0x0C, "CSeqDissolveDetector::mfReset0C");
    static_assert(offsetof(CSeqDissolveDetector, mfReset10)           == 0x10, "CSeqDissolveDetector::mfReset10");
    static_assert(offsetof(CSeqDissolveDetector, miReset14)           == 0x14, "CSeqDissolveDetector::miReset14");
    static_assert(offsetof(CSeqDissolveDetector, miReset18)           == 0x18, "CSeqDissolveDetector::miReset18");
    static_assert(offsetof(CSeqDissolveDetector, miReset1C)           == 0x1C, "CSeqDissolveDetector::miReset1C");
    static_assert(offsetof(CSeqDissolveDetector, miReset20)           == 0x20, "CSeqDissolveDetector::miReset20");
    static_assert(offsetof(CSeqDissolveDetector, miReset24)           == 0x24, "CSeqDissolveDetector::miReset24");
    static_assert(offsetof(CSeqDissolveDetector, maState)             == 0x28, "CSeqDissolveDetector::maState");
    static_assert(sizeof(CSeqDissolveDetector) == 0x88, "CSeqDissolveDetector size");
}
