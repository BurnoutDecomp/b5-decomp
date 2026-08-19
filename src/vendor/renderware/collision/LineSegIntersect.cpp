#include "vendor/renderware/collision/LineSegIntersect.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"   // TriangleNearestPointRegion (7-param, GPTriangle.cpp)

#include <cmath>     // sqrt, fabs, powf
#include <cstring>   // memcpy (bit-exact float constant)

// ===========================================================================
// rw::collision line-segment testers -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (dedicated VMX pass).
//
//   rw::collision::FatTriangleLineSegIntersect   @ 0x82BBAD98
//   rw::collision::SolveQuarticRoots             @ 0x82BACA30
//   rw::collision::rwcSphereLineSegIntersect     @ 0x82BA81D8   (wave 2)
//   rw::collision::rwcCylinderLineSegIntersect   @ 0x82BAF8A0   (wave 2)
//   rw::collision::rwcTorusLineSegIntersect      @ 0x82BADAB0   (wave 2)
//   rw::collision::ThinTriangleLineSegIntersect  @ 0x82BB9EB8   (wave 2)
//   rw::collision::TriangleLineSegIntersect      @ 0x82BBB7B8   (wave 2)
//
// Every hand-vectorised body is lowered to portable scalar maths per the
// committed Feature / FeatureEdge / AALineClipper precedent; branch polarity,
// early-outs, loop structure and every caller-visible store are preserved.
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    // dot3 of the xyz lanes (vmsum3fp128).
    inline f32 Dot3(const Vec4& a, const Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // vsubfp / vaddfp: per-lane a -/+ b.
    inline Vec4 Sub(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x - b.x;
        r.y = a.y - b.y;
        r.z = a.z - b.z;
        r.w = a.w - b.w;
        return r;
    }

    inline Vec4 Add(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x + b.x;
        r.y = a.y + b.y;
        r.z = a.z + b.z;
        r.w = a.w + b.w;
        return r;
    }

    // vmulfp128 against a splat: per-lane a * s.
    inline Vec4 Scale(const Vec4& a, f32 s)
    {
        Vec4 r;
        r.x = a.x * s;
        r.y = a.y * s;
        r.z = a.z * s;
        r.w = a.w * s;
        return r;
    }

    // vmaddfp against a splat multiplier: per-lane a*s + b.
    inline Vec4 MaddScalar(const Vec4& a, f32 s, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x * s + b.x;
        r.y = a.y * s + b.y;
        r.z = a.z * s + b.z;
        r.w = a.w * s + b.w;
        return r;
    }

    // Cross product via the VMX two-permute idiom; the residue w lane is
    // exactly 0.
    inline Vec4 Cross(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.y * b.z - a.z * b.y;
        r.y = a.z * b.x - a.x * b.z;
        r.z = a.x * b.y - a.y * b.x;
        r.w = a.w * b.w - a.w * b.w;
        return r;
    }

    // Assemble a row from four lanes (the asm's stack-built rows / the
    // vmrghw-interleaved (u, v, ...) parameter stores).
    inline Vec4 MakeVec4(f32 x, f32 y, f32 z, f32 w)
    {
        Vec4 r;
        r.x = x;
        r.y = y;
        r.z = z;
        r.w = w;
        return r;
    }

    // Bit-exact float constant (the epsilon below is a subnormal).
    inline f32 F32FromBits(u32 luBits)
    {
        f32 lfValue;
        std::memcpy(&lfValue, &luBits, sizeof(lfValue));
        return lfValue;
    }

}

// flt_82180A24: raw 0x00200000 == 2^-128 (subnormal). The "effectively
// parallel / degenerate direction" threshold of every fat test below
// (Hex-Rays: COERCE_FLOAT(0x200000)). Initialised via the bit pattern to keep
// it exact; every consumer reads it at call time.
static const f32 KF_PARALLEL_EPSILON_BITS = F32FromBits(0x00200000u);

