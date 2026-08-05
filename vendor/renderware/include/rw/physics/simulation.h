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
struct Contact;   // contact.h -- class-key `struct` there; must match here (same MSVC
                  // mangling rule as the Joint/Drive note above)

// simulation.h:202 (DWARF). The committed header's guess ("bit0 joints, bit1 drives,
// bit2 contacts") was right; these are the real names.
enum SpyingFlag
{
    SPY_NOTHING  = 0,
    SPY_JOINTS   = 1,
    SPY_DRIVES   = 2,
    SPY_CONTACTS = 4
};

// =====================================================================================
// The 48-byte record SpyJointJacobians @0x82BC24F8 emits, IN PLACE, over the HEAD of
// m_JJ_Stack (the emit cursor starts at the array base and advances 0x30 per spied joint,
// strictly behind the 0x180-stride read cursor -- same invariant on the host, where
// sizeof(Jacobian) > 48). Consumed by CgsPhysics::PhysicsSimulationModule::
// AddJointSpiesToOutputQueue @0x828A58E0, which loads +0x00/+0x10, scales both rows by
// (m_TimeStep * 59.999996f) and pushes them into OutJointSpy events, and chases +0x20 to
// the Joint for its slot index (m_tag). Both ends access it BY NAME on the host, so the
// pointer widening moves muTag from console +0x24 to +0x28 harmlessly (per-frame scratch,
// never serialised -- the Jacobian::mpNode precedent, not the serialised-slot rule).
// =====================================================================================
struct JointJacobianSpy
{
    rw::math::vpu::Vector4 mForce;    // console +0x00  trilinear of rows 12/16/20, * 1/ts^2
    rw::math::vpu::Vector4 mTorque;   // console +0x10  MatIBT + rows 14/18/22 trilinear, * 1/ts^2
    Joint*                 mpJoint;   // console +0x20  the jacobian's mpNode
    u32                    muTag;     // console +0x24  Joint::m_tag (the JointData slot index)
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
    // ⭐ 2026-08-05 -- the Xbox One build's SimulationUpdate was located (sub_1409B7240,
    // structure-identical to the committed PC body, and it CONFIRMS the selector CODE above
    // the old comment's "2,3 -> Osiris" claim: 3 == joints+contacts goes to HORUS). Its call
    // set names an x64 ALGORITHM oracle for every remaining stage -- offsets stay X360:
    //     ContactBatchBuild = sub_1409AE210 (per-RECORD there: XB1's SimulationUpdate loops it
    //                         at stride 272 == this Contact widened for two body POINTERS the
    //                         batch parks in it -- see the SpyContactJacobians coupling note),
    //     Anubis = sub_1409ADBF0, Osiris = sub_1409B5E80, Isis = sub_1409B53E0,
    //     Horus = sub_1409B3CD0, DynamicUpdate = sub_1409B3180,
    //     SpyJoint = sub_1409B7B80, SpyDrive = sub_1409B7640, SpyContact = sub_1409B7390.
    // ⚠️ XB1 jacobian ROW GROUPING differs (2 groups of 5, stride 0x50 -- Jacobian.hpp note),
    // so no XB1 row offset may be imported; only the algebra.
    void SpyJointJacobians();     // @ 0x82BC24F8   97 insn  BODIED (Simulation.cpp, 2026-08-05)
    void SpyDriveJacobians();     // @ 0x82BC3010  193 insn
    void SpyContactJacobians();   // @ 0x82BC4138  107 insn
    // ⚠️⚠️ COUPLING, measured 2026-08-05 -- do not land these two piecemeal:
    //   * SpyContactJacobians reads the POST-ContactBatchBuild record: the batch parks TWO
    //     RigidBody POINTERS over the +0x7C/+0xAC snapshot lanes (`lwz r7,0x7C / lwz r8,0xAC`
    //     then `lvx128 vX, rN, 0x10` THROUGH them at 0x82BC4260) and a scalar over +0xDC.
    //     8-byte host pointers cannot live in those 4-byte lanes -- the PC ContactBatchBuild
    //     must either widen Contact (the XB1 route: 256 -> 272) or park mId indices and have
    //     the spy resolve them; EITHER WAY the two functions share one record contract and
    //     land TOGETHER, with contact.h's pins moved in the same commit.
    //   * SpyDriveJacobians additionally walks the Drive node (+0x00/+0x10/+0x14 pointers) for
    //     a ~60-insn quaternion/twist block (0x82BC30D0..0x82BC323C) that is NOT yet decoded
    //     to landing standard; its emit tail is the SpyJointJacobians trilinear + the two
    //     scalars {var_130.x, fabs(1 - dot4)} at +0x20/+0x24 of a 48-byte record.

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

