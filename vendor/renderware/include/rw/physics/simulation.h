#pragma once

// =====================================================================================
// rw::physics::Simulation -- the EATech RenderWare rigid-body physics simulation object.
// It owns the per-frame constraint solver: intrusive free/static/frozen/active lists of
// rigid bodies, joints and drives, plus the three jacobian scratch buffers carved out of
// the SimulationWorkspace. SimulationUpdate() drives one solver tick (contact/joint/drive
// batch build -> a solver pipeline -> the integrator -> optional jacobian "spy" dumps).
//
// EATech RenderWare physics. Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative for WHAT EXISTS and for the offsets.
//
// ⚠️⚠️ CORRECTION 2026-08-04 -- THE COMMENT THAT USED TO STAND HERE SAID
//      "No Feb-2007 reference source and no DecFIGS DWARF exist for this TU."
//      **THAT WAS FALSE AND IT COST THREE WAVES.** Twenty real DWARF headers sit in
//      references/DecFIGS/dwarfdump/SDKs/EATech/include/cmn/rw/physics/, simulation.h and
//      rigidbody.h among them. Every member name below is now the DWARF's own
//      (simulation.h:638..695) instead of a guess, and the same false claim in
//      rw/physics/pairset.h:9 is corrected there.
//
// ⭐ WHAT THE DWARF CHANGED, AND WHY IT IS NOT A COSMETIC RENAME. Seventeen members were
//    misnamed, and three of the errors were live semantic bugs:
//      * The three jacobian array pointers were ROTATED. +0x10 is the CONTACT array
//        (ContactBatchBuild @0x82BC14D4 pairs `lwz r9,0x64` with `lwz r11,0x10`), +0x14 the
//        JOINT array (JointBatchBuild @0x82BC6A5C), +0x18 the DRIVE array (DriveBatchBuild
//        @0x82BC6AE4, and Isis_Pipeline @0x82BC2230 pairs `lwz r30,0x74` with `lwz r3,0x18`).
//      * +0xA4 and +0xAC were swapped: +0xA4 is m_CoolDown (the SLEEP-COUNTER CEILING that
//        DynamicUpdate clamps mCool to at 0x82BC2F50), +0xAC is m_MaxIteration. The old names
//        put "max iterations" on the field the integrator uses as a frame count.
//      * The three "*Overflow" counters are the SPY counters m_CS_Count / m_JS_Count /
//        m_DS_Count.
//    ⭐ INDEPENDENT CROSS-CHECK that the DWARF names land on the right offsets: SetWorkspace's
//    body below was transcribed from the asm long before the DWARF was consulted. It stores
//    ws+0 at +0x14, ws+384*a2 at +0x18 and ws+384*a2+384*a3 at +0x10, and routes a2->+0x80,
//    a3->+0x84, a4->+0x7C. Under the DWARF names that reads exactly
//    `SetWorkspace(ws, maxJoints, maxDrives, maxContacts)` with the three buffers laid out
//    joint, drive, contact in that order -- self-consistent. Under the old names it is nonsense.
//
// LAYOUT: every member is accessed BY NAME -- the X360 byte offset in the trailing comment is
// decode documentation only; the PC/x64 layout naturally differs (8-byte pointers), which is
// fine because nothing here relies on byte offsets (semantic parity, not byte matching).
// =====================================================================================

#include "types.hpp"                 // u32 / f32
#include "rw/rwcore_structs.h"       // rw::BaseResourceDescriptors<5>
#include "rw/math/vpu/types.h"       // rw::math::vpu::Vector3
#include "rw/physics/rigidbody.h"    // rw::physics::RigidBody, BodyState

namespace rw
{
namespace physics
{

// ⚠️ These MUST match the class-key Joint.hpp / Drive.hpp use (`class`). MSVC mangles a
// reference by the FIRST declaration the TU sees, so a `struct` here and a `class` there
// produces two different mangled names for the same function and an LNK2019 that names a
// symbol which looks identical in the log.
class Joint;
class Drive;

// simulation.h:202 (DWARF). The committed header's guess ("bit0 joints, bit1 drives,
// bit2 contacts") was right; these are the real names.
enum SpyingFlag
{
    SPY_NOTHING  = 0,
    SPY_JOINTS   = 1,
    SPY_DRIVES   = 2,
    SPY_CONTACTS = 4
};

class Simulation
{
public:
    // The CONSOLE stride, in bytes, of one solver jacobian block (joint/drive share it).
    // VERIFIED three ways: X360 `mulli r10,r11,0x180` (DriveBatchBuild @0x82BC6B14,
    // JointBatchBuild @0x82BC6A8C); BurnoutPR `lea ecx,[eax+eax*2]` + `shl ecx,7`; and the
    // Xbox One build's `imul rcx,rax,190h` = 400, which is this 384 plus the node pointer
    // widened out of a w lane (+8) plus 8 bytes of tail alignment.
    //
    // ⚠️⚠️ DO NOT INDEX A HOST ARRAY WITH THIS. The PC record is larger for exactly the reason
    // the Xbox One one is, so SetWorkspace and the two batch builders walk with
    // `rw::physics::JacobianStride()` == sizeof(Jacobian). Indexing with 384 would overlap
    // consecutive records and the solver would read its neighbour's constraint rows.
    // Kept here as decode documentation.
    static const u32 KU_JACOBIAN_STRIDE = 0x180u;   // 384 -- CONSOLE ONLY