// ===========================================================================
// rw::collision::FatTriangleLineSegIntersect @ 0x82BBAD98
//
// X360 register map (__fastcall + VMX128 vector args):
//   r3 = lpResult                    f1 = afFatness
//   v1 = aLineStart   v2 = aLineDelta   v3 = aV0   v4 = aV1   v5 = aV2
//
// rodata (all values attested by the extract):
//   flt_82001CC0 = 0.0f    flt_82001C98 = 1.0f    flt_820037C8 = -1.0f
//   flt_82180A24 = raw 0x00200000 (2^-128 subnormal) -- parallel epsilon
//
// Algorithm (fully decoded from the asm):
//   1. Fat Moller-Trumbore prism rejection: det = dot(E1, cross(D,E2)); the
//      u*det / v*det / (u+v)*det interval tests are widened by
//      fatness * L1(pvec|qvec) and run against |det| with a folded sign.
//   2. If not parallel (|det| > 2^-128): slide the start onto the fat plane
//      offset by fatness toward the approach side; a hit whose u/v (at the
//      shifted start) lie inside the face is a face hit; otherwise advance
//      the segment start to that plane crossing and shorten the remaining
//      delta.
//   3. Classify the (possibly advanced) start against the triangle's nearest
//      feature (helper @ 0x82BBA748): inside the fat surface -> immediate
//      hit; face region -> plane hit with side-dependent normal flip; else
//      walk the vertex-sphere / edge-cylinder Voronoi regions for at most 5
//      steps, migrating across cap planes via deferred-division Fractions.
//
// Result block side effects (offsets attested by the r30-relative stores):
//   lineParam (+0x40) is zeroed up front, then written on the offset-plane
//   advance and accumulated by the walk -- INCLUDING on the t'>1 reject path.
//   normal (+0x20) is an INPUT (caller-seeded unit plane normal) before it is
//   overwritten. The two finalize paths that store an unnormalised normal and
//   immediately overwrite it are collapsed to the final store (the committed
//   volume.cpp precedent for redundant re-stores).
//
// VMX lowering notes: vrefp + 2x Newton-Raphson -> exact 1/x; vrsqrtefp + 2x
// NR -> exact 1/sqrt; cntlzw/extrwi feature==k bit tricks + fcfid ->
// (feature == k) ? 1.0f : 0.0f; srawi+addze signed /2 on non-negative values
// -> plain integer /2.
// ===========================================================================
s32 FatTriangleLineSegIntersect(VolumeLineSegIntersectResult* lpResult,
                                Vec4 aLineStart,
                                Vec4 aLineDelta,
                                Vec4 aV0,
                                Vec4 aV1,
                                Vec4 aV2,
                                f32  afFatness)
{
    // ---- prism rejection ---------------------------------------------------
    // stfs f31, 0x40(r30): the line parameter accumulates from zero.
    lpResult->lineParam = 0.0f;                                // flt_82001CC0

    const Vec4 lvEdge1 = Sub(aV1, aV0);                        // vsubfp128 v124
    const Vec4 lvEdge2 = Sub(aV2, aV0);                        // vsubfp128 v122
    Vec4 lvStartRel = Sub(aLineStart, aV0);                    // vsubfp128 v0

    // pvec = delta x edge2 (vpermwi128 0x63 / vmulfp128 / vnmsubfp / vpermwi128).
    const Vec4 lvPVec = Cross(aLineDelta, lvEdge2);            // v10
    // det = dot3(edge1, pvec) (vmsum3fp128 v12, v124, v10).
    const f32 lfDet = Dot3(lvEdge1, lvPVec);

    // Fold the determinant sign: sign = +/-1 (flt_82001C98 / flt_820037C8);
    // all interval tests below run against |det|.
    f32 lfSign   = 1.0f;                                       // f0
    f32 lfAbsDet = lfDet;                                      // f3
    if (lfDet < 0.0f)                                          // fcmpu / bge
    {
        lfAbsDet = -lfDet;                                     // fneg
        lfSign   = -1.0f;
    }
    Fraction lDist;                                            // var_190
    lDist.num = lfSign;                                        // stfs f0, var_190
    lDist.den = 0.0f;   // X360 leaves this word undefined until a helper writes it

    // Plane rejection against the caller-seeded unit normal (lvx128 v7, r0, r29):
    // start more than fatness behind the approach side -> miss.
    const Vec4 lvPlaneNormal = lpResult->normal;
    if (Dot3(lvPlaneNormal, lvStartRel) * lfSign < -afFatness) // fmuls / fcmpu / blt
    {
        return 0;
    }

    // u*det interval, widened by fatness * L1(pvec) (vandc sign-strip; the
    // fadds chain sums (|y| + |z|) + |x| in that order).
    const f32 lfUDet = Dot3(lvStartRel, lvPVec) * lfSign;                     // v145[0]
    const f32 lfMarginU =
        ((std::fabs(lvPVec.y) + std::fabs(lvPVec.z)) + std::fabs(lvPVec.x)) * afFatness;
    if (lfUDet < -lfMarginU)
    {
        return 0;
    }
    if (lfUDet > lfMarginU + lfAbsDet)
    {
        return 0;
    }

    // qvec = edge1 x delta (NOT delta x edge1 -- the polarity rides on lfSign).
    const Vec4 lvQVec = Cross(lvEdge1, aLineDelta);            // v8
    const f32 lfVDet = Dot3(lvStartRel, lvQVec) * lfSign;                     // v147[0]
    const f32 lfMarginV =
        ((std::fabs(lvQVec.y) + std::fabs(lvQVec.z)) + std::fabs(lvQVec.x)) * afFatness;
    if (lfVDet < -lfMarginV)
    {
        return 0;
    }
    if (lfVDet > lfMarginV + lfAbsDet)
    {
        return 0;
    }
    // (u+v)*det against the summed margins.
    if (lfVDet + lfUDet > (lfMarginU + lfMarginV) + lfAbsDet)
    {
        return 0;
    }

    Vec4 lvCurDelta = aLineDelta;   // var_150 (seeded by the prologue stvx128 v2)

    // ---- offset-plane stage (only when the segment is not parallel) --------
    if (lfAbsDet > KF_PARALLEL_EPSILON_BITS)                   // fcmpu f3 / ble
    {
        // detNormal = edge2 x edge1, oriented so dot3(delta, detNormal) == det.
        const Vec4 lvDetNormal = Cross(lvEdge2, lvEdge1);      // v11 -> vpermwi -> v13
        // Slide the start onto the plane offset by fatness toward the approach
        // side (vmulfp128 normal * splat(sign*fatness); vsubfp).
        const Vec4 lvShifted = Sub(lvStartRel, Scale(lvPlaneNormal, lfSign * afFatness));
        // t*|det| = -(dot3(shifted, detNormal) * sign).
        const f32 lfTNum = -(Dot3(lvShifted, lvDetNormal) * lfSign);          // f12
        if (lfTNum > lfAbsDet)      // crossing beyond the segment end -> miss
        {
            return 0;
        }
        if (lfTNum >= 0.0f)         // blt skips the whole stage (start behind the plane)
        {
            // 1/|det|: vrefp estimate + two vnmsubfp/vmaddfp Newton-Raphson
            // steps (@ 0x82BBAFE4..0x82BBB00C), rendered exact.
            const f32 lfRecipAbsDet = 1.0f / lfAbsDet;
            const f32 lfUShift = Dot3(lvShifted, lvPVec) * lfSign;            // v71
            const f32 lfT = lfRecipAbsDet * lfTNum;                           // v73
            // stfs f12, 0(r28): stored even when the face test below fails.
            lpResult->lineParam = lfT;

            if (lfUShift >= 0.0f && lfUShift <= lfAbsDet)
            {
                const f32 lfVShift = Dot3(lvShifted, lvQVec) * lfSign;        // v75
                if (lfVShift >= 0.0f && lfVShift + lfUShift <= lfAbsDet)
                {
                    // Face hit on the fat offset plane.
                    lpResult->position = MaddScalar(aLineDelta, lfT, aLineStart);
                    lpResult->normal   = Scale(lpResult->normal, lfSign);   // flip toward approach
                    lpResult->volParam = MakeVec4(lfRecipAbsDet * lfUShift,
                                                  lfRecipAbsDet * lfVShift,
                                                  0.0f, 0.0f);              // r30+0x30
                    return 1;
                }
            }
            // Clamp: advance the start to the offset-plane crossing and keep
            // only the remaining fraction of the segment.
            lvStartRel = MaddScalar(aLineDelta, lfT, lvStartRel);   // vmaddfp v0
            lvCurDelta = Scale(aLineDelta, 1.0f - lfT);             // vmulfp128 -> var_150
        }
    }

    // ---- nearest-feature stage ----------------------------------------------
    Vec4 lvPoint = Add(lvStartRel, aV0);   // vaddfp128 v1 -> var_1A0 (current point)
    Vec4 lvFocus;                          // var_160: nearest point, later the sphere centre
    f32 lfNearU = 0.0f;                    // var_188
    f32 lfNearV = 0.0f;                    // var_178
    s32 liFeature = TriangleNearestPointRegion(&lvFocus, &lfNearU, &lfNearV,
                                               lvPoint, aV0, aV1, aV2);   // bl loc_82BBA748 (7 params:
                                               // the callee writes f0/f4-f13, never f3 -- there is NO
                                               // plane-distance side channel; see the face arm below)

    const Vec4 lvToPoint = Sub(lvPoint, lvFocus);              // vsubfp v0 = v1 - v0
    const f32 lfDistSq  = Dot3(lvToPoint, lvToPoint);          // vmsum3fp128 -> var_170
    const f32 lfDepthSq = afFatness * afFatness - lfDistSq;    // fmsubs
    if (lfDepthSq > 0.0f)
    {
        // The (possibly advanced) start already sits inside the fat surface.
        lpResult->position = lvPoint;                                     // r30+0x10
        lpResult->volParam = MakeVec4(lfNearU, lfNearV, lfDepthSq, 0.0f); // r30+0x30
        // vrsqrtefp + two Newton-Raphson steps (@ 0x82BBB198..0x82BBB1B8),
        // rendered exact.
        lpResult->normal = Scale(lvToPoint, 1.0f / std::sqrt(lfDistSq));  // r29
        return 1;
    }
    if (liFeature == 6)
    {
        // Face region: keep the caller's plane normal, flipped to the point's
        // side of the plane (the classifier's f3 signed distance).
        lpResult->position = lvPoint;                          // stvx128 v1, r30, 0x10
        // The console's `fcmpu f3, f31 / bge` at 0x82BBB1D4 compares f3 -- which is THIS
        // function's own |det| (lfAbsDet, staged at 0x82BBAE30/0x82BBAE44; the callee never
        // writes f3) -- against 0.0. |det| >= 0 always, so the flip arm below never fires;
        // it is kept in the console's shape rather than deleted (2026-08-18 wave Q5 fix).
        if (lfAbsDet < 0.0f)                                   // fcmpu f3, f31 / bge
        {
            lpResult->normal = Scale(lpResult->normal, -1.0f); // flt_820037C8 broadcast
        }
        lpResult->volParam = MakeVec4(lfNearU, lfNearV, 0.0f, 0.0f);
        return 1;
    }

    // ---- vertex-sphere / edge-cylinder Voronoi walk (at most 5 steps, r27) --
    s32 liStepsLeft = 5;
    for (;;)
    {
        --liStepsLeft;                                         // addi r27, r27, -1
        s32 liHitCode;                                         // r3 of the primitive test

        if (liFeature <= 2)
        {
            // ===== vertex: sphere of radius fatness centred on lvFocus =====
            liHitCode = rwcSphereLineSegIntersect(&lDist, &lvPoint, &lvCurDelta,
                                                  &lvFocus, afFatness);
            if (liHitCode < 0)
            {
                return 0;
            }

            // The two triangle edges leaving this vertex (cmplwi r31, 1 dispatch).
            Vec4 lvEdgeA;
            Vec4 lvEdgeB;
            if (liFeature < 1)
            {
                lvEdgeA = lvEdge1;
                lvEdgeB = lvEdge2;
            }
            else if (liFeature == 1)
            {
                lvEdgeA = Sub(aV0, aV1);
                lvEdgeB = Sub(aV2, aV1);
            }
            else
            {
                lvEdgeA = Sub(aV0, aV2);
                lvEdgeB = Sub(aV1, aV2);
            }

            // Voronoi migration: if the segment leaves the vertex region across
            // an edge cap-plane no later than the sphere hit, that edge takes
            // over. Fractions compare cross-multiplied (no division yet).
            Fraction lCandidate;                               // var_180
            lCandidate.den = Dot3(lvCurDelta, lvEdgeA);        // v152
            if (lCandidate.den > KF_PARALLEL_EPSILON_BITS)
            {
                lCandidate.num = Dot3(Sub(lvFocus, lvPoint), lvEdgeA);   // v158
                if (lCandidate.num > 0.0f
                    && (liHitCode == 0
                        || lCandidate.den * lDist.num >= lCandidate.num * lDist.den))
                {
                    liHitCode = (liFeature + 6) / 2;   // srawi+addze: 0,1 -> 3; 2 -> 4
                    lDist = lCandidate;                // 8-byte ld/std Fraction copy
                }
            }
            lCandidate.den = Dot3(lvCurDelta, lvEdgeB);        // v157
            if (lCandidate.den > KF_PARALLEL_EPSILON_BITS)
            {
                lCandidate.num = Dot3(Sub(lvFocus, lvPoint), lvEdgeB);   // v155
                if (lCandidate.num > 0.0f
                    && (liHitCode == 0
                        || lCandidate.den * lDist.num >= lCandidate.num * lDist.den))
                {
                    liHitCode = (liFeature + 9) / 2;   // 0 -> 4; 1,2 -> 5
                    lDist = lCandidate;
                }
            }

            if (liHitCode == 0)
            {
                return 0;
            }
            if (liHitCode <= 2)
            {
                // Direct hit on the vertex sphere: finalize.
                const f32 lfLocalT = lDist.num / lDist.den;                 // fdivs
                lpResult->lineParam = lpResult->lineParam + lfLocalT;       // fadds accumulate
                const Vec4 lvHit = MaddScalar(lvCurDelta, lfLocalT, lvPoint);   // vmaddfp v12
                lpResult->position = lvHit;                                 // r30+0x10
                // (hit - centre) / fatness: vrefp + two Newton-Raphson steps
                // (@ 0x82BBB5E8..0x82BBB648), rendered exact. |hit-centre| is
                // the sphere radius, so this is the unit normal.
                lpResult->normal = Scale(Sub(lvHit, lvFocus), 1.0f / afFatness);   // r29
                // (u,v) = the vertex's barycentric corner (cntlzw/extrwi + fcfid).
                lpResult->volParam = MakeVec4((liFeature == 1) ? 1.0f : 0.0f,
                                              (liFeature == 2) ? 1.0f : 0.0f,
                                              0.0f, 0.0f);                  // r30+0x30
                return 1;
            }
            liFeature = liHitCode;   // migrated onto an edge -> advance below
        }
        else
        {
            // ===== edge: infinite cylinder of radius fatness around the edge =====
            Vec4 lvAxis;
            Vec4 lvBase;
            if (liFeature == 3)
            {
                lvAxis = lvEdge1;
                lvBase = aV0;
            }
            else if (liFeature == 4)
            {
                lvAxis = lvEdge2;
                lvBase = aV0;
            }
            else
            {
                lvAxis = Sub(aV2, aV1);
                lvBase = aV1;
            }
            lvFocus = lvBase;                                  // stvx128 v3 -> var_160
            const f32 lfAxisLenSq = Dot3(lvAxis, lvAxis);      // vmsum3fp128 -> var_120

            liHitCode = rwcCylinderLineSegIntersect(&lDist, lfAxisLenSq, afFatness,
                                                    0, 0,      // li r6/r7, 0: abInvert=0, abIgnoreInside=0
                                                    lvPoint, lvCurDelta, lvBase, lvAxis);
            if (liHitCode < 0)
            {
                return 0;
            }

            const f32 lfAxisDen = Dot3(lvCurDelta, lvAxis);    // v154
            Fraction lCandidate;                               // var_180
            bool lbMigratedFar = false;

            lCandidate.den = lfAxisDen;
            if (lfAxisDen > KF_PARALLEL_EPSILON_BITS)
            {
                // Far cap: does the segment leave through the plane at base+axis
                // no later than the cylinder hit?
                const Vec4 lvFarEnd = Add(lvFocus, lvAxis);    // vaddfp128 v0
                lCandidate.num = Dot3(Sub(lvFarEnd, lvPoint), lvAxis);   // v156
                if (lCandidate.num > 0.0f
                    && (liHitCode == 0
                        || lCandidate.den * lDist.num >= lCandidate.num * lDist.den))
                {
                    // The far vertex takes over. NOTE: this path skips the
                    // liHitCode==0 rejection below (b loc_82BBB510).
                    lvFocus = lvFarEnd;                        // stvx128 v10 -> var_160
                    liFeature = liFeature / 2;                 // srawi+addze: 3->1, 4->2, 5->2
                    lDist = lCandidate;
                    lbMigratedFar = true;
                }
            }
            if (!lbMigratedFar)
            {
                // Near cap (axis direction negated: fmuls f0, f0, f28).
                const f32 lfNegDen = lfAxisDen * -1.0f;
                lCandidate.den = lfNegDen;
                if (lfNegDen > KF_PARALLEL_EPSILON_BITS)
                {
                    lCandidate.num = Dot3(Sub(lvPoint, lvFocus), lvAxis);   // v151[0]
                    if (lCandidate.num > 0.0f
                        && (liHitCode == 0
                            || lDist.num * lfNegDen >= lCandidate.num * lDist.den))
                    {
                        liHitCode = 1;                         // li r3, 1
                        liFeature = (liFeature - 3) / 2;       // 3,4 -> 0; 5 -> 1
                        lDist = lCandidate;
                    }
                }
                if (liHitCode == 0)
                {
                    return 0;
                }
            }

            if (liFeature > 2)
            {
                // Direct hit on the edge cylinder: finalize.
                const f32 lfLocalT = lDist.num / lDist.den;                 // fdivs
                lpResult->lineParam = lpResult->lineParam + lfLocalT;       // fadds accumulate
                const Vec4 lvHit = MaddScalar(lvCurDelta, lfLocalT, lvPoint);   // vmaddfp v12
                // Axial parameter of the hit along the edge; both reciprocals
                // (1/lenSq, 1/fatness) are vrefp + two interleaved Newton-
                // Raphson steps (@ 0x82BBB6D8..0x82BBB714), rendered exact.
                const f32 lfEdgeLenSq = Dot3(lvAxis, lvAxis);   // vmsum3fp128 v9 (recomputed)
                const f32 lfS = Dot3(Sub(lvHit, lvFocus), lvAxis) * (1.0f / lfEdgeLenSq);
                const Vec4 lvAxisPoint = MaddScalar(lvAxis, lfS, lvFocus);   // vmaddfp128 v10
                lpResult->normal = Scale(Sub(lvHit, lvAxisPoint), 1.0f / afFatness);   // r29
                lpResult->position = lvHit;                    // stvx128 v12, r30, 0x10
                // Barycentric (u,v) from the axial parameter, per edge id.
                if (liFeature == 3)
                {
                    lpResult->volParam = MakeVec4(lfS, 0.0f, 0.0f, 0.0f);
                }
                else if (liFeature == 4)
                {
                    lpResult->volParam = MakeVec4(0.0f, lfS, 0.0f, 0.0f);
                }
                else
                {
                    lpResult->volParam = MakeVec4(1.0f - lfS, lfS, 0.0f, 0.0f);
                }
                return 1;
            }
            // migrated onto a vertex -> advance below
        }

        // ===== advance the walk start by the pending fraction (loc_82BBB518) =====
        const f32 lfLocalT = lDist.num / lDist.den;            // fdivs -> var_188
        const Vec4 lvNewPoint = MaddScalar(lvCurDelta, lfLocalT, lvPoint);   // vmaddfp v1
        // Fold the local fraction into the whole-segment parameter -- stored
        // BEFORE the t'>1 rejection (the asm stores through r28 either way).
        lpResult->lineParam = (1.0f - lpResult->lineParam) * lfLocalT
                            + lpResult->lineParam;             // fmadds / stfs 0(r28)
        lvPoint    = lvNewPoint;                               // stvx128 v1 -> var_1A0
        lvCurDelta = Scale(lvCurDelta, 1.0f - lfLocalT);       // vmulfp128 -> var_150
        if (lfLocalT > 1.0f)                                   // fcmpu f0, f29 / bgt
        {
            return 0;
        }
        if (liStepsLeft == 0)   // budget exhausted: report the walked state as a hit
        {
            return 1;          // b loc_82BBB790
        }
    }
}

