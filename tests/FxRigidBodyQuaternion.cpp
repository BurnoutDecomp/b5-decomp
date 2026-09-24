// FX-RIGIDBODY (H2-D1) -- rw::physics::Quaternion::UnitQuaternionToMatrix @0x82BC3EC0 and the
// orientation tail of rw::physics::RigidBody::DynamicUpdate @0x82BC2B78 (0x82BC2C58..0x82BC2D38),
// run AS SHIPPED: run_fxrigidbody_quaternion.py compiles the production Quaternion.cpp,
// quaternion.h and RigidBody.cpp (or a pre-fix git revision of them) into this program.
//
// Two oracles, both from the console's own instructions:
//   * CONSOLE VECTORS -- the REAL ARTIST words of 0x82BC3EC0 (export hole, decoded from the image)
//     and 0x82BC2B78 executed on the campaign's VMX emulator (FX-FX vmxemu.py + the FX-RIGIDBODY
//     extension, scratch/CRASHPARITY_0922/fixes/FX-RIGIDBODY.emu/). The two hardware-internal
//     steps the listing cannot pin are modelled as the PC lowers them (RigidBody.cpp): vrsqrtefp
//     = 1/sqrt cut to 13 significant bits, vmsum4fp128 = the sequential float sum.
//   * HAND ORACLES -- the same instruction sequences transcribed op for op below (fused ops as
//     std::fmaf), for pseudo-random sweeps and a 3600-step spinning-body drift run.
#include "rw/physics/quaternion.h"
#include "rw/physics/rigidbody.h"
#include "rw/physics/simulation.h"

#include <cmath>
#include <cstdio>
#include <cstring>

typedef rw::math::vpu::Quaternion Quat;
typedef rw::math::vpu::Matrix33   M33;
typedef rw::math::vpu::Vector3    V3;

static unsigned gChecks = 0, gFailures = 0;
static void Check(bool lbPass, const char* lpName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpName); }
}