    // -------------------------------------------------------------------------------------
    // GetResourceDescriptor @ 0x82BC5090 -- build-time sizer for the simulation's own block.
    // STATIC (r3 is the output descriptor, not a `this`). Fills a 5-entry serialised resource
    // descriptor whose entry[0] sizes the single 64-byte-aligned block the simulation needs:
    //     entry[0] = { m_size = 240*luCountA + 32*(luCountB + luCountC + 32), m_alignment = 64 }
    //     entry[1..4] = { m_size = 0, m_alignment = 1 }
    // -------------------------------------------------------------------------------------
    static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(
        rw::BaseResourceDescriptors<5>* lpResult, int luCountA, int luCountB, int luCountC);

    // SetWorkspace @ 0x82BC54D8 -- carve the three jacobian scratch buffers out of the
    // pre-allocated workspace bump region and reset the per-frame counters. Returns true.
    // Argument names are the DWARF's order as recovered by the cross-check in the banner.
    bool SetWorkspace(void* lpWorkspace, int liMaxJoints, int liMaxDrives, int liMaxContacts);

    // SimulationUpdate @ 0x82BC6B40 -- run one solver tick for the given time step. No-ops
    // (returns false) when no bodies are active. Builds the contact/joint/drive jacobian
    // batches, dispatches the matching solver pipeline, integrates, then dumps any enabled
    // jacobian spies. Returns true when a step ran.
    // ⛔ Its home TU, Simulation_SimulationUpdate.cpp, is NOT MOUNTED -- see that file.
    bool SimulationUpdate(f32 lfTimeStep);

    // -------------------------------------------------------------------------------------
    // Solver stages driven by SimulationUpdate. Declared here (the class owns them).
    // BatchIntegrator / JointBatchBuild / DriveBatchBuild are bodied; the eight below them
    // are not (heavy VMX contact/solver math, still unreconstructed), which is exactly why
    // SimulationUpdate has to live in its own unmounted TU.
    // -------------------------------------------------------------------------------------
    void BatchIntegrator();       // @ 0x82BC2FC8  BODIED
    void JointBatchBuild();       // @ 0x82BC6A30  BODIED
    void DriveBatchBuild();       // @ 0x82BC6AB8  BODIED

    // ⭐ 2026-08-04 (task #138) -- MEASURED SIZES, so the next wave can plan instead of guess.
    // Instruction counts are from the IDA export set except Osiris, which is measured off the
    // .i64 (see below). Total for the eight: 1,805 instructions.
    void ContactBatchBuild();     // @ 0x82BC14C0  343 insn  (not reconstructed)
    void Anubis_Pipeline();       // @ 0x82BC11C0  192 insn  (contacts only)
    // ⚠️ CORRECTION 2026-08-04: Osiris_Pipeline is ABSENT FROM .ida-exports -- there is no
    // 0x82BC2680.json. Notes elsewhere in the tree treat "no export" as "no body"; it has one.
    // Recovered headless (IDA Pro 9.3 `idat.exe -A -S<script> -L<log> <copy>.i64`, the same
    // route that produced PairSet::ClearAll and PhysicsSimulationModule::Destruct):
    //     range 0x82BC2680 .. 0x82BC294C  = 716 bytes = 179 instructions
    //     the ONLY xref to it is SimulationUpdate @0x82BC6BE8
    // That makes it the SMALLEST of the four pipelines and the cheapest one to land first.
    void Osiris_Pipeline();       // @ 0x82BC2680  179 insn  (joints, or joints + contacts)
    void Isis_Pipeline();         // @ 0x82BC2218  184 insn  (drives only)
    void Horus_Pipeline();        // @ 0x82BC1A20  510 insn  (anything mixed with drives)
    void SpyJointJacobians();     // @ 0x82BC24F8   97 insn
    void SpyDriveJacobians();     // @ 0x82BC3010  193 insn
    void SpyContactJacobians();   // @ 0x82BC4138  107 insn

