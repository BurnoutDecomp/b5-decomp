#include "SDKs/Packages/MassiveAd/MassiveAdClient3Benchmark.h"

// ===========================================================================
// MassiveAdClient3::CMassiveBenchmark -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// SHAPE and BODIES both from the X360 asm (no leak / DecFIGS). Stores reproduced
// member-for-member; see MassiveAdClient3Benchmark.h for the layout. The class
// installs its own vftable over the CMassiveBaseObject slot -- modelled by the
// virtual destructor, so no vftable store is written by hand.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveBenchmark::CMassiveBenchmark @ 0x82BD38E8
//
//   bl CMassiveBaseObject::CMassiveBaseObject(this, "CMassiveBenchMark")
//   stw off_821859F4, 0(this)                 -> install vftable (virtual dtor)
//   li r11, 0 ; stw r11, 0x1C ; stw r11, 0x18 ; stw r11, 0x14
//                                             -> mnCount = mnTotalB = mnTotalA = 0
//
// Note the base-object name string is "CMassiveBenchMark" (capital M in Mark) --
// reproduced verbatim from the X360 rodata.
// ---------------------------------------------------------------------------
CMassiveBenchmark::CMassiveBenchmark()
    : CMassiveBaseObject("CMassiveBenchMark")
    , mnTotalA(0)  // stw 0, 0x14
    , mnTotalB(0)  // stw 0, 0x18
    , mnCount(0)   // stw 0, 0x1C
{
}

// ---------------------------------------------------------------------------
// CMassiveBenchmark::~CMassiveBenchmark @ 0x82BD3940 (vector deleting destructor)
//
// No own teardown -- the X360 vector deleting destructor chains
// ~CMassiveBaseObject and conditionally frees through the base operator delete.
// ---------------------------------------------------------------------------
CMassiveBenchmark::~CMassiveBenchmark()
{
}

// ---------------------------------------------------------------------------
// CMassiveBenchmark::AddData @ 0x82BD38A8
//
//   r9  = 0x14(r3) + r4 ; stw r9, 0x14(r3)    -> mnTotalA += nSampleA
//   r10 = 0x18(r3) + r5 ; stw r10, 0x18(r3)   -> mnTotalB += nSampleB
//   r11 = 0x1C(r3) + 1  ; stw r11, 0x1C(r3)   -> ++mnCount
//   return r3 (this)
// ---------------------------------------------------------------------------
CMassiveBenchmark* CMassiveBenchmark::AddData(int nSampleA, int nSampleB)
{
    mnTotalA += nSampleA;
    mnTotalB += nSampleB;
    mnCount  += 1;
    return this;
}

// ---------------------------------------------------------------------------
// CMassiveBenchmark::Reset @ 0x82BD38D0
//
//   li r11, 0 ; stw r11, 0x1C ; stw r11, 0x18 ; stw r11, 0x14
//   return r3 (this)
// ---------------------------------------------------------------------------
CMassiveBenchmark* CMassiveBenchmark::Reset()
{
    mnTotalA = 0;
    mnTotalB = 0;
    mnCount  = 0;
    return this;
}

} // namespace MassiveAdClient3