static u32 Bits(float lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static float Flt(u32 lu) { float lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool Same(const float* lpA, const float* lpB, int n)
{
    for (int i = 0; i < n; ++i) if (Bits(lpA[i]) != Bits(lpB[i])) return false;
    return true;
}
static bool SameXyz(const V3& a, const V3& b) { return Same(&a.x, &b.x, 3); }

// ============================================================================================
// CONSOLE VECTORS (generated from the ARTIST words; see the banner).
// UnitQuaternionToMatrix @0x82BC3EC0 -- {q x,y,z,w, rows 0x00/0x10/0x20 (x,y,z,w)} as bits.
// Row 0 is (1,0,0,1)'s  (1,0,0,0) (0,-1,2,0) (0,-2,-1,0): q is NOT normalised and NOT written back.
static const u32 kaUqtmConsole[][16] = {
    { 0x3F800000, 0x00000000, 0x00000000, 0x3F800000,
      0x3F800000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xBF800000, 0x40000000, 0x00000000, 0x00000000, 0xC0000000, 0xBF800000, 0x00000000 },
    { 0x3F34B607, 0x3EB231D9, 0xBED65AA7, 0x3EE7FC25,
      0x3ED07AF6, 0x3DE55254, 0xBF680D67, 0x00000000, 0x3F5EE938, 0xBEB1BD9C, 0x3EB24FB7, 0x00000000, 0xBE8D25D2, 0xBF6E5CA0, 0xBE749690, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x3F800000,
      0x3F800000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x3F800000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x3F800000, 0x00000000 },
    { 0x3E12AB36, 0xBE168796, 0xBF07A1FC, 0x3F52A9D6,
      0x3ECA6E3C, 0xBF6A0178, 0x3DB8A7FC, 0x00000000, 0x3F5471F8, 0x3ECB8D18, 0x3EC87280, 0x00000000, 0xBEC9942E, 0xBDA3C402, 0x3F6A6EA3, 0x00000000 },
    { 0x3DE34F89, 0x3F6A4AA7, 0x3EC67508, 0x3DA7B2A8,
      0xBF79C5BC, 0x3E8884B4, 0xBD82BCFC, 0x00000000, 0x3E0F0891, 0x3F2CC4B0, 0x3F3A4815, 0x00000000, 0x3E7195EC, 0x3F30F99D, 0xBF3327D0, 0x00000000 },
    { 0x3F7DCB05, 0xBDE25802, 0x3D1E74DB, 0x3D70CEAC,
      0x3F78FADB, 0xBE5BBC0E, 0x3DB7B44E, 0x00000000, 0xBE650CE6, 0xBF77F9FA, 0x3DDD3805, 0x00000000, 0x3D8279FE, 0xBE001F37, 0xBF7D76CE, 0x00000000 },
    { 0xBB0FB281, 0xBE0AA992, 0xBF4FC029, 0xBE71B70E,
      0xBEB5281C, 0x3EC47649, 0xBD7745EC, 0x00000000, 0xBEC3DA9F, 0xBEA26290, 0x3E621DC1, 0x00000000, 0x3D8A369C, 0x3E5FFF09, 0x3F769BF6, 0x00000000 },
    { 0x3EE9C73B, 0x3EC800C4, 0x3F26BAE5, 0xBEED43F3,
      0xBE1D39B0, 0xBE7CD46A, 0x3F74F0C2, 0x00000000, 0x3F75D974, 0xBE87D7E8, 0x3DAF661C, 0x00000000, 0x3E6E4BD8, 0x3F6E9842, 0x3E8E428A, 0x00000000 },
};

// DynamicUpdate @0x82BC2B78 with zero spin, torque and reaction accumulator (so the integration
// at 0x82BC2C54 adds exactly nothing): input mQuat, then mQuat after 0x82BC2C90 and mRi / mUp / mAt
// x,y,z after 0x82BC2D20 / 0x82BC2D2C / 0x82BC2D38, as bits. dt 1/60, unit inertia, no drag.
static const u32 kaDynamicUpdateConsole[][17] = {
    { 0x00000000, 0x00000000, 0x00000000, 0x40000000, 0x00000000, 0x00000000, 0x00000000, 0x3F800000,
      0x3F800000, 0x00000000, 0x00000000, 0x00000000, 0x3F800000, 0x00000000, 0x00000000, 0x00000000, 0x3F800000 },
    { 0x3F800000, 0x00000000, 0x00000000, 0x3F800000, 0x3F3504F3, 0x00000000, 0x00000000, 0x3F3504F3,
      0x3F800000, 0x00000000, 0x00000000, 0x00000000, 0x34000000, 0x3F7FFFFE, 0x00000000, 0xBF7FFFFE, 0x34000000 },
    { 0x3E99999A, 0xBF000000, 0x3F333333, 0x3ECCCCCD, 0x3E9A5FB2, 0xBF00A514, 0x3F341A4F, 0x3ECDD4ED,
      0xBEFD69FE, 0x3E8676F0, 0x3F540A56, 0xBF5E6242, 0xBE2FD694, 0xBEEDE622, 0x3CA57EB0, 0xBF731219, 0x3EA052C0 },
    { 0x3E3BE073, 0x3CF9EC6A, 0xBF72E6B2, 0xBE099723, 0x3E4075FB, 0x3D0002C1, 0xBF78D3EC, 0xBE0CF290,
      0xBF643694, 0x3E8F037A, 0xBEB6A9F0, 0xBE82FBD8, 0xBF75CCB4, 0xBDE66384, 0xBEBF794A, 0xBC13AE0C, 0x3F6D69D4 },
    { 0xBEDEA38B, 0xBF451C52, 0x3E0B10E3, 0x3EEE42A7, 0xBEDCC734, 0xBF43769A, 0x3E09E75B, 0x3EEC44E4,
      0xBE4F13DC, 0x3F48639D, 0x3F16AA67, 0x3F08C06B, 0x3F178354, 0xBF1A8715, 0xBF522177, 0x3E44EFDF, 0xBF09AF46 },
    { 0xBED56B9D, 0x3EEE0CDD, 0x3E8DB3A8, 0x3F1F94CD, 0xBEE6CB89, 0x3F00B70D, 0x3E993CE2, 0x3F2C92A7,
      0x3EA167EE, 0xBD4BE1B0, 0xBF729CB5, 0xBF5B577B, 0x3ED433D1, 0xBE9D11E6, 0x3ED0EC5A, 0x3F68A0F9, 0x3DB43D9C },
    { 0x3EEE8D30, 0xBEB28F9D, 0xBF199980, 0xBF31EE1C, 0x3EDB7031, 0xBEA44119, 0xBF0D4AFA, 0xBF23AC89,
      0x3E3D5BA7, 0x3EDC8C30, 0xBF622136, 0xBF7B11C4, 0x3CBF8260, 0xBE46917A, 0xBD80C748, 0x3F66F44A, 0x3EDA8362 },
    { 0xBE458F71, 0x3EAE4985, 0xBF5D9DBA, 0xBF057F1B, 0xBE364C74, 0x3EA0D2C9, 0xBF4C7F01, 0xBEF65E1C,
      0xBEF272CA, 0x3F282BC0, 0x3F163284, 0xBF616EB0, 0xBEADDED5, 0xBEA93744, 0xBC9264F0, 0xBF2C53E0, 0x3F3D41D2 },
    { 0xBE1FCCB9, 0xBE127CD5, 0xBF74ADDB, 0x3E56440D, 0xBE1FA198, 0xBE12554C, 0xBF746BD2, 0x3E560A39,
      0xBF5D3017, 0xBEB58C0E, 0x3EB6FF89, 0x3EE32BBA, 0xBF5F2CAE, 0x3E54B222, 0x3E73A5ED, 0x3EAD14CB, 0x3F691A0F },
    { 0xBE86DE08, 0x3D0BFE3D, 0xBF5108C7, 0xBF037A55, 0xBE86CBB6, 0x3D0BEB38, 0xBF50EC61, 0xBF036879,
      0xBEAB35C7, 0x3F51E1AA, 0x3EEDF894, 0xBF5B1738, 0xBEF0FDC7, 0x3E5BACDA, 0x3ECA0F58, 0xBEA6EE95, 0x3F5BEA1B },
    { 0xBDE4D21B, 0xBE9FB278, 0xBDE87944, 0xBF704A69, 0xBDE46646, 0xBE9F6736, 0xBDE80BB6, 0xBF6FD92C,
      0x3F47CCA7, 0x3E9041CA, 0xBF0EE05C, 0xBE124BFB, 0x3F730F24, 0x3E8F1DD3, 0x3F1BD0CE, 0xBE0DBED4, 0x3F48011E },
};

// ============================================================================================
// HAND ORACLES -- transcribed from the words, independently of the shipped bodies.
// 0x82BC3EC0: fmuls products, vmulfp128 x splat(flt_82001D9C = 0x40000000), fadds, fsubs from
// flt_82001C98 = 0x3F800000; row w lanes = the three `stw r11(=0)`.
static M33 OracleUnitQuaternionToMatrix(const Quat& q)
{
    const float lfTwo = Flt(0x40000000u), lfOne = Flt(0x3F800000u);
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;      // 0x82BC3F0C..3F1C
    const float wx = q.w * q.x, wy = q.y * q.w, wz = q.z * q.w;      // 0x82BC3F2C / 3F50 / 3F54
    const float xy = q.y * q.x, yz = q.z * q.y, zx = q.z * q.x;      // 0x82BC3F68 / 3F70 / 3F78
    const float a2xx = xx * lfTwo, a2yy = yy * lfTwo, a2zz = zz * lfTwo;   // vmulfp128 0x82BC3F84
    const float a2wx = wx * lfTwo, a2wy = wy * lfTwo, a2wz = wz * lfTwo;   // vmulfp128 0x82BC3FB4
    const float a2xy = xy * lfTwo, a2yz = yz * lfTwo, a2zx = zx * lfTwo;   // vmulfp128 0x82BC3FB8
    const float f9 = a2zz + a2xx, f8 = a2yy + a2zz, f7 = a2yy + a2xx;     // fadds 0x82BC3FD0..3FD8
    M33 m;
    m.xAxis = { lfOne - f8, a2wz + a2xy, a2zx - a2wy, 0.0f };             // -> r3+0x00 @0x82BC4078
    m.yAxis = { a2xy - a2wz, lfOne - f9, a2yz + a2wx, 0.0f };             // -> r3+0x10 @0x82BC4074
    m.zAxis = { a2zx + a2wy, a2yz - a2wx, lfOne - f7, 0.0f };             // -> r3+0x20 @0x82BC4080
    return m;
}

// 0x82BC2C5C..0x82BC2C8C: n = vmsum4fp128(q,q); e = vrsqrtefp(n) (13-bit-cut stand-in);
// twice { vmulfp128 e*e, e*0.5; vnmsubfp r = 1 - n*(e*e); vmaddfp e = (e*0.5)*r + e }; q * e.
static Quat OracleNormalize(const Quat& q)
{
    const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    float e = Flt(Bits(1.0f / std::sqrt(n)) & 0xFFFFF800u);
    for (int i = 0; i < 2; ++i)
    {
        const float ee = e * e, he = e * 0.5f;
        const float r = std::fmaf(-n, ee, 1.0f);          // vnmsubfp v9 = v0 - v9*v5
        e = std::fmaf(he, r, e);                          // vmaddfp  v12 = v4*v9 + v12
    }
    const Quat lResult = { q.x * e, q.y * e, q.z * e, q.w * e };
    return lResult;
}

// 0x82BC2C94..0x82BC2D38, operand for operand (vperm controls 0x82CDA3D0 / 450 / 410 dumped from the
// image: 04050607 10111213 .. / 00010203 18191A1B .. / 08090A0B 14151617 ..; vrlimi128 mask 2 rot 0/3/2).
static M33 OracleMatrix33FromQuaternion(const Quat& q)
{
    const float k = Flt(0x3FB504F3u);                                         // gSqrt2s @0x821814F0
    const float s[4] = { k * q.x, k * q.y, k * q.z, k * q.w };                  // vmulfp128 @0x82BC2C9C
    float d[4];
    for (int i = 0; i < 4; ++i) d[i] = std::fmaf(-s[i], s[i], 0.5f);           // vnmsubfp @0x82BC2CA8
    const float syzxx[4] = { s[1], s[2], s[0], s[0] };                          // vpermwi128 0x60
    const float szxyx[4] = { s[2], s[0], s[1], s[0] };                          // vpermwi128 0x84
    const float dyzxx[4] = { d[1], d[2], d[0], d[0] };                          // vpermwi128 0x60
    float add[4], sub[4], diag[4];
    for (int i = 0; i < 4; ++i)
    {
        const float p = s[i] * syzxx[i];                                        // vmulfp128 @0x82BC2CB8
        const float r = s[3] * szxyx[i];                                        // vmulfp128 @0x82BC2CBC
        add[i] = p + r; sub[i] = p - r;                                         // @0x82BC2CCC / 0x82BC2CD4
        diag[i] = d[i] + dyzxx[i];                                              // vaddfp @0x82BC2CC4
    }
    M33 m;
    m.xAxis = { diag[1], add[0], sub[2], 0.0f };    // vperm(diag, add, 3D0) ; z <- sub  rot 0
    m.yAxis = { sub[0], diag[2], add[1], 0.0f };    // vperm(sub, diag, 450) ; z <- add  rot 3
    m.zAxis = { add[2], sub[1], diag[0], 0.0f };    // vperm(add, sub, 410)  ; z <- diag rot 2
    return m;
}

// 0x82BC2C24..0x82BC2C54 (for the drift run): cross via vnmsubfp, vector part via vmaddfp (fused),
// w = q.w + (-0.5 * vmsum3fp128(spin, q)).
static Quat OracleIntegrate(const Quat& q, const float* lpSpin)
{
    const float v5[3] = { lpSpin[0] * q.y, lpSpin[1] * q.z, lpSpin[2] * q.x };
    const float v8[3] = { std::fmaf(-lpSpin[1], q.x, v5[0]), std::fmaf(-lpSpin[2], q.y, v5[1]),
                          std::fmaf(-lpSpin[0], q.z, v5[2]) };
    const float cross[3] = { v8[1], v8[2], v8[0] };                             // vpermwi128 0x63
    const float dot = lpSpin[0] * q.x + lpSpin[1] * q.y + lpSpin[2] * q.z;
    Quat r;
    r.x = q.x + std::fmaf(lpSpin[0], q.w, cross[0]) * 0.5f;
    r.y = q.y + std::fmaf(lpSpin[1], q.w, cross[1]) * 0.5f;
    r.z = q.z + std::fmaf(lpSpin[2], q.w, cross[2]) * 0.5f;
    r.w = q.w + dot * -0.5f;
    return r;
}

// ============================================================================================
// A body wired to its own Simulation / Inertia / reaction block, as DynamicUpdate reads them.
struct Rig
{
    rw::physics::Simulation mSim;
    rw::physics::Inertia    mInertia;
    rw::physics::RigidBody  mBody;
    alignas(16) float       mafAccum[16];

    Rig() : mSim(), mInertia(), mBody(), mafAccum()
    {
        mSim.m_RF_Stack = mafAccum;
        mSim.m_TimeStep = 1.0f / 60.0f;
        mSim.m_Gravity  = { 0.0f, 0.0f, 0.0f, 0.0f };
        mBody.mStasis   = &mSim;
        mBody.mInertia  = &mInertia;
        mBody.mId       = 0u;
        mBody.mInvm     = 1.0f;
        mBody.mRi = { 1.0f, 0.0f, 0.0f, Flt(0x7FC0ABCDu) };   // w sentinels: DynamicUpdate keeps w
        mBody.mUp = { 0.0f, 1.0f, 0.0f, Flt(0x7FC0BCDEu) };
        mBody.mAt = { 0.0f, 0.0f, 1.0f, Flt(0x7FC0CDEFu) };
    }
};

static V3 Row(const rw::math::vpu::Vector4& v) { const V3 r = { v.x, v.y, v.z, 0.0f }; return r; }

// Tiny deterministic generator (xorshift32) so the sweeps are identical on every run.
static u32 gSeed = 0x82BC3EC0u;
static float Uniform(float lo, float hi)
{
    gSeed ^= gSeed << 13; gSeed ^= gSeed >> 17; gSeed ^= gSeed << 5;
    return lo + (hi - lo) * (float)(gSeed >> 8) * (1.0f / 16777216.0f);
}

int main()
{
    // ---- 1. UnitQuaternionToMatrix @0x82BC3EC0 vs the console words -----------------------
    for (const u32* lpCase : kaUqtmConsole)
    {
        Quat q = { Flt(lpCase[0]), Flt(lpCase[1]), Flt(lpCase[2]), Flt(lpCase[3]) };
        const Quat lqBefore = q;
        M33 m;
        std::memset(&m, 0xCD, sizeof(m));
        rw::physics::Quaternion::UnitQuaternionToMatrix(&m, &q);
        const float lafExpect[12] = { Flt(lpCase[4]), Flt(lpCase[5]), Flt(lpCase[6]), Flt(lpCase[7]),
                                      Flt(lpCase[8]), Flt(lpCase[9]), Flt(lpCase[10]), Flt(lpCase[11]),
                                      Flt(lpCase[12]), Flt(lpCase[13]), Flt(lpCase[14]), Flt(lpCase[15]) };
        Check(Same(&m.xAxis.x, &lafExpect[0], 4) && Same(&m.yAxis.x, &lafExpect[4], 4)
              && Same(&m.zAxis.x, &lafExpect[8], 4),
              "0x82BC3EC0 console vector: the three rows (and their zero w lanes) bit for bit");
        Check(Same(&q.x, &lqBefore.x, 4), "0x82BC3EC0 console vector: the quaternion is NOT modified");
    }

    // ---- 2. UnitQuaternionToMatrix vs the hand oracle, 4096 unit and non-unit quaternions ---
    {
        bool lbRows = true, lbConst = true;
        for (int i = 0; i < 4096; ++i)
        {
            const float lfScale = (i & 1) ? Uniform(0.25f, 4.0f) : 1.0f;
            Quat q = { Uniform(-1, 1), Uniform(-1, 1), Uniform(-1, 1), Uniform(-1, 1) };
            const float lfInv = lfScale / std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
            q.x *= lfInv; q.y *= lfInv; q.z *= lfInv; q.w *= lfInv;
            const Quat lqBefore = q;
            M33 m;
            rw::physics::Quaternion::UnitQuaternionToMatrix(&m, &q);
            const M33 lExpect = OracleUnitQuaternionToMatrix(lqBefore);
            lbRows  = lbRows && Same(&m.xAxis.x, &lExpect.xAxis.x, 4) && Same(&m.yAxis.x, &lExpect.yAxis.x, 4)
                             && Same(&m.zAxis.x, &lExpect.zAxis.x, 4);
            lbConst = lbConst && Same(&q.x, &lqBefore.x, 4);
        }
        Check(lbRows, "0x82BC3EC0 sweep: rows == oracle 1-(a+b) grouping, w lanes 0 (4096 quaternions)");
        Check(lbConst, "0x82BC3EC0 sweep: q unchanged -- no normalise, no write-back (4096 quaternions)");
    }

    // ---- 3. DynamicUpdate's orientation tail vs the console words (zero spin) --------------
    for (const u32* lpCase : kaDynamicUpdateConsole)
    {
        Rig lRig;
        lRig.mBody.mQuat = { Flt(lpCase[0]), Flt(lpCase[1]), Flt(lpCase[2]), Flt(lpCase[3]) };
        lRig.mBody.DynamicUpdate();
        const float lafQuat[4] = { Flt(lpCase[4]), Flt(lpCase[5]), Flt(lpCase[6]), Flt(lpCase[7]) };
        const V3 lRi = { Flt(lpCase[8]),  Flt(lpCase[9]),  Flt(lpCase[10]), 0.0f };
        const V3 lUp = { Flt(lpCase[11]), Flt(lpCase[12]), Flt(lpCase[13]), 0.0f };
        const V3 lAt = { Flt(lpCase[14]), Flt(lpCase[15]), Flt(lpCase[16]), 0.0f };
        Check(Same(&lRig.mBody.mQuat.x, lafQuat, 4),
              "0x82BC2C90 console vector: mQuat renormalised IN DynamicUpdate, bit for bit");
        Check(SameXyz(Row(lRig.mBody.mRi), lRi) && SameXyz(Row(lRig.mBody.mUp), lUp)
              && SameXyz(Row(lRig.mBody.mAt), lAt),
              "0x82BC2D20/2D2C/2D38 console vector: mRi/mUp/mAt in the gSqrt2s form, bit for bit");
    }

    // ---- 4. DynamicUpdate vs the hand oracles, 2048 zero-spin bodies ----------------------
    {
        bool lbQuat = true, lbRows = true, lbW = true;
        for (int i = 0; i < 2048; ++i)
        {
            Rig lRig;
            const float lfScale = Uniform(0.9f, 1.1f);
            Quat q = { Uniform(-1, 1), Uniform(-1, 1), Uniform(-1, 1), Uniform(-1, 1) };
            const float lfInv = lfScale / std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
            q.x *= lfInv; q.y *= lfInv; q.z *= lfInv; q.w *= lfInv;
            lRig.mBody.mQuat = q;
            lRig.mBody.DynamicUpdate();
            const Quat lqN = OracleNormalize(q);
            const M33 lExpect = OracleMatrix33FromQuaternion(lqN);
            lbQuat = lbQuat && Same(&lRig.mBody.mQuat.x, &lqN.x, 4);
            lbRows = lbRows && SameXyz(Row(lRig.mBody.mRi), lExpect.xAxis) && SameXyz(Row(lRig.mBody.mUp), lExpect.yAxis)
                            && SameXyz(Row(lRig.mBody.mAt), lExpect.zAxis);
            lbW = lbW && Bits(lRig.mBody.mRi.w) == 0x7FC0ABCDu && Bits(lRig.mBody.mUp.w) == 0x7FC0BCDEu
                      && Bits(lRig.mBody.mAt.w) == 0x7FC0CDEFu;
        }
        Check(lbQuat, "DynamicUpdate sweep: mQuat == vmsum4fp128 / vrsqrtefp / 2 fused NR / multiply (2048 bodies)");
        Check(lbRows, "DynamicUpdate sweep: basis == Matrix33FromQuaternion(normalised mQuat) (2048 bodies)");
        Check(lbW, "DynamicUpdate sweep: mRi/mUp/mAt w lanes untouched (vrlimi128 mask 1)");
    }

    // ---- 5. Guard: a body whose quaternion is (0,0,0,2) comes out (0,0,0,1) with an identity basis.
    //         Passes before AND after the fix -- it catches anyone who just deletes the normalise.
    {
        Rig lRig;
        lRig.mBody.mQuat = { 0.0f, 0.0f, 0.0f, 2.0f };
        lRig.mBody.DynamicUpdate();
        const float lafUnit[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        const V3 lX = { 1, 0, 0, 0 }, lY = { 0, 1, 0, 0 }, lZ = { 0, 0, 1, 0 };
        Check(Same(&lRig.mBody.mQuat.x, lafUnit, 4) && SameXyz(Row(lRig.mBody.mRi), lX)
              && SameXyz(Row(lRig.mBody.mUp), lY) && SameXyz(Row(lRig.mBody.mAt), lZ),
              "DynamicUpdate guard: (0,0,0,2) -> (0,0,0,1) and the identity basis");
    }

    // ---- 6. Drift: a free body spinning at (3.1, -4.7, 6.3) rad/s for 3600 steps (60 s at 1/60) --
    {
        Rig lRig;
        lRig.mInertia.mMaxOmega = 1.0e6f;
        lRig.mBody.mOmega = { 3.1f, -4.7f, 6.3f, 0.0f };
        lRig.mBody.mQuat  = { 0.0f, 0.0f, 0.0f, 1.0f };
        Quat lqOracle = lRig.mBody.mQuat;                 // an independent trajectory fed the same spin
        float lfMaxNormErr = 0.0f, lfMaxOrtho = 0.0f;
        float lfMaxStep = 0.0f;
        bool lbRows = true;
        for (int liStep = 0; liStep < 3600; ++liStep)
        {
            const float lfDt = lRig.mSim.m_TimeStep;
            // zero torque / accumulator: the console's spin is exactly RN(omega * dt)
            const float lafSpin[3] = { lRig.mBody.mOmega.x * lfDt, lRig.mBody.mOmega.y * lfDt,
                                       lRig.mBody.mOmega.z * lfDt };
            const Quat lqPre = lRig.mBody.mQuat;
            lRig.mBody.DynamicUpdate();
            const Quat& lrQ = lRig.mBody.mQuat;
            const Quat lqStep = OracleNormalize(OracleIntegrate(lqPre, lafSpin));
            lqOracle = OracleNormalize(OracleIntegrate(lqOracle, lafSpin));
            for (int l = 0; l < 4; ++l)
                lfMaxStep = std::fmax(lfMaxStep, std::fabs((&lrQ.x)[l] - (&lqStep.x)[l]));
            const float lfN = lrQ.x * lrQ.x + lrQ.y * lrQ.y + lrQ.z * lrQ.z + lrQ.w * lrQ.w;
            lfMaxNormErr = std::fmax(lfMaxNormErr, std::fabs(lfN - 1.0f));
            const M33 lExpect = OracleMatrix33FromQuaternion(lrQ);
            lbRows = lbRows && SameXyz(Row(lRig.mBody.mRi), lExpect.xAxis) && SameXyz(Row(lRig.mBody.mUp), lExpect.yAxis)
                            && SameXyz(Row(lRig.mBody.mAt), lExpect.zAxis);
            const V3 a = Row(lRig.mBody.mRi), b = Row(lRig.mBody.mUp), c = Row(lRig.mBody.mAt);
            const float lafDots[6] = { a.x * b.x + a.y * b.y + a.z * b.z, b.x * c.x + b.y * c.y + b.z * c.z,
                                       c.x * a.x + c.y * a.y + c.z * a.z, a.x * a.x + a.y * a.y + a.z * a.z - 1.0f,
                                       b.x * b.x + b.y * b.y + b.z * b.z - 1.0f, c.x * c.x + c.y * c.y + c.z * c.z - 1.0f };
            for (float lfD : lafDots) lfMaxOrtho = std::fmax(lfMaxOrtho, std::fabs(lfD));
        }
        float lfFinal = 0.0f;
        for (int l = 0; l < 4; ++l)
            lfFinal = std::fmax(lfFinal, std::fabs((&lRig.mBody.mQuat.x)[l] - (&lqOracle.x)[l]));
        std::printf("drift: max |1-|q|^2| %.3g, max step deviation vs oracle %.3g, max basis ortho err %.3g, "
                    "60 s trajectory vs oracle %.3g\n", lfMaxNormErr, lfMaxStep, lfMaxOrtho, lfFinal);
        Check(lfMaxNormErr < 1.0e-6f, "drift: |mQuat| stays 1 over 3600 steps (renormalised every step)");
        // 4 ulp at the unit quaternion's own scale (2^-24 below 1.0); the residue is the untouched
        // integration at 0x82BC2C24..0x82BC2C54 (split FMAs on the PC where the console fuses).
        Check(lfMaxStep <= 4.0f * 5.9604645e-8f, "drift: every step's mQuat within 4 unit-scale ulp of the console sequence");
        Check(lbRows, "drift: every step's basis == Matrix33FromQuaternion(stored mQuat), bit for bit");
        Check(lfMaxOrtho < 2.0e-6f, "drift: the basis stays orthonormal");
        Check(lfFinal < 1.0e-4f, "drift: 60 s trajectory within 1e-4 of the independent oracle trajectory");
    }

    std::printf("FxRigidBodyQuaternion: %u/%u checks passed\n", gChecks - gFailures, gChecks);
    return gFailures == 0 ? 0 : 1;
}