    // GetFreeContact (DWARF simulation.h:475) -- bump-allocate the next 256-byte contact
    // record out of m_CJ_Stack, NULL when m_CT_Count reaches m_CT_Max. The console inlines it
    // into ProcessAddContactQueue @0x828A36B0 (`cmplw 0x64 vs 0x7C` / `slwi count,8` / `stw
    // count+1,0x64`); defined inline in contact.h, where Contact is complete. The DWARF's
    // GetFreeContactBatch (:479) / GetContact (:483) siblings have no witnessed body and are
    // not declared.
    Contact* GetFreeContact();

    // -------------------------------------------------------------------------------------
    // AddRigidBody @ 0x82BC3318 (669 instructions) -- **BODIED 2026-08-04 (task #140)**,
    // together with its only caller, CgsPhysics::PhysicsSimulationModule::
    // ProcessAddRigidBodyQueue @0x828A2708, so it is not an orphan. Task #138 decoded it and
    // deliberately did not write it, on the grounds that it had no caller anywhere in the
    // tree and a wrong lane would have been invisible; that condition no longer holds.
    //
    // SIGNATURE is DWARF-exact (DecFIGS simulation.h:308) and matches the asm: r3=this,
    // r4=&frame, r5=Inertia*, r6=BodyState.
    //
    // WHY 669 INSTRUCTIONS FOR ~120 OF LOGIC: three near-identical paths chosen by leState,
    // each with QuaternionFromMatrix33 and InertiaUpdate inlined into it. The DWARF names
    // the pieces the compiler is inlining (rigidbody.h :365 SetStatic, :369 SetDynamic,
    // :373 SetState, :387 InertiaUpdate, :391 ResetForces), which is why the body below
    // reads as six short steps rather than a 669-line transcription.
    //
    //   per-path table -- the three differ in FIVE VALUES ONLY:
    //       leState        ACTIVE(4) fallthrough  FROZEN(2) @0x82BC36CC  STATIC(1) @0x82BC3A28
    //       mState  +0x8C  4                      2                      1
    //       mCool   +0xAC  0                      m_CoolDown (+0xA4)     m_CoolDown (+0xA4)
    //       mInertia+0x5C  lpInertia (r5)         lpInertia (r5)         NULL   <-- ⚠️ ignores r5
    //       anchor         m_ActiveRB_Anchor+0x28 m_FrozenRB_Anchor+0x24 m_StaticRB_Anchor+0x20
    //       count          ++m_ActiveRB_Count     ++m_FrozenRB_Count     ++m_StaticRB_Count
    //                        (+0x48)                (+0x44)                (+0x40)
    //   Verified store by store this wave: `li r10,4`/`stw 0x8C` + `li r31,0`/`stw 0xAC` +
    //   `lwz r10,0x28(r3)` (ACTIVE); `li r10,2` + `lwz r10,0xA4(r3)` + `lwz r10,0x24(r3)`
    //   (FROZEN); `li r28,1` + `lwz r10,0xA4(r3)` + `lwz r10,0x20(r3)` + `stw r29(=0),0x5C`
    //   (STATIC -- the inertia argument really is dropped on this path).
    //
    // ⚠️⚠️ CORRECTION 2026-08-04 (#140) TO THE NOTE THAT USED TO STAND HERE. It read
    // "`vrlimi128 vD,vS,1,0` = take xyz from vS, keep w of vD". THE MECHANISM IS INVERTED.
    // The VMX128 mask is four bits, one per word, bit0 == word3, so mask==1 selects THE W
    // LANE ONLY: vD.w = vS.w, and vD.xyz is what was already in vD. PROOF from this very
    // function: `vmr v13,v1(=0)` ; `lvx128 v12,r0,&mVel` ; `vrlimi128 v13,v12,1,0` ;
    // `stvx128 v13,r0,&mVel`. Under the old reading that stores {mVel.xyz, 0} -- zeroing
    // mVel.w, which on the console is **mRight, the intrusive list `next` pointer**. Under
    // the correct reading it stores {0,0,0,mVel.w}: "zero the vector, keep the packed
    // scalar". The old note's CONCLUSION (w lanes are preserved) was right; a future wave
    // applying its stated mechanism literally would corrupt the body lists.
    //
    // The four constants the entry block materialises are quaternion/inertia scratch, not
    // state: `vspltisw v7,1` + `vcfsx v0,v7,1` == 0.5f and `vcfsx v7,v7,0` == 1.0f.
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

    // The three jacobian scratch arrays (:646..648). ⚠️ CORRECTION 2026-08-05: the stride
    // note that stood here ("KU_JACOBIAN_STRIDE apart") is true of the JOINT and DRIVE arrays
    // only. m_CJ_Stack is an array of 256-byte rw::physics::Contact records -- GetFreeContact
    // walks it at `slwi 8` (== sizeof(Contact), unchanged on the host: no pointer lanes) and
    // ContactBatchBuild at `addi r11, r11, 256`; the workspace sizer's `liMaxContacts << 8`
    // term agrees. 384 would overlap nothing today but would mis-seat every record.
    void*      m_CJ_Stack;           // X360 +0x10  contacts (stride sizeof(Contact) == 256)
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
