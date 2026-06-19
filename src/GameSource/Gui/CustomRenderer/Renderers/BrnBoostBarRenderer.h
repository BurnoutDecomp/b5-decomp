#ifndef BRN_BOOST_BAR_RENDERER_H
#define BRN_BOOST_BAR_RENDERER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector3 (rw::math::vpu::Vector3)
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostType.h" // BrnWorld::EBoostType

namespace BrnGui
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//   BoostBarRenderer        @ 0x827DF4F0  (constructor; EXECUTED in the boot trace)
//   GetInnerBoostBarColour  @ 0x824EC750  (debug colour getter)
//   GetOuterBoostBarColour  @ 0x824EC7D0  (debug colour getter)
//
// The real guest object is very large (~53 KB) and is only PARTIALLY recovered here:
// these three functions are the entire in-scope slice, so this class models ONLY the
// members they touch. Every other DWARF member (DeltaInterpolator, Interpolator<f32>,
// ImRendererSet*, the ~12 TextureState pairs, the GuiCache pointer, the real base
// CgsGui::CustomRenderComponentInterface, etc.) is uncommitted and intentionally OMITTED
// — nothing in scope references it.  FLAG: minimal-slice class (see .cpp header comment).
//
// CrashNavIcon-style reconstruction: standalone class (NO base class, NO raw offset
// casts), members declared BY NAME, repeated stride writes grouped into arrays cleared
// by loops, data-segment pointers held as named constants. Exact byte offsets are NOT
// reproduced because the gate compiles for a 64-bit host (pointers widen 4 -> 8 bytes),
// so the offsets in the X360 pseudocode are not load-bearing here.

// One entry of the constructor's billboard-renderer array (DWARF h:788
// `BillboardRenderer mBillboardRenderer[6]`).  FLAG: minimal-slice billboard — only the
// ten leading dwords the ctor zeroes per element are modelled; the full BillboardRenderer
// type is uncommitted (the guest stride was 2067 dwords / element).
struct BillboardRenderer
{
    // Per element the ctor clears two 5-dword runs (guest v2-6..v2-2 and v2..v2+4), each
    // with a duplicate-first-write; 10 distinct dwords total.
    u32 maClearedFields[10];
};

class BoostBarRenderer
{
public:
    BoostBarRenderer();

    // Debug-only colour getters. Return the boost-type-indexed colour by value; assert
    // that meCurrentBoostType is a real boost type (not COUNT / NONE) first. Non-const to
    // match the DWARF signatures (h:843/846) — the bodies only read, so const would be
    // semantically fine, but DWARF is the shape authority for the attested signature.
    Vector3 GetInnerBoostBarColour();
    Vector3 GetOuterBoostBarColour();

private:
    // Leading vtable slot (ctor writes &off_820CF8A0 to *this).  FLAG: vtable pointer
    // modelled as an opaque slot set to a named data-segment constant.
    void* mpVtable;

    // ---- members read by the two getters (DWARF h:617/620/623) ----
    // NOTE: the constructor does NOT initialise these three (the pseudocode begins writing
    // at guest +212, past where the colour arrays / boost type live), so they are declared
    // for the getters but deliberately left out of the ctor body — faithful to the asm.
    Vector3              mav3BoostOuterColours[3]; // DWARF h:617
    Vector3              mav3BoostInnerColours[3]; // DWARF h:620
    BrnWorld::EBoostType meCurrentBoostType;       // DWARF h:623

    // ---- ctor-only state (CrashNavIcon-style grouped/named members) ----
    // Bounding-box / extent accumulators the ctor primes with FLT_MAX sentinels and zero.
    // Grouped by the sentinel each field receives (see the .cpp for the exact write order).
    f32 mfExtentNegA;          // guest +212  (-FLT_MAX)
    f32 mfExtentNegB;          // guest +216  (-FLT_MAX)
    f32 mav3ExtentMinZero[3];  // guest +1376/+1380/+1384  (0.0f)
    f32 mfExtentNegC;          // guest +1388 (-FLT_MAX)
    f32 mfExtentPos;           // guest +1392 (+FLT_MAX)
    f32 maExtentNegGroup[8];   // guest +1404/+1408/+1420/+1424/+1436/+1440/+1452/+1456 (-FLT_MAX)

    // Pointer slots the ctor fills with a shared default element pointer: guest +1480..+1568
    // written as FOUR groups of five dwords (heads +1480/+1504/+1528/+1552, each with the
    // compiler's duplicate-first-write unroll artifact). The groups are on a 24-byte stride,
    // so the 6th dword of each group (+1500/+1524/+1548) is a GAP the guest ctor never writes
    // — modelled here the same gap-aware way as maZeroGroups (only the 5 real slots/group).
    // One real slot, guest +1560 (group 3, slot 2), is an uninitialised-stack-slot artifact
    // (back_chain) modelled as 0. FLAG: default-pointer groups + gap dwords + stack artifact.
    void* mapDefaultElement[4][5];

    // Zero-initialised state arrays (guest +1576..+1784): nine groups of five dwords, each
    // group written by the compiler as 5 stores + a duplicate-first-write (stride-5 clear).
    // FLAG: grouped zero-init slice (the per-group +24-byte stride leaves a gap dword that
    // is not modelled).
    u32 maZeroGroups[9][5];

    // DWARF h:788 — six billboard renderers, each cleared by the ctor's 6-iteration loop.
    BillboardRenderer mBillboardRenderer[6];

    // Trailing index/handle the ctor sets to -1 (guest +53456).  FLAG: named s32 sentinel
    // standing in for a far-trailing guest field.
    s32 miTrailingSentinel;
};
}

#endif