// ===========================================================================
// rw::collision::SolveQuarticRoots @ 0x82BACA30
// Sole caller: rw::collision::rwcTorusLineSegIntersect (pending).
//
// Damped Newton-Raphson search for one real root of the quartic
//     f(x) = c4*x^4 + c3*x^3 + c2*x^2 + c1*x + c0
// with lafCoefficients[k] = coefficient of x^k (the pre-scaled derivative
// coefficients 4*c4 / 3*c3 / 2*c2 come from the prologue fmuls).
//
// The X360 body is hand-vectorised: each iteration evaluates f(x) and f'(x)
// by inlining the VMX128 vectorised powf FIVE times (exponents 2/3/4 for f,
// then 2/3 for f'), each instance broadcasting the scalar iterate x from the
// stack (lvlx + vspltw) and folding the result back into lane 0. The Newton
// update itself is plain scalar FPU (fdivs / fnmsubs). Per the dedicated-VMX
// pass rules the powf estimate+refine pipeline (vandc sign strip; vlogefp +
// minimax refinement from the unk_82014AC0..AF0 coefficient rows; vrfim/
// vexptefp128 scale; vrefp reciprocal for the negative path; the vcmpeqfp/
// vcmpgtfp/vrfiz + vsel ANSI-specials network) is rendered as the
// mathematically equivalent scalar powf -- the special cases it encodes are
// exactly ANSI powf semantics, so scalar powf is faithful for every input.
//
// Per iteration i (damping factor (50 - i) * 0.02, i.e. 1.0 on the first step
// decaying to 0.02 on the last):
//     x <- x - damping * f(x) / f'(x)                (fdivs / fnmsubs)
// The loop-back test (fcmpu cr6 / bgt) uses the residual evaluated at the
// PRE-update x, so the returned root always carries one extra damped step
// beyond the iterate whose residual passed. That asymmetry is preserved.
// ===========================================================================

