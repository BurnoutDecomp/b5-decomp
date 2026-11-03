#pragma once

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::{Quaternion, Vector3, Vector4, Matrix33}

// ===========================================================================
// rw::physics::Jacobian_RQD -- the "rotational quaternion-derivative" Jacobian
// block of a RenderWare physics constraint. JointJacobian::Build and
// DriveJacobian::Build call Create() to fill a 4x4 (16-float) Jacobian from two
// quaternion-shaped 4-vectors.
//
// OWNING HOME for the two functions the X360 binary defines in this bucket:
//     rw::physics::Jacobian_RQD::Create      @ 0x82BC0FA8
//     rw::physics::DriveJacobian::GetMatIBT  @ 0x82BC1128
// and the declaration home for the two constraint builders:
//     rw::physics::JointJacobian::Build      @ 0x82BC42E8   (JointJacobian_Build.cpp)
//     rw::physics::DriveJacobian::Build      @ 0x82BC5590   (DriveJacobian_Build.cpp)
//
// No DWARF hints exist for the jacobian RECORD: the SDK defines it as a private type inside
// the .cpp, so it ships no jacobian.h. Every offset below is X360's, read off X360 stores.
// ===========================================================================

namespace rw
{
namespace physics
{

class Jacobian_RQD
{
public:
    // @ 0x82BC0FA8 -- build the 16-float (4x4) Jacobian into mData from the two input
    // quaternions. Returns this. X360: r3 = this, r4 = lpA, r5 = lpB.
    //
    // SOLVED IN CLOSED FORM: the 64 bytes it writes are the 4x4 ROW-MAJOR matrix of the
    // quaternion map  x |-> conj(A) (x) x (x) B  in the basis (i, j, k, 1) with components
    // ordered (x, y, z, w), i.e. M = L(conj A).R(B). Column 3 is therefore conj(A) (x) B
    // itself, the relative orientation -- which is what makes this the jacobian of a
    // relative-orientation constraint, and is exactly how JointJacobian::Build uses it
    // (it gathers the four .w components into one 16-byte local and feeds THAT to
    // UnitQuaternionToMatrix, which only accepts a unit quaternion).
    //
    // The two inputs are quaternions -- the callers pass qA' and qB', each a Hamilton
    // product of a body orientation with a frame orientation. (This declaration used to
    // spell them as a local `Vec4 {lane0..lane3}` struct; it is the vpu Quaternion, and
    // naming it as such is what lets the two Build bodies call this without a cast.)
    Jacobian_RQD* Create(const rw::math::vpu::Quaternion* lpA,
                         const rw::math::vpu::Quaternion* lpB);

    // Row i of the 4x4, as the (x,y,z,w) quadruple at mData[4*i .. 4*i+3].
    f32 RowX(u32 luRow) const { return mData[luRow * 4u + 0u]; }
    f32 RowY(u32 luRow) const { return mData[luRow * 4u + 1u]; }
    f32 RowZ(u32 luRow) const { return mData[luRow * 4u + 2u]; }
    f32 RowW(u32 luRow) const { return mData[luRow * 4u + 3u]; }

    f32 mData[16];   // +0x00..+0x3C  the 4x4 Jacobian block
};

// ARTIST writes 24 consecutive 16-byte rows at stride 0x180. The W words
// of rows 0, 1, 4 and 12 carry reaction-block addresses, spy flag and node.
struct Jacobian
{
    rw::math::vpu::Vector4 mRows[24];
};
CGS_STATIC_ASSERT((sizeof(Jacobian) == 0x180), "ARTIST Jacobian stride");
inline u32 JacobianStride() { return static_cast<u32>(sizeof(Jacobian)); }
const u32 KU_JACOBIAN_STRIDE_X360 = 0x180u;

class Joint;
class Drive;
class Simulation;

struct JointJacobian : public Jacobian
{
    // @ 0x82BC42E8 (873 X360 instructions).
    // SIGNATURE: three registers at the call site -- JointBatchBuild @0x82BC6A30 passes
    // r3 = m_JJ_Stack + m_JT_Count*0x180, r4 = the Joint, r5 = the Simulation, and Build
    // dereferences r5 only at +0xA0 = m_TimeStep. (The DecFIGS mangled name carries a
    // 4-argument PS3/SPU form `Build(const Joint&, int, const JointRefs&, Simulation*)`;
    // those two extra parameters do not exist in this build.)
    void Build(const Joint& lrJoint, Simulation* lpSim);
};

struct DriveJacobian : public Jacobian
{
    // @ 0x82BC5590 (1320 X360 instructions) -- same 3-parameter shape.
    void Build(const Drive& lrDrive, Simulation* lpSim);

    // @ 0x82BC1128 -- gather the nine MatIBT scalar lanes of a jacobian record into a 3x3
    // matrix held as three 16-byte rows. Debug/spy only: its two callers are
    // SpyJointJacobians @0x82BC24F8 and SpyDriveJacobians @0x82BC3010, which is also the
    // proof that JOINT and DRIVE jacobians share this sub-layout.
    //     MatIBT[i][j] == *(jac + 0xDC + i*0x40 + j*0x10)
    static rw::math::vpu::Matrix33* GetMatIBT(rw::math::vpu::Matrix33* lpDst,
                                              const Jacobian* lpJac);
};

} // namespace physics
} // namespace rw
