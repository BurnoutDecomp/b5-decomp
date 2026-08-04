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

// =====================================================================================
// THE JACOBIAN RECORD -- 384 bytes on the console, KU_JACOBIAN_STRIDE apart in the three
// scratch arrays Simulation::SetWorkspace carves.
//
// GEOMETRY, verified three ways:
//   stride   X360 `mulli r10,r11,0x180` (DriveBatchBuild @0x82BC6B14, JointBatchBuild
//            @0x82BC6A8C) ; BurnoutPR `lea ecx,[eax+eax*2]` + `shl ecx,7` ; Xbox One
//            `imul rcx,rax,190h` = 400 (= 384 + the widened node pointer + tail align).
//   cover    JointJacobian::Build issues exactly 24 `stvx128` against r31 at +0x00..+0x170,
//            a complete non-overlapping cover, plus 2 `stw`; DriveJacobian::Build writes all
//            96 four-byte slots individually; Isis_Pipeline `lvx128`s all 24 rows and
//            scalar-reads only +0x0C and +0x1C.
//
// ⚠️ IT IS NOT `Vector4[24]`. It is 24 x { 12-byte packed triple + one 4-byte scalar }: the
// tail writes +0xA0,+0xA4,+0xA8 then +0xB0,+0xB4,+0xB8, SKIPPING +0xAC/+0xBC. Most of those
// scalar lanes really are floats and stay in `.w` below. FOUR ARE NOT, and those four are
// promoted to their own members:
//
//   console lane      payload                              PC
//   ----------------  -----------------------------------  --------------------------------
//   mRows[0].w  +0x0C bodyA's reaction-force block index    u32 mIdA   -- STAYS 32 BITS
//   mRows[1].w  +0x1C bodyB's                               u32 mIdB   -- STAYS 32 BITS
//   mRows[4].w  +0x4C the node's m_spy flag                 u32 mSpy
//   mRows[12].w +0xCC the Joint*/Drive* itself              void* mpNode -- 8 BYTES ON x64
//
// ⚠️⚠️ DO NOT PIN offsetof(mpNode) == 0xCC, AND DO NOT PACK IT INTO A w LANE. +0xCC is the w
// lane of the +0xC0 vector: the console stores the whole 16-byte vector at +0xC0 (line 699 of
// the X360 listing) and then `stw r27,0xCC` over its w. That works only because X360 pointers
// are 4 bytes; an x64 pointer there overruns into +0xD0, which holds I_A^-1.(rA x L0). The
// shipping x64 build made exactly this move: node promoted to its own 8-byte field, record
// grown 384 -> 400.
//   ⭐ This is the OPPOSITE conclusion to the serialised-slot rule ("pointer slots inside
//   serialised records stay 4 bytes on x64"), and the deciding test is that THIS RECORD IS
//   PER-FRAME SCRATCH off Simulation::m_DJ_Stack and is NEVER SERIALISED -- nothing rebases
//   it, so nothing forces it to stay 4 bytes. mIdA/mIdB are different again: they stay 32-bit
//   because they are indices BY DECLARATION, not because a pointer was narrowed.
//
// ⛔ ORACLE DISCIPLINE, STILL BINDING. BurnoutPR and Xbox One are ALGORITHM oracles for this
// record and nothing more. Measured divergences: BurnoutPR puts the Drive node pointer at
// +0x12C and the inverse masses at +0xEC/+0x13C, and stores m_spy SHIFTED LEFT BY ONE at
// +0xBC; Xbox One puts m_spy at +0xBC, the node at +0xD0 and the inverse masses at
// +0xF4/+0x144 (2 groups of 5, stride 0x50, where X360 uses 3 groups of 4, stride 0x40).
// NO OFFSET FROM EITHER MAY BE IMPORTED.
// =====================================================================================

struct Jacobian
{
    // xyz = the constraint rows and the M^-1 J^T products; `.w` is a genuine float lane for
    // every row except 0, 1, 4 and 12, whose payloads are the four members below.
    rw::math::vpu::Vector4 mRows[24];

    u32   mIdA;      // console mRows[0].w   +0x0C
    u32   mIdB;      // console mRows[1].w   +0x1C
    u32   mSpy;      // console mRows[4].w   +0x4C
    void* mpNode;    // console mRows[12].w  +0xCC -- the only true pointer in the record
};

// ⚠️⚠️ THE ARRAY STRIDE IS `sizeof(Jacobian)`, **NOT** THE CONSOLE'S 384.
// Promoting the node pointer out of a w lane makes the host record larger, exactly as it made
// the Xbox One record 400 bytes. Every walk of m_CJ_Stack / m_JJ_Stack / m_DJ_Stack -- the two
// batch builders, SetWorkspace's carving, and any future pipeline -- must index with this, or
// consecutive records OVERLAP and the solver reads its neighbour's constraint rows. That is a
// silent, plausible-output corruption of exactly the class this subsystem keeps producing.
// The console literal is kept next to it, for decode documentation only.
inline u32 JacobianStride()      { return static_cast<u32>(sizeof(Jacobian)); }
const u32 KU_JACOBIAN_STRIDE_X360 = 0x180u;   // 384 -- CONSOLE, do not index the host with it

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