namespace
{
    // .rdata scalar constants (values attested by the Hex-Rays literals for
    // the labelled loads in the prologue/epilogue):
    const f32 KF_INITIAL_ROOT     = 0.0f;                    // flt_82001CC0
    const f32 KF_HUGE_RESIDUAL    = 3.4028235e38f;           // flt_821802D8 (FLT_MAX)
    const f64 KD_ENTRY_GUARD      = 3.402823466385289e38;    // dbl_82180398
    const f32 KF_RESIDUAL_EPSILON = 1.0e-6f;                 // flt_820AD47C
    const f32 KF_ACCEPT_EPSILON   = 0.001f;                  // flt_82013F90
    const f32 KF_DAMPING_SLOPE    = 0.02f;                   // flt_82005574
    const f32 KF_ITERATION_SPAN   = 50.0f;                   // flt_820138DC
    const f32 KF_TWO              = 2.0f;                    // flt_82001D9C
    const f32 KF_THREE            = 3.0f;                    // flt_82004270
    const f32 KF_FOUR             = 4.0f;                    // flt_82004EF4
    const s32 KI_MAX_ITERATIONS   = 50;                      // cmpwi cr6, r7, 0x32

    // The VMX128 vectorised powf the compiler inlined five times per
    // iteration -- rendered as the equivalent scalar powf (see the block
    // comment above for the full instruction citation).
    inline f32 VmxPowF(f32 lfBase, f32 lfExponent)
    {
        return powf(lfBase, lfExponent);
    }
}

RwBool SolveQuarticRoots(f32 lafCoefficients[5], f32& arRoot)
{
    const f32 lfC4 = lafCoefficients[4];   // lfs f9, 0x10(r3)
    const f32 lfC3 = lafCoefficients[3];   // lfs f8, 0xC(r3)
    const f32 lfC2 = lafCoefficients[2];   // lfs f7, 8(r3)
    const f32 lfC1 = lafCoefficients[1];   // lfs f10, 4(r3)

    // Derivative coefficients, pre-scaled once in the prologue (fmuls f4/f3/f2).
    const f32 lfDerivC3 = lfC4 * KF_FOUR;   // 4*c4
    const f32 lfDerivC2 = lfC3 * KF_THREE;  // 3*c3
    const f32 lfDerivC1 = lfC2 * KF_TWO;    // 2*c2

    f32 lfRoot      = KF_INITIAL_ROOT;      // stfs f0, var_16C -- the iterate slot
    f32 lfResidual  = KF_HUGE_RESIDUAL;     // f11 seed: forces the accept test false
                                            // if the loop never runs
    s32 liConverged = 0;                    // r11
    s32 liIteration = 0;                    // r7

    // fabs(dbl_82180398) > 1e-6: a constant-true guard in the original (the
    // compiler could not fold the memory loads). Kept for control-flow parity.
    if (fabs(KD_ENTRY_GUARD) > static_cast<f64>(KF_RESIDUAL_EPSILON))
    {
        while (liIteration < KI_MAX_ITERATIONS)     // cmpwi/bge at loc_82BACAEC
        {
            // Five inlined VMX powf instances, in asm order (exponent stores
            // to var_170): x^2, x^3, x^4 for f, then x^2, x^3 for f'.
            const f32 lfX2      = VmxPowF(lfRoot, KF_TWO);
            const f32 lfX3      = VmxPowF(lfRoot, KF_THREE);
            const f32 lfX4      = VmxPowF(lfRoot, KF_FOUR);
            const f32 lfDerivX2 = VmxPowF(lfRoot, KF_TWO);
            const f32 lfDerivX3 = VmxPowF(lfRoot, KF_THREE);

            // f(x) fold (lane 0 of the vector chain, stored to var_50):
            //   vmulfp128 v8 = x^3 * c3
            //   vmaddfp   v8 = x^4 * c4 + v8
            //   vmaddfp   v8 = x^2 * c2 + v8
            //   vaddfp    v8 += c1*x   (c1*x precomputed by fmuls into var_70)
            //   vaddfp    v8 += c0     (var_B0)
            f32 lfValue = lfX3 * lfC3;
            lfValue = lfX4 * lfC4 + lfValue;
            lfValue = lfX2 * lfC2 + lfValue;
            lfValue = lfValue + lfC1 * lfRoot;
            lfValue = lfValue + lafCoefficients[0];
            lfResidual = lfValue;                   // lfs f11, var_50

            // f'(x) fold (stored to var_30):
            //   vmulfp128 v11 = x^2 * 3c3
            //   vmaddfp   v0  = x^3 * 4c4 + v11
            //   vaddfp    v0 += 2c2*x  (precomputed by fmuls into var_60)
            //   vaddfp    v0 += c1     (var_E0)
            f32 lfSlope = lfDerivX2 * lfDerivC2;
            lfSlope = lfDerivX3 * lfDerivC3 + lfSlope;
            lfSlope = lfSlope + lfDerivC1 * lfRoot;
            lfSlope = lfSlope + lfC1;

            // Damping uses the PRE-increment iteration index (extsw/fcfid of
            // the old r7): 1.0 on iteration 0, 0.02 on iteration 49.
            const f32 lfDamping =
                (KF_ITERATION_SPAN - static_cast<f32>(liIteration)) * KF_DAMPING_SLOPE;
            ++liIteration;                          // addi r7, r7, 1

            const f32 lfStep = lfResidual / lfSlope;        // fdivs f29, f11, f29
            lfRoot = lfRoot - lfDamping * lfStep;           // fnmsubs f0, f30, f29, f0

            // Loop-back: bgt cr6 on |f(pre-update x)| > 1e-6 (fabs/fcmpu were
            // issued before the update; the branch polarity is preserved).
            if (!(fabsf(lfResidual) > KF_RESIDUAL_EPSILON))
            {
                break;
            }
        }
    }

    arRoot = lfRoot;                                // stfs f0, 0(r4)

    if (fabsf(lfResidual) < KF_ACCEPT_EPSILON)      // fabs f13, f11 / fcmpu
    {
        liConverged = 1;                            // li r11, 1
    }
    return liConverged;                             // mr r3, r11
}