    // -------------------------------------------------------------------------------------
    // Rigid-body list membership. All three are the same shape: unlink from the current list,
    // append at the tail of a new circular one, rewrite the state word, move one body between
    // two counters. BODIED (Simulation.cpp) -- the "console RigidBody node offsets have no
    // faithful home" note that used to block them is retired; mRight/mLeft/mState are real
    // named members of RigidBody now.
    // -------------------------------------------------------------------------------------
    void ActivateRigidBody(RigidBody* lpBody);   // @ 0x82BC29E8  BODIED
    void FreezeRigidBody(RigidBody* lpBody);     // @ 0x82BC2A58  BODIED
    void RemoveRigidBody(RigidBody* lpBody);     // @ 0x82BC2950  BODIED

    // -------------------------------------------------------------------------------------
    // Still un-reconstructed: AddRigidBody is 669 VMX instructions, and the Add/Remove
    // Joint/Drive quartet needs the free-list splice the same way.
    //
    // ⭐⭐ 2026-08-04 (task #138) -- FULLY DECODED, NOT YET WRITTEN. The body is deliberately
    // NOT committed this wave because it PROVABLY CANNOT EXECUTE: its only caller would be
    // ProcessAddRigidBodyQueue @0x828A2708, which is absent from the tree, reached only
    // through ProcessInputBuffers @0x828A73C0, reached only from PhysicsSimulationModule::
    // Update @0x828A74D0 (not declared) under PhysicsModule::Update @0x825B0640 (still a link
    // stub in WorldLinkStubs.cpp). An unwitnessable body on the exact member the whole
    // campaign depends on is this project's worst-documented failure shape, so the DECODE is
    // recorded here instead and the next wave can transcribe it without re-deriving anything.
    //
    // SIGNATURE is DWARF-exact (DecFIGS simulation.h:308) and matches the asm: r3=this,
    // r4=&frame, r5=Inertia*, r6=BodyState.
    //
    // SHAPE: pop the free list, then ONE OF THREE near-identical paths chosen by leState.
    // The three differ in five values ONLY; everything else is the same code emitted 3x
    // (which is why the function is 669 instructions for ~120 of logic).
    //
    //   entry @0x82BC3318:
    //       if (m_FreeRB_Count == 0) return NULL;          // lwz 0x3C ; beq -> li r3,0
    //       --m_FreeRB_Count;                              // stw 0x3C
    //       RigidBody* b = m_FreeRB_Anchor->mRight;        // lwz 0x1C ; lwz 0x2C(anchor)
    //
    //   per-path table (X360 offsets in the trailing comments are decode documentation):
    //       leState        ACTIVE(4) fallthrough  FROZEN(2) @0x82BC36CC  STATIC(1) @0x82BC3A28
    //       mState  +0x8C  4                      2                      1
    //       mCool   +0xAC  0                      m_CoolDown (+0xA4)     m_CoolDown (+0xA4)
    //       mInertia+0x5C  lpInertia (r5)         lpInertia (r5)         NULL   <-- ⚠️ ignores r5
    //       anchor         m_ActiveRB_Anchor+0x28 m_FrozenRB_Anchor+0x24 m_StaticRB_Anchor+0x20
    //       count          ++m_ActiveRB_Count     ++m_FrozenRB_Count     ++m_StaticRB_Count
    //                        (+0x48)                (+0x44)                (+0x40)
    //
    //   common body, in emission order:
    //     1. unlink b from the free list and append it at the tail of the chosen circular
    //        list -- byte-for-byte the splice ActivateRigidBody/FreezeRigidBody already use.
    //     2. frame copy, w LANES PRESERVED (`vrlimi128 vD,vS,1,0` = take xyz from vS, keep
    //        w of vD). On the PC the w lanes are separate members, so this is a plain
    //        .xyz assignment of four rows:
    //             mRi  (+0x40) = frame.xAxis      mUp  (+0x50) = frame.yAxis
    //             mAt  (+0x60) = frame.zAxis      mCom (+0x10) = frame.pos
    //     3. mQuat (+0x00) = rw::math::vpu::QuaternionFromMatrix33(basis).
    //        ⭐ ALREADY COMMITTED -- src/vendor/renderware/physics/JointFrames.hpp:63. The
    //        three `vxor` masks the asm loads (unk_8327F120/F100/F0F0) are that inline's
    //        gQuatFromMat_{x,y,z}Signs. They read ALL ZERO out of the image because they sit
    //        in a 9,216-byte zero run of the RW `.data` segment (0x8327E000..0x83280400) --
    //        do NOT try to byte-recover them, and do NOT read the zeros as "no sign flip".
    //        The routine's ground truth is the Feb-2007 rwmath source already cited there.
    //     4. world inverse inertia, gated on `mInertia != NULL` (so the STATIC path always
    //        skips it -- the block is emitted but dead there). With I = mInertia's local
    //        inverse diagonal and the basis just written:
    //             I_world = Ix*(Ri (x) Ri) + Iy*(Up (x) Up) + Iz*(At (x) At)
    //        stored in the split rigidbody.h documents:
    //             mIfull (+0x70) = {Ixx, Ixy, Ixz}     mIsplt (+0x80) = {Izz, Iyy, Iyz}
    //        The asm builds it with `vpermwi128 .. 0x97` (= .zyyw) and `0x9B` (= .zyzw),
    //        exactly the pair RigidBody::DynamicUpdate already uses, then
    //             mInvm (+0x7C) = *(f32*)((char*)mInertia + 0x10)      // lvlx + vspltw
    //        ⚠️ `vmaddfp vD,vA,vB,vC` is `vD = vA*vC + vB` (ISA), not what the operand order
    //        reads like -- getting this backwards silently transposes the tensor.
    //     5. common tail:
    //             mVel (+0x20) = 0        mOmega (+0x30) = 0      mTorque (+0xA0) = 0
    //             mKine (+0x9C) = FLT_MAX                      // flt_821815B0 = 0x7F7FFFFF
    //             mForce (+0x90) = m_Gravity                   // lvx from mStasis + 0x90
    //        ⚠️ mForce is SEEDED WITH GRAVITY, not zeroed. mStasis (+0x4C) is NOT written
    //        here -- Initialize threads it when it builds the free list.
    //     6. return b.                                        // mr r3, r11
    // -------------------------------------------------------------------------------------
    RigidBody* AddRigidBody(const rw::math::vpu::Matrix44Affine& lrFrame,
                            Inertia* lpInertia, BodyState leState);   // @ 0x82BC3318
    Joint* AddJoint(RigidBody* lpA, RigidBody* lpB, void* lpFrames, void* lpLimits);   // @ 0x82BC3D90
    Drive* AddDrive(RigidBody* lpA, RigidBody* lpB, void* lpFrames, void* lpDynamics);  // @ 0x82BC3E28
    void   RemoveJoint(Joint* lpJoint);          // @ 0x82BC2AC8
    void   RemoveDrive(Drive* lpDrive);          // @ 0x82BC2B20

