// FX-FOLLOWUPS (crash parity 2026-09-25, stage (b)): the three volume line kernels the VolumeLineQuery walk calls
// through the descriptor's lineSegIntersect slot, and the slab test behind the box's.
//   rw::collision::SphereVolume::LineSegIntersect   @ 0x82BA82C8
//   rw::collision::BoxVolume::LineSegIntersect      @ 0x82BA9478
//   rw::collision::CapsuleVolume::LineSegIntersect  @ 0x82BAFCF8
//   rw::collision::rwcPlaneLineSegIntersect         @ 0x82BA8818   (all four in LineSegIntersect.cpp)
// run_fxfollowups_line_kernels.py compiles the revision's LineSegIntersect.cpp (the home of all four) beside this file,
// with the revision's headers shadowed in. A revision without the kernels cannot build this file: every numeric check
// then counts as failed.
//
// What is checked, against the ARTIST listing:
//   S  the sphere: the hit (t, position, normal) from outside, the fatness pull-back, a start inside (the guarded
//      normalise), a start at the centre (|n| == 0 keeps n), a miss (only result.v written), and the centre put
//      through tm with FUSED vmaddfp (0x82BA830C..0x82BA831C) -- an input where the unfused chain differs.
//   B  the box: the inside arm (largest separation), a face-plane hit, a start on the fattened face, a corner-sphere
//      hit and a start inside it, a start inside the edge cylinder and an edge-cylinder hit, and two walks: corner ->
//      edge -> cylinder and edge -> face -> face plane, where lineParam SUMS the step fractions (each over the
//      remaining segment); volParam carries the region codes; a frame + tm case back in world space; the frame round
//      trip rounded the console's way (fused ComposeFrame / ToLocal / FramePoint) on an input where unfused differs.
//   C  the capsule: a barrel hit, a cap-sphere hit, a walk cap -> barrel -> cylinder, a start inside the barrel and
//      inside a cap, a miss that leaves the last point in result.position, and volParam = (cap, delta.z, start.z, 0).
//   P  the plane test: every return code and the ONE rounding of fmsubs (an input where two roundings differ).
//   H  the rounding idioms over many inputs: vmsum3fp128 as one rounding of the f64 sum, vnmsubfp's negated fused
//      result (an exact cancellation is -0), the refined 1/x and 1/sqrt(x) (vrefp / vrsqrtefp + two fused steps).
#include "types.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>

#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "vendor/renderware/collision/CapsuleVolume.hpp"
#include "vendor/renderware/collision/LineSegIntersect.hpp"
#include "vendor/renderware/collision/LineSegKernelMath.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"   // TriangleNearestPointRegion (below)

static unsigned guChecks = 0, guFailures = 0, guUnexpected = 0;
static void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass) ++guFailures;
    std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
}

// The three kernels and the plane test are compiled from the revision's LineSegIntersect.cpp, beside this file.
namespace rw { namespace collision {
// LineSegIntersect.cpp's fat triangle walk calls it; nothing here reaches that walk.
s32 TriangleNearestPointRegion(Vec4*, f32*, f32*, Vec4, Vec4, Vec4, Vec4) { ++guUnexpected; return 0; }
} }

using namespace rw::collision;

// ---- helpers ----------------------------------------------------------------------------------------------------
static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static Vec4 V(f32 x, f32 y, f32 z, f32 w = 0.0f) { Vec4 l = { x, y, z, w }; return l; }
static bool Same(const Vec4& a, const Vec4& b) { return std::memcmp(&a, &b, sizeof(Vec4)) == 0; }
static bool Near(f64 a, f64 b, f64 tol) { return std::fabs(a - b) <= tol; }
static bool Near3(const Vec4& a, f64 x, f64 y, f64 z, f64 tol) { return Near(a.x, x, tol) && Near(a.y, y, tol) && Near(a.z, z, tol); }
static void Print(const char* lpc, const Vec4& a) { std::printf("  %s (%.9g, %.9g, %.9g, %.9g)\n", lpc, a.x, a.y, a.z, a.w); }

static const u32 KU_SENTINEL = 0xA5A5A5A5u;
static void Fill(VolumeLineSegIntersectResult& lr) { std::memset(&lr, 0xA5, sizeof(lr)); }
static bool Untouched(const Vec4& a) { return Bits(a.x) == KU_SENTINEL && Bits(a.y) == KU_SENTINEL && Bits(a.z) == KU_SENTINEL && Bits(a.w) == KU_SENTINEL; }
static bool Untouched(f32 a) { return Bits(a) == KU_SENTINEL; }

// Row 3 (the translation) is all zero, w included -- as BoxVolume.cpp's ConstructVolume seeds it -- so every result
// lane below is exact.
static const Vec4 KV_IDENTITY[4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 0 } };

static SphereVolume MakeSphere(const Vec4& c, f32 r)
{
    SphereVolume l; std::memset(&l, 0, sizeof(l));
    l.maTransform[0] = KV_IDENTITY[0]; l.maTransform[1] = KV_IDENTITY[1]; l.maTransform[2] = KV_IDENTITY[2];
    l.maTransform[3] = c; l.mfRadius = r; l.muVTableSlot = E_VOLUMETYPE_SPHERE; l.muFlags = 1;
    return l;
}
static BoxVolume MakeBox(f32 hx, f32 hy, f32 hz, f32 r, const Vec4* lpFrame = KV_IDENTITY)
{
    BoxVolume l; std::memset(&l, 0, sizeof(l));
    for (int i = 0; i < 4; ++i) l.maTransform[i] = lpFrame[i];
    l.mBoxData.mfHx = hx; l.mBoxData.mfHy = hy; l.mBoxData.mfHz = hz; l.mfRadius = r;
    l.muVTableSlot = E_VOLUMETYPE_BBOX; l.muFlags = 1;
    return l;
}
static CapsuleVolume MakeCapsule(f32 hh, f32 r)
{
    CapsuleVolume l; std::memset(&l, 0, sizeof(l));
    for (int i = 0; i < 4; ++i) l.maFrame[i] = KV_IDENTITY[i];
    l.maFrame[3] = V(0, 0, 0, 0); l.mfHalfHeight = hh; l.mfRadius = r; l.muFlags = 1;
    return l;
}