// ===========================================================================
// rw::collision::rwcSphereLineSegIntersect @ 0x82BA81D8
//
// X360 register map (__fastcall):
//   r3 = lpDist   r4 = lpLineStart   r5 = lpLineDelta   r6 = lpCentre
//   f1 = afRadius
//
// rodata: flt_82001CC0 = 0.0f    flt_82001C98 = 1.0f
//
// Deferred-division near-root sphere test. toCentre = centre - start; a
// start strictly inside the sphere -> immediate hit t = 0/1. Otherwise
// require the segment to approach (dot(toCentre, delta) > 0, else -1), form
// the |delta|^2-scaled discriminant
//     disc = |delta|^2 * radius^2 - |cross(toCentre, delta)|^2
// (negative -> the infinite line misses), reject a near root past the
// segment end (proj - |delta|^2 > 0 with its square beyond disc), and return
// the near root as the Fraction (proj - sqrt(disc)) / |delta|^2.
//
// VMX lowering notes: vmsum3fp128 -> Dot3; the vpermwi128 0x63 / vmulfp128 /
// vnmsubfp / vpermwi128 two-permute idiom @ 0x82BA8250..0x82BA8270 is exactly
// Cross(toCentre, aLineDelta). fmsubs f11,f12,f0,f11 (scalar operand order
// fD = fA*fC - fB, multiplier SECOND) = deltaLenSq*radiusSq - crossSq. Branch
// polarity is preserved exactly, including the unordered (NaN) direction of
// every fcmpu, via the !(...) forms below (the SolveQuarticRoots loop-back
// precedent).
// ===========================================================================
s32 rwcSphereLineSegIntersect(Fraction* lpDist,           // r3
                              const Vec4* lpLineStart,    // r4
                              const Vec4* lpLineDelta,    // r5
                              const Vec4* lpCentre,       // r6
                              f32 afRadius)               // f1
{
    const f32 lfRadiusSq = afRadius * afRadius;           // fmuls f0, f1, f1

    // toCentre = centre - start (lvx128 v0 / v13 ; vsubfp v13).
    const Vec4 lvToCentre = Sub(*lpCentre, *lpLineStart);
    // |toCentre|^2 (vmsum3fp128 v0, v13, v13 -> stack round-trip -> lfs f13).
    const f32 lfDistSq = Dot3(lvToCentre, lvToCentre);

    // fcmpu f13, f0 / bge loc_82BA821C: only a STRICTLY inside start takes
    // the immediate-hit path (unordered goes to the main path with the asm).
    if (lfDistSq < lfRadiusSq)
    {
        // Start inside the sphere: hit at once, t = 0/1.
        lpDist->den = 1.0f;   // flt_82001C98 (stfs f13, 4(r11))
        lpDist->num = 0.0f;   // flt_82001CC0 (the shared tail store @ 0x82BA82B8)
        return 1;             // li r3, 1
    }

    // proj = dot3(toCentre, delta) (vmsum3fp128 v12, v13, v0).
    const f32 lfProj = Dot3(lvToCentre, *lpLineDelta);
    if (!(lfProj > 0.0f))     // fcmpu f10, f13(0.0) / bgt skips -> li r3, -1
    {
        return -1;            // segment points away from (or grazes) the sphere
    }

    const f32 lfDeltaLenSq = Dot3(*lpLineDelta, *lpLineDelta);   // vmsum3fp128 v11
    // |cross(toCentre, delta)|^2 -- the two-permute cross idiom + vmsum3fp128.
    const Vec4 lvCross   = Cross(lvToCentre, *lpLineDelta);
    const f32  lfCrossSq = Dot3(lvCross, lvCross);

    // disc = |delta|^2 * r^2 - crossSq (fmsubs f11, f12, f0, f11).
    const f32 lfDisc = lfDeltaLenSq * lfRadiusSq - lfCrossSq;
    if (!(lfDisc >= 0.0f))    // fcmpu f11, f13(0.0) / bge skips -> li r3, 0
    {
        return 0;             // the infinite line misses the sphere
    }

    // Segment-end rejection (fsubs f0, f10, f12): the near root lies past
    // t = 1 iff proj - |delta|^2 > 0 and its square exceeds the discriminant.
    const f32 lfPastEnd = lfProj - lfDeltaLenSq;
    if (lfPastEnd > 0.0f                              // fcmpu / ble skips the square test
        && lfPastEnd * lfPastEnd > lfDisc)            // fmuls / fcmpu / bgt -> li r3, 0
    {
        return 0;
    }

    // Near root as a deferred fraction: t = (proj - sqrt(disc)) / |delta|^2.
    lpDist->den = lfDeltaLenSq;                       // stfs f12, 4(r11)
    lpDist->num = lfProj - std::sqrt(lfDisc);         // fsqrts / fsubs / stfs 0(r11)
    return 1;                                         // li r3, 1
}