    // -------------------------------------------------------------------------------------
    // Initialize @ 0x82BC5158 (224 instructions) -- carve the whole node graph out of the
    // single block the allocator handed back for GetResourceDescriptor's size, and thread
    // the three free lists. BODIED.
    //
    // ⚠️ THE COMMITTED DECLARATION THAT USED TO STAND HERE, `Simulation* Initialize(void**)`,
    // WAS WRONG TWICE: it is STATIC (r3 is the Resource block array, not a `this` -- the
    // object being initialised is `memory[0]`), and it takes the three counts. The DWARF
    // spells it `Simulation* Initialize(const Resource&, uint32_t, uint32_t, uint32_t)`
    // (simulation.h:277); the X360 reads the counts out of r4/r5/r6.
    // -------------------------------------------------------------------------------------
    static Simulation* Initialize(void** lpMemory, int liNumBodies, int liNumJoints, int liNumDrives);

    // DWARF accessors (simulation.h:351..371 / :548). All inlined away on the console.
    f32                           GetTimeStep() const       { return m_TimeStep; }
    const rw::math::vpu::Vector3& GetGravity() const        { return m_Gravity; }
    u32                           GetCoolDown() const       { return m_CoolDown; }
    f32                           GetFreezingEnergy() const { return m_MinEnergy; }
    u32                           GetMaxIteration() const   { return m_MaxIteration; }
    SpyingFlag                    GetSpyingMode() const     { return m_SpyFlag; }
    void*                         GetReactionForces() const { return m_RF_Stack; }
    u32                           GetMaxRigidBody() const   { return m_RF_Max; }
    u32                           GetFreeBodyCount() const  { return m_FreeRB_Count; }
    u32                           GetActiveBodyCount() const { return m_ActiveRB_Count; }
    u32                           GetFreeJointCount() const { return m_FreeJT_Count; }
    u32                           GetFreeDriveCount() const { return m_FreeDR_Count; }

