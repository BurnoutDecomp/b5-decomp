#include "vendor/renderware/collision/GPInstance.hpp"

// ===========================================================================
// rw::collision::GPSphere -- the sphere primitive's VolumeMethods callbacks,
// reconstructed from BURNOUT_X360_ARTIST.XEX (dedicated VMX pass wave 2).
//
//   GPSphere::GetMaximumFeature @ 0x82BA8088   (VolumeMethods +0xA4)
//   GPSphere::GetInterval   @ 0x82BA80A8   (VolumeMethods +0xA8)
//   GPSphere::GetIntervals  @ 0x82BA80C0   (VolumeMethods +0xAC)
//
// Canonical declarations Feb-2007 rwccore.h:1190/1191 (non-static const
// members on PS3); reconstructed as static plain-function callbacks matching
// the committed GPInstance::VolumeMethods typedefs per the X360 delta note in
// GPInstance.hpp. A GP sphere's core primitive is a point -- the radius rides
// in GPInstance::mFatness and is applied by the callers (the fatness cull /
// contact resolve in the committed PrimitiveIntersect.cpp), so the projection
// interval collapses to min == max == dot3(dir, centre).
//
// CORRECTED 2026-08-18 (waveQ5 C1): the old banner said "GPSphere::
// GetMaximumFeature / GetBBox exist in the binary at other addresses outside
// this TU and are not homed here". GetMaximumFeature @ 0x82BA8088 is 0x20
// bytes BEFORE GetInterval -- inside this same TU run -- and is homed below.
// GetBBox has no per-type body at all: the +0xB0 slot of every one of the five
// records in unk_82F918F0 is 0x82AD5078, the XEX's ICF-folded shared empty
// `blr` (dumped in waveQ5 C1; AGENTS gotcha 6), so the shared no-op lives in
// GPRegistration.cpp with the table.
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128; the hardware broadcasts
    // the fold across all four lanes).
    inline f32 Dot3(const Vec4& a, const Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // The vmsum3fp128 / vspltw broadcast row: every lane carries s.
    inline Vec4 Splat(f32 s)
    {
        Vec4 r;
        r.x = s;
        r.y = s;
        r.z = s;
        r.w = s;
        return r;
    }
}

// ===========================================================================
// rw::collision::GPSphere::GetMaximumFeature @ 0x82BA8088   (waveQ5 C1)
//
// A GP sphere's core primitive is a POINT (the radius rides in mFatness), so
// the maximum feature along any direction is that point -- the body never
// reads the direction (v1) or abCcw (r4) at all.
//
//   li       r11, 0
//   lvx128   v0, r0, r3        ; v0 = GPInstance::mPos (this+0x00)
//   li       r10, 0x220
//   stw      r11, 0x230(r5)    ; feature.numedges = 0   (POINT feature)
//   stvx128  v0, r5, r10       ; feature.pt       = mPos (+0x220)
//   stw      r11, 0(r5)        ; feature.region   = 0
//   blr
//
// Store order preserved (numedges, then pt, then region). X360 register image
// matches the sibling GPCapsule/GPTriangle callbacks: r3 = this, r4 = abCcw,
// r5 = &arFeature, the query direction in v1.
// ===========================================================================
void GPSphere::GetMaximumFeature(const GPInstance* lpThis, RwBool /*abCcw*/,
                                 const Vec4& /*arDir*/, Feature& arFeature)
{
    arFeature.numedges = 0;              // stw r11(0), 0x230(r5)
    arFeature.pt       = lpThis->mPos;   // stvx128 v0, r5, r10(0x220)
    arFeature.region   = 0;              // stw r11(0), 0(r5)
}

// ===========================================================================
// rw::collision::GPSphere::GetInterval @ 0x82BA80A8
//
//   lvx128      v0, r0, r3        ; centre row = GPInstance::mPos (this+0x00)
//   vmsum3fp128 v0, v1, v0        ; dot3(dir, centre) broadcast to all lanes
//   stvx128     v0, r4, r11(0x10) ; interval.max (+0x10)
//   stvx128     v0, r0, r4        ; interval.min (+0x00)
//
// Store order preserved (max row first, then min); the Interval padding rows
// (+0x20..+0x2F) are not written, exactly as on the console.
// ===========================================================================
void GPSphere::GetInterval(const GPInstance* lpThis,
                           const Vec4& arDir, Interval& arInterval)
{
    // vmsum3fp128 v0, v1, v0 -- dot3(dir, mPos), lane-broadcast.
    const f32 lfCentreProj = Dot3(arDir, lpThis->mPos);

    arInterval.max = Splat(lfCentreProj);   // stvx128 v0, r4, r11 (+0x10)
    arInterval.min = Splat(lfCentreProj);   // stvx128 v0, r0, r4  (+0x00)
}

// ===========================================================================
// rw::collision::GPSphere::GetIntervals @ 0x82BA80C0
// Called by the committed FindBestSeparatingDirection through
// mMethods.mGetIntervals.
//
//   cmplwi cr6, r10, 0 / beqlr           ; zero directions -> return
//   loop (bne loc_82BA80D4):
//     lvx128      v0, r0, r9             ; dirs[i]           (r9 += 0x10)
//     lvx128      v13, r0, r3            ; centre = mPos (this+0x00)
//     vmsum3fp128 v0, v0, v13            ; dot3(dir, centre) broadcast
//     stvx128     v0, r11, r8(0x10)      ; intervals[i].max  (+0x10)
//     stvx128     v0, r0, r11            ; intervals[i].min  (+0x00)
//                                        ; r11 += 0x30 (= sizeof(Interval))
//
// Store order preserved (max row first, then min); the Interval padding rows
// are not written. The 0x30 output stride is sizeof(Interval) (static-
// asserted in GPInstance.hpp), so plain array indexing carries it.
// ===========================================================================
void GPSphere::GetIntervals(const GPInstance* lpThis, const Vec4* lapDirs,
                            u32 auNumDirs, Interval* lapIntervals)
{
    // cmplwi/beqlr: the zero-count call falls straight out.
    for (u32 luDir = 0; luDir < auNumDirs; ++luDir)
    {
        // vmsum3fp128 v0, v0, v13 -- dot3(dirs[i], mPos), lane-broadcast.
        const f32 lfCentreProj = Dot3(lapDirs[luDir], lpThis->mPos);

        lapIntervals[luDir].max = Splat(lfCentreProj);   // stvx128 v0, r11, r8
        lapIntervals[luDir].min = Splat(lfCentreProj);   // stvx128 v0, r0, r11
    }
}

} // namespace collision
} // namespace rw