// ===========================================================================
// rw::collision::rwcCylinderLineSegIntersect @ 0x82BAF8A0
//
// X360 register map (__fastcall; the vector InParams ride in VMX v1..v4, the
// two scalar floats in f1/f2 shadowing the r4/r5 GPR slots, so the RwBools
// land in r6/r7 -- exactly the two integers the asm tests):
//   r3 = lpDist         f1 = afAxisLengthSq   f2 = afRadius
//   r6 = abInvert       r7 = abIgnoreInside
//   v1 = aLineStart(orig)  v2 = aLineDelta(seg)  v3 = aBase(center)  v4 = aAxis
//
// rodata: flt_82001CC0 = 0.0f  flt_82001C98 = 1.0f  flt_820037C8 = -1.0f
//
// INFINITE cylinder of radius `radius` around (base, axis), everything
// |axis|^2-scaled so no normalisation is needed. With toBase = base - start:
//   radialSq(t) = |cross(toBase - t*delta, axis)|^2
//               = crossBaseSq - 2t*proj + t^2*crossDeltaSq,
//   proj         = dot3(cross(toBase, axis), cross(delta, axis)),
//   hit when radialSq(t) = |axis|^2 * r^2, i.e.
//   t = (proj -/+ sqrt(disc)) / crossDeltaSq,
//   disc = crossDeltaSq*|axis|^2*r^2 + proj^2 - crossDeltaSq*crossBaseSq.
// abIgnoreInside suppresses the radially-inside immediate accept (t = 0/1);
// abInvert selects the FAR root (exit surface, sign -1, only when the exit
// lies strictly inside the segment) and drops the receding-line -1 abort.
//
// VMX lowering notes: both crosses are the two-permute idiom -> Cross;
// vmsum3fp128 -> Dot3. Scalar tail: fmuls f13,f10,f13 / fmsubs f13,f11,f11,
// f13 / fmadds f13,f10,f0,f13 build the discriminant in exactly the order
// below; fnmsubs f0,f13,f0,f11 (fD = -(fA*fC - fB)) = proj - sign*sqrt(disc).
// Branch polarity (incl. the unordered direction of every fcmpu) is
// preserved via the !(...) forms.
// ===========================================================================
s32 rwcCylinderLineSegIntersect(Fraction* lpDist,        // r3
                                f32 afAxisLengthSq,      // f1
                                f32 afRadius,            // f2
                                s32 abInvert,            // r6 (canonical `invert`)
                                s32 abIgnoreInside,      // r7 (canonical `ignoreInside`)
                                Vec4 aLineStart,         // v1 (canonical `orig`)
                                Vec4 aLineDelta,         // v2 (canonical `seg`)
                                Vec4 aBase,              // v3 (canonical `center`)
                                Vec4 aAxis)              // v4
{
    // |axis|^2 * r^2 -- the scaled squared-radius limit (fmuls f0,f1,f2 ;
    // fmuls f0,f0,f2).
    const f32 lfRadialLimit = (afAxisLengthSq * afRadius) * afRadius;

    // toBase = base - start (vsubfp v0, v3, v1) and its axis cross
    // (two-permute idiom @ 0x82BAF8A4..0x82BAF8C4).
    const Vec4 lvToBase      = Sub(aBase, aLineStart);
    const Vec4 lvCrossBase   = Cross(lvToBase, aAxis);
    const f32  lfCrossBaseSq = Dot3(lvCrossBase, lvCrossBase);   // vmsum3fp128 v12

    // fcmpu f13, f0 / bge loc_82BAF8FC ; cmplwi r7 / bne loc_82BAF8FC: only a
    // STRICTLY radially-inside start with abIgnoreInside == 0 early-accepts
    // (unordered takes the main path with the asm).
    if (lfCrossBaseSq < lfRadialLimit && abIgnoreInside == 0)
    {
        lpDist->den = 1.0f;   // flt_82001C98 (stfs f13, 4(r10))
        lpDist->num = 0.0f;   // flt_82001CC0 (the shared tail store @ loc_82BAF99C)
        return 1;             // li r3, 1
    }

    // cross(delta, axis) (two-permute idiom @ 0x82BAF8FC..0x82BAF910) and the
    // projected approach term proj = dot3(crossBase, crossDelta)
    // (vmsum3fp128 v0, v0, v13).
    const Vec4 lvCrossDelta = Cross(aLineDelta, aAxis);
    const f32  lfProj       = Dot3(lvCrossBase, lvCrossDelta);

    // cmplwi r6 / bne skips ; fcmpu f11, f12(0.0) / bgt skips -> li r3, -1.
    if (abInvert == 0 && !(lfProj > 0.0f))
    {
        return -1;            // segment points away from the surface
    }

    const f32 lfCrossDeltaSq = Dot3(lvCrossDelta, lvCrossDelta);   // vmsum3fp128 v0

    // Discriminant, in asm order (fmuls f13, f10, f13 ; fmsubs f13, f11, f11,
    // f13 ; fmadds f13, f10, f0, f13):
    //   disc = crossDeltaSq*|axis|^2*r^2 + (proj^2 - crossDeltaSq*crossBaseSq)
    f32 lfDisc = lfCrossDeltaSq * lfCrossBaseSq;
    lfDisc = lfProj * lfProj - lfDisc;
    lfDisc = lfCrossDeltaSq * lfRadialLimit + lfDisc;
    if (!(lfDisc >= 0.0f))    // fcmpu f13, f12(0.0) / bge skips -> li r3, 0
    {
        return 0;             // the infinite line misses the cylinder
    }

    // pastEnd = proj - crossDeltaSq (fsubs f0, f11, f10): the near root sits
    // past t = 1 exactly when pastEnd >= 0 and pastEnd^2 >= disc.
    const f32 lfPastEnd = lfProj - lfCrossDeltaSq;
    f32 lfSign;               // the fsel-free +/-1 root selector (f0)
    if (abInvert == 0)
    {
        // fcmpu f0, f12 / blt loc_82BAF988 skips the square test.
        if (!(lfPastEnd < 0.0f))
        {
            // fmuls f0, f0, f0 ; fcmpu f0, f13 / bge loc_82BAF960 -> miss.
            if (!(lfPastEnd * lfPastEnd < lfDisc))
            {
                return 0;     // near root beyond the segment end
            }
        }
        lfSign = 1.0f;        // flt_82001C98 (loc_82BAF988): near root
    }
    else
    {
        // loc_82BAF9A8 -- exit-surface (far root) selection: require the far
        // root strictly inside the segment (crossDeltaSq - proj > sqrt(disc)).
        if (!(lfPastEnd < 0.0f))                      // fcmpu / bge -> miss
        {
            return 0;
        }
        if (!(lfPastEnd * lfPastEnd > lfDisc))        // fmuls ; fcmpu / ble -> miss
        {
            return 0;
        }
        lfSign = -1.0f;       // flt_820037C8: far root
    }

    // t = (proj - sign*sqrt(disc)) / crossDeltaSq as a deferred fraction
    // (fsqrts f13 ; stfs f10, 4(r10) ; fnmsubs f0, f13, f0, f11 ; stfs 0(r10)).
    lpDist->den = lfCrossDeltaSq;
    lpDist->num = -(std::sqrt(lfDisc) * lfSign - lfProj);
    return 1;                 // li r3, 1
}

// ===========================================================================
// rw::collision::rwcTorusLineSegIntersect @ 0x82BADAB0
// Sole caller of SolveQuarticRoots (above).
//
// X360 register map (__fastcall):
//   r3 = &arDist   v1 = aLineStart(orig)   v2 = aLineDir(dir)
//   f1 = afMajorRadius   f2 = afMinorRadius
//
// rodata (all valued): flt_82001CC0 = 0.0f, flt_82004EF4 = 4.0f,
//   flt_82001D9C = 2.0f, flt_82004C88 = 8.0f.
//
// Torus centred at the local origin with its axis along local z (the vspltw
// v1/v2 lane-2 broadcasts), major radius R = f1, minor radius r = f2. For
// q(t) = orig + t*dir the implicit surface
//   (|q|^2 - (R^2 + r^2))^2 - 4R^2(r^2 - qz^2) = 0
// expands to the quartic sum(c[k] t^k) with (od = orig.dir, dd = dir.dir,
// oo = orig.orig, C0 = oo - (R^2 + r^2), G0 = r^2 - origZ^2):
//   c4 = dd^2
//   c3 = 4*od*dd
//   c2 = 4*od^2 + 2*C0*dd + 4R^2*dirZ^2
//   c1 = 4*C0*od + 8R^2*origZ*dirZ          <- the KF_EIGHT confirmation
//   c0 = C0^2 - 4R^2*G0
// The coefficients feed the damped-Newton SolveQuarticRoots with &arDist as
// the root slot (mr r4, r3); the return is the solver's converged flag
// normalised to 0/1 (cntlzw/extrwi/xori == `!= 0`).
//
// VMX lowering notes: the body is pure coefficient assembly -- vmsum3fp128
// dot folds (-> Dot3) plus splat/stack round-trips that carry SCALARS through
// vector registers; all are lowered to scalars. Every fmuls/fadds/fsubs/
// fmadds/fmsubs association below transcribes the asm order exactly. The only
// caller-visible stores are *r3 (the up-front 0.0f seed and SolveQuarticRoots'
// unconditional root write) -- both preserved.
// ===========================================================================

