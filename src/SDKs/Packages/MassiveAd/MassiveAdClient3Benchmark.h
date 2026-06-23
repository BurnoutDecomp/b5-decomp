#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveBenchmark -- a tiny running-totals accumulator the
// MassiveAd client uses to benchmark a zone session (vendor middleware). It
// derives the polymorphic CMassiveBaseObject (MassiveAdClient3.h) like every
// other MassiveAd client object and adds three running counters.
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses:
//     MassiveAdClient3::CMassiveBenchmark::CMassiveBenchmark   @ 0x82BD38E8
//     MassiveAdClient3::CMassiveBenchmark::AddData             @ 0x82BD38A8
//     MassiveAdClient3::CMassiveBenchmark::Reset               @ 0x82BD38D0
//     MassiveAdClient3::CMassiveBenchmark::`vector deleting destructor' @ 0x82BD3940
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CMassiveBenchmark class / its methods) are PRESERVED
// VERBATIM -- external middleware API, not project-owned code.
//
// Layout (members BY NAME over the 0x14-byte CMassiveBaseObject base; the X360
// stores at +0x14/+0x18/+0x1C are reproduced by member name, not asserted on the
// 64-bit host where the base is wider):
//     +0x14  mnTotalA  (a1[5]; AddData adds its first sample arg here)
//     +0x18  mnTotalB  (a1[6]; AddData adds its second sample arg here)
//     +0x1C  mnCount   (a1[7]; AddData increments by one per call)
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

namespace MassiveAdClient3
{

class CMassiveBenchmark : public CMassiveBaseObject
{
public:
    // @ 0x82BD38E8. Chains the base ctor ("CMassiveBenchMark"), installs this
    // class's vftable (off_821859F4 -- modelled by the virtual dtor), and zeroes
    // all three counters.
    CMassiveBenchmark();

    // @ 0x82BD3940 (vector deleting destructor). Rewrites the vftable and chains
    // the base dtor; the deleting thunk frees via the base operator delete when
    // its low bit is set.
    virtual ~CMassiveBenchmark();

    // @ 0x82BD38A8. Accumulates one sample: mnTotalA += nSampleA, mnTotalB +=
    // nSampleB, ++mnCount. Returns this (the X360 returns r3).
    CMassiveBenchmark* AddData(int nSampleA, int nSampleB);

    // @ 0x82BD38D0. Zeroes all three counters. Returns this.
    CMassiveBenchmark* Reset();

private:
    int mnTotalA;  // +0x14
    int mnTotalB;  // +0x18
    int mnCount;   // +0x1C
};

} // namespace MassiveAdClient3