    // DWARF mutators (simulation.h:383..399). All inlined away on the console -- they are
    // the individual stores CgsPhysics::PhysicsSimulationModule::AllocateMemoryAndInitialiseRW
    // makes into +0x90 / +0xA4 / +0xA8 / +0xAC / +0xB0 right after Initialize returns.
    void SetGravity(const rw::math::vpu::Vector3& lrGravity) { m_Gravity = lrGravity; }
    void SetCoolDown(u32 luCoolDown)                         { m_CoolDown = luCoolDown; }
    void SetFreezingEnergy(f32 lfEnergy)                     { m_MinEnergy = lfEnergy; }
    void SetMaxIteration(u32 luIterations)                   { m_MaxIteration = luIterations; }
    void SetSpyingMode(SpyingFlag leFlag)                    { m_SpyFlag = leFlag; }

private:
    // Node pool bases (simulation.h:638..640).
    RigidBody* m_RB_Stack;           // X360 +0x00
    Joint*     m_JT_Stack;           // X360 +0x04
    Drive*     m_DR_Stack;           // X360 +0x08

    // The reaction-force block array: 64 bytes per body, indexed by RigidBody::mId.
    // (:643. The old name here was `mpBodyPoses`; the block is four impulse/delta
    // accumulators that DynamicUpdate consumes and zeroes every tick, not a pose.)
    void*      m_RF_Stack;           // X360 +0x0C

    // The three jacobian scratch arrays, KU_JACOBIAN_STRIDE apart (:646..648).
    void*      m_CJ_Stack;           // X360 +0x10  contacts
    void*      m_JJ_Stack;           // X360 +0x14  joints
    void*      m_DJ_Stack;           // X360 +0x18  drives

    // Circular intrusive list anchors (:651..658).
    RigidBody* m_FreeRB_Anchor;      // X360 +0x1C
    RigidBody* m_StaticRB_Anchor;    // X360 +0x20
    RigidBody* m_FrozenRB_Anchor;    // X360 +0x24
    RigidBody* m_ActiveRB_Anchor;    // X360 +0x28
    Joint*     m_FreeJT_Anchor;      // X360 +0x2C
    Joint*     m_ActiveJT_Anchor;    // X360 +0x30
    Drive*     m_FreeDR_Anchor;      // X360 +0x34
    Drive*     m_ActiveDR_Anchor;    // X360 +0x38

    // Membership counts (:661..668).
    u32        m_FreeRB_Count;       // X360 +0x3C
    u32        m_StaticRB_Count;     // X360 +0x40
    u32        m_FrozenRB_Count;     // X360 +0x44
    u32        m_ActiveRB_Count;     // X360 +0x48   <- what SimulationUpdate gates on
    u32        m_FreeJT_Count;       // X360 +0x4C
    u32        m_ActiveJT_Count;     // X360 +0x50
    u32        m_FreeDR_Count;       // X360 +0x54
    u32        m_ActiveDR_Count;     // X360 +0x58

    // Per-frame solver counters (:669..680). The `*S_Count` trio are the SPY counters.
    u32        m_JT_Stride;          // X360 +0x5C  (= KU_JACOBIAN_STRIDE)
    u32        m_DR_Stride;          // X360 +0x60  (= KU_JACOBIAN_STRIDE)
    u32        m_CT_Count;           // X360 +0x64  contact jacobians built this frame
    u32        m_CS_Count;           // X360 +0x68  contact spies
    u32        m_JT_Count;           // X360 +0x6C  joint jacobians built this frame
    u32        m_JS_Count;           // X360 +0x70  joint spies
    u32        m_DR_Count;           // X360 +0x74  drive jacobians built this frame
    u32        m_DS_Count;           // X360 +0x78  drive spies
    u32        m_CT_Max;             // X360 +0x7C
    u32        m_JT_Max;             // X360 +0x80
    u32        m_DR_Max;             // X360 +0x84
    u32        m_RF_Max;             // X360 +0x88  reaction-force block capacity

    // Solver parameters (:683..690). The console pads +0x8C so the vector lands 16-aligned.
    rw::math::vpu::Vector3 m_Gravity;   // X360 +0x90
    f32        m_TimeStep;           // X360 +0xA0  set by SimulationUpdate
    u32        m_CoolDown;           // X360 +0xA4  the sleep-counter CEILING
    f32        m_MinEnergy;          // X360 +0xA8  the freezing-energy threshold
    u32        m_MaxIteration;       // X360 +0xAC
    SpyingFlag m_SpyFlag;            // X360 +0xB0

    u32        m_selectedStages;     // X360 +0xB4  (:694)
    char       m_DMApad[16];         // X360 +0xB8  (:695)
};

} // namespace physics
} // namespace rw