namespace
{
    // flt_82004C88 -- the 8R^2 factor of the quartic's linear term (label
    // valued twice in the committed tree: BrnBehaviourGameplayExternal.cpp
    // "flt_82004C88 = 8.0f" and SDKs/EATech/eajobs/detail.cpp
    // "flt_82004C88 == 8.0"; independently forced by the torus expansion).
    const f32 KF_EIGHT = 8.0f;
}

s32 rwcTorusLineSegIntersect(f32& arDist,          // r3
                             Vec4 aLineStart,      // v1 (canonical `orig`)
                             Vec4 aLineDir,        // v2 (canonical `dir`)
                             f32 afMajorRadius,    // f1
                             f32 afMinorRadius)    // f2
{
    // stfs f11(flt_82001CC0), 0(r4): the root slot is seeded 0.0f up front
    // (caller-visible; SolveQuarticRoots overwrites it unconditionally).
    arDist = 0.0f;

    const f32 lfMajorSq = afMajorRadius * afMajorRadius;   // fmuls f13, f1, f1
    const f32 lfMinorSq = afMinorRadius * afMinorRadius;   // fmuls f12, f2, f2

    const f32 lfOD = Dot3(aLineStart, aLineDir);    // vmsum3fp128 v11 -> var_50
    const f32 lfDD = Dot3(aLineDir, aLineDir);      // vmsum3fp128 v10 -> var_20
    const f32 lfOO = Dot3(aLineStart, aLineStart);  // vmsum3fp128 v12

    const f32 lfOrigZ = aLineStart.z;               // vspltw v0, v1, 2
    const f32 lfDirZ  = aLineDir.z;                 // vspltw v13, v2, 2

    // C0 = oo - (r^2 + R^2)  (fadds f10, f12, f13 -> var_80 splat ; vsubfp v12).
    const f32 lfC0 = lfOO - (lfMinorSq + lfMajorSq);
    // G0 = r^2 - origZ^2  (vmulfp128 v10, v0, v0 -> var_80 ; fsubs f13, f12, f13).
    const f32 lfG0 = lfMinorSq - lfOrigZ * lfOrigZ;

    const f32 lfFourMajorSq  = lfMajorSq * KF_FOUR;    // fmuls f11, f13, f0  (4R^2)
    const f32 lfEightMajorSq = lfMajorSq * KF_EIGHT;   // fmuls f13, f13, f10 (8R^2)

    // Quartic coefficients c[k] of t^k, ordered exactly as SolveQuarticRoots'
    // lafCoefficients[5] expects (asm store order: [0] var_40, [4] var_30,
    // [3] var_34, [1] var_3C, [2] var_38).
    f32 lafCoefficients[5];

    // c0 = C0*C0 - G0*4R^2
    // (vmulfp128 v0, v12, v0 -> var_50 ; fmsubs f10, f13, f13, f10).
    lafCoefficients[0] = lfC0 * lfC0 - lfG0 * lfFourMajorSq;

    // c1 = (C0*od)*4 + (dirZ*8R^2)*origZ
    // (vmulfp128 v11, v13, v11 ; vmulfp128 v0, v11, v0 -> var_40 = f9 ;
    //  fmuls f10, f13, f12 ; fmadds f10, f10, f0, f9 -> var_8C).
    lafCoefficients[1] = (lfC0 * lfOD) * KF_FOUR
                       + (lfDirZ * lfEightMajorSq) * lfOrigZ;

    // c2 = (od*od)*4 + (C0*dd)*2 + (dirZ*4R^2)*dirZ
    // (fmuls f9, f12, f12 ; fmuls f10, f13, f11 ; fmuls f13, f10, f13(2.0) ;
    //  fmadds f0, f9, f0(4.0), f13 ; vmulfp128 v0, v13, v0 ;
    //  vmulfp128 v0, v0, v13 ; fadds f0, f0, f13).
    lafCoefficients[2] = ((lfOD * lfOD) * KF_FOUR + (lfC0 * lfDD) * KF_TWO)
                       + (lfDirZ * lfFourMajorSq) * lfDirZ;

    // c3 = (od*dd)*4  (fmuls f12, f12, f11 ; fmuls f12, f12, f0 -> var_34).
    lafCoefficients[3] = (lfOD * lfDD) * KF_FOUR;

    // c4 = dd*dd  (fmuls f13, f11, f11 -> var_30).
    lafCoefficients[4] = lfDD * lfDD;

    // bl SolveQuarticRoots (r3 = &coefficients, r4 = the original r3 = &dist);
    // cntlzw/extrwi/xori normalise the converged flag to exactly 0/1.
    return SolveQuarticRoots(lafCoefficients, arDist) != 0;
}

// ===========================================================================
// rw::collision::ThinTriangleLineSegIntersect @ 0x82BB9EB8
//
// rodata (all values attested by the extract's literals):
//   flt_8200D5F0 = 0.0000000099999999  == 1.0e-8f   (backface/degeneracy gate)
//   flt_82180AA4 = -0.0000099999997    == -1.0e-5f  (relative tolerance factor)
//   flt_82001CC0 = 0.0f                             (volParam z lane)
//
// VMX lowering notes: both cross products are the committed two-permute
// idiom; vmsum3fp128 == Dot3; 1/det is vrefp + TWO vnmsubfp/vmaddfp Newton-
// Raphson steps (@ 0x82BB9FCC..0x82BB9FE8), rendered exact per the wave-1
// precedent. The final position is vmaddfp v0, v2, v1, v0 == delta * splat(t)
// + start, where the t splat is lvlx'd back from the just-stored lineParam
// field (identical value).
//
// Caller-visible store contract (preserved exactly): lineParam (+0x40)
// receives the RAW t*det dot BEFORE the t interval test -- a miss on that
// final test still leaves t*det in the caller's lineParam field. On a hit it
// is overwritten with t, then volParam (+0x30) = (u, v, 0, 0) and position
// (+0x10) are stored. normal (+0x20) is never touched here.
// ===========================================================================

// flt_8200D5F0: the one-sided determinant gate. det <= 1e-8 rejects backfacing
// AND near-parallel segments in one compare (no fabs -- the test is one-sided).
static const f32 KF_THIN_DET_EPSILON = 1.0e-8f;

// flt_82180AA4: the relative interval tolerance factor. Multiplied by the
// (positive) determinant it widens every barycentric interval test by
// det*1e-5 on both sides: low bound = det * -1e-5, high bound = det - low.
static const f32 KF_THIN_TOLERANCE = -1.0e-5f;