// ---- the independent models of the console idioms (written here, not taken from the production header) ---------
static f32 MDot3(const Vec4& a, const Vec4& b) { return (f32)((f64)a.x * b.x + (f64)a.y * b.y + (f64)a.z * b.z); }
static f32 MNmsub(f32 a, f32 c, f32 b) { f32 d = std::fma(a, c, -b); return (d != d) ? d : -d; }
static f32 MRecip(f32 x) { f32 e = (f32)(1.0 / (f64)x); for (int i = 0; i < 2; ++i) { f32 r = MNmsub(e, x, 1.0f); e = std::fma(e, r, e); } return e; }
static f32 MRsqrt(f32 x)
{
    f32 e = (f32)(1.0 / std::sqrt((f64)x));
    for (int i = 0; i < 2; ++i) { f32 sq = e * e; f32 h = e * 0.5f; f32 r = MNmsub(x, sq, 1.0f); e = std::fma(h, r, e); }
    return e;
}
static Vec4 MFma(const Vec4& a, f32 s, const Vec4& b) { return V(std::fma(a.x, s, b.x), std::fma(a.y, s, b.y), std::fma(a.z, s, b.z), std::fma(a.w, s, b.w)); }
static Vec4 MMad(const Vec4& a, f32 s, const Vec4& b) { return V(a.x * s + b.x, a.y * s + b.y, a.z * s + b.z, a.w * s + b.w); }   // unfused (mutant model)
static Vec4 MMul(const Vec4& a, f32 s) { return V(a.x * s, a.y * s, a.z * s, a.w * s); }
static Vec4 MSub(const Vec4& a, const Vec4& b) { return V(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
// The frame chains, fused (the console) or not (the mutant).
static void MCompose(const Vec4* B, const Vec4* T, Vec4 (&W)[4], bool fused)
{
    Vec4 (*mad)(const Vec4&, f32, const Vec4&) = fused ? MFma : MMad;
    for (int r = 0; r < 3; ++r) W[r] = mad(T[2], B[r].z, mad(T[1], B[r].y, MMul(T[0], B[r].x)));
    W[3] = mad(T[2], B[3].z, mad(T[1], B[3].y, mad(T[0], B[3].x, T[3])));
}
static Vec4 MToLocal(const Vec4 (&W)[4], const Vec4& p, bool fused)
{
    Vec4 (*mad)(const Vec4&, f32, const Vec4&) = fused ? MFma : MMad;
    const Vec4 c0 = V(W[0].x, W[1].x, W[2].x, 0), c1 = V(W[0].y, W[1].y, W[2].y, 0), c2 = V(W[0].z, W[1].z, W[2].z, 0);
    const Vec4 n = MSub(V(0, 0, 0, 0), W[3]);
    const Vec4 t = mad(c0, n.x, mad(c1, n.y, MMul(c2, n.z)));
    return mad(c2, p.z, mad(c1, p.y, mad(c0, p.x, t)));
}
static Vec4 MPoint(const Vec4* W, const Vec4& p, bool fused)
{
    Vec4 (*mad)(const Vec4&, f32, const Vec4&) = fused ? MFma : MMad;
    return mad(W[2], p.z, mad(W[1], p.y, mad(W[0], p.x, W[3])));
}
static Vec4 MDir(const Vec4* W, const Vec4& n, bool fused)
{
    Vec4 (*mad)(const Vec4&, f32, const Vec4&) = fused ? MFma : MMad;
    return mad(W[2], n.z, mad(W[1], n.y, MMul(W[0], n.x)));
}

// ---- double-precision references for the geometry (first contact along pt1 -> pt2) -----------------------------
static f64 SdfRoundedBox(f64 x, f64 y, f64 z, f64 hx, f64 hy, f64 hz, f64 r)
{
    const f64 qx = std::fabs(x) - hx, qy = std::fabs(y) - hy, qz = std::fabs(z) - hz;
    const f64 ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0, oz = qz > 0 ? qz : 0;
    const f64 inner = std::fmin(std::fmax(qx, std::fmax(qy, qz)), 0.0);
    return std::sqrt(ox * ox + oy * oy + oz * oz) + inner - r;
}
static f64 SdfCapsule(f64 x, f64 y, f64 z, f64 hh, f64 r)
{
    const f64 cz = z < -hh ? -hh : (z > hh ? hh : z);
    return std::sqrt(x * x + y * y + (z - cz) * (z - cz)) - r;
}
template <typename F> static f64 FirstContact(F sdf, const Vec4& a, const Vec4& b)
{
    const int N = 20000;
    f64 prev = 0.0;
    for (int i = 1; i <= N; ++i)
    {
        const f64 s = (f64)i / N;
        if (sdf(a.x + s * (b.x - a.x), a.y + s * (b.y - a.y), a.z + s * (b.z - a.z)) <= 0.0)
        {
            f64 lo = prev, hi = s;
            for (int k = 0; k < 80; ++k)
            {
                const f64 m = 0.5 * (lo + hi);
                if (sdf(a.x + m * (b.x - a.x), a.y + m * (b.y - a.y), a.z + m * (b.z - a.z)) <= 0.0) hi = m; else lo = m;
            }
            return hi;
        }
        prev = s;
    }
    return -1.0;
}

int main()
{
    // ================================================================================================= P. the plane
    {
        Fraction d;
        s32 rc = rwcPlaneLineSegIntersect(&d, 4.0f, -8.0f, 1.0f, 2.0f);
        Check(rc == 1 && d.num == 2.0f && d.den == 8.0f,
              "P1 plane: num = orig*sign - disp = 2, den = -(seg*sign) = 8, num < den -> 1 (0x82BA8818..0x82BA8878)");
        rc = rwcPlaneLineSegIntersect(&d, 1.5f, -8.0f, 1.0f, 2.0f);
        Check(rc == 1 && Bits(d.num) == Bits(0.0f) && d.den == 1.0f,
              "P2 plane: a start on or past the plane (num NOT > 0) -> {0.0, 1.0}, 1 (flt_82001CC0 / flt_82001C98)");
        rc = rwcPlaneLineSegIntersect(&d, 4.0f, 8.0f, 1.0f, 2.0f);
        const bool lbAway = (rc == -1 && d.num == 2.0f && d.den == -8.0f);
        rc = rwcPlaneLineSegIntersect(&d, 4.0f, -0.0f, 1.0f, 2.0f);
        Check(lbAway && rc == -1, "P3 plane: moving away (den < 2^-128, flt_821801B0) and parallel -> -1, num/den stored first");
        rc = rwcPlaneLineSegIntersect(&d, 4.0f, -1.0f, 1.0f, 2.0f);
        const bool lbBeyond = (rc == 0 && d.num == 2.0f && d.den == 1.0f);
        rc = rwcPlaneLineSegIntersect(&d, 1.0e30f, -1.0e-10f, 1.0f, 0.0f);
        Check(lbBeyond && rc == -1, "P4 plane: a crossing past the segment end -> 0; den < num * 2^-128 -> -1");
        rc = rwcPlaneLineSegIntersect(&d, std::nanf(""), -8.0f, 1.0f, 2.0f);
        Check(rc == 1 && Bits(d.num) == Bits(0.0f) && d.den == 1.0f, "P5 plane: a NaN num is NOT > 0 -> {0, 1}, 1 (fcmpu ; bgt)");
        // fmsubs: ONE rounding. orig = 1 + 3*2^-23, sign = 1 + 2^-23, disp = 1: fused 2^-21 + 2^-44, two roundings 2^-21.
        const f32 lfO = FromBits(0x3F800003u), lfS = FromBits(0x3F800001u);
        rc = rwcPlaneLineSegIntersect(&d, lfO, -1.0f, lfS, 1.0f);
        const f32 lfFused = std::fma(lfO, lfS, -1.0f), lfTwice = lfO * lfS - 1.0f;
        std::printf("  fmsubs num 0x%08X (fused 0x%08X, two roundings 0x%08X)\n", Bits(d.num), Bits(lfFused), Bits(lfTwice));
        Check(Bits(d.num) == Bits(lfFused) && Bits(lfFused) != Bits(lfTwice),
              "P6 plane: num is fmsubs f0, f1, f3, f4 -- ONE rounding (ROUNDING_RULE rule 3), on an input where two differ");
        rc = rwcPlaneLineSegIntersect(&d, 4.0f, -2.0f, 1.0f, 2.0f);
        Check(rc == 0 && d.num == 2.0f && d.den == 2.0f,
              "P7 plane: a crossing exactly at the segment end (num == den) is NOT inside -> 0 (`fcmpu num, den ; bltlr`)");
    }

    // ================================================================================================= S. the sphere
    {
        SphereVolume s = MakeSphere(V(0, 0, 0, 0), 1.0f);
        VolumeLineSegIntersectResult r; Fill(r);
        RwBool rc = s.LineSegIntersect(V(-4, 0, 0), V(4, 0, 0), 0, r, 0.0f);
        Check(rc == 1 && r.v == reinterpret_cast<uintptr_t>(&s) && r.lineParam == 0.375f && Same(r.position, V(-1, 0, 0))
              && Same(r.normal, V(-1, 0, 0)) && Untouched(r.volParam),
              "S1 sphere from outside: t = 24/64 = 0.375, position (-1, 0, 0), normal (hit - centre) * 1/R, volParam not written");

        s = MakeSphere(V(0, 0, 0, 0), 1.0f); Fill(r);
        rc = s.LineSegIntersect(V(-4, 0, 0), V(4, 0, 0), 0, r, 0.5f);
        // R = 1.5: t = (32 - 12) / 64; hit (-1.5, 0, 0); normal = (-1.5) * refined 1/1.5; position -= normal * 0.5.
        const f32 lfN = -1.5f * MRecip(1.5f);
        Check(rc == 1 && r.lineParam == 0.3125f && Bits(r.normal.x) == Bits(lfN) && r.normal.y == 0.0f
              && Bits(r.position.x) == Bits(-1.5f - lfN * 0.5f),
              "S2 sphere, fatness 0.5: R = radius + fatness (fadds), normal * refined 1/R (vrefp + 2 steps), position -= normal * fatness");

        s = MakeSphere(V(0, 0, 0, 0), 2.0f); Fill(r);
        rc = s.LineSegIntersect(V(0.5f, 0, 0), V(4, 0, 0), 0, r, 0.25f);
        Check(rc == 1 && r.lineParam == 0.0f && Same(r.normal, V(1, 0, 0)) && r.position.x == 0.25f,
              "S3 sphere, start inside: t = 0, the normal NORMALISED through the guarded length (num <= 0 arm, |n| > FLT_MIN)");

        s = MakeSphere(V(1, 2, 3, 0), 2.0f); Fill(r);
        rc = s.LineSegIntersect(V(1, 2, 3), V(4, 0, 0), 0, r, 0.25f);
        Check(rc == 1 && Same(r.normal, V(0, 0, 0, 0)) && Same(r.position, V(1, 2, 3, 0)),
              "S4 sphere, start at the centre: |n|^2 == 0 maps to 0 (vcmpeqfp / vsel), NOT > FLT_MIN (unk_821800C0): n stays 0");

        s = MakeSphere(V(0, 0, 0, 0), 1.0f); Fill(r);
        rc = s.LineSegIntersect(V(-4, 3, 0), V(4, 3, 0), 0, r, 0.0f);
        Check(rc == 0 && r.v == reinterpret_cast<uintptr_t>(&s) && Untouched(r.lineParam) && Untouched(r.position)
              && Untouched(r.normal), "S5 sphere miss: 0, and only result.v is written (stw @0x82BA8328 precedes the test)");

        // S6: the centre through tm, FUSED (0x82BA830C / 0x82BA8318 / 0x82BA831C). Search an input where the fused and
        // unfused centres differ and the start lies inside (so the normal shows the centre exactly: n = pt1 - centre,
        // normalised by the refined 1/sqrt).
        std::mt19937 lRng(1234);
        std::uniform_real_distribution<f32> lU(-3.0f, 3.0f);
        bool lbFound = false, lbMatch = false;
        for (int k = 0; k < 20000 && !lbFound; ++k)
        {
            Vec4 T[4] = { V(lU(lRng), lU(lRng), lU(lRng), 0), V(lU(lRng), lU(lRng), lU(lRng), 0), V(lU(lRng), lU(lRng), lU(lRng), 0),
                          V(lU(lRng), lU(lRng), lU(lRng), 1) };
            const Vec4 c = V(lU(lRng), lU(lRng), lU(lRng), 1);
            const Vec4 cf = MPoint(T, c, true), cu = MPoint(T, c, false);
            if (Same(cf, cu)) continue;
            lbFound = true;
            SphereVolume ss = MakeSphere(c, 50.0f);
            const Vec4 p1 = V(cf.x + 0.75f, cf.y - 0.5f, cf.z + 0.25f);
            Fill(r);
            rc = ss.LineSegIntersect(p1, V(p1.x + 1, p1.y, p1.z), T, r, 0.0f);
            const Vec4 n = MSub(p1, cf);
            const Vec4 nExp = MMul(n, MRsqrt(MDot3(n, n)));
            lbMatch = (rc == 1 && Same(r.normal, nExp) && !Same(MMul(MSub(p1, cu), MRsqrt(MDot3(MSub(p1, cu), MSub(p1, cu)))), nExp));
            Print("S6 normal", r.normal); Print("S6 fused model", nExp);
        }
        Check(lbFound && lbMatch, "S6 sphere through tm: the centre is fma(row2, c.z, fma(row1, c.y, fma(row0, c.x, row3))) -- "
                                  "ONE rounding per vmaddfp; an input where the unfused centre gives another normal");
    }

    // ================================================================================================= B. the box
    {
        // B1 the start inside: the largest of -h - p / p - h (here p.x - hx = -0.5) gives the normal, t = 0.
        BoxVolume b = MakeBox(1, 2, 3, 0.0f);
        VolumeLineSegIntersectResult r; Fill(r);
        RwBool rc = b.LineSegIntersect(V(0.5f, 0.25f, -1), V(5, 5, 5), 0, r, 0.125f);
        Check(rc == 1 && r.v == reinterpret_cast<uintptr_t>(&b) && r.lineParam == 0.0f && Same(r.normal, V(1, 0, 0, 0))
              && Same(r.position, V(0.375f, 0.25f, -1, 0)) && Same(r.volParam, V(0, 0, 0, 0)),
              "B1 box, start inside (3 inside axes): normal = the largest separation's axis * sign, position = start - normal*fatness");

        // B2 a face-plane hit: x outside (+1), y / z inside -> face x; plane(4, -8, +1, h + R = 2) = 2/8.
        b = MakeBox(1, 2, 3, 0.5f); Fill(r);
        rc = b.LineSegIntersect(V(4, 0.5f, 0.5f), V(-4, 0.5f, 0.5f), 0, r, 0.5f);
        Check(rc == 1 && r.lineParam == 0.25f && Same(r.normal, V(1, 0, 0, 0)) && Same(r.position, V(1.5f, 0.5f, 0.5f, 0))
              && Same(r.volParam, V(1, 0, 0, 0)),
              "B2 box face: the fattened face plane (h + R) hit at t = 0.25; normal = axis * region; volParam = the region codes");

        // B3 a start on / past the fattened face plane: an immediate hit at the start.
        b = MakeBox(1, 2, 3, 1.0f); Fill(r);
        rc = b.LineSegIntersect(V(1.5f, 0, 0), V(-4, 0, 0), 0, r, 0.0f);
        Check(rc == 1 && r.lineParam == 0.0f && Same(r.position, V(1.5f, 0, 0, 0)) && Same(r.normal, V(1, 0, 0, 0)),
              "B3 box face, start within the fattened face: {0, 1} -> the hit is the start (0x82BA9E30)");

        // B4 the corner sphere: (3,3,3) -> (-1,-1,-1), R = 0.5, corner (1,1,1).
        b = MakeBox(1, 1, 1, 0.5f); Fill(r);
        rc = b.LineSegIntersect(V(3, 3, 3), V(-1, -1, -1), 0, r, 0.0f);
        Fraction d; Vec4 P = V(3, 3, 3, 0), D = V(-4, -4, -4, 0), C = V(1, 1, 1, 0);
        rwcSphereLineSegIntersect(&d, &P, &D, &C, 0.5f);
        f32 t = d.num / d.den;
        Vec4 pos = MFma(D, t, P);
        Vec4 nrm = MMul(MSub(pos, C), MRecip(0.5f));
        Check(rc == 1 && r.lineParam == t && Same(r.position, pos) && Same(r.normal, nrm) && Same(r.volParam, V(1, 1, 1, 0))
              && Near(t, 0.4278312, 1e-6),
              "B4 box corner: the corner sphere (radius R) before the three faces; hit = delta*t + start (fused), normal * refined 1/R");

        // B5 a start inside the corner sphere.
        b = MakeBox(1, 1, 1, 0.5f); Fill(r);
        rc = b.LineSegIntersect(V(1.25f, 1.125f, 1.0625f), V(5, 5, 5), 0, r, 0.0f);
        const Vec4 lvOut = V(0.25f, 0.125f, 0.0625f, 0);
        Check(rc == 1 && r.lineParam == 0.0f && Same(r.position, V(1.25f, 1.125f, 1.0625f, 0))
              && Same(r.normal, MMul(lvOut, MRsqrt(MDot3(lvOut, lvOut)))),
              "B5 box corner, start inside the sphere: the start, (start - corner) * refined 1/sqrt (0x82BA9CA8)");

        // B6 a start inside the edge cylinder (edge z of the (+,+) corner column).
        b = MakeBox(1, 1, 1, 0.5f); Fill(r);
        rc = b.LineSegIntersect(V(1.25f, 1.125f, 0.5f), V(5, 5, 5), 0, r, 0.0f);
        const Vec4 lvRad = V(0.25f, 0.125f, 0, 0);
        Check(rc == 1 && r.lineParam == 0.0f && Same(r.position, V(1.25f, 1.125f, 0.5f, 0))
              && Same(r.normal, MMul(lvRad, MRsqrt(MDot3(lvRad, lvRad)))) && Same(r.volParam, V(1, 1, 0, 0)),
              "B6 box edge, start inside the cylinder: the radial part (edge axis zeroed) * refined 1/sqrt (0x82BA9D6C)");

        // B7 an edge-cylinder hit: (3,3,0) -> (-1,-1,0), edge z, radius 0.5; both faces (t 0.5) and the cap (t 1) come later.
        b = MakeBox(1, 1, 1, 0.5f); Fill(r);
        rc = b.LineSegIntersect(V(3, 3, 0), V(-1, -1, 0), 0, r, 0.0f);
        P = V(3, 3, 0, 0); D = V(-4, -4, 0, 0);
        rwcCylinderLineSegIntersect(&d, 1.0f, 0.5f, 0, 0, P, D, V(1, 1, 0, 0), V(0, 0, 1, 0));
        t = d.num / d.den; pos = MFma(D, t, P);
        Vec4 rad = MSub(pos, V(1, 1, 0, 0)); rad.z = 0.0f;
        nrm = MMul(rad, MRecip(0.5f));
        Check(rc == 1 && r.lineParam == t && Same(r.position, pos) && Same(r.normal, nrm) && Same(r.volParam, V(1, 1, 0, 0))
              && Near(t, 0.4116117, 1e-6),
              "B7 box edge: the cylinder along the edge (|axis|^2 1, radius R) before the faces and the cap; normal = radial * 1/R");

        // B8 the walk corner -> edge -> cylinder: (3,3,3) -> (-1,-1,-5). The z face comes first (t 0.25 of the
        // segment), then the edge cylinder at u = (1 - 0.5/sqrt2)/3 of the REMAINING segment: lineParam = 0.25 + u.
        b = MakeBox(1, 1, 1, 0.5f); Fill(r);
        rc = b.LineSegIntersect(V(3, 3, 3), V(-1, -1, -5), 0, r, 0.0f);
        const f64 lfU = (1.0 - 0.5 / std::sqrt(2.0)) / 3.0;
        const f64 lfTrue = FirstContact([](f64 x, f64 y, f64 z) { return SdfRoundedBox(x, y, z, 1, 1, 1, 0.5); }, V(3, 3, 3), V(-1, -1, -5));
        std::printf("  B8 lineParam %.9g (0.25 + u = %.9g), true contact s = %.9g\n", r.lineParam, 0.25 + lfU, lfTrue);
        Check(rc == 1 && Near(r.lineParam, 0.25 + lfU, 2e-6) && Near3(r.position, 3 - 4 * lfTrue, 3 - 4 * lfTrue, 3 - 8 * lfTrue, 2e-5)
              && Near3(r.normal, std::sqrt(0.5), std::sqrt(0.5), 0.0, 2e-6) && Same(r.volParam, V(1, 1, 0, 0)),
              "B8 box walk corner -> edge -> cylinder: the corner's z face first, then the edge cylinder; lineParam SUMS the step "
              "fractions (0.25 + u), the position is the true contact, volParam (1, 1, 0)");

        // B9 the walk edge -> face -> face plane: (2, 1.5, 0) -> (0, -0.5, 0), R = 0.25. The y face (t 0.25) comes before the
        // x face (t 0.5) and the cap; then the fattened x face at 0.25/1.5 of the remainder: lineParam = 0.25 + 1/6.
        b = MakeBox(1, 1, 1, 0.25f); Fill(r);
        rc = b.LineSegIntersect(V(2, 1.5f, 0), V(0, -0.5f, 0), 0, r, 0.0f);
        std::printf("  B9 lineParam %.9g\n", r.lineParam);
        Check(rc == 1 && Near(r.lineParam, 0.25 + 1.0 / 6.0, 1e-6) && Near3(r.position, 1.25, 0.75, 0.0, 1e-6)
              && Same(r.normal, V(1, 0, 0, 0)) && Same(r.volParam, V(1, 0, 0, 0)),
              "B9 box walk edge -> face -> face plane: the region becomes a face (luAxis = 3 - j - e), the hit (1.25, 0.75, 0)");

        // B10 a miss after walking: the line passes the rounded edge by (distance 0.354 > 0.25) and recedes from the y face.
        b = MakeBox(1, 1, 1, 0.25f); Fill(r);
        rc = b.LineSegIntersect(V(2, 0.5f, 0), V(0, 2.5f, 0), 0, r, 0.0f);
        Check(rc == 0 && r.v == reinterpret_cast<uintptr_t>(&b) && Untouched(r.position) && Untouched(r.normal) && r.lineParam > 0.0f,
              "B10 box miss after a walk: 0; lineParam holds the steps taken, position / normal are not written");

        // B13 the walk edge -> corner (the cap along the edge) -> corner sphere: (1.5, 1.5, -0.95) -> (1, 1, -1.15), R = 0.5.
        // The end cap z = -1 (t 0.25) comes before the edge cylinder (t 0.293): region z becomes dir z (-1), then the
        // sphere at the corner (1, 1, -1).
        b = MakeBox(1, 1, 1, 0.5f); Fill(r);
        rc = b.LineSegIntersect(V(1.5f, 1.5f, -0.95f), V(1, 1, -1.15f), 0, r, 0.0f);
        {
            const f64 lfS = FirstContact([](f64 x, f64 y, f64 z) { return SdfRoundedBox(x, y, z, 1, 1, 1, 0.5); },
                                         V(1.5f, 1.5f, -0.95f), V(1, 1, -1.15f));
            std::printf("  B13 lineParam %.9g, true contact s = %.9g\n", r.lineParam, lfS);
            Check(rc == 1 && Near3(r.position, 1.5 - 0.5 * lfS, 1.5 - 0.5 * lfS, -0.95 - 0.2 * lfS, 2e-5)
                  && Same(r.volParam, V(1, 1, -1, 0)) && r.lineParam > 0.25f && r.lineParam < 0.35f
                  && r.normal.z < 0.0f && Near(r.normal.x, r.normal.y, 1e-7),
                  "B13 box walk edge -> corner -> sphere: the edge's end cap first makes region z = dir z (-1), then the corner "
                  "sphere at (1, 1, -1) is the hit; volParam (1, 1, -1)");
        }

        // B14 the walk face -> edge -> cylinder: (1.8, 0.5, 0.2) -> (0.8, 1.5, 0.4), R = 0.25. The in-face exit through y = 1
        // (t 0.5) comes before the fattened x face (t 0.55): the edge is z (luAxis = 3 - y - x), and its cylinder about
        // (1, 1) is hit at u = 0.3 - sqrt(0.035) of the remainder. (The line climbs in z so that a wrong edge axis --
        // a sphere at (1, 1, 0) -- cannot give the same answer.)
        b = MakeBox(1, 1, 1, 0.25f); Fill(r);
        rc = b.LineSegIntersect(V(1.8f, 0.5f, 0.2f), V(0.8f, 1.5f, 0.4f), 0, r, 0.0f);
        {
            const f64 lfS = FirstContact([](f64 x, f64 y, f64 z) { return SdfRoundedBox(x, y, z, 1, 1, 1, 0.25); },
                                         V(1.8f, 0.5f, 0.2f), V(0.8f, 1.5f, 0.4f));
            const f64 lfU = 0.3 - std::sqrt(0.035);
            std::printf("  B14 lineParam %.9g (0.5 + u = %.9g), true contact s = %.9g\n", r.lineParam, 0.5 + lfU, lfS);
            Check(rc == 1 && Near(r.lineParam, 0.5 + lfU, 2e-6) && Near3(r.position, 1.8 - lfS, 0.5 + lfS, 0.2 + 0.2 * lfS, 2e-5)
                  && Same(r.volParam, V(1, 1, 0, 0)) && Near(r.normal.z, 0.0, 1e-7) && r.normal.x > 0.9f,
                  "B14 box walk face -> edge -> cylinder: the exit makes the edge z (luAxis = 3 - j - n), the rounded edge "
                  "about (1, 1) is the hit; volParam (1, 1, 0)");
        }

        // B15 the walk corner -> edge -> face -> face plane: (1.6, 1.2, 1.5) -> (1.1, 0.2, -0.5), R = 0.25. Three step
        // fractions: y at 0.2 (the corner's earliest face), z at 0.1/1.6 of the rest (the edge's), then the fattened x
        // face at 0.225/0.375 of the rest: lineParam = 0.2 + 0.0625 + 0.6.
        b = MakeBox(1, 1, 1, 0.25f); Fill(r);
        rc = b.LineSegIntersect(V(1.6f, 1.2f, 1.5f), V(1.1f, 0.2f, -0.5f), 0, r, 0.0f);
        std::printf("  B15 lineParam %.9g\n", r.lineParam);
        Check(rc == 1 && Near(r.lineParam, 0.8625, 2e-6) && Near3(r.position, 1.25, 0.5, 0.1, 2e-6)
              && Same(r.normal, V(1, 0, 0, 0)) && Same(r.volParam, V(1, 0, 0, 0)),
              "B15 box walk corner -> edge -> face -> face plane: every step ADDS its fraction to lineParam (0.2 + 0.0625 + 0.6)");

        // B11 a frame and tm: the box rotated a quarter turn about z (local x = world y) at (10, 0, 0), tm moves it by
        // (0, 5, 0): the face-plane hit comes back in world space.
        const Vec4 F[4] = { V(0, 1, 0, 0), V(-1, 0, 0, 0), V(0, 0, 1, 0), V(10, 0, 0, 1) };
        const Vec4 T[4] = { V(1, 0, 0, 0), V(0, 1, 0, 0), V(0, 0, 1, 0), V(0, 5, 0, 1) };
        b = MakeBox(1, 2, 3, 1.0f, F); Fill(r);
        rc = b.LineSegIntersect(V(10, 9, 0.5f), V(10, 1, 0.5f), T, r, 0.0f);
        Check(rc == 1 && r.lineParam == 0.25f && Same(r.position, V(10, 7, 0.5f, 1)) && Same(r.normal, V(0, 1, 0, 0))
              && Same(r.volParam, V(1, 0, 0, 0)),
              "B11 box with a frame and tm: the line into the composed frame and the hit / normal back out (world (10, 7, 0.5))");

        // B12 the frame round trip rounded the console's way: the inside arm returns the start through ComposeFrame /
        // ToLocal / FramePoint and the normal through FrameDirection. Search an input where fused and unfused differ.
        std::mt19937 lRng(99);
        std::uniform_real_distribution<f32> lA(0.0f, 6.2831853f), lT(-50.0f, 50.0f);
        bool lbFound = false, lbMatch = false;
        for (int k = 0; k < 20000 && !lbFound; ++k)
        {
            const f32 a = lA(lRng), c = std::cos(a), s = std::sin(a), a2 = lA(lRng), c2 = std::cos(a2), s2 = std::sin(a2);
            const Vec4 Fr[4] = { V(c, s, 0, 0), V(-s, c, 0, 0), V(0, 0, 1, 0), V(lT(lRng), lT(lRng), lT(lRng), 1) };
            const Vec4 Tr[4] = { V(1, 0, 0, 0), V(0, c2, s2, 0), V(0, -s2, c2, 0), V(lT(lRng), lT(lRng), lT(lRng), 1) };
            Vec4 Wf[4], Wu[4];
            MCompose(Fr, Tr, Wf, true); MCompose(Fr, Tr, Wu, false);
            const Vec4 p1 = MPoint(Wf, V(0.25f, -0.5f, 0.75f), true);   // a world point near the box centre
            const Vec4 lf = MToLocal(Wf, p1, true), lu = MToLocal(Wu, p1, false);
            const Vec4 pf = MSub(MPoint(Wf, lf, true), MMul(MDir(Wf, V(0, 0, 1, 0), true), 0.125f));
            const Vec4 pu = MSub(MPoint(Wu, lu, false), MMul(MDir(Wu, V(0, 0, 1, 0), false), 0.125f));
            if (Same(pf, pu)) continue;
            // the largest separation must be +z for the expected normal: |p| is (0.25, 0.5, 0.75) with h (4, 4, 1)
            lbFound = true;
            BoxVolume bb = MakeBox(4, 4, 1, 0.0f, Fr);
            Fill(r);
            rc = bb.LineSegIntersect(p1, V(0, 0, 0), Tr, r, 0.125f);
            lbMatch = (rc == 1 && Same(r.position, pf) && Same(r.normal, MDir(Wf, V(0, 0, 1, 0), true)));
            Print("B12 position", r.position); Print("B12 fused model", pf); Print("B12 unfused model", pu);
        }
        Check(lbFound && lbMatch, "B12 box frame round trip: ComposeFrame / InvertFrame / ToLocal / FramePoint / FrameDirection "
                                  "are fused vmaddfp chains (0x82BA9530..0x82BA9664, 0x82BA9ED4..0x82BA9EF0), bit for bit, on an "
                                  "input where the unfused chain differs");
    }

    // ================================================================================================= C. the capsule
    {
        CapsuleVolume c = MakeCapsule(2.0f, 0.5f);
        VolumeLineSegIntersectResult r; Fill(r);
        RwBool rc = c.LineSegIntersect(V(3, 0, 0), V(-3, 0, 0), 0, r, 0.0f);
        Fraction d; Vec4 P = V(3, 0, 0, 0), D = V(-6, 0, 0, 0);
        rwcCylinderLineSegIntersect(&d, 1.0f, 0.5f, 0, 0, P, D, V(0, 0, 0, 0), V(0, 0, 1, 0));
        f32 t = d.num / d.den;
        Check(rc == 1 && r.v == reinterpret_cast<uintptr_t>(&c) && r.lineParam == t && Same(r.position, MFma(D, t, P))
              && Near3(r.normal, 1, 0, 0, 1e-6) && Same(r.volParam, V(0, 0, 0, 0)) && Near(t, 2.5 / 6.0, 1e-6),
              "C1 capsule barrel: the cylinder (radius R about z) before the cap plane; volParam = (0, delta.z 0, start.z 0, 0)");

        c = MakeCapsule(2.0f, 0.5f); Fill(r);
        rc = c.LineSegIntersect(V(0, 0, 5), V(0, 0, -5), 0, r, 0.0f);
        Check(rc == 1 && r.lineParam == 0.25f && Same(r.position, V(0, 0, 2.5f, 0)) && Same(r.normal, V(0, 0, 1, 0))
              && Same(r.volParam, V(1, -10, 5, 0)),
              "C2 capsule cap: the cap sphere (0, 0, hh) strictly before its plane (t 0.25 < 0.3); volParam (cap +1, delta.z -10, "
              "start.z 5, 0)");

        c = MakeCapsule(2.0f, 0.5f); Fill(r);
        rc = c.LineSegIntersect(V(3, 0, 4), V(-3, 0, -2), 0, r, 0.0f);
        std::printf("  C3 lineParam %.9g\n", r.lineParam);
        Check(rc == 1 && Near(r.lineParam, 1.0 / 3.0 + 0.125, 1e-6) && Near3(r.position, 0.5, 0.0, 1.5, 1e-6)
              && Near3(r.normal, 1, 0, 0, 1e-6) && Same(r.volParam, V(0, -4, 4, 0)),
              "C3 capsule walk cap -> barrel -> cylinder: the cap plane at 1/3, then the barrel at 1/8 of the remainder "
              "(lineParam 1/3 + 1/8); volParam (0, the last delta.z -4, start.z 4, 0)");

        c = MakeCapsule(2.0f, 0.5f); Fill(r);
        rc = c.LineSegIntersect(V(0.25f, 0.125f, 0.5f), V(4, 4, 0.5f), 0, r, 0.0f);
        const Vec4 lvRad = V(0.25f, 0.125f, 0, 0);
        Check(rc == 1 && r.lineParam == 0.0f && Same(r.position, V(0.25f, 0.125f, 0.5f, 0))
              && Same(r.normal, MMul(lvRad, MRsqrt(MDot3(lvRad, lvRad)))),
              "C4 capsule, start inside the barrel: the start, its radial part (p - axis*(p.axis), fused) * refined 1/sqrt");

        c = MakeCapsule(2.0f, 0.5f); Fill(r);
        rc = c.LineSegIntersect(V(0, 0.125f, 2.25f), V(3, 3, 5), 0, r, 0.0f);
        const Vec4 lvCap = V(0, 0.125f, 0.25f, 0);
        Check(rc == 1 && r.lineParam == 0.0f && Same(r.position, V(0, 0.125f, 2.25f, 0))
              && Same(r.normal, MMul(lvCap, MRsqrt(MDot3(lvCap, lvCap)))) && r.volParam.x == 1.0f,
              "C5 capsule, start inside a cap sphere: the (local) start, (start - cap centre) * refined 1/sqrt");

        c = MakeCapsule(2.0f, 0.5f); Fill(r);
        rc = c.LineSegIntersect(V(3, 0, -1), V(3, 0, 1), 0, r, 0.0f);
        Check(rc == 0 && Same(r.position, V(3, 0, -1, 0)) && r.lineParam == 0.0f && Untouched(r.normal),
              "C6 capsule miss (parallel to the axis, outside): 0; result.position holds the walk's point, lineParam 0");

        // C8 the walk barrel -> cap -> cap sphere: (0.5, 0.5, 1.95) -> (0, 0, 2.15). The cap plane z = 2 (t 0.25) comes
        // before the barrel (t 0.293): cap +1 = fsel(delta.z), then its sphere is the hit.
        c = MakeCapsule(2.0f, 0.5f); Fill(r);
        rc = c.LineSegIntersect(V(0.5f, 0.5f, 1.95f), V(0, 0, 2.15f), 0, r, 0.0f);
        {
            const f64 lfS = FirstContact([](f64 x, f64 y, f64 z) { return SdfCapsule(x, y, z, 2.0, 0.5); },
                                         V(0.5f, 0.5f, 1.95f), V(0, 0, 2.15f));
            std::printf("  C8 lineParam %.9g, true contact s = %.9g, volParam (%g, %g, %g)\n", r.lineParam, lfS,
                        r.volParam.x, r.volParam.y, r.volParam.z);
            Check(rc == 1 && Near3(r.position, 0.5 - 0.5 * lfS, 0.5 - 0.5 * lfS, 1.95 + 0.2 * lfS, 2e-5)
                  && r.volParam.x == 1.0f && Near(r.volParam.y, 0.15, 1e-6) && r.volParam.z == 1.95f
                  && r.lineParam > 0.25f && r.lineParam < 0.35f && r.normal.z > 0.0f,
                  "C8 capsule walk barrel -> cap -> sphere: the cap plane first (cap = fsel(delta.z) +1), then the cap sphere; "
                  "volParam (1, the last delta.z 0.15, start.z 1.95, 0)");
        }

        // C7 a frame and tm on the capsule: a quarter turn about x (local z = world -y), tm moves it by (0, 0, 7).
        CapsuleVolume cf = MakeCapsule(2.0f, 0.5f);
        cf.maFrame[0] = V(1, 0, 0, 0); cf.maFrame[1] = V(0, 0, 1, 0); cf.maFrame[2] = V(0, -1, 0, 0); cf.maFrame[3] = V(0, 0, 0, 1);
        const Vec4 T[4] = { V(1, 0, 0, 0), V(0, 1, 0, 0), V(0, 0, 1, 0), V(0, 0, 7, 1) };
        Fill(r);
        rc = cf.LineSegIntersect(V(3, 0, 7), V(-3, 0, 7), T, r, 0.0f);
        Check(rc == 1 && Near3(r.position, 0.5, 0, 7, 1e-6) && Near3(r.normal, 1, 0, 0, 1e-6),
              "C7 capsule with a frame and tm: the barrel hit comes back in world space ((0.5, 0, 7), normal +x)");
    }

    // ================================================================================================= H. the idioms
    {
        std::mt19937 lRng(7);
        std::uniform_int_distribution<u32> lBits(0u, 0xFFFFFFFFu);
        std::uniform_real_distribution<f32> lU(-1000.0f, 1000.0f), lPos(1.0e-6f, 1.0e6f);
        using namespace rw::collision::linemath;
        bool lbDot = true, lbDotDiffers = false;
        for (int k = 0; k < 200000; ++k)
        {
            const Vec4 a = V(lU(lRng), lU(lRng), lU(lRng)), b = V(lU(lRng), lU(lRng), lU(lRng));
            const f32 lfModel = MDot3(a, b);
            lbDot = lbDot && Bits(Dot3(a, b)) == Bits(lfModel);
            lbDotDiffers = lbDotDiffers || Bits(lfModel) != Bits(a.x * b.x + a.y * b.y + a.z * b.z);
        }
        Check(lbDot && lbDotDiffers, "H1 vmsum3fp128 = ONE rounding of the f64 sum (FLAG model, xenia), 200000 inputs; the "
                                     "sequential f32 sum differs on some");
        bool lbNm = true;
        for (int k = 0; k < 200000; ++k)
        {
            const f32 a = lU(lRng), c = lU(lRng), b = lU(lRng);
            lbNm = lbNm && Bits(Nmsub(a, c, b)) == Bits(MNmsub(a, c, b));
        }
        Check(lbNm && Bits(Nmsub(1.0f, 1.0f, 1.0f)) == 0x80000000u,
              "H2 vnmsubfp / fnmsubs = -(a*c - b), one rounding then negated: 200000 inputs, an exact cancellation is -0");
        bool lbRecip = true, lbRsqrt = true, lbDiffers = false;
        for (int k = 0; k < 200000; ++k)
        {
            const f32 x = lPos(lRng);
            lbRecip = lbRecip && Bits(RefinedRecip(x)) == Bits(MRecip(x));
            lbRsqrt = lbRsqrt && Bits(RefinedRsqrt(x)) == Bits(MRsqrt(x));
            lbDiffers = lbDiffers || Bits(MRsqrt(x)) != Bits((f32)(1.0 / std::sqrt((f64)x)));
        }
        Check(lbRecip && lbRsqrt && lbDiffers,
              "H3 vrefp / vrsqrtefp + two fused Newton-Raphson steps: 200000 inputs, bit for bit; the refined 1/sqrt differs from "
              "the correctly rounded one on some (the refinement is kept)");
        (void)lBits;
    }

    Check(guUnexpected == 0, "X no unexpected call (the fat triangle walk is not reached)");
    std::printf("FxFollowupsLineKernels: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
