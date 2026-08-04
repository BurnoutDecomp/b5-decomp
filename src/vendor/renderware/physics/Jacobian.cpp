#include "vendor/renderware/physics/Jacobian.hpp"

// ===========================================================================
// rw::physics jacobian bucket -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   Jacobian_RQD::Create      @ 0x82BC0FA8
//   DriveJacobian::GetMatIBT  @ 0x82BC1128
//
// Both are pure scalar (Create is fmuls/fadds/fsubs, GetMatIBT is lfs/stfs plus three
// 16-byte stores), so both transcribe store-for-store as portable float maths.
// ===========================================================================

namespace rw
{
namespace physics
{

// ---------------------------------------------------------------------------
// Jacobian_RQD::Create @ 0x82BC0FA8
//
// The 27 intermediates (v2..v27) and the 16 output stores are transcribed verbatim from the
// asm, which the fmuls/fadds/fsubs sequence confirms store-for-store. Inputs:
//   lpA (r4): a0..a3 = lfs 0/4/8/0xC(r4)      -- qA'
//   lpB (r5): b0..b3 = lfs 0/4/8/0xC(r5)      -- qB'
// Output mData[i] is the store at byte offset 4*i.
//
// ⚠️ +0x2C and +0x38 are NEAR-MIRRORS: the same four products with the opposite sign on the
// (b*a) pair. X360 emits two separate subtractions in opposite order (0x82BC1024
// `fsubs f22,f29,f1` vs 0x82BC10B4 `fsubs f29,f1,f29`). Do not copy one line to the other.
//
// ⚠️ BurnoutPR's `return a3` is a Hex-Rays artefact (a3 was live in eax). X360 never
// reassigns r3, so the function returns the destination.
// ---------------------------------------------------------------------------
Jacobian_RQD* Jacobian_RQD::Create(const rw::math::vpu::Quaternion* lpA,
                                   const rw::math::vpu::Quaternion* lpB)
{
    const f32 a0 = lpA->x;   // *v0
    const f32 a1 = lpA->y;   // *(v0 + 4)
    const f32 a2 = lpA->z;   // *(v0 + 8)
    const f32 a3 = lpA->w;   // *(v0 + 12)

    const f32 b0 = lpB->x;   // v1[0]
    const f32 b1 = lpB->y;   // v1[1]
    const f32 b2 = lpB->z;   // v1[2]
    const f32 b3 = lpB->w;   // v1[3]

    // Intermediates v2..v27 (verbatim from the asm/pseudocode).
    const f32 v2  = b2 * a3;
    const f32 v3  = b2 * a0;
    const f32 v4  = b0 * a2;
    const f32 v5  = b0 * a3;
    const f32 v6  = b0 * a1;
    const f32 v7  = b3 * a0;
    const f32 v8  = b0 * a0;
    const f32 v9  = b3 * a2;
    const f32 v10 = b1 * a2;
    const f32 v11 = b1 * a0;
    const f32 v12 = b2 * a1;
    const f32 v13 = b1 * a1;
    const f32 v14 = b3 * a3;
    const f32 v15 = b2 * a2;
    const f32 v16 = (b2 * a3) - (b3 * a2);
    const f32 v17 = (b3 * a1) + (b1 * a3);
    const f32 v18 = (b3 * a1) - (b1 * a3);
    const f32 v19 = ((b0 * a3) - (b3 * a0)) + (b1 * a2);
    const f32 v20 = ((b3 * a2) + (b2 * a3)) + (b1 * a0);
    const f32 v21 = (b2 * a2) + (b3 * a3);
    const f32 v22 = ((b1 * a0) + (b0 * a1)) - (b2 * a3);
    const f32 v23 = (b3 * a0) - (b0 * a3);
    const f32 v24 = (b2 * a1) + (b1 * a2);
    const f32 v25 = ((b1 * a3) - (b3 * a1)) + (b2 * a0);
    const f32 v26 = ((b0 * a0) + (b3 * a3)) - (b1 * a1);
    const f32 v27 = ((b1 * a1) + (b3 * a3)) - (b2 * a2);

    // 16 stores; index = byte offset / 4.
    mData[2]  = (((b0 * a2) + (b2 * a0)) - (b1 * a3)) - (b3 * a1);  // +0x08
    mData[3]  = v19 - v12;                                          // +0x0C
    mData[0]  = v26 - v15;                                          // +0x00
    mData[4]  = v22 - v9;                                           // +0x10
    mData[5]  = v27 - v8;                                           // +0x14
    mData[7]  = v25 - v4;                                           // +0x1C
    mData[8]  = (v17 + v4) + v3;                                    // +0x20
    mData[10] = (v21 - v8) - v13;                                   // +0x28
    mData[11] = (v16 + v6) - v11;                                   // +0x2C
    mData[12] = (v23 + v10) - v12;                                  // +0x30
    mData[13] = (v18 + v3) - v4;                                    // +0x34
    mData[1]  = v20 + v6;                                           // +0x04
    mData[6]  = (v24 + v7) + v5;                                    // +0x18
    mData[9]  = (v24 - v5) - v7;                                    // +0x24
    mData[14] = ((v9 - v2) + v6) - v11;                             // +0x38
    mData[15] = ((v15 + v13) + v8) + v14;                           // +0x3C

    return this;
}

// ---------------------------------------------------------------------------
// DriveJacobian::GetMatIBT @ 0x82BC1128   (38 instructions, all read this wave)
//
// Gathers the nine MatIBT scalar lanes into a 3x3 matrix held as three 16-byte rows:
//     MatIBT[i][j] = *(jac + 0xDC + i*0x40 + j*0x10)
// i.e. row i reads +0xDC/+0xEC/+0xFC, then +0x11C/+0x12C/+0x13C, then +0x15C/+0x16C/+0x17C
// -- which are the `.w` lanes of record rows {13,14,15}, {17,18,19}, {21,22,23}.
//
// The destination's w lane is explicitly zeroed (`li r11,0` plus three `stw`) BEFORE the
// three `stvx128`, so the rows go out as (x, y, z, 0).
// ---------------------------------------------------------------------------
rw::math::vpu::Matrix33* DriveJacobian::GetMatIBT(rw::math::vpu::Matrix33* lpDst,
                                                  const Jacobian* lpJac)
{
    rw::math::vpu::Vector3 lRows[3];

    for (u32 luRow = 0u; luRow < 3u; ++luRow)
    {
        const u32 luBase = 13u + luRow * 4u;          // rows 13, 17, 21
        lRows[luRow].x = lpJac->mRows[luBase + 0u].w;   // +0xDC + i*0x40
        lRows[luRow].y = lpJac->mRows[luBase + 1u].w;   // +0xEC + i*0x40
        lRows[luRow].z = lpJac->mRows[luBase + 2u].w;   // +0xFC + i*0x40
        lRows[luRow].w = 0.0f;                         // `li r11,0` / `stw r11,0(rN)`
    }

    // `stvx128 v0,r0,r3` / `stvx128 v13,r3,r7(=0x10)` / `stvx128 v12,r3,r6(=0x20)`
    lpDst->xAxis = lRows[0];
    lpDst->yAxis = lRows[1];
    lpDst->zAxis = lRows[2];
    return lpDst;
}

} // namespace physics
} // namespace rw
