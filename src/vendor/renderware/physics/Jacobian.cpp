#include "vendor/renderware/physics/Jacobian.hpp"

// ===========================================================================
// rw::physics::Jacobian_RQD -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   Jacobian_RQD::Create  @ 0x82BC0FA8
//
// Pure scalar FPU arithmetic in the X360 asm. The 27 intermediates (v2..v27)
// and the 16 output stores are transcribed verbatim from the pseudocode, which
// the fmuls/fadds/fsubs sequence confirms store-for-store. Inputs:
//   lpA (r4): a0=lane0, a1=lane1, a2=lane2, a3=lane3  (lfs 0/4/8/0xC(r4))
//   lpB (r5): b0=lane0, b1=lane1, b2=lane2, b3=lane3  (lfs 0/4/8/0xC(r5))
// Output mData[i] is the store at byte offset 4*i.
// ===========================================================================

namespace rw
{
namespace physics
{

Jacobian_RQD* Jacobian_RQD::Create(const Vec4* lpA, const Vec4* lpB)
{
    const f32 a0 = lpA->lane0;   // *v0
    const f32 a1 = lpA->lane1;   // *(v0 + 4)
    const f32 a2 = lpA->lane2;   // *(v0 + 8)
    const f32 a3 = lpA->lane3;   // *(v0 + 12)

    const f32 b0 = lpB->lane0;   // v1[0]
    const f32 b1 = lpB->lane1;   // v1[1]
    const f32 b2 = lpB->lane2;   // v1[2]
    const f32 b3 = lpB->lane3;   // v1[3]

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

} // namespace physics
} // namespace rw