s32 ThinTriangleLineSegIntersect(VolumeLineSegIntersectResult* lpResult,   // r3
                                 Vec4 aLineStart,                          // v1
                                 Vec4 aLineDelta,                          // v2
                                 Vec4 aV0,                                 // v3
                                 Vec4 aV1,                                 // v4
                                 Vec4 aV2)                                 // v5
{
    // ---- determinant (one-sided Moller-Trumbore) ---------------------------
    const Vec4 lvEdge2 = Sub(aV2, aV0);                 // vsubfp v12, v5, v3
    const Vec4 lvEdge1 = Sub(aV1, aV0);                 // vsubfp v0, v4, v3

    // pvec = delta x edge2 (vpermwi128 0x63 / vmulfp128 / vnmsubfp / vpermwi128).
    const Vec4 lvPVec = Cross(aLineDelta, lvEdge2);     // v13
    // det = dot3(edge1, pvec) (vmsum3fp128 v11, v0, v13; lane 0 via the stack).
    const f32 lfDet = Dot3(lvEdge1, lvPVec);

    // fcmpu cr6 / ble: backfacing or (near-)parallel -> miss. ONE-SIDED: a
    // negative determinant is rejected outright, there is no sign fold here
    // (unlike the fat tester).
    if (lfDet <= KF_THIN_DET_EPSILON)                   // flt_8200D5F0
    {
        return 0;
    }

    // Relative tolerance band for every interval test below (fmuls / fsubs):
    //   low  = det * -1e-5      (a small NEGATIVE margin)
    //   high = det - low        (det * (1 + 1e-5))
    const f32 lfLowBound  = lfDet * KF_THIN_TOLERANCE;  // f0  (flt_82180AA4)
    const f32 lfHighBound = lfDet - lfLowBound;         // f11

    // ---- u interval ---------------------------------------------------------
    const Vec4 lvTVec = Sub(aLineStart, aV0);           // vsubfp v11, v1, v3
    const f32  lfUDet = Dot3(lvTVec, lvPVec);           // vmsum3fp128 v13, v11, v13
    if (lfUDet < lfLowBound)                            // fcmpu / blt
    {
        return 0;
    }
    if (lfUDet > lfHighBound)                           // fcmpu / bgt
    {
        return 0;
    }

    // ---- v interval ---------------------------------------------------------
    // qvec = tvec x edge1 (vmr/vpermwi128/vmulfp128/vnmsubfp/vpermwi128 block;
    // vnmsubfp v0, v10, v0, v13 decodes as v0 = v0 - perm(tvec)*edge1).
    const Vec4 lvQVec = Cross(lvTVec, lvEdge1);         // v0
    const f32  lfVDet = Dot3(aLineDelta, lvQVec);       // vmsum3fp128 v13, v2, v0
    if (lfVDet < lfLowBound)                            // fcmpu / blt
    {
        return 0;
    }
    // fadds f13, f10, f12: the u+v fold compares v + u against the high bound.
    if (lfVDet + lfUDet > lfHighBound)                  // fcmpu / bgt
    {
        return 0;
    }

    // ---- t interval ---------------------------------------------------------
    const f32 lfTDet = Dot3(lvEdge2, lvQVec);           // vmsum3fp128 v0, v12, v0

    // stfs f13, 0x40(r11): the RAW t*det is stored to the caller's lineParam
    // slot BEFORE the interval test -- a reject below leaves it there.
    lpResult->lineParam = lfTDet;

    if (lfTDet < lfLowBound || lfTDet > lfHighBound)    // fcmpu blt / bgt
    {
        return 0;
    }

    // ---- hit: divide through by the determinant and fill the result --------
    // vrefp + two Newton-Raphson refine steps on the splatted determinant
    // (@ 0x82BB9FCC..0x82BB9FE8), rendered exact.
    const f32 lfRecipDet = 1.0f / lfDet;                // v22 lane 0

    // fmuls f13, f0, f13 / stfs f13, 0(r10): overwrite the raw store with t.
    const f32 lfT = lfRecipDet * lfTDet;
    lpResult->lineParam = lfT;                          // +0x40

    // volParam row assembled on the stack in lane order (u, v, 0.0f, 0).
    lpResult->volParam = MakeVec4(lfRecipDet * lfUDet,
                                  lfRecipDet * lfVDet,
                                  0.0f, 0.0f);          // r11+0x30

    // vmaddfp v0, v2, v1, v0 (== delta * splat(t) + start; the splat is the
    // lvlx/vspltw reload of the lineParam field just stored) -> +0x10.
    lpResult->position = MaddScalar(aLineDelta, lfT, aLineStart);   // r11+0x10

    return 1;                                           // li r3, 1
}

// ===========================================================================
// rw::collision::TriangleLineSegIntersect @ 0x82BBB7B8
// Called by: rw::collision::TriangleKDTreeProcedural::LineIntersectionQueryThis,
//            rw::collision::TriangleVolume::LineSegIntersect (both pending).
//
// rodata: flt_82001CC0 = 0.0f (the fatness == 0 dispatch compare).
//
// VMX lowering notes: the prologue's v125/v126/v127 saves are the callee-
// saved VMX128 bank spill (keeping V0/V1/V2 across the calls); they lower to
// ordinary by-value parameters. The unit-normal block is emitted TWICE by the
// compiler (thin-hit path @ 0x82BBB818, fat prologue @ 0x82BBB884) with
// identical instructions; it is factored into the TU-local helper below. Its
// vrsqrtefp + TWO Newton-Raphson steps are rendered exact; no zero guard is
// emitted (degenerate triangles propagate INF exactly as the console does in
// this scalar form). The fat-hit position pull-back is a splat(fatness)
// vmulfp128 + vsubfp over all four lanes.
// ===========================================================================

namespace
{
    // The unit geometric normal of (V0, V1, V2): normalize(Cross(V0-V1, V0-V2)).
    // Emitted twice inline on the X360 (see the lowering notes above); the
    // subtraction ORDER (V0 - V1, V0 - V2) is the asm's.
    inline Vec4 TriangleUnitNormal(const Vec4& arV0, const Vec4& arV1, const Vec4& arV2)
    {
        const Vec4 lvCross = Cross(Sub(arV0, arV1), Sub(arV0, arV2));
        const f32  lfLenSq = Dot3(lvCross, lvCross);    // vmsum3fp128 v0, v13, v13
        // vrsqrtefp + two Newton-Raphson steps, rendered exact.
        return Scale(lvCross, 1.0f / std::sqrt(lfLenSq));
    }
}

s32 TriangleLineSegIntersect(VolumeLineSegIntersectResult* lpResult,   // r3 (r31)
                             Vec4 aLineStart,                          // v1
                             Vec4 aLineDelta,                          // v2
                             Vec4 aV0,                                 // v3 -> v127
                             Vec4 aV1,                                 // v4 -> v126
                             Vec4 aV2,                                 // v5 -> v125
                             f32  afFatness)                           // f1 -> f31
{
    s32 liHit;                                                         // r3

    if (afFatness == 0.0f)                              // fcmpu vs flt_82001CC0 / bne
    {
        // ---- thin path: test first, derive the normal only on a hit --------
        liHit = ThinTriangleLineSegIntersect(lpResult, aLineStart, aLineDelta,
                                             aV0, aV1, aV2);   // bl 0x82BB9EB8
        if (liHit)                                      // cmplwi / beq
        {
            // stvx128 v0, r31, 0x20: the unit face normal.
            lpResult->normal = TriangleUnitNormal(aV0, aV1, aV2);
        }
    }
    else
    {
        // ---- fat path: the normal is an INPUT to the fat tester ------------
        // stvx128 v0, r0, r30 (r30 = result + 0x20) BEFORE the call: the
        // FatTriangleLineSegIntersect above reads +0x20 as the caller-seeded
        // unit plane normal (its documented input contract).
        lpResult->normal = TriangleUnitNormal(aV0, aV1, aV2);

        liHit = FatTriangleLineSegIntersect(lpResult, aLineStart, aLineDelta,
                                            aV0, aV1, aV2, afFatness);   // bl 0x82BBAD98
        if (liHit)                                      // cmplwi / beq
        {
            // Pull the fat-surface hit back onto the core triangle:
            //   position -= normal * fatness
            // (the normal is reloaded from the result block, i.e. the fat
            // tester's OUTPUT normal; all four lanes.)
            lpResult->position =
                Sub(lpResult->position, Scale(lpResult->normal, afFatness));
        }
    }

    return liHit;                                       // r3 pass-through
}

} // namespace collision
} // namespace rw
