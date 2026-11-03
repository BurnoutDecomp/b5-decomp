// =====================================================================================
// rw::physics::Simulation -- definition home for the RenderWare physics simulation object.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is authoritative.
//
// ⚠️ CORRECTION 2026-08-04: the banner that used to stand here said "No Feb-2007 reference
// source and no DecFIGS DWARF exist for this TU". That was FALSE -- see the correction block
// in rw/physics/simulation.h. Names below are DWARF-authoritative.
//
// This TU has 22 X360 functions. Reconstructed here:
//     GetResourceDescriptor  @ 0x82BC5090      RemoveJoint/RemoveDrive/AddJoint/AddDrive
//     SetWorkspace           @ 0x82BC54D8      Initialize            @ 0x82BC5158
//     BatchIntegrator        @ 0x82BC2FC8      AddRigidBody          @ 0x82BC3318
//     ActivateRigidBody      @ 0x82BC29E8      SpyJointJacobians     @ 0x82BC24F8
//     FreezeRigidBody        @ 0x82BC2A58      ContactBatchBuild     @ 0x82BC14C0   [08-06]
//     RemoveRigidBody        @ 0x82BC2950      Anubis/Osiris/Isis/Horus pipelines   [08-06]
//     JointBatchBuild        @ 0x82BC6A30      SpyContact/SpyDrive Jacobians        [08-06]
//     DriveBatchBuild        @ 0x82BC6AB8      SimulationUpdate      @ 0x82BC6B40   [08-06]
//
// ⭐ THE BLOCK NOTE THIS FILE USED TO CARRY IS RETIRED. It said the list splice/walk
// functions were blocked because "these read the console RigidBody's intrusive node fields
// (next @+0x2C, prev @+0x3C, state @+0x8C) ... named as the pose-vector FLOAT LANES in the
// committed rw/physics/rigidbody.h, so naming them faithfully would require retyping that
// committed home". That retyping is exactly what happened: mRight / mLeft / mState / mCool
// are real named members now, and the four functions are transcribed instruction for
// instruction below.
//
// ⭐ 2026-08-04 (task #135): Initialize @0x82BC5158 IS NOW BODIED -- it is the function that
// makes every other one above reachable, because it is what CgsPhysics::PhysicsSimulationModule::
// AllocateMemoryAndInitialiseRW assigns to mpSimulation.
//
// ⭐⭐ 2026-08-06 (the pipelines wave): THE "STILL BLOCKED" LIST THAT STOOD HERE IS RETIRED.
// ContactBatchBuild, the four pipelines and the two remaining spies are reconstructed at the
// bottom of this file, SimulationUpdate moved home, and the quarantine TU is deleted. The
// TU's 22 X360 functions are all accounted for. Oracle discipline for the new bodies:
//   [X360] every offset, the loop structure, the branchless vsel masking, the store map --
//          asm read instruction for instruction (Anubis clamp, batch mint/restitution gate
//          and both spies lane-verified; permute tables 0x82181650..0x821817A0 read off the
//          image with x360rd, incl. 0x82181760 = the {a.w,b.w,c.w} gather and 0x821817A0 =
//          {FLT_MAX,0,0,FLT_MAX} = the friction clamp's open corners).
//   [XB1]  sub_1409AE210 (batch, per-record), sub_1409ADBF0/B5E80/B53E0/B3CD0 (pipelines),
//          sub_1409B7390/7640 (spies) -- ALGORITHM oracles; SSE immediates settle every
//          lane. ⛔ Its record slots differ from X360's and none are imported.
// ⚠️ NOTHING CALLS SimulationUpdate YET (PhysicsSimulationModule::Update @0x828A74D0 is
// unbodied), so /OPT:REF strips the whole solver cluster: mounting it enforces link closure
// and buys zero exe bytes. That is the expected outcome, not a failure.
// =====================================================================================

#include "rw/physics/simulation.h"
#include "rw/physics/contact.h"                     // Contact + the ContactJacobian overlay

#include "vendor/renderware/physics/Jacobian.hpp"   // the 384-byte record + JacobianStride()
#include "vendor/renderware/physics/JacobianMath.hpp"
#include "vendor/renderware/physics/Joint.hpp"
#include "vendor/renderware/physics/Drive.hpp"
#include "vendor/renderware/physics/DriveFrames.hpp" // SpyDriveJacobians re-runs Build's prologue
#include "vendor/renderware/physics/JointFrames.hpp" // rw::math::vpu::QuaternionFromMatrix33 (AddRigidBody)

#include <string.h>   // memset -- the `vspltisb v0,0` + four stvx128 block-clear
#include <cfloat>     // FLT_MAX -- AddRigidBody's mKine seed (X360 flt_821815B0 = 0x7F7FFFFF)
#include <cmath>      // std::fabs -- SpyDriveJacobians' angular separation

namespace rw
{
namespace physics
{

// [DIAG] b5-decomp#2 witness hook (simulation.h). NULL == inert. DELETE-WHEN closed.
Simulation::ContactDiagHook Simulation::gpContactDiagHook = 0;

namespace
{
    // -------------------------------------------------------------------------------------
    // THE ONE BLOCK LAYOUT, shared by GetResourceDescriptor (which sizes it) and Initialize
    // (which carves it). Keeping them in one place is not tidiness: if the sizer and the
    // carver ever disagree by one byte the solver walks off the end of its own allocation
    // with no diagnostic.
    //
    // The console block is, in order (all read off Initialize @0x82BC5158):
    //     [ Simulation ]                     RoundUp(192, alignment)   <- the object itself
    //     [ reaction-force blocks ]          64  * liNumBodies
    //     [ RigidBody array ]                176 * (liNumBodies + 4)   <- +4 list sentinels
    //     [ Joint array ]                    32  * (liNumJoints + 2)   <- +2 list sentinels
    //     [ Drive array ]                    32  * (liNumDrives + 2)   <- +2 list sentinels
    // and 192 + 4*176 + 2*32 + 2*32 == 1024, which is exactly the constant folded into
    // GetResourceDescriptor's `32 * (joints + drives + 32)`, and 64 + 176 == 240 is its
    // per-body term. The two functions agree on the console; they must agree here too.
    //
    // ⚠️⚠️ EVERY ONE OF THOSE FIVE STRIDES WIDENS ON x64 (sizeof(RigidBody) is 240, not 176;
    // sizeof(Joint)/sizeof(Drive) are 96, not 32 -- the DWARF's m_offset/m_pad[8] tail is
    // PS3-only but it is in the committed type, and the six node pointers are 8 bytes each).
    // Carving with the console literals would overlap consecutive bodies and the integrator
    // would read its neighbour's pose. Same class of bug as KU_JACOBIAN_STRIDE. All five
    // come from sizeof() here and the console numbers are decode documentation only.
    // -------------------------------------------------------------------------------------
    const u32 KU_REACTION_FORCE_STRIDE = 64u;   // four impulse/delta accumulators; no pointers,
                                                // so this one really is 64 on both.

    inline u32 RoundUpTo(u32 luValue, u32 luAlignment)
    {
        // X360: `addi r9,r11,0xBF ; addi r10,r11,-1 ; andc r9,r9,r10` with r11 = alignment,
        // i.e. (alignment - 1 + 192) & ~(alignment - 1) == RoundUp(192, alignment).
        return (luValue + luAlignment - 1u) & ~(luAlignment - 1u);
    }

    inline u32 SimulationBlockSize(int liNumBodies, int liNumJoints, int liNumDrives, u32 luAlignment)
    {
        return RoundUpTo(static_cast<u32>(sizeof(Simulation)), luAlignment)
             + KU_REACTION_FORCE_STRIDE      * static_cast<u32>(liNumBodies)
             + static_cast<u32>(sizeof(RigidBody)) * static_cast<u32>(liNumBodies + 4)
             + static_cast<u32>(sizeof(Joint))     * static_cast<u32>(liNumJoints + 2)
             + static_cast<u32>(sizeof(Drive))     * static_cast<u32>(liNumDrives + 2);
    }
}

// -------------------------------------------------------------------------------------
// Simulation::GetResourceDescriptor @ 0x82BC5090
//
// STATIC sizer (r3 = output descriptor). The compiler materialised the 5-entry {0,1}
// descriptor on a stack scratch several times before writing it through the result
// pointer; those scratch fills are dead, so only the observable result fill is modelled
// (same pattern as SimulationWorkspace::GetResourceDescriptor).
//
//   entry[0..4] = { m_size = 0, m_alignment = 1 }
//   r9 = 32 * (a3 + a4 + 32) + 240 * a2                 ; (r5+r6+0x20)<<5 + r4*0xF0
//   entry[0] = { m_size = r9, m_alignment = 64 }        ; std {size,64} qword, big-endian
// -------------------------------------------------------------------------------------
rw::BaseResourceDescriptors<5>* Simulation::GetResourceDescriptor(
    rw::BaseResourceDescriptors<5>* lpResult, int luCountA, int luCountB, int luCountC)
{
    rw::BaseResourceDescriptor* lpEntries = lpResult->m_baseResourceDescriptors;

    // entry[0..4] = { m_size = 0, m_alignment = 1 }
    for (u32 luEntry = 0u; luEntry < 5u; ++luEntry)
    {
        lpEntries[luEntry].m_size      = 0u;
        lpEntries[luEntry].m_alignment = 1u;
    }

    // CONSOLE: size = 240 * luCountA + 32 * (luCountB + luCountC + 32)
    //   ; (r5+r6+0x20)<<5 + r4*0xF0
    // HOST: the same block, measured with the host's own strides -- see SimulationBlockSize
    // above for why the console literals cannot be used to size a block Initialize then
    // carves with sizeof(). The alignment (64) is a hardware constraint, not a stride, and
    // is unchanged.
    const u32 luSize = SimulationBlockSize(luCountA, luCountB, luCountC, 64u);

    // entry[0] = { m_size = size, m_alignment = 64 }
    lpEntries[0].m_size      = luSize;
    lpEntries[0].m_alignment = 64u;
    return lpResult;
}

// -------------------------------------------------------------------------------------
// Simulation::RemoveJoint @ 0x82BC2AC8   (21 instructions, all scalar)
// Simulation::RemoveDrive @ 0x82BC2B20   (21 instructions, all scalar)
//
// The rigid-body MoveToListTail splice, one node type over: unlink from the active list,
// re-insert at the TAIL of the free list, then move one entry between the two counters.
// Joint and Drive share the layout (m_right @+0x08, m_left @+0x0C), so the two bodies are
// instruction-identical apart from the anchor (+0x2C vs +0x34) and the counter pair
// (+0x50/+0x4C vs +0x58/+0x54).
//
// ⚠️ There is NO state word to clear here and no `& 7` switch: unlike a rigid body, a joint
// or a drive is either on the active list or on the free list, so the counter move is
// unconditional. Reproducing RemoveRigidBody's switch would be inventing a branch.
// (The `clrlwi r10,r4,0` before the last store is the 64-bit register file's 32-bit
// zero-extend of a pointer, same artefact as in MoveToListTail; it moves no data.)
// -------------------------------------------------------------------------------------
void Simulation::RemoveJoint(Joint* lpJoint)
{
    lpJoint->m_right->m_left = lpJoint->m_left;
    lpJoint->m_left->m_right = lpJoint->m_right;

    Joint* const lpAnchor = m_FreeJT_Anchor;              // +0x2C
    lpJoint->m_right          = lpAnchor;
    lpJoint->m_left           = lpAnchor->m_left;
    lpAnchor->m_left->m_right = lpJoint;
    lpAnchor->m_left          = lpJoint;

    --m_ActiveJT_Count;                                   // +0x50
    ++m_FreeJT_Count;                                     // +0x4C
}

void Simulation::RemoveDrive(Drive* lpDrive)
{
    lpDrive->m_right->m_left = lpDrive->m_left;
    lpDrive->m_left->m_right = lpDrive->m_right;

    Drive* const lpAnchor = m_FreeDR_Anchor;              // +0x34
    lpDrive->m_right          = lpAnchor;
    lpDrive->m_left           = lpAnchor->m_left;
    lpAnchor->m_left->m_right = lpDrive;
    lpAnchor->m_left          = lpDrive;

    --m_ActiveDR_Count;                                   // +0x58
    ++m_FreeDR_Count;                                     // +0x54
}

// -------------------------------------------------------------------------------------
// Simulation::AddDrive @ 0x82BC3E28   (37 instructions)  -- BODIED 2026-08-04 (task #143).
// Sole in-game caller: PhysicsSimulationModule::ProcessAddDriveQueue @0x828A4E98.
//
// ⭐ THIS IS THE EXACT INVERSE OF RemoveDrive DIRECTLY ABOVE, and that is the control that
// makes it safe to write: RemoveDrive was decoded from the same headless dump in the same
// pass and reproduced the already-committed body instruction-for-instruction, so the splice
// idiom here is not being read for the first time. Pop the head of the free list, unlink it,
// re-insert at the TAIL of the active list, fill the payload, move one entry between the two
// counters.
//
// ⚠️ EXHAUSTION IS A NULL RETURN, NOT AN ASSERT -- `lwz r10,0x54(r11)` / `cmplwi r10,0` /
// `li r3,0`, the same convention AddRigidBody uses for its own pool.
//
// ⚠️ THE TWO BODY ARGUMENTS ARE NOT SYMMETRIC, and the mapping is pinned by BOTH ends rather
// than guessed. The console stores `stw r5,0x10` and `stw r4,0x14`, i.e. arg2 -> m_bodyA and
// arg1 -> m_bodyB. ProcessAddDriveQueue passes the PARENT body in r4 and the CHILD in r5, and
// ProcessRemoveDriveQueue then reads +0x10 as the child and +0x14 as the parent when it calls
// PairSet::UnlinkParts in the same (parent, child) order that ProcessAddDriveQueue passes to
// PairSet::LinkParts. ⭐ That independently CONFIRMS the two [INFERRED] annotations on
// Drive::GetChild()/GetParent() in Drive.hpp -- m_bodyA really is the child.
//
// ⚠️ m_tag (+0x18) is NOT written here. The caller writes it immediately afterwards
// (`stw r30,0x18(r3)` at 0x828A4EA8, the DriveData slot index), along with m_spy. Only the
// `stw r8,0x1C` zeroing of m_spy belongs to this function.
// -------------------------------------------------------------------------------------
Drive* Simulation::AddDrive(RigidBody* lpA, RigidBody* lpB, void* lpFrames, void* lpDynamics)
{
    if (m_FreeDR_Count == 0u)                             // +0x54
        return 0;

    Drive* const lpDrive = m_FreeDR_Anchor->m_right;      // +0x34, head of the free list

    lpDrive->m_right->m_left = lpDrive->m_left;
    lpDrive->m_left->m_right = lpDrive->m_right;

    Drive* const lpAnchor = m_ActiveDR_Anchor;            // +0x38
    lpDrive->m_right          = lpAnchor;
    lpDrive->m_left           = lpAnchor->m_left;
    lpAnchor->m_left->m_right = lpDrive;
    lpAnchor->m_left          = lpDrive;

    lpDrive->m_skel  = static_cast<DriveFrames*>(lpFrames);      // +0x00  `stw r6,0(r3)`
    lpDrive->m_crtl  = static_cast<DriveDynamics*>(lpDynamics);  // +0x04  `stw r7,4(r3)`
    lpDrive->m_bodyA = lpB;                                      // +0x10  `stw r5,0x10(r3)`
    lpDrive->m_bodyB = lpA;                                      // +0x14  `stw r4,0x14(r3)`
    lpDrive->m_spy   = 0u;                                       // +0x1C  `stw r8(=0),0x1C(r3)`

    --m_FreeDR_Count;                                     // +0x54
    ++m_ActiveDR_Count;                                   // +0x58
    return lpDrive;
}

// -------------------------------------------------------------------------------------
// Simulation::AddJoint @ 0x82BC3D90   (37 instructions)  -- BODIED 2026-08-04 (task #144).
// Sole in-game caller: PhysicsSimulationModule::ProcessAddJointQueue @0x828A4960.
//
// ⭐ THE SAME CONTROL THAT MADE AddDrive SAFE APPLIES HERE, one node type over: this is the
// exact inverse of RemoveJoint above, and RemoveJoint was decoded from the same headless dump
// in the same pass and reproduced its already-committed body instruction-for-instruction (all
// 21). So the splice idiom is not being read for the first time. Pop the head of the free list,
// unlink it, re-insert at the TAIL of the active list, fill the payload, move one entry between
// the two counters. AddDrive is this function with +0x34/+0x38/+0x54/+0x58 in place of
// +0x2C/+0x30/+0x4C/+0x50; both are 37 instructions.
//
// ⚠️ EXHAUSTION IS A NULL RETURN, NOT AN ASSERT -- `lwz r10,0x4C(r11)` / `cmplwi r10,0` /
// `li r3,0`, the same convention AddRigidBody and AddDrive use for their own pools.
//
// ⚠️ THE TWO BODY ARGUMENTS ARE NOT SYMMETRIC, and the mapping is pinned by BOTH ends rather
// than guessed. The console stores `stw r5,0x10` and `stw r4,0x14`, i.e. arg2 -> m_bodyA and
// arg1 -> m_bodyB. ProcessAddJointQueue passes the PARENT body in r4 and the CHILD in r5, and
// ProcessRemoveJointQueue then reads +0x10 and +0x14 back out for PairSet::UnlinkParts in the
// same order ProcessAddJointQueue passes to PairSet::LinkParts. ⭐ That independently CONFIRMS
// the two [INFERRED] annotations on Joint::GetChild()/GetParent() in Joint.hpp -- m_bodyA
// really is the child -- by the identical argument the drive wave used for Drive.
//
// ⚠️ m_tag (+0x18) is NOT written here. The caller writes it immediately afterwards
// (`stw r28,0x18(r3)` at 0x828A4974, the JointData slot index) along with m_spy. Only the
// `stw r8,0x1C` zeroing of m_spy belongs to this function.
//
// ⚠️⚠️ THE `void*` PARAMETERS ARE THE CONSOLE'S, AND THEY ARE A FORK DETECTOR SWITCHED OFF.
// They are why CgsPhysics::JointFrames/JointLimits could shadow the real records for as long as
// they did -- a forked pointer converts here implicitly and silently, and neither the compiler
// nor the linker can see it. The forks are retired (task #144); the signature is kept as
// shipped. See [[odr-forks-link-silently]].
// -------------------------------------------------------------------------------------
Joint* Simulation::AddJoint(RigidBody* lpA, RigidBody* lpB, void* lpFrames, void* lpLimits)
{
    if (m_FreeJT_Count == 0u)                             // +0x4C
        return 0;

    Joint* const lpJoint = m_FreeJT_Anchor->m_right;      // +0x2C, head of the free list

    lpJoint->m_right->m_left = lpJoint->m_left;
    lpJoint->m_left->m_right = lpJoint->m_right;

    Joint* const lpAnchor = m_ActiveJT_Anchor;            // +0x30
    lpJoint->m_right          = lpAnchor;
    lpJoint->m_left           = lpAnchor->m_left;
    lpAnchor->m_left->m_right = lpJoint;
    lpAnchor->m_left          = lpJoint;

    lpJoint->m_skel  = static_cast<JointFrames*>(lpFrames);   // +0x00  `stw r6,0(r3)`
    lpJoint->m_limit = static_cast<JointLimits*>(lpLimits);   // +0x04  `stw r7,4(r3)`
    lpJoint->m_bodyA = lpB;                                   // +0x10  `stw r5,0x10(r3)`
    lpJoint->m_bodyB = lpA;                                   // +0x14  `stw r4,0x14(r3)`
    lpJoint->m_spy   = 0u;                                    // +0x1C  `stw r8(=0),0x1C(r3)`

    --m_FreeJT_Count;                                     // +0x4C
    ++m_ActiveJT_Count;                                   // +0x50
    return lpJoint;
}

// -------------------------------------------------------------------------------------
// Simulation::Initialize @ 0x82BC5158   (224 instructions)
//
// STATIC. r3 is the Resource block array the allocator filled, NOT a `this`: the object
// being initialised is memory[0], and every list node is carved out of the same block
// behind it. r4/r5/r6 carry the three capacities. Returns the Simulation*.
//
// Instruction-for-instruction, the console does:
//   1. re-run GetResourceDescriptor for the block ALIGNMENT alone (it reads only
//      `lwz r11, var_8C(r1)` == entry[0].m_alignment); the four unrolled {0,1} descriptor
//      fills either side of it are dead stack scratch and are not modelled.
//   2. m_RF_Max = bodies; m_RF_Stack = block + RoundUp(192, alignment).
//   3. zero the reaction-force blocks (`vspltisb v0,0` + four stvx128 per 64-byte block).
//   4. carve rigid bodies, joints, drives; the first FOUR bodies and the first TWO joints
//      and TWO drives are LIST SENTINELS, not usable nodes:
//        rb[0] free anchor, rb[1] static, rb[2] frozen, rb[3] active, rb[4..] the pool;
//        jt[0] free anchor, jt[1] active, jt[2..] the pool; likewise drives.
//   5. thread every pool node onto its free list (circular, through the anchor), and
//      self-link the anchors of the lists that start empty.
//   6. seed the solver parameters.
//
// ⚠️ THE SENTINELS ARE ONLY PARTLY INITIALISED, on purpose. The console writes mRight/mLeft
// on rb[0..3] and jt[0..1]/dr[0..1] and NOTHING else -- no mState, no mStasis, no mId. They
// are never dereferenced as bodies, only as list heads. Zeroing them "to be safe" would be
// inventing stores the console does not make.
//
// ARTIST stores each reaction-force block address in its body's +0x1C slot.
// -------------------------------------------------------------------------------------
Simulation* Simulation::Initialize(void** lpMemory, int liNumBodies, int liNumJoints, int liNumDrives)
{
    // 1. the alignment, straight back out of the sizer (the console calls it for this alone)
    rw::BaseResourceDescriptors<5> lDescriptor;
    Simulation::GetResourceDescriptor(&lDescriptor, liNumBodies, liNumJoints, liNumDrives);
    const u32 luAlignment = lDescriptor.m_baseResourceDescriptors[0].m_alignment;   // 64

    // 2. `lwz r3, 0(r29)` -- the object is the head of its own block.
    Simulation* const lpSim   = static_cast<Simulation*>(lpMemory[0]);
    char* const       lpBlock = reinterpret_cast<char*>(lpSim);

    lpSim->m_RF_Max = static_cast<u32>(liNumBodies);                                  // +0x88

    char* const lpReactionForces =
        lpBlock + RoundUpTo(static_cast<u32>(sizeof(Simulation)), luAlignment);
    lpSim->m_RF_Stack = lpReactionForces;                                             // +0x0C

    RigidBody* const lpaBodies = reinterpret_cast<RigidBody*>(
        lpReactionForces + KU_REACTION_FORCE_STRIDE * static_cast<u32>(liNumBodies));
    Joint* const lpaJoints = reinterpret_cast<Joint*>(
        reinterpret_cast<char*>(lpaBodies) + sizeof(RigidBody) * static_cast<u32>(liNumBodies + 4));
    Drive* const lpaDrives = reinterpret_cast<Drive*>(
        reinterpret_cast<char*>(lpaJoints) + sizeof(Joint) * static_cast<u32>(liNumJoints + 2));

    // 3. clear the reaction-force blocks
    if (liNumBodies != 0)
        memset(lpReactionForces, 0, KU_REACTION_FORCE_STRIDE * static_cast<size_t>(liNumBodies));

    // ---- rigid bodies -----------------------------------------------------------------
    lpSim->m_FreeRB_Anchor   = &lpaBodies[0];                    // +0x1C
    lpSim->m_FreeRB_Count    = static_cast<u32>(liNumBodies);    // +0x3C
    lpSim->m_StaticRB_Count  = 0u;                               // +0x40
    lpSim->m_FrozenRB_Count  = 0u;                               // +0x44
    lpSim->m_ActiveRB_Count  = 0u;                               // +0x48
    lpSim->m_StaticRB_Anchor = &lpaBodies[1];                    // +0x20
    lpSim->m_FrozenRB_Anchor = &lpaBodies[2];                    // +0x24
    lpSim->m_ActiveRB_Anchor = &lpaBodies[3];                    // +0x28
    lpSim->m_RB_Stack        = &lpaBodies[4];                    // +0x00

    // The three lists that start empty are self-linked circles.
    lpaBodies[1].mRight = &lpaBodies[1];   // stw r10, 0xDC(r11)  == rb[1] + 0x2C
    lpaBodies[1].mLeft  = &lpaBodies[1];
    lpaBodies[2].mRight = &lpaBodies[2];
    lpaBodies[2].mLeft  = &lpaBodies[2];
    lpaBodies[3].mRight = &lpaBodies[3];
    lpaBodies[3].mLeft  = &lpaBodies[3];

    if (liNumBodies != 0)
    {
        RigidBody* lpNode = lpSim->m_RB_Stack;
        RigidBody* lpPrev = lpSim->m_FreeRB_Anchor;
        u32        luId   = 0u;

        // `addic. r10,r4,-1 ; beq` -- the walk runs numBodies-1 times, the last node is
        // finished off below (it is the one that closes the circle).
        for (int liRemaining = liNumBodies - 1; liRemaining != 0; --liRemaining)
        {
            lpNode->mLeft   = lpPrev;                            // stw r8, 0x3C(r11)
            lpPrev          = lpNode;
            lpNode->mId     = reinterpret_cast<u32>(lpReactionForces + luId * KU_REACTION_FORCE_STRIDE);
            lpNode->mState  = FREE_BODY;                         // stw r31,0x8C(r11)
            ++luId;
            lpNode->mStasis = lpSim;                             // stw r3, 0x4C(r11)
            ++lpNode;                                            // addi r11,r11,0xB0
            lpPrev->mRight  = lpNode;                            // stw r11,0x2C(r8)
        }

        lpNode->mState  = FREE_BODY;
        lpNode->mLeft   = lpPrev;
        lpNode->mStasis = lpSim;
        lpNode->mId     = reinterpret_cast<u32>(lpReactionForces + static_cast<u32>(liNumBodies - 1) * KU_REACTION_FORCE_STRIDE);
        lpNode->mRight  = lpSim->m_FreeRB_Anchor;

        lpSim->m_FreeRB_Anchor->mRight = lpSim->m_RB_Stack;
        lpSim->m_FreeRB_Anchor->mLeft  = lpNode;
    }
    else
    {
        lpSim->m_FreeRB_Anchor->mRight = lpSim->m_FreeRB_Anchor;
        lpSim->m_FreeRB_Anchor->mLeft  = lpSim->m_FreeRB_Anchor;
    }

    // ---- joints -----------------------------------------------------------------------
    lpSim->m_FreeJT_Anchor   = &lpaJoints[0];                    // +0x2C
    lpSim->m_FreeJT_Count    = static_cast<u32>(liNumJoints);    // +0x4C
    lpSim->m_ActiveJT_Count  = 0u;                               // +0x50
    lpSim->m_ActiveJT_Anchor = &lpaJoints[1];                    // +0x30
    lpSim->m_JT_Stack        = &lpaJoints[2];                    // +0x04

    lpaJoints[1].m_right = &lpaJoints[1];   // stw r11, 0x28(r30)  == jt[1] + 0x08
    lpaJoints[1].m_left  = &lpaJoints[1];

    if (liNumJoints != 0)
    {
        Joint* lpNode = lpSim->m_JT_Stack;
        Joint* lpPrev = lpSim->m_FreeJT_Anchor;

        for (int liRemaining = liNumJoints - 1; liRemaining != 0; --liRemaining)
        {
            lpNode->m_left  = lpPrev;
            lpPrev          = lpNode;
            ++lpNode;
            lpPrev->m_right = lpNode;
        }

        lpNode->m_left  = lpPrev;
        lpNode->m_right = lpSim->m_FreeJT_Anchor;

        lpSim->m_FreeJT_Anchor->m_right = lpSim->m_JT_Stack;
        lpSim->m_FreeJT_Anchor->m_left  = lpNode;
    }
    else
    {
        lpSim->m_FreeJT_Anchor->m_right = lpSim->m_FreeJT_Anchor;
        lpSim->m_FreeJT_Anchor->m_left  = lpSim->m_FreeJT_Anchor;
    }

    // ---- drives (the identical body, one array over) -----------------------------------
    lpSim->m_FreeDR_Anchor   = &lpaDrives[0];                    // +0x34
    lpSim->m_FreeDR_Count    = static_cast<u32>(liNumDrives);    // +0x54
    lpSim->m_ActiveDR_Count  = 0u;                               // +0x58
    lpSim->m_ActiveDR_Anchor = &lpaDrives[1];                    // +0x38
    lpSim->m_DR_Stack        = &lpaDrives[2];                    // +0x08

    lpaDrives[1].m_right = &lpaDrives[1];
    lpaDrives[1].m_left  = &lpaDrives[1];

    if (liNumDrives != 0)
    {
        Drive* lpNode = lpSim->m_DR_Stack;
        Drive* lpPrev = lpSim->m_FreeDR_Anchor;

        for (int liRemaining = liNumDrives - 1; liRemaining != 0; --liRemaining)
        {
            lpNode->m_left  = lpPrev;
            lpPrev          = lpNode;
            ++lpNode;
            lpPrev->m_right = lpNode;
        }

        lpNode->m_left  = lpPrev;
        lpNode->m_right = lpSim->m_FreeDR_Anchor;

        lpSim->m_FreeDR_Anchor->m_right = lpSim->m_DR_Stack;
        lpSim->m_FreeDR_Anchor->m_left  = lpNode;
    }
    else
    {
        lpSim->m_FreeDR_Anchor->m_right = lpSim->m_FreeDR_Anchor;
        lpSim->m_FreeDR_Anchor->m_left  = lpSim->m_FreeDR_Anchor;
    }

    // ---- solver defaults ---------------------------------------------------------------
    // ⭐ THE GRAVITY DEFAULT IS -600.0f, NOT -9.81f, AND IT WAS READ, NOT INFERRED.
    // flt_8202CF3C was resolved by a headless IDA read of the X360 image: raw C4160000
    // big-endian == -600.0f. (The other two constants in this tail read 0.0f and 1.0f.)
    // The obvious guess -- "-9.81, because BrnPhysics::PhysicsModule::Prepare stage 3 fills
    // its SimulationParams with -9.81" -- is WRONG; that is the GAME's gravity, overwritten
    // through SetGravity two calls later. -600.0f is corroborated independently:
    // BrnGameState::GameStateModule::Construct @0x82380388 is flt_8202CF3C's only other
    // user and Hex-Rays renders it there as `-600.0`.
    lpSim->m_SpyFlag      = SPY_NOTHING;                                 // +0xB0
    lpSim->m_CoolDown     = 30u;                                         // +0xA4  li r8,0x1E
    lpSim->m_MinEnergy    = 1.0f;                                        // +0xA8  flt_82001C98
    lpSim->m_Gravity = rw::math::vpu::Vector3(0.0f, -600.0f, 0.0f);
    lpSim->m_MaxIteration = 50u;                                         // +0xAC  li r10,0x32

    return lpSim;
}

// -------------------------------------------------------------------------------------
// Simulation::SetWorkspace @ 0x82BC54D8
//
// Carve the three per-frame jacobian buffers out of the pre-allocated workspace bump
// region and reset the counters. (The three dead stack {0,1} descriptor fills the
// compiler emitted before the real stores are not modelled -- they are never read.)
//
// ⭐ THE ARGUMENT ORDER IS RECOVERED, NOT ASSUMED. The stores are ws+0 -> +0x14,
// ws+384*a2 -> +0x18, ws+384*a2+384*a3 -> +0x10, and a2 -> +0x80, a3 -> +0x84, a4 -> +0x7C.
// With the DWARF's names on those offsets (+0x14 m_JJ_Stack, +0x18 m_DJ_Stack,
// +0x10 m_CJ_Stack, +0x80 m_JT_Max, +0x84 m_DR_Max, +0x7C m_CT_Max) that reads as
// (joints, drives, contacts) with the buffers laid out in that same order -- consistent on
// six independent slots at once. The previous naming made the joint buffer the contact one.
// -------------------------------------------------------------------------------------
bool Simulation::SetWorkspace(void* lpWorkspace, int liMaxJoints, int liMaxDrives, int liMaxContacts)
{
    // ⚠️ THE STRIDE IS THE HOST `sizeof(Jacobian)`, NOT THE CONSOLE'S 384. Promoting the node
    // pointer out of a w lane makes the record larger on x64 (the shipping x64 build grew it
    // 384 -> 400 for the same reason). Carving with the console literal would make every
    // buffer overlap its neighbour by the difference. See Jacobian.hpp.
    const u32 luStride = JacobianStride();

    char* lpJointBase   = static_cast<char*>(lpWorkspace);
    char* lpDriveBase   = lpJointBase + luStride * static_cast<u32>(liMaxJoints);
    char* lpContactBase = lpDriveBase + luStride * static_cast<u32>(liMaxDrives);

    m_CT_Count = 0u;   // +0x64
    m_CS_Count = 0u;   // +0x68
    m_JT_Count = 0u;   // +0x6C
    m_JS_Count = 0u;   // +0x70
    m_DR_Count = 0u;   // +0x74
    m_DS_Count = 0u;   // +0x78

    m_JJ_Stack = lpJointBase;     // +0x14
    m_CJ_Stack = lpContactBase;   // +0x10
    m_DJ_Stack = lpDriveBase;     // +0x18

    m_JT_Max = static_cast<u32>(liMaxJoints);     // +0x80
    m_DR_Max = static_cast<u32>(liMaxDrives);     // +0x84
    m_CT_Max = static_cast<u32>(liMaxContacts);   // +0x7C

    m_JT_Stride = luStride;   // +0x5C  (the console stores its own 384 here)
    m_DR_Stride = luStride;   // +0x60
    return true;
}

// -------------------------------------------------------------------------------------
// Simulation::JointBatchBuild @ 0x82BC6A30   (34 instructions)
// Simulation::DriveBatchBuild @ 0x82BC6AB8   (34 instructions)
//
// Identical bodies against different members. VERIFIED three ways -- X360, BurnoutPR
// (sub_5996AD0 / sub_5992420, byte-identical Simulation offsets), and Xbox One
// (sub_1409B5B50 / sub_1409B30F0, every pointer widened 4 -> 8 and the stride 384 -> 400).
//
// The membership test is `(A->mState | B->mState) & ACTIVE_BODY`: X360 `or r11,r11,r10` then
// `rlwinm. r11,r11,0,29,29`, i.e. a constraint is built if EITHER body is awake. That single
// instruction pair is also what makes ACTIVE_BODY == 4 unarguable.
//
// ⚠️ The spy counter is zeroed BEFORE the built-count (X360 stores +0x70 then +0x6C), and the
// early-out happens after both -- so a frame with no active joints still clears the counters.
// -------------------------------------------------------------------------------------
void Simulation::JointBatchBuild()
{
    m_JS_Count = 0u;                                     // +0x70
    m_JT_Count = 0u;                                     // +0x6C

    if (m_ActiveJT_Count == 0u)                          // +0x50
        return;

    char* const lpArray = static_cast<char*>(m_JJ_Stack);   // +0x14
    const u32   luStride = JacobianStride();
    Joint*      lpNode   = m_ActiveJT_Anchor->m_right;      // +0x30, then node+0x08

    do
    {
        if (((lpNode->m_bodyA->mState | lpNode->m_bodyB->mState) & ACTIVE_BODY) != 0)
        {
            const u32 luSlot = m_JT_Count++;
            reinterpret_cast<JointJacobian*>(lpArray + luSlot * luStride)->Build(*lpNode, this);
        }
        lpNode = lpNode->m_right;
    }
    while (lpNode != m_ActiveJT_Anchor);
}

void Simulation::DriveBatchBuild()
{
    m_DS_Count = 0u;                                     // +0x78
    m_DR_Count = 0u;                                     // +0x74

    if (m_ActiveDR_Count == 0u)                          // +0x58
        return;

    char* const lpArray = static_cast<char*>(m_DJ_Stack);   // +0x18
    const u32   luStride = JacobianStride();
    Drive*      lpNode   = m_ActiveDR_Anchor->m_right;      // +0x38, then node+0x08

    do
    {
        if (((lpNode->m_bodyA->mState | lpNode->m_bodyB->mState) & ACTIVE_BODY) != 0)
        {
            const u32 luSlot = m_DR_Count++;
            reinterpret_cast<DriveJacobian*>(lpArray + luSlot * luStride)->Build(*lpNode, this);
        }
        lpNode = lpNode->m_right;
    }
    while (lpNode != m_ActiveDR_Anchor);
}

// -------------------------------------------------------------------------------------
// Simulation::BatchIntegrator @ 0x82BC2FC8   (17 instructions, no VMX)
//
// A circular intrusive walk of the ACTIVE body list, calling DynamicUpdate on each:
//     0x82BC2FDC  lwz r11,0x28(r31)      ; m_ActiveRB_Anchor
//     0x82BC2FE0  lwz r3,0x2C(r11)       ; anchor->mRight  -- the first real body
//     0x82BC2FE4  bl  RigidBody::DynamicUpdate
//     0x82BC2FE8  lwz r3,0x2C(r3)        ; <returned body>->mRight
//     0x82BC2FEC  lwz r11,0x28(r31)      ; RE-LOAD the anchor every iteration
//     0x82BC2FF0  cmplw / bne loc_82BC2FE4
//
// Two faithfulness details the asm settles and a paraphrase would lose:
//   * `lwz r3,0x2C(r3)` AFTER the call proves DynamicUpdate returns the body it was given
//     (r3 is never reassigned inside it) -- so BurnoutPR's Hex-Rays `return *(a1+76)` is an
//     artefact, and the Xbox One build agrees (its inlined copy at 0x1409B7308 reads
//     `mov rdi,[rdi+30h]` from the body it passed IN).
//   * the anchor is re-loaded from `this` on EVERY iteration, not hoisted. Kept.
//
// It is a DO-WHILE: the list is never empty here because SimulationUpdate has already
// returned early when m_ActiveRB_Count == 0.
// -------------------------------------------------------------------------------------
void Simulation::BatchIntegrator()
{
    RigidBody* lpBody = m_ActiveRB_Anchor->mRight;
    do
    {
        lpBody = lpBody->DynamicUpdate()->mRight;
    }
    while (lpBody != m_ActiveRB_Anchor);
}

namespace
{
    // The shared splice all three membership editors inline: unlink the body from whatever
    // circular list it is on, then re-insert it immediately BEFORE the target anchor (i.e.
    // at the tail of the target list).
    //     body->mRight->mLeft = body->mLeft;      ; lwz 0x2C / lwz 0x3C / stw 0x3C
    //     body->mLeft->mRight = body->mRight;     ; lwz 0x3C / lwz 0x2C / stw 0x2C
    //     body->mRight        = anchor;
    //     body->mLeft         = anchor->mLeft;
    //     anchor->mLeft->mRight = body;
    //     anchor->mLeft         = body;
    // (The `clrlwi r10,r4,0` before the last store is a 32-bit zero-extend of the pointer,
    // an artefact of the 64-bit PPC register file; it moves no data.)
    inline void MoveToListTail(RigidBody* lpBody, RigidBody* lpAnchor)
    {
        lpBody->GetRight()->SetLeft(lpBody->GetLeft());
        lpBody->GetLeft()->SetRight(lpBody->GetRight());

        lpBody->SetRight(lpAnchor);
        lpBody->SetLeft(lpAnchor->GetLeft());
        lpAnchor->GetLeft()->SetRight(lpBody);
        lpAnchor->SetLeft(lpBody);
    }
}

// -------------------------------------------------------------------------------------
// Simulation::ActivateRigidBody @ 0x82BC29E8   (28 instructions, all scalar)
//
// `rlwimi r11,r9,2,29,27` with r9 == 1: the mask MB=29/ME=27 wraps, so it covers every bit
// EXCEPT bit 28 (value 8) -- i.e. it writes (1 << 2) and PRESERVES SPY_BODY. That single
// instruction is what proves BodyState is a bitmask with ACTIVE_BODY == 4, and it is why the
// committed { DISABLED = 0, FROZEN = 1, ENABLED = 2 } enum was wrong.
// -------------------------------------------------------------------------------------
void Simulation::ActivateRigidBody(RigidBody* lpBody)
{
    MoveToListTail(lpBody, m_ActiveRB_Anchor);                                   // +0x28

    lpBody->mState = static_cast<BodyState>(ACTIVE_BODY | (lpBody->mState & SPY_BODY));
    lpBody->mCool  = m_CoolDown - 1u;   // +0xAC <- +0xA4 - 1

    --m_FrozenRB_Count;                 // +0x44
    ++m_ActiveRB_Count;                 // +0x48
}

// -------------------------------------------------------------------------------------
// Simulation::FreezeRigidBody @ 0x82BC2A58   (28 instructions)
// The mirror of Activate, with `rlwimi r11,r9,1,29,27` (FROZEN_BODY == 2) and the sleep
// counter seeded at the FULL cooldown rather than one below it.
// -------------------------------------------------------------------------------------
void Simulation::FreezeRigidBody(RigidBody* lpBody)
{
    MoveToListTail(lpBody, m_FrozenRB_Anchor);                                   // +0x24

    lpBody->mState = static_cast<BodyState>(FROZEN_BODY | (lpBody->mState & SPY_BODY));
    lpBody->mCool  = m_CoolDown;        // +0xAC <- +0xA4

    --m_ActiveRB_Count;                 // +0x48
    ++m_FrozenRB_Count;                 // +0x44
}

// -------------------------------------------------------------------------------------
// Simulation::RemoveRigidBody @ 0x82BC2950   (38 instructions)
//
// ⭐ THIS FUNCTION SETTLES A DISCREPANCY THE SPINE WAVE COULD ONLY NAME. It decrements
// +0x40 when (state & 7) == 1, +0x44 when == 2, and +0x48 otherwise. The committed count
// names (+0x40 static/disabled, +0x44 frozen, +0x48 active) and the committed enum
// (DISABLED = 0, FROZEN = 1, ENABLED = 2) could not both be right. The answer is that the
// COUNT NAMES WERE RIGHT and the enum was wrong: with the DWARF's bitmask
// (STATIC_BODY = 1, FROZEN_BODY = 2) every arm lands on its own counter.
// -------------------------------------------------------------------------------------
void Simulation::RemoveRigidBody(RigidBody* lpBody)
{
    MoveToListTail(lpBody, m_FreeRB_Anchor);                                     // +0x1C

    switch (lpBody->mState & STATE_FILTER)          // `clrlwi r11,r11,29`
    {
    case STATIC_BODY: --m_StaticRB_Count; break;    // +0x40
    case FROZEN_BODY: --m_FrozenRB_Count; break;    // +0x44
    default:          --m_ActiveRB_Count; break;    // +0x48
    }

    lpBody->mState = FREE_BODY;         // +0x8C <- 0
    ++m_FreeRB_Count;                   // +0x3C
}

// -------------------------------------------------------------------------------------
// Simulation::AddRigidBody @ 0x82BC3318   (669 instructions)  -- the exact inverse of
// RemoveRigidBody above: pop one node off the free list and splice it onto the static,
// frozen or active list. See simulation.h for the per-path table and the transcription
// notes; every offset quoted here was read off the asm this wave (task #140).
//
// The three console paths are ONE source-level body: the compiler emitted it three times
// because SetStatic/SetDynamic, QuaternionFromMatrix33 and InertiaUpdate are all inlined
// into each arm. Written once here, with the five per-path values selected up front --
// that is what the source has to have looked like for the console to emit what it emits.
// -------------------------------------------------------------------------------------
RigidBody* Simulation::AddRigidBody(const rw::math::vpu::Matrix44Affine& lrFrame,
                                    Inertia* lpInertia, BodyState leState)
{
    // `lwz r11,0x3C(r3)` ; `cmplwi r11,0` ; `li r3,0` -- the free list is the pool, so an
    // exhausted pool is a NULL return, not an assert.
    if (m_FreeRB_Count == 0u)
        return 0;

    --m_FreeRB_Count;                                   // `addi r9,r11,-1` ; `stw r9,0x3C`
    RigidBody* const lpBody = m_FreeRB_Anchor->mRight;  // `lwz r10,0x1C` ; `lwz r11,0x2C(r10)`

    // ---- the five per-path values (simulation.h's table) --------------------------------
    RigidBody* lpAnchor;
    u32*       lpuCount;
    u32        luCool;
    Inertia*   lpBodyInertia;

    switch (leState)                                    // `cmpwi r6,1` then `cmpwi r6,2`
    {
    case STATIC_BODY:                                   // @0x82BC3A28
        lpAnchor      = m_StaticRB_Anchor;              // +0x20
        lpuCount      = &m_StaticRB_Count;              // +0x40
        luCool        = m_CoolDown;                     // +0xAC <- +0xA4
        lpBodyInertia = 0;                        // ⚠️ `stw r29(=0),0x5C` -- drops lpInertia
        break;

    case FROZEN_BODY:                                   // @0x82BC36CC
        lpAnchor      = m_FrozenRB_Anchor;              // +0x24
        lpuCount      = &m_FrozenRB_Count;              // +0x44
        luCool        = m_CoolDown;                     // +0xAC <- +0xA4
        lpBodyInertia = lpInertia;                      // `stw r5,0x5C`
        break;

    default:                                            // ACTIVE_BODY, the fallthrough arm
        lpAnchor      = m_ActiveRB_Anchor;              // +0x28
        lpuCount      = &m_ActiveRB_Count;              // +0x48
        luCool        = 0u;                             // `li r31,0` ; `stw r31,0xAC`
        lpBodyInertia = lpInertia;                      // `stw r5,0x5C`
        break;
    }

    // ---- 1. unlink from the free list, append at the tail of the chosen list -------------
    // 0x82BC3374..0x82BC33C4 is MoveToListTail store for store.
    MoveToListTail(lpBody, lpAnchor);

    lpBody->mState   = leState;                         // +0x8C
    lpBody->mCool    = luCool;                          // +0xAC
    lpBody->mInertia = lpBodyInertia;                   // +0x5C

    // ---- 2. the frame, w lanes preserved -------------------------------------------------
    // `lvx128` the CURRENT register, `lvx128` the frame row, `vrlimi128 vFrame,vCur,1,0`
    // (w lane from the current value), store. Each fourth word is a distinct
    // packed member, so retain it while replacing the three vector lanes.
    //     mRi (+0x40) <- r4+0x00     mUp  (+0x50) <- r4+0x10
    //     mAt (+0x60) <- r4+0x20     mCom (+0x10) <- r4+0x30
    __stvx(__vrlimi(lrFrame.xAxis.mV, __lvx(&lpBody->mRi, 0), 1, 0), &lpBody->mRi, 0);
    __stvx(__vrlimi(lrFrame.yAxis.mV, __lvx(&lpBody->mUp, 0), 1, 0), &lpBody->mUp, 0);
    __stvx(__vrlimi(lrFrame.zAxis.mV, __lvx(&lpBody->mAt, 0), 1, 0), &lpBody->mAt, 0);
    __stvx(__vrlimi(lrFrame.wAxis.mV, __lvx(&lpBody->mCom, 0), 1, 0), &lpBody->mCom, 0);

    // ---- 3. the orientation quaternion ----------------------------------------------------
    // 0x82BC33D0..0x82BC35A0. The three `vxor` masks the asm loads (unk_8327F120/F100/F0F0)
    // are gQuatFromMat_{x,y,z}Signs; they read ALL ZERO out of the image only because they
    // sit in a 9,216-byte zero run of the RW `.data` segment (0x8327E000..0x83280400).
    // ⛔ Do NOT byte-recover them and do NOT read the zeros as "no sign flip" -- the routine
    // is already committed from the Feb-2007 rwmath source it was compiled from.
    {
        rw::math::vpu::Matrix33 lBasis;
        lBasis.xAxis = lrFrame.xAxis;
        lBasis.yAxis = lrFrame.yAxis;
        lBasis.zAxis = lrFrame.zAxis;
        lpBody->mQuat = rw::math::vpu::QuaternionFromMatrix33(lBasis);   // +0x00
    }

    // ---- 4. the world inverse inertia, gated on the MEMBER, not the argument ---------------
    // `lwz r10,0x5C(r11)` ; `cmplwi r10,0` ; `beq` -- the console re-reads mInertia back out
    // of the body it just stored it into (0x82BC34C0/0x82BC3814/0x82BC3B74, one per path), so
    // the STATIC arm always skips the block even though the block is emitted there.
    if (lpBody->mInertia != 0)
        lpBody->InertiaUpdate(lpBody->mInertia);        // mIfull/+0x70, mIsplt/+0x80, mInvm/+0x7C

    // ---- 5. the common tail ---------------------------------------------------------------
    // 0x82BC3660..0x82BC36B8. mVel/mOmega/mTorque are zeroed .xyz-only; mKine takes
    // flt_821815B0, re-read from the image this wave as 0x7F7FFFFF == FLT_MAX.
    const __vector4 lZero = __vspltisw(0);
    __stvx(__vrlimi(lZero, __lvx(&lpBody->mVel, 0), 1, 0), &lpBody->mVel, 0);
    __stvx(__vrlimi(lZero, __lvx(&lpBody->mOmega, 0), 1, 0), &lpBody->mOmega, 0);
    __stvx(__vrlimi(lZero, __lvx(&lpBody->mTorque, 0), 1, 0), &lpBody->mTorque, 0);
    lpBody->mKine     = FLT_MAX;                        // +0x9C

    // ⚠️⚠️ mForce IS SEEDED WITH GRAVITY, NOT ZEROED. `lwz r10,0x4C(r11)` reads the body's own
    // mStasis and `lvx128 v12,r10,r6(=0x90)` loads m_Gravity through it -- so the source
    // really is `ResetForces(GetSimulation()->GetGravity())` on the body, not `this`. Same
    // object (Initialize threads mStasis when it builds the free list), kept faithful anyway.
    // ⚠️ The DEFAULT gravity in this build is -600.0f (flt_8202CF3C), not -9.81f.
    {
        const rw::math::vpu::Vector3& lrG = lpBody->mStasis->GetGravity();
        __stvx(__vrlimi(lrG.mV, __lvx(&lpBody->mForce, 0), 1, 0), &lpBody->mForce, 0);
    }

    ++(*lpuCount);                                      // `lwz/addi/stw` on +0x40/+0x44/+0x48
    return lpBody;                                      // `mr r3,r11`
}

// -------------------------------------------------------------------------------------
// Simulation::SpyJointJacobians @ 0x82BC24F8   (97 instructions)   [2026-08-05]
//
// Walk the frame's joint jacobians and, for every one whose mSpy flag is set, emit a
// 48-byte JointJacobianSpy record over the HEAD of m_JJ_Stack (emit cursor = array base,
// +0x30 per record; read cursor = 0x180 per record on the console, sizeof(Jacobian) here,
// so the emit can never catch the read on either target).
//
//   force  = (R12*r2.x + R16*r2.y + R20*r2.z) / ts^2                 ; rows +0xC0/0x100/0x140
//   torque = (M0*r3.x + M1*r3.y + M2*r3.z                            ; M = GetMatIBT(jac)
//           +  R14*r3.x + R18*r3.y + R22*r3.z) / ts^2                ; rows +0xE0/0x120/0x160
// where r2/r3 = mRows[2]/mRows[3] (+0x20/+0x30) and 1/ts^2 is the console's vrefp + two
// Newton-Raphson refinements of 1/(m_TimeStep^2).
//
// ⭐ DECODE NOTE that unblocked this body (banked for the seven remaining stages): in this
// IDA listing plain `vmaddfp vD,vA,vB,vC` is the VA-form with the ADDEND PRINTED THIRD
// (vD = vA*vC + vB -- the brief's standing rule), while `vmaddfp128 vD,vA,vB,vC` prints the
// addend FOURTH (vD = vA*vB + vC). Both occur in this one function and each decodes to
// algebra the XB1 oracle (sub_1409B7B80) confirms only under its own rule.
//
// The original stores all four lanes, including the packed pointer and inverse-mass
// words that participate in its vector arithmetic.
//
// -------------------------------------------------------------------------------------
void Simulation::SpyJointJacobians()
{
    // ARTIST 0x82BC2558..2578: squared timestep, reciprocal estimate and
    // two fused Newton refinements. This path does not use a scalar divide.
    const __vector4 lTimeStep = __vspltw(__lvlx(&m_TimeStep, 0), 0);
    const __vector4 lTimeStepSq = __vmulfp(lTimeStep, lTimeStep);
    const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
    __vector4 lRecip = __vrefp(lTimeStepSq);
    lRecip = __vmaddfp(lRecip, __vnmsubfp(lTimeStepSq, lRecip, lOne), lRecip);
    lRecip = __vmaddfp(lRecip, __vnmsubfp(lTimeStepSq, lRecip, lOne), lRecip);
    const __vector4 lInvTsSq = __vmulfp(lRecip, lOne);

    if (m_JT_Count == 0u)                               // +0x6C (`ble` after the recip setup)
        return;

    char*             lpArray = static_cast<char*>(m_JJ_Stack);            // +0x14, read cursor
    JointJacobianSpy* lpOut   = reinterpret_cast<JointJacobianSpy*>(m_JJ_Stack);  // emit cursor

    for (u32 luIndex = 0u; luIndex < m_JT_Count; ++luIndex, lpArray += JacobianStride())
    {
        const Jacobian* lpJac = reinterpret_cast<const Jacobian*>(lpArray);
        if (lpJac->mRows[4].mV.u[3] == 0u)                          // +0x4C
            continue;

        Joint* const lpJoint = reinterpret_cast<Joint*>(lpJac->mRows[12].mV.u[3]);         // +0xCC

        rw::math::vpu::Matrix33 lMat;
        DriveJacobian::GetMatIBT(&lMat, lpJac);         // shared joint/drive sub-layout

        const rw::math::vpu::Vector4& lrR2  = lpJac->mRows[2];    // +0x20
        const rw::math::vpu::Vector4& lrR3  = lpJac->mRows[3];    // +0x30
        const rw::math::vpu::Vector4& lrR12 = lpJac->mRows[12];   // +0xC0
        const rw::math::vpu::Vector4& lrR14 = lpJac->mRows[14];   // +0xE0
        const rw::math::vpu::Vector4& lrR16 = lpJac->mRows[16];   // +0x100
        const rw::math::vpu::Vector4& lrR18 = lpJac->mRows[18];   // +0x120
        const rw::math::vpu::Vector4& lrR20 = lpJac->mRows[20];   // +0x140
        const rw::math::vpu::Vector4& lrR22 = lpJac->mRows[22];   // +0x160

        // ARTIST 0x82BC25C8..2638: round the first products, fuse the
        // remaining terms in each chain, and retain all four stored lanes.
        const __vector4 lR2X = __vspltw(lrR2.mV, 0);
        const __vector4 lR2Y = __vspltw(lrR2.mV, 1);
        const __vector4 lR2Z = __vspltw(lrR2.mV, 2);
        __vector4 lForce = __vmulfp(lrR12.mV, lR2X);
        lForce = __vmaddfp(lrR16.mV, lR2Y, lForce);
        lForce = __vmaddfp(lrR20.mV, lR2Z, lForce);
        lForce = __vmulfp(lForce, lInvTsSq);

        const __vector4 lR3X = __vspltw(lrR3.mV, 0);
        const __vector4 lR3Y = __vspltw(lrR3.mV, 1);
        const __vector4 lR3Z = __vspltw(lrR3.mV, 2);
        __vector4 lMatrixTorque = __vmulfp(lMat.xAxis.mV, lR3X);
        __vector4 lBodyTorque = __vmulfp(lrR14.mV, lR3X);
        lMatrixTorque = __vmaddfp(lMat.yAxis.mV, lR3Y, lMatrixTorque);
        lBodyTorque = __vmaddfp(lrR18.mV, lR3Y, lBodyTorque);
        lMatrixTorque = __vmaddfp(lMat.zAxis.mV, lR3Z, lMatrixTorque);
        lBodyTorque = __vmaddfp(lrR22.mV, lR3Z, lBodyTorque);
        const __vector4 lTorque = __vmulfp(__vaddfp(lBodyTorque, lMatrixTorque), lInvTsSq);

        lpOut->mForce = rw::math::vpu::Vector4(lForce);
        lpOut->mTorque = rw::math::vpu::Vector4(lTorque);
        lpOut->mpJoint = lpJoint;                       // stw     @r30+0x20
        lpOut->muTag   = lpJoint->GetTag();             // `lwz 0x18(r28)` -> stw @r30+0x24
        ++lpOut;                                        // addi r30, r30, 0x30

        ++m_JS_Count;                                   // +0x70
    }
}

// =====================================================================================
// ⭐⭐ THE SOLVER CLUSTER (2026-08-06) -- ContactBatchBuild, the four pipelines, the two
// remaining spies, and SimulationUpdate back home. Shared conventions, stated once:
//
//   * Each reaction-force block has four Vector4 rows: linear velocity impulse,
//     linear position correction, angular velocity impulse, and angular position correction.
//     ARTIST stores the resolved block address in each body's packed mId word and copies
//     that address into the Jacobian. The solver reads those addresses directly.
//   * NO BODY-STATE BRANCHES. X360 solves every pair branchlessly: ContactBatchBuild and
//     the two jacobian builders zero-mask invm and I^-1 for non-ACTIVE bodies, so a static
//     side receives and contributes exactly nothing (its RF block provably stays zero:
//     zeroed at Initialize, only ever incremented by zero-scaled rows, and DynamicUpdate
//     never runs on it). XB1 instead BRANCHES on a flags bit -- and its batch even swaps
//     the pair so the dynamic body is always in seat A -- reaching the same fixed point.
//     The host follows X360.
//   * IDA's classic `vmaddfp vD,vA,vB,vC` means vA*vC + vB; `vnmsubfp`
//     means vB - vA*vC. The C++ intrinsics use their documented argument order.
//   * Effective-mass reciprocals: the console batch uses a SINGLE UNREFINED `vrefp`
//     (~12-bit) on the denominators, and the spies refine `vrefp`/`vrsqrtefp` with two
//     Newton-Raphson steps; XB1 uses exact divides everywhere. Compilation alone is not
//     evidence of numerical or runtime parity.
// =====================================================================================

// Shared native vector and quaternion operations used by the builders and spies.
using namespace jacobian_detail;

namespace
{
    inline rw::math::vpu::Vector4* ReactionRows(void* lpRFStack, s32 liId)
    {
        (void)lpRFStack;
        return reinterpret_cast<rw::math::vpu::Vector4*>(liId);
    }

    // The relative velocity both jacobian pipelines start from (Osiris @0x82BC26FC..0x82BC279C,
    // Isis @0x82BC229C..0x82BC2350, Horus's two segments identically):
    //     vel = (RF_B0 + cross(RF_B2, rB)) - (RF_A0 + cross(RF_A2, rA))     -- B minus A
    //     omg =  RF_B2 - RF_A2
    // (XB1 computes A-minus-B and compensates downstream; the two agree term for term.)
    inline void JacobianRelVel(const rw::physics::Jacobian& lrJ,
                               const rw::math::vpu::Vector4* lpA,
                               const rw::math::vpu::Vector4* lpB,
                               V3& lrVel, V3& lrOmg)
    {
        // ARTIST 0x82BC26FC..279C: the two cross-product terms are
        // fused into each full RF row before subtracting B minus A.
        const __vector4 lRA = lrJ.mRows[0].mV;
        const __vector4 lRB = lrJ.mRows[1].mV;
        const __vector4 lOmegaA = lpA[2].mV;
        const __vector4 lOmegaB = lpB[2].mV;
        const __vector4 lMotionA = __vnmsubfp(__vpermwi(lOmegaA, 0x87), __vpermwi(lRA, 0x63),
            __vmaddfp(__vpermwi(lOmegaA, 0x63), __vpermwi(lRA, 0x87), lpA[0].mV));
        const __vector4 lMotionB = __vnmsubfp(__vpermwi(lOmegaB, 0x87), __vpermwi(lRB, 0x63),
            __vmaddfp(__vpermwi(lOmegaB, 0x63), __vpermwi(lRB, 0x87), lpB[0].mV));
        lrVel = V3(__vsubfp(lMotionB, lMotionA));
        lrOmg = V3(__vsubfp(lOmegaB, lOmegaA));
    }

    // J . v through the transposed, PRE-DIVIDED rows the builders store (linear rows 4/6/8,
    // angular rows 5/7/9): lane i of the result = dot(axis_i, v) / mEff_i.
    // Osiris 0x82BC27B0..27F0 starts the projection in the existing
    // impulse row and fuses each of the three products in sequence.
    inline V3 ProjectRowsAccum(const rw::math::vpu::Vector4& lrRx,
                               const rw::math::vpu::Vector4& lrRy,
                               const rw::math::vpu::Vector4& lrRz,
                               const V3& lrV, const rw::math::vpu::Vector4& lrAccum)
    {
        __vector4 lResult = __vmaddfp(lrRx.mV, __vspltw(lrV.mV, 0), lrAccum.mV);
        lResult = __vmaddfp(lrRy.mV, __vspltw(lrV.mV, 1), lResult);
        lResult = __vmaddfp(lrRz.mV, __vspltw(lrV.mV, 2), lResult);
        return V3(lResult);
    }

    // The 12-product apply tail both jacobian record types share (rows 12..23 have the
    // identical meaning in JointJacobian::Build and DriveJacobian::Build):
    //     RF_A0 += invmA * (row12*dL.x + row16*dL.y + row20*dL.z)        (invm in 16.w/20.w)
    //     RF_B0 -= invmB * (same world impulse)
    //     RF_A2 += row13*dL.x + row17*dL.y + row21*dL.z                  (I_A^-1.(rA x L_i))
    //            + row14*dA.x + row18*dA.y + row22*dA.z                  (I_A^-1.axis_i)
    //     RF_B2 -= row15*dL.x + row19*dL.y + row23*dL.z                  (I_B^-1.(rB x L_i))
    //            + {13.w,14.w,15.w}*dA.x + {17..19.w}*dA.y + {21..23.w}*dA.z
    //              (the w-spread I_B^-1.axis_i vectors, gathered on the console with
    //               vmrglw + vperm 0x82181760 = {a.w, b.w, c.w})
    // Verified store for store on Osiris AND Isis (and Horus's fused copies).
    inline void ApplyJacobianDeltas(const rw::physics::Jacobian& lrJ,
                                    rw::math::vpu::Vector4* lpA,
                                    rw::math::vpu::Vector4* lpB,
                                    const V3& lrDLin, const V3& lrDAng)
    {
        // ARTIST 0x82BC2830..2938: both linear and angular chains
        // operate on full rows; the packed W words also participate.
        static const CGS_ALIGN(16) u32 kGather[4] = {0x08090A0B, 0x0C0D0E0F, 0x1C1D1E1F, 0x1C1D1E1F};
        const __vector4 lControl = __lvx(kGather, 0);
        const __vector4 z = __vspltisw(0);
        const __vector4 lx = __vspltw(lrDLin.mV, 0), ly = __vspltw(lrDLin.mV, 1), lz = __vspltw(lrDLin.mV, 2);
        const __vector4 ax = __vspltw(lrDAng.mV, 0), ay = __vspltw(lrDAng.mV, 1), az = __vspltw(lrDAng.mV, 2);
        const __vector4 lOldA0 = lpA[0].mV, lOldB0 = lpB[0].mV;
        const __vector4 lOldA2 = lpA[2].mV, lOldB2 = lpB[2].mV;
        __vector4 lWorld = __vmaddfp(lrJ.mRows[12].mV, lx, z);
        lWorld = __vmaddfp(lrJ.mRows[16].mV, ly, lWorld);
        lWorld = __vmaddfp(lrJ.mRows[20].mV, lz, lWorld);
        lpA[0].mV = __vmaddfp(lWorld, __vspltw(lrJ.mRows[16].mV, 3), lOldA0);
        lpB[0].mV = __vnmsubfp(lWorld, __vspltw(lrJ.mRows[20].mV, 3), lOldB0);

        __vector4 lLinA = __vmaddfp(lrJ.mRows[13].mV, lx, lOldA2);
        lLinA = __vmaddfp(lrJ.mRows[17].mV, ly, lLinA);
        lLinA = __vmaddfp(lrJ.mRows[21].mV, lz, lLinA);
        __vector4 lAngA = __vmaddfp(lrJ.mRows[14].mV, ax, z);
        lAngA = __vmaddfp(lrJ.mRows[18].mV, ay, lAngA);
        lAngA = __vmaddfp(lrJ.mRows[22].mV, az, lAngA);
        lpA[2].mV = __vaddfp(lLinA, lAngA);

        const __vector4 b0 = __vperm(__vmrglw(lrJ.mRows[13].mV, lrJ.mRows[14].mV), lrJ.mRows[15].mV, lControl);
        const __vector4 b1 = __vperm(__vmrglw(lrJ.mRows[17].mV, lrJ.mRows[18].mV), lrJ.mRows[19].mV, lControl);
        const __vector4 b2 = __vperm(__vmrglw(lrJ.mRows[21].mV, lrJ.mRows[22].mV), lrJ.mRows[23].mV, lControl);
        __vector4 lLinB = __vnmsubfp(lrJ.mRows[15].mV, lx, lOldB2);
        lLinB = __vnmsubfp(lrJ.mRows[19].mV, ly, lLinB);
        lLinB = __vnmsubfp(lrJ.mRows[23].mV, lz, lLinB);
        __vector4 lAngB = __vnmsubfp(b0, ax, z);
        lAngB = __vnmsubfp(b1, ay, lAngB);
        lAngB = __vnmsubfp(b2, az, lAngB);
        lpB[2].mV = __vaddfp(lLinB, lAngB);
    }

    // ---------------------------------------------------------------------------------
    // One CONTACT record, one iteration -- the per-record body of Anubis_Pipeline
    // @0x82BC11C0 (and Horus's contact segment 0x82BC1B48..0x82BC1D58, fused copy).
    // Lane-verified against the X360 asm; algorithm cross-read on XB1 sub_1409ADBF0.
    // ---------------------------------------------------------------------------------
    inline __vector4 ContactMotion(__vector4 linear, __vector4 angular, __vector4 radius)
    {
        const __vector4 first = __vmaddfp(__vpermwi(angular, 0x63), __vpermwi(radius, 0x87), linear);
        return __vnmsubfp(__vpermwi(angular, 0x87), __vpermwi(radius, 0x63), first);
    }

    inline void SolveContactRecord(rw::physics::ContactJacobian& lrJ, void* lpRFStack)
    {
        rw::math::vpu::Vector4* const lpA = ReactionRows(lpRFStack, lrJ.mBodyA);
        rw::math::vpu::Vector4* const lpB = ReactionRows(lpRFStack, lrJ.mBodyB);
        // ARTIST 0x82BC1294..1418. Four-lane controls retain the positional
        // update in W and carry the packed metadata exactly as the loads do.
        static const CGS_ALIGN(16) u32 kJ[4] = {0x00010203,0x04050607,0x08090A0B,0x00010203};
        static const CGS_ALIGN(16) u32 kX[4] = {0x00010203,0x00010203,0x00010203,0x10111213};
        static const CGS_ALIGN(16) u32 kY[4] = {0x04050607,0x04050607,0x04050607,0x14151617};
        static const CGS_ALIGN(16) u32 kZ[4] = {0x08090A0B,0x08090A0B,0x08090A0B,0x18191A1B};
        static const CGS_ALIGN(16) u32 kFriction[4] = {0x10111213,0x00010203,0x00010203,0x10111213};
        static const CGS_ALIGN(16) u32 kCorner[4] = {0x7F7FFFFF,0,0,0x7F7FFFFF};
        const __vector4 zero = __vspltisw(0);
        const __vector4 ra = __lvx(&lrJ.mRA, 0), rb = __lvx(&lrJ.mRB, 0);
        const __vector4 a0 = lpA[0].mV, a1 = lpA[1].mV, a2 = lpA[2].mV, a3 = lpA[3].mV;
        const __vector4 b0 = lpB[0].mV, b1 = lpB[1].mV, b2 = lpB[2].mV, b3 = lpB[3].mV;
        const __vector4 pos = __vsubfp(ContactMotion(b1,b3,rb), ContactMotion(a1,a3,ra));
        const __vector4 vel = __vsubfp(ContactMotion(b0,b2,rb), ContactMotion(a0,a2,ra));
        const __vector4 total = __vaddfp(vel, pos);
        const __vector4 lambda = __lvx(&lrJ.mLambda, 0);
        const __vector4 jx = __lvx(&lrJ.mJx, 0), jy = __lvx(&lrJ.mJy, 0), jz = __lvx(&lrJ.mJz, 0);
        const __vector4 jc = __lvx(kJ, 0);
        __vector4 raw = __vaddfp(lambda, __lvx(&lrJ.mBias, 0));
        raw = __vmaddfp(__vperm(jx,jx,jc), __vperm(total,pos,__lvx(kX,0)), raw);
        raw = __vmaddfp(__vperm(jy,jy,jc), __vperm(total,pos,__lvx(kY,0)), raw);
        raw = __vmaddfp(__vperm(jz,jz,jc), __vperm(total,pos,__lvx(kZ,0)), raw);
        const __vector4 center = __vperm(lambda,zero,__lvx(kFriction,0));
        const __vector4 mus = __vspltw(jy,3), mud = __vspltw(jz,3);
        const __vector4 corner = __lvx(kCorner,0);
        const __vector4 minStatic = __vnmsubfp(mus,center,zero);
        const __vector4 maxStatic = __vmaddfp(mus,center,corner);
        const __vector4 minDynamic = __vnmsubfp(mud,center,zero);
        const __vector4 maxDynamic = __vmaddfp(mud,center,corner);
        const __vector4 lower = __vsel(minDynamic,raw,__vcmpgefp(raw,minStatic));
        const __vector4 next = __vsel(maxDynamic,lower,__vcmpgefp(maxStatic,raw));
        const __vector4 delta = __vsubfp(next,lambda);
        __stvx(next, &lrJ.mLambda, 0);

        // 0x82BC141C..14AC: all eight RF rows are full-lane stores.
        const __vector4 dx = __vspltw(delta,0), dy = __vspltw(delta,1);
        const __vector4 dz = __vspltw(delta,2), dp = __vspltw(delta,3);
        const __vector4 n = __lvx(&lrJ.mRi,0), up = __lvx(&lrJ.mUp,0), at = __lvx(&lrJ.mAt,0);
        const __vector4 tnA = __lvx(&lrJ.mTnA,0), tnB = __lvx(&lrJ.mTnB,0);
        const __vector4 tuA = __lvx(&lrJ.mTupA,0), tuB = __lvx(&lrJ.mTupB,0);
        const __vector4 taA = __lvx(&lrJ.mTatA,0), taB = __lvx(&lrJ.mTatB,0);
        __vector4 world = __vmaddfp(n,dx,zero);
        world = __vmaddfp(up,dy,world);
        world = __vmaddfp(at,dz,world);
        const __vector4 worldPos = __vmaddfp(n,dp,zero);
        __vector4 angA = __vmaddfp(tnA,dx,a2);
        angA = __vmaddfp(tuA,dy,angA);
        angA = __vmaddfp(taA,dz,angA);
        __vector4 angB = __vnmsubfp(tnB,dx,b2);
        angB = __vnmsubfp(tuB,dy,angB);
        angB = __vnmsubfp(taB,dz,angB);
        lpA[3].mV = __vmaddfp(tnA,dp,a3);
        lpB[3].mV = __vnmsubfp(tnB,dp,b3);
        lpB[2].mV = angB;
        lpA[2].mV = angA;
        lpB[1].mV = __vnmsubfp(worldPos,__vspltw(tnB,3),b1);
        lpB[0].mV = __vnmsubfp(world,__vspltw(tnB,3),b0);
        lpA[1].mV = __vmaddfp(worldPos,__vspltw(tnA,3),a1);
        lpA[0].mV = __vmaddfp(world,__vspltw(tnA,3),a0);
    }

    // ---------------------------------------------------------------------------------
    // One JOINT jacobian, one iteration -- the per-record body of Osiris_Pipeline
    // @0x82BC2680 (headless recovery; and Horus's joint segment 0x82BC1D88..0x82BC1FAC).
    //
    // ⭐ THE PROJECTION IS A BOX-SHRINK, NOT A CLAMP:  new = max(x+lo,0) + min(x+hi,0)
    // (X360 `vaddfp/vminfp/vmaxfp/vaddfp` @0x82BC27D8..0x82BC2814). With the limit windows
    // JointJacobian::Build packs, that one formula covers every joint type: FREE rows have
    // (lo,hi) = (-FLT_MAX,+FLT_MAX) and the impulse is driven to 0; LOCKED rows have
    // lo == hi == the error and become equality constraints; CONE's one-sided lo gives a
    // unilateral row. No gain, no bias row -- unlike the drive, x = L + J.v exactly.
    // ---------------------------------------------------------------------------------
    inline void SolveJointRecord(rw::physics::Jacobian& lrJ, void* lpRFStack)
    {
        rw::math::vpu::Vector4* const lpA = ReactionRows(lpRFStack, static_cast<s32>(lrJ.mRows[0].mV.u[3]));
        rw::math::vpu::Vector4* const lpB = ReactionRows(lpRFStack, static_cast<s32>(lrJ.mRows[1].mV.u[3]));

        V3 lvVel, lvOmg;
        JacobianRelVel(lrJ, lpA, lpB, lvVel, lvOmg);

        const V3 lvXLin = ProjectRowsAccum(lrJ.mRows[4], lrJ.mRows[6], lrJ.mRows[8],
                                          lvVel, lrJ.mRows[2]);
        const V3 lvXAng = ProjectRowsAccum(lrJ.mRows[5], lrJ.mRows[7], lrJ.mRows[9],
                                          lvOmg, lrJ.mRows[3]);

        // 0x82BC27C0..2818: the control at 0x82181760 gathers
        // {row6.w,row7.w,row10.w,row10.w}, and likewise for the high bound.
        static const CGS_ALIGN(16) u32 kGather[4] = {0x08090A0B, 0x0C0D0E0F, 0x1C1D1E1F, 0x1C1D1E1F};
        const __vector4 lControl = __lvx(kGather, 0);
        const __vector4 lAngLo = __vperm(__vmrglw(lrJ.mRows[6].mV, lrJ.mRows[7].mV), lrJ.mRows[10].mV, lControl);
        const __vector4 lAngHi = __vperm(__vmrglw(lrJ.mRows[8].mV, lrJ.mRows[9].mV), lrJ.mRows[11].mV, lControl);
        const __vector4 lZero = __vspltisw(0);
        const __vector4 lNewLin = __vaddfp(__vmaxfp(__vaddfp(lrJ.mRows[10].mV, lvXLin.mV), lZero),
                                           __vminfp(__vaddfp(lrJ.mRows[11].mV, lvXLin.mV), lZero));
        const __vector4 lNewAng = __vaddfp(__vmaxfp(__vaddfp(lAngLo, lvXAng.mV), lZero),
                                           __vminfp(__vaddfp(lAngHi, lvXAng.mV), lZero));
        const V3 lvDLin(__vsubfp(lNewLin, lrJ.mRows[2].mV));
        const V3 lvDAng(__vsubfp(lNewAng, lrJ.mRows[3].mV));
        lrJ.mRows[2].mV = lNewLin;
        lrJ.mRows[3].mV = lNewAng;

        ApplyJacobianDeltas(lrJ, lpA, lpB, lvDLin, lvDAng);
    }

    // ---------------------------------------------------------------------------------
    // One DRIVE jacobian, one iteration -- the per-record body of Isis_Pipeline
    // @0x82BC2218 (and Horus's drive segment 0x82BC1FD8..0x82BC2208).
    //
    // The drive is a leaky-accumulator servo:  new = clamp((L + J.v)*gain + bias, +/-clamp)
    // with gain = the builder's acceleration gain (mRows[2].w linear / mRows[3].w angular --
    // 1.0 for NO/HARD drives, <1 for SOFT), bias = the pre-divided impulse rows 10/11, and
    // the symmetric clamp triples {6C,7C,AC} / {8C,9C,BC} = mStrength*h^2 (X360 gathers
    // them with the same vmrglw+vperm as the joint's windows; vminfp-then-vmaxfp
    // @0x82BC23A0..0x82BC23AC). The stores PRESERVE w (`vsel` with the {F,F,F,0} mask) --
    // the gains live there.
    // ---------------------------------------------------------------------------------
    inline void SolveDriveRecord(rw::physics::Jacobian& lrJ, void* lpRFStack)
    {
        rw::math::vpu::Vector4* const lpA = ReactionRows(lpRFStack, static_cast<s32>(lrJ.mRows[0].mV.u[3]));
        rw::math::vpu::Vector4* const lpB = ReactionRows(lpRFStack, static_cast<s32>(lrJ.mRows[1].mV.u[3]));

        V3 lvVel, lvOmg;
        JacobianRelVel(lrJ, lpA, lpB, lvVel, lvOmg);

        const V3 lvPLin = ProjectRowsAccum(lrJ.mRows[4], lrJ.mRows[6], lrJ.mRows[8], lvVel, lrJ.mRows[2]);
        const V3 lvPAng = ProjectRowsAccum(lrJ.mRows[5], lrJ.mRows[7], lrJ.mRows[9], lvOmg, lrJ.mRows[3]);
        const __vector4 lXLin = __vmaddfp(lvPLin.mV, __vspltw(lrJ.mRows[2].mV, 3), lrJ.mRows[10].mV);
        const __vector4 lXAng = __vmaddfp(lvPAng.mV, __vspltw(lrJ.mRows[3].mV, 3), lrJ.mRows[11].mV);
        static const CGS_ALIGN(16) u32 kGather[4] = {0x08090A0B, 0x0C0D0E0F, 0x1C1D1E1F, 0x1C1D1E1F};
        static const CGS_ALIGN(16) u32 kXYZ[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0};
        const __vector4 lControl = __lvx(kGather, 0), lZero = __vspltisw(0);
        const __vector4 lClampL = __vperm(__vmrglw(lrJ.mRows[6].mV, lrJ.mRows[7].mV), lrJ.mRows[10].mV, lControl);
        const __vector4 lClampA = __vperm(__vmrglw(lrJ.mRows[8].mV, lrJ.mRows[9].mV), lrJ.mRows[11].mV, lControl);
        const __vector4 lNewLin = __vmaxfp(__vminfp(lXLin, lClampL), __vsubfp(lZero, lClampL));
        const __vector4 lNewAng = __vmaxfp(__vminfp(lXAng, lClampA), __vsubfp(lZero, lClampA));
        // Deltas use the computed W result; the subsequent stores preserve the gain.
        const V3 lvDLin(__vsubfp(lNewLin, lrJ.mRows[2].mV));
        const V3 lvDAng(__vsubfp(lNewAng, lrJ.mRows[3].mV));
        lrJ.mRows[2].mV = __vsel(lrJ.mRows[2].mV, lNewLin, __lvx(kXYZ, 0));
        lrJ.mRows[3].mV = __vsel(lrJ.mRows[3].mV, lNewAng, __lvx(kXYZ, 0));

        ApplyJacobianDeltas(lrJ, lpA, lpB, lvDLin, lvDAng);
    }
} // anonymous namespace

// -------------------------------------------------------------------------------------
// Simulation::ContactBatchBuild @ 0x82BC14C0   (343 instructions, branchless VMX)
//
// Rewrite every drain-time Contact into the in-place ContactJacobian overlay (contact.h's
// slot map, verified against this function's store cover). Reads ONLY the record -- the
// drain snapshot rows carry everything, which is what the snapshot region exists for (XB1
// re-reads the LIVE bodies instead; same-tick values, divergence noted, X360 followed).
//
// ⚠️ THE CONSOLE'S EFFECTIVE-MASS RECIPROCAL IS A SINGLE UNREFINED `vrefp` (@0x82BC1970) --
// ~12-bit; the native transcription retains that estimate.
//
// THE RESTITUTION GATE (four cases, verified on BOTH oracles -- XB1's branch tree
// @0x1409AEC5D..0x1409AECAD == X360's vsel chain @0x82BC19EC..0x82BC1A04): with
// e = the normal-lane error, P = dot(n, posB-posA), bounce = -res*ts*dot(n, vel):
//     e <= bounce                 -> subtract nothing
//     e > bounce, P >= 0          -> e -= bounce
//     e > bounce, P < 0, bounce<0 -> e -= (P + bounce)
//     e > bounce, P < 0, bounce>=0-> subtract nothing
// (X360 compares the recip-scaled values -- same order, positive scale; equivalent.)
// -------------------------------------------------------------------------------------
void Simulation::ContactBatchBuild()
{
    // ARTIST 0x82BC14C0..1A14. The controls are the native big-endian
    // words at 0x82181650..16EF; the register transcription retains every
    // full-row load/store, packed W lane, active mask and unrefined vrefp.
    static const CGS_ALIGN(16) u32 kControl[10][4] = {
        {0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u},
        {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u},
        {0x08090A0Bu, 0x00010203u, 0x04050607u, 0x0C0D0E0Fu},
        {0x04050607u, 0x08090A0Bu, 0x00010203u, 0x0C0D0E0Fu},
        {0x00010203u, 0x04050607u, 0x08090A0Bu, 0x00010203u},
        {0x08090A0Bu, 0x18191A1Bu, 0x10111213u, 0x0C0D0E0Fu},
        {0x04050607u, 0x14151617u, 0x18191A1Bu, 0x0C0D0E0Fu},
        {0x08090A0Bu, 0x0C0D0E0Fu, 0x14151617u, 0x10111213u},
        {0x00010203u, 0x04050607u, 0x18191A1Bu, 0x10111213u},
        {0x00010203u, 0x04050607u, 0x10111213u, 0x14151617u}
    };
    const __vector4 time = __vspltw(__lvlx(&m_TimeStep, 0), 0);
    Contact* lpC = static_cast<Contact*>(m_CJ_Stack);
    for (u32 i = 0; i < m_CT_Count; ++i, ++lpC)
    {
        Contact& lrC = *lpC;
        __vector4 v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63;
        __vector4 stack_100, stack_110, stack_120, stack_50, stack_60, stack_70, stack_80, stack_90, stack_A0, stack_B0, stack_C0, stack_D0, stack_E0, stack_F0;
        v0 = __vspltisw(0); v18 = __vspltisw(4); v56 = __vspltisw(8);
        v55 = time; v54 = __vmulfp(time, time);
        v57 = __lvx(kControl[0], 0); v13 = __lvx(kControl[1], 0);
        v6 = __lvx(kControl[2], 0); v7 = __lvx(kControl[3], 0);
        v1 = __lvx(kControl[5], 0);
        stack_100 = __lvx(kControl[6], 0); stack_110 = __lvx(kControl[7], 0);
        stack_F0 = __lvx(kControl[8], 0); stack_120 = __lvx(kControl[9], 0);
        v29 = __lvx(&lrC.mPosA, 0); // 82BC1584
        v9 = __lvx(&lrC.mIspltA, 0); // 82BC158C
        v26 = __vspltw(v9, 3); // 82BC1594
        v20 = v9; // 82BC159C
        stack_90 = v1; // 82BC15A8
        v30 = __lvx(&lrC.mComB, 0); // 82BC15AC
        v28 = __lvx(&lrC.mPosB, 0); // 82BC15B4
        v31 = __lvx(&lrC.mComA, 0); // 82BC15BC
        v4 = __vsubfp(v28, v30); // 82BC15C0
        v5 = __vsubfp(v29, v31); // 82BC15C4
        v8 = __lvx(&lrC.mIspltB, 0); // 82BC15C8
        v11 = __lvx(&lrC.mRi, 0); // 82BC15CC
        v25 = __vspltw(v8, 3); // 82BC15D0
        v24 = __vsel(v29, v11, v13); // 82BC15D4
        v10 = __lvx(&lrC.mUp, 0); // 82BC15D8
        v23 = __vsel(v28, v10, v13); // 82BC15DC
        v19 = v8; // 82BC15E4
        v15 = __vor(v26, v25); // 82BC15EC
        v12 = __lvx(&lrC.mAt, 0); // 82BC15F0
        v26 = __vand(v26, v18); // 82BC15F4
        __stvx(v24, &lrC.mComB, 0); // 82BC15F8
        v25 = __vand(v25, v18); // 82BC15FC
        __stvx(v23, &lrC.mIspltA, 0); // 82BC1604
        v27 = __lvx(&lrC.mVel, 0); // 82BC160C
        v24 = __vcmpequw(v26, v18); // 82BC1614
        v8 = __vsel(v30, v4, v13); // 82BC161C
        v17 = __vperm(v12, v12, v6); // 82BC1620
        v9 = __vsel(v31, v5, v13); // 82BC1624
        v31 = __vperm(v5, v5, v7); // 82BC1628
        v23 = __vcmpequw(v25, v18); // 82BC162C
        v63 = __lvx(&lrC.mForceB, 0); // 82BC1630
        v22 = __vsel(v27, v12, v13); // 82BC1634
        v3 = __lvx(&lrC.mTorqueA, 0); // 82BC1638
        __stvx(v8, &lrC.mPosB, 0); // 82BC163C
        v30 = __vsel(v0, v20, v24); // 82BC1640
        __stvx(v9, &lrC.mPosA, 0); // 82BC1644
        v9 = __lvx(&lrC.mIfullA, 0); // 82BC164C
        v26 = __vsel(v0, v19, v23); // 82BC1650
        v8 = __lvx(&lrC.mIfullB, 0); // 82BC1654
        v9 = __vsel(v0, v9, v24); // 82BC1658
        v8 = __vsel(v0, v8, v23); // 82BC165C
        __stvx(v22, &lrC.mForceB, 0); // 82BC1660
        v2 = __lvx(&lrC.mTorqueB, 0); // 82BC1664
        v14 = __vperm(v3, v3, v6); // 82BC1668
        v25 = __vperm(v10, v10, v6); // 82BC166C
        stack_70 = v6; // 82BC1674
        v22 = __vperm(v9, v30, v1); // 82BC1678
        v21 = __vperm(v8, v26, v1); // 82BC167C
        v1 = stack_100; // 82BC1680
        v20 = __vperm(v9, v30, v1); // 82BC1688
        v30 = __vperm(v4, v4, v7); // 82BC168C
        v62 = __vperm(v2, v2, v6); // 82BC1690
        v5 = __vperm(v5, v5, v6); // 82BC1694
        v4 = __vperm(v4, v4, v6); // 82BC1698
        v1 = stack_100; // 82BC169C
        v19 = __vperm(v8, v26, v1); // 82BC16A4
        v26 = __vperm(v11, v11, v6); // 82BC16A8
        v53 = __vperm(v11, v11, v7); // 82BC16AC
        v6 = __lvx(&lrC.mForceA, 0); // 82BC16B0
        v14 = __vnmsubfp(v31, v14, v6); // 82BC16B4
        v61 = v30; // 82BC16B8
        v16 = __vmaddfp(v31, v26, v0); // 82BC16BC
        v60 = v25; // 82BC16C0
        stack_50 = v18; // 82BC16C8
        v58 = v17; // 82BC16CC
        stack_B0 = v15; // 82BC16D4
        v59 = v17; // 82BC16D8
        stack_D0 = v12; // 82BC16E0
        v63 = __vnmsubfp(v61, v62, v63); // 82BC16E4
        v62 = v26; // 82BC16E8
        v61 = v25; // 82BC16EC
        v60 = __vmaddfp(v30, v60, v0); // 82BC16F0
        v58 = __vmaddfp(v30, v58, v0); // 82BC16F4
        v17 = __vor(v55, v55); // 82BC16F8
        v59 = __vmaddfp(v31, v59, v0); // 82BC16FC
        v26 = __vperm(v10, v10, v7); // 82BC1700
        v62 = __vmaddfp(v30, v62, v0); // 82BC1704
        v30 = __vor(v53, v53); // 82BC1708
        v61 = __vmaddfp(v31, v61, v0); // 82BC170C
        v53 = v4; // 82BC1710
        v31 = __vsubfp(v28, v29); // 82BC1714
        v52 = v5; // 82BC1718
        v17 = __vmaddfp(v27, v17, v0); // 82BC171C
        v27 = v26; // 82BC1720
        v28 = v30; // 82BC1724
        v30 = __vnmsubfp(v5, v30, v16); // 82BC1728
        v51 = v4; // 82BC172C
        v25 = __vperm(v12, v12, v7); // 82BC1730
        v50 = v5; // 82BC1734
        v3 = __vperm(v3, v3, v7); // 82BC1738
        v49 = v4; // 82BC173C
        v16 = v25; // 82BC1740
        v3 = __vmaddfp(v5, v3, v14); // 82BC1744
        v29 = __vor(v62, v62); // 82BC1748
        v62 = v25; // 82BC174C
        v25 = __vor(v58, v58); // 82BC1750
        stack_60 = v31; // 82BC1758
        stack_A0 = v13; // 82BC1760
        v29 = __vnmsubfp(v53, v28, v29); // 82BC1764
        v28 = __vor(v61, v61); // 82BC1768
        v6 = __vspltw(v30, 1); // 82BC176C
        stack_E0 = v8; // 82BC1774
        v25 = __vnmsubfp(v49, v62, v25); // 82BC1778
        v1 = __vspltw(v30, 2); // 82BC177C
        stack_C0 = v9; // 82BC1784
        v28 = __vnmsubfp(v52, v27, v28); // 82BC1788
        v27 = __vor(v60, v60); // 82BC178C
        stack_80 = v17; // 82BC1794
        v27 = __vnmsubfp(v51, v26, v27); // 82BC1798
        v26 = __vor(v59, v59); // 82BC179C
        v26 = __vnmsubfp(v50, v16, v26); // 82BC17A0
        v16 = __vspltw(v30, 0); // 82BC17A4
        v18 = __vspltw(v29, 0); // 82BC17A8
        v15 = __vspltw(v29, 1); // 82BC17AC
        v31 = __vspltw(v25, 0); // 82BC17B0
        v16 = __vmaddfp(v9, v16, v0); // 82BC17B4
        v13 = __vspltw(v28, 1); // 82BC17B8
        v18 = __vmaddfp(v8, v18, v0); // 82BC17BC
        v17 = __vspltw(v25, 1); // 82BC17C0
        v12 = __vspltw(v29, 2); // 82BC17C4
        v31 = __vmaddfp(v8, v31, v0); // 82BC17C8
        v16 = __vmaddfp(v20, v6, v16); // 82BC17CC
        v6 = __vmaddfp(v19, v15, v18); // 82BC17D0
        v18 = __vspltw(v27, 0); // 82BC17D4
        v15 = __vspltw(v26, 0); // 82BC17D8
        v18 = __vmaddfp(v8, v18, v0); // 82BC17DC
        v8 = __vspltw(v27, 1); // 82BC17E0
        v15 = __vmaddfp(v9, v15, v0); // 82BC17E4
        v16 = __vmaddfp(v22, v1, v16); // 82BC17E8
        v1 = __vspltw(v28, 0); // 82BC17EC
        v5 = __vmaddfp(v21, v12, v6); // 82BC17F0
        v1 = __vmaddfp(v9, v1, v0); // 82BC17F4
        v9 = __vspltw(v26, 1); // 82BC17F8
        v8 = __vmaddfp(v19, v8, v18); // 82BC17FC
        v18 = __vspltw(v25, 2); // 82BC1800
        v9 = __vmaddfp(v20, v9, v15); // 82BC1804
        v13 = __vmaddfp(v20, v13, v1); // 82BC1808
        v20 = __vspltw(v28, 2); // 82BC180C
        v1 = __vmaddfp(v19, v17, v31); // 82BC1810
        v19 = __vspltw(v27, 2); // 82BC1814
        v31 = __vspltw(v26, 2); // 82BC1818
        v2 = __vperm(v2, v2, v7); // 82BC181C
        v20 = __vmaddfp(v22, v20, v13); // 82BC1820
        v30 = __vmaddfp(v30, v16, v0); // 82BC1824
        v3 = __vsel(v0, v3, v24); // 82BC1828
        v22 = __vmaddfp(v22, v31, v9); // 82BC182C
        v13 = stack_A0; // 82BC1834
        v19 = __vmaddfp(v21, v19, v8); // 82BC1838
        v8 = stack_E0; // 82BC1840
        v63 = __vmaddfp(v4, v2, v63); // 82BC1844
        v31 = stack_60; // 82BC184C
        v21 = __vmaddfp(v21, v18, v1); // 82BC1850
        v9 = stack_C0; // 82BC1858
        v12 = stack_D0; // 82BC1860
        v17 = stack_80; // 82BC1868
        v28 = __vmaddfp(v28, v20, v0); // 82BC186C
        __stvx(v20, &lrC.mIspltB, 0); // 82BC1870
        v29 = __vmaddfp(v29, v5, v30); // 82BC1874
        v5 = __vsel(v8, v5, v13); // 82BC1878
        v30 = __vmaddfp(v26, v22, v0); // 82BC187C
        v26 = __vsel(v9, v16, v13); // 82BC1880
        __stvx(v19, &lrC.mForceA, 0); // 82BC1884
        v4 = __vor(v63, v63); // 82BC188C
        v9 = __vspltw(v9, 3); // 82BC1890
        __stvx(v5, &lrC.mIfullB, 0); // 82BC1894
        v8 = __vspltw(v8, 3); // 82BC1898
        __stvx(v26, &lrC.mIfullA, 0); // 82BC189C
        __stvx(v22, &lrC.mTorqueA, 0); // 82BC18A0
        v2 = __vsel(v0, v4, v23); // 82BC18A4
        v5 = stack_120; // 82BC18A8
        v4 = __vmrghw(v11, v10); // 82BC18B0
        __stvx(v21, &lrC.mTorqueB, 0); // 82BC18B4
        v2 = __vsubfp(v2, v3); // 82BC18B8
        v3 = __vspltw(v11, 3); // 82BC18BC
        v28 = __vmaddfp(v27, v19, v28); // 82BC18C0
        v27 = v0; // 82BC18C4
        v6 = stack_110; // 82BC18C8
        v5 = __vperm(v4, v12, v5); // 82BC18D0
        v4 = __vperm(v4, v12, v6); // 82BC18D4
        v3 = __vnmsubfp(v17, v3, v0); // 82BC18D8
        v11 = __vmrglw(v11, v10); // 82BC18DC
        v6 = stack_120; // 82BC18E0
        v27 = __vmaddfp(v2, v54, v27); // 82BC18E8
        v2 = __vmaddfp(v25, v21, v30); // 82BC18EC
        v30 = __vaddfp(v31, v27); // 82BC18F0
        v27 = __vmrghw(v29, v28); // 82BC18F4
        v29 = __vmrglw(v29, v28); // 82BC18F8
        v26 = __vperm(v27, v2, v6); // 82BC18FC
        v6 = stack_110; // 82BC1900
        v28 = __vaddfp(v26, v9); // 82BC1908
        v9 = __vperm(v27, v2, v6); // 82BC190C
        v27 = __vspltw(v3, 0); // 82BC1910
        v6 = stack_F0; // 82BC1914
        v2 = __vperm(v29, v2, v6); // 82BC191C
        v30 = __vaddfp(v17, v30); // 82BC1920
        v29 = __vspltw(v31, 0); // 82BC1924
        v8 = __vaddfp(v9, v8); // 82BC1928
        v26 = __vspltw(v3, 1); // 82BC192C
        v3 = __vspltw(v3, 2); // 82BC1930
        v6 = stack_F0; // 82BC1934
        v29 = __vmaddfp(v5, v29, v0); // 82BC1938
        v9 = __vperm(v11, v12, v6); // 82BC193C
        v2 = __vaddfp(v2, v28); // 82BC1940
        v28 = __vmaddfp(v5, v27, v0); // 82BC1944
        v27 = __vspltw(v31, 1); // 82BC1948
        v31 = __vspltw(v31, 2); // 82BC194C
        v11 = __vspltw(v30, 0); // 82BC1950
        v25 = __vspltw(v30, 1); // 82BC1954
        v30 = __vspltw(v30, 2); // 82BC1958
        v11 = __vmaddfp(v5, v11, v0); // 82BC195C
        v8 = __vaddfp(v2, v8); // 82BC1960
        v2 = __vmaddfp(v4, v27, v29); // 82BC1964
        v29 = __vmaddfp(v4, v26, v28); // 82BC1968
        v28 = __vmaddfp(v4, v25, v11); // 82BC196C
        v11 = __vrefp(v8); // 82BC1970
        v8 = __vmaddfp(v9, v31, v2); // 82BC1974
        v2 = __vmaddfp(v9, v3, v29); // 82BC1978
        v15 = stack_B0; // 82BC1980
        v31 = __vmaddfp(v9, v30, v28); // 82BC1984
        v1 = stack_90; // 82BC198C
        v3 = __vmaddfp(v8, v11, v0); // 82BC1990
        v6 = stack_70; // 82BC1998
        v5 = __vmaddfp(v5, v11, v0); // 82BC199C
        v18 = stack_50; // 82BC19A4
        v4 = __vmaddfp(v4, v11, v0); // 82BC19A8
        __stvx(v0, &lrC.mVel, 0); // 82BC19B4
        v8 = __vmaddfp(v2, v11, v0); // 82BC19B8
        v2 = __vmaddfp(v31, v11, v0); // 82BC19BC
        v28 = __vcmpgefp(v3, v0); // 82BC19C0
        v11 = __vmaddfp(v9, v11, v0); // 82BC19C4
        v10 = __vsel(v10, v4, v13); // 82BC19C8
        __stvx(v10, &lrC.mUp, 0); // 82BC19CC
        v30 = __vaddfp(v3, v8); // 82BC19D0
        v27 = v8; // 82BC19D4
        v31 = __vcmpgefp(v8, v0); // 82BC19D8
        v3 = __vspltw(v3, 0); // 82BC19DC
        v8 = __vcmpgefp(v8, v2); // 82BC19E0
        v12 = __vsel(v12, v11, v13); // 82BC19E4
        __stvx(v12, &lrC.mAt, 0); // 82BC19E8
        v31 = __vsel(v30, v0, v31); // 82BC19EC
        v31 = __vsel(v31, v27, v28); // 82BC19F0
        v9 = __vsel(v31, v0, v8); // 82BC19F4
        v8 = __vand(v56, v15); // 82BC19F8
        v9 = __vand(v9, v57); // 82BC19FC
        v8 = __vsel(v8, v5, v13); // 82BC1A00
        v9 = __vsubfp(v2, v9); // 82BC1A04
        __stvx(v8, &lrC.mRi, 0); // 82BC1A08
        v12 = __vsel(v3, v9, v13); // 82BC1A0C
        __stvx(v12, &lrC.mComA, 0); // 82BC1A10
    }
}

// -------------------------------------------------------------------------------------
// The four solver pipelines. Each is m_MaxIteration sweeps of dense per-record iteration;
// Horus @0x82BC1A20 is EXACTLY the other three's per-record bodies fused into one
// iteration loop (contacts 0x82BC1B48..0x82BC1D58, joints 0x82BC1D88..0x82BC1FAC, drives
// 0x82BC1FD8..0x82BC2208 -- segment signatures verified against the standalones), so all
// four share the per-record solvers above; the console compiler inlined the same bodies.
// Contact records walk at sizeof(Contact) (console 0x100); jacobians at JacobianStride()
// (console 0x180).
// -------------------------------------------------------------------------------------
void Simulation::Anubis_Pipeline()                      // @ 0x82BC11C0  (contacts only)
{
    for (u32 luIt = m_MaxIteration; luIt != 0u; --luIt)             // +0xAC
    {
        ContactJacobian* lpJ = static_cast<ContactJacobian*>(m_CJ_Stack);
        for (u32 luI = m_CT_Count; luI != 0u; --luI, ++lpJ)         // +0x64
            SolveContactRecord(*lpJ, m_RF_Stack);
    }
}

void Simulation::Osiris_Pipeline()                      // @ 0x82BC2680  (joints only)
{
    for (u32 luIt = m_MaxIteration; luIt != 0u; --luIt)
    {
        char* lpJ = static_cast<char*>(m_JJ_Stack);                 // +0x14
        for (u32 luI = m_JT_Count; luI != 0u; --luI, lpJ += JacobianStride())   // +0x6C
            SolveJointRecord(*reinterpret_cast<Jacobian*>(lpJ), m_RF_Stack);
    }
}

void Simulation::Isis_Pipeline()                        // @ 0x82BC2218  (drives only)
{
    for (u32 luIt = m_MaxIteration; luIt != 0u; --luIt)
    {
        char* lpJ = static_cast<char*>(m_DJ_Stack);                 // +0x18
        for (u32 luI = m_DR_Count; luI != 0u; --luI, lpJ += JacobianStride())   // +0x74
            SolveDriveRecord(*reinterpret_cast<Jacobian*>(lpJ), m_RF_Stack);
    }
}

void Simulation::Horus_Pipeline()                       // @ 0x82BC1A20  (any mix)
{
    for (u32 luIt = m_MaxIteration; luIt != 0u; --luIt)
    {
        ContactJacobian* lpC = static_cast<ContactJacobian*>(m_CJ_Stack);
        for (u32 luI = m_CT_Count; luI != 0u; --luI, ++lpC)
            SolveContactRecord(*lpC, m_RF_Stack);

        char* lpJ = static_cast<char*>(m_JJ_Stack);
        for (u32 luI = m_JT_Count; luI != 0u; --luI, lpJ += JacobianStride())
            SolveJointRecord(*reinterpret_cast<Jacobian*>(lpJ), m_RF_Stack);

        char* lpD = static_cast<char*>(m_DJ_Stack);
        for (u32 luI = m_DR_Count; luI != 0u; --luI, lpD += JacobianStride())
            SolveDriveRecord(*reinterpret_cast<Jacobian*>(lpD), m_RF_Stack);
    }
}

// -------------------------------------------------------------------------------------
// Simulation::SpyContactJacobians @ 0x82BC4138   (107 instructions)
//
// Emit a ContactJacobianSpy over the HEAD of m_CJ_Stack for every post-batch record with
// the spy flag AND a positive accumulated normal impulse (gate order is the console's:
// flags first @0x82BC41C4, then `fcmpu` L.x > 0 @0x82BC41DC). The emit cursor advances
// only on emit (console 0x70/record) and can never catch the read cursor.
//
// The world points are rebuilt by chasing the record's body pointers -- [ptr+0x10] is
// RigidBody::mCom on the console (`lvx128 v5,r8,0x10` @0x82BC4260); named member here.
// 1/ts^2 is the console's NR-refined vrefp; 1/(invmA+invmB) is a genuine `fdivs` there too.
// -------------------------------------------------------------------------------------
void Simulation::SpyContactJacobians()
{
    CGS_STATIC_ASSERT((sizeof(ContactJacobianSpy) <= sizeof(Contact)),
                  "the in-place emit must stay behind the read cursor");

    const __vector4 lTime = __vspltw(__lvlx(&m_TimeStep, 0), 0);
    const __vector4 lTimeSq = __vmulfp(lTime, lTime);
    const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
    __vector4 lInvTsSq = __vrefp(lTimeSq);
    lInvTsSq = __vmaddfp(lInvTsSq, __vnmsubfp(lTimeSq, lInvTsSq, lOne), lInvTsSq);
    lInvTsSq = __vmaddfp(lInvTsSq, __vnmsubfp(lTimeSq, lInvTsSq, lOne), lInvTsSq);
    lInvTsSq = __vmulfp(lInvTsSq, lOne);

    const ContactJacobian* lpJ   = static_cast<const ContactJacobian*>(m_CJ_Stack);
    ContactJacobianSpy*    lpOut = static_cast<ContactJacobianSpy*>(m_CJ_Stack);

    for (u32 luI = 0u; luI < m_CT_Count; ++luI, ++lpJ)              // +0x64
    {
        if ((lpJ->mFlags & static_cast<u32>(SPY_BODY)) == 0u)       // `lwz 0x2C` + `& 8`
            continue;
        if (lpJ->mLambda.X() <= 0.0f)                               // fcmpu + ble
            continue;

        const f32 lfWeight = 1.0f / (lpJ->mInvmB + lpJ->mInvmA);    // fdivs, +0x9C + +0x8C
        RigidBody* const lpBodyA = lpJ->mpBodyA;
        RigidBody* const lpBodyB = lpJ->mpBodyB;
        const u32 luTag = lpJ->mTag;
        const __vector4 lN = __lvx(&lpJ->mRi, 0), lUp = __lvx(&lpJ->mUp, 0), lAt = __lvx(&lpJ->mAt, 0);
        const __vector4 lLambda = __lvx(&lpJ->mLambda, 0);
        const __vector4 lPointA = __vaddfp(__lvx(&lpBodyA->mCom, 0), __lvx(&lpJ->mRA, 0));
        const __vector4 lPointB = __vaddfp(__lvx(&lpBodyB->mCom, 0), __lvx(&lpJ->mRB, 0));
        const __vector4 lMassA = __vspltw(__lvlx(&lpJ->mInvmA, 0), 0);
        const __vector4 lMassB = __vspltw(__lvlx(&lpJ->mInvmB, 0), 0);
        const __vector4 lWeight = __vspltw(__lvlx(&lfWeight, 0), 0);
        const __vector4 lPoint = __vmulfp(__vmaddfp(lPointA, lMassA, __vmulfp(lPointB, lMassB)), lWeight);
        const __vector4 lForceN = __vmulfp(__vmulfp(lN, __vspltw(lLambda, 0)), lInvTsSq);
        const __vector4 lForceT = __vmulfp(__vmaddfp(lUp, __vspltw(lLambda, 1),
                                                     __vmulfp(lAt, __vspltw(lLambda, 2))), lInvTsSq);
        lpOut->mRi.mV = lN; lpOut->mUp.mV = lUp; lpOut->mAt.mV = lAt;
        lpOut->mPoint.mV = lPoint; lpOut->mForceN.mV = lForceN; lpOut->mForceT.mV = lForceT;
        lpOut->mpBodyA = lpBodyA;
        lpOut->mpBodyB = lpBodyB;
        lpOut->muTag = luTag;
        ++lpOut;                                        // addi r10, r10, 0x70

        ++m_CS_Count;                                   // +0x68
    }
}

// -------------------------------------------------------------------------------------
// Simulation::SpyDriveJacobians @ 0x82BC3010   (193 instructions)
//
// Emit a DriveJacobianSpy over the HEAD of m_DJ_Stack for every drive jacobian with mSpy
// set. Force/torque are the SpyJointJacobians shape (rows 12/16/20 . mRows[2] and
// MatIBT-gather + rows 14/18/22 . mRows[3], each * 1/ts^2 -- NR-refined vrefp there, exact
// divide here). The held-back "quaternion block" (0x82BC30D0..0x82BC323C) decodes as
// DriveJacobian::Build's OWN prologue re-run against the live bodies:
//     anchorA = comA + R_A . skel.mPosA        anchorB = comB + R_B . skel.mPosB
//     qA'     = bodyA.mQuat (x) skel.mQuatA    qB'     = bodyB.mQuat (x) skel.mQuatB
// emitting |anchorB - anchorA| (vcmpeqfp-guarded vrsqrtefp + 2xNR == jacobian_detail::Sqrt)
// and fabs(1 - dot4(qA', qB')). The Hamilton products are the `vpermwi128 0x63` (yzxw)
// pairs -- QuatMul's exact console shape.
// -------------------------------------------------------------------------------------
void Simulation::SpyDriveJacobians()
{
    const __vector4 lTime = __vspltw(__lvlx(&m_TimeStep, 0), 0);
    const __vector4 lTimeSq = __vmulfp(lTime, lTime), lOne = __vcfsx(__vspltisw(1), 0);
    __vector4 lInvTsSq = __vrefp(lTimeSq);
    lInvTsSq = __vmaddfp(lInvTsSq, __vnmsubfp(lTimeSq, lInvTsSq, lOne), lInvTsSq);
    lInvTsSq = __vmaddfp(lInvTsSq, __vnmsubfp(lTimeSq, lInvTsSq, lOne), lInvTsSq);
    lInvTsSq = __vmulfp(lInvTsSq, lOne);

    char*             lpArray = static_cast<char*>(m_DJ_Stack);     // +0x18, read cursor
    DriveJacobianSpy* lpOut   = static_cast<DriveJacobianSpy*>(m_DJ_Stack);   // emit cursor

    for (u32 luI = 0u; luI < m_DR_Count; ++luI, lpArray += JacobianStride())  // +0x74
    {
        const Jacobian* lpJ = reinterpret_cast<const Jacobian*>(lpArray);
        if (lpJ->mRows[4].mV.u[3] == 0u)                            // `lwz 0x4C` @0x82BC30C4
            continue;

        Drive* const lpDrive = reinterpret_cast<Drive*>(lpJ->mRows[12].mV.u[3]);    // `lwz 0xCC` @0x82BC30D0
        const DriveFrames& lrF = *lpDrive->GetFrames();             // `lwz 0(r29)`
        RigidBody* const lpA = lpDrive->GetChild();                 // `lwz 0x10(r29)`
        RigidBody* const lpB = lpDrive->GetParent();                // `lwz 0x14(r29)`

        // anchor separation (Build block 2's r = axis*component + com, per body)
        const V3 lvPA = lrF.GetChildPosition(), lvPB = lrF.GetParentPosition();
        __vector4 lAnchorA = __vmaddfp(__lvx(&lpA->mRi, 0), __vspltw(lvPA.mV, 0), __lvx(&lpA->mCom, 0));
        lAnchorA = __vmaddfp(__lvx(&lpA->mUp, 0), __vspltw(lvPA.mV, 1), lAnchorA);
        lAnchorA = __vmaddfp(__lvx(&lpA->mAt, 0), __vspltw(lvPA.mV, 2), lAnchorA);
        __vector4 lAnchorB = __vmaddfp(__lvx(&lpB->mRi, 0), __vspltw(lvPB.mV, 0), __lvx(&lpB->mCom, 0));
        lAnchorB = __vmaddfp(__lvx(&lpB->mUp, 0), __vspltw(lvPB.mV, 1), lAnchorB);
        lAnchorB = __vmaddfp(__lvx(&lpB->mAt, 0), __vspltw(lvPB.mV, 2), lAnchorB);
        const __vector4 lSep = __vsubfp(lAnchorB, lAnchorA);
        const f32 lfSep = Sqrt(__vmsum3fp(lSep, lSep).v[0]);

        // orientation separation (Build block 1's two Hamilton products)
        const Quat lqA = QuatMul(lpA->mQuat, lrF.GetChildOrientation());    // skel +0x00
        const Quat lqB = QuatMul(lpB->mQuat, lrF.GetParentOrientation());   // skel +0x20
        const f32  lfDot4 = __vmsum4fp(__lvx(&lqA, 0), __lvx(&lqB, 0)).v[0];
        const f32  lfAngSep = std::fabs(1.0f - lfDot4); // fsubs + fabs @0x82BC3290/0x82BC32A8

        M33 lMat;
        DriveJacobian::GetMatIBT(&lMat, lpJ);           // bl @0x82BC3240

        const __vector4 lLam = lpJ->mRows[2].mV, lAng = lpJ->mRows[3].mV;
        __vector4 lForce = __vmulfp(lpJ->mRows[12].mV, __vspltw(lLam, 0));
        lForce = __vmaddfp(lpJ->mRows[16].mV, __vspltw(lLam, 1), lForce);
        lForce = __vmaddfp(lpJ->mRows[20].mV, __vspltw(lLam, 2), lForce);
        __vector4 lTorqueM = __vmulfp(lMat.xAxis.mV, __vspltw(lAng, 0));
        lTorqueM = __vmaddfp(lMat.yAxis.mV, __vspltw(lAng, 1), lTorqueM);
        lTorqueM = __vmaddfp(lMat.zAxis.mV, __vspltw(lAng, 2), lTorqueM);
        __vector4 lTorqueB = __vmulfp(lpJ->mRows[14].mV, __vspltw(lAng, 0));
        lTorqueB = __vmaddfp(lpJ->mRows[18].mV, __vspltw(lAng, 1), lTorqueB);
        lTorqueB = __vmaddfp(lpJ->mRows[22].mV, __vspltw(lAng, 2), lTorqueB);
        lpOut->mForce.mV = __vmulfp(lForce, lInvTsSq);
        lpOut->mTorque.mV = __vmulfp(__vaddfp(lTorqueB, lTorqueM), lInvTsSq);
        lpOut->mSeparation    = lfSep;                  // stfs +0x20
        lpOut->mAngSeparation = lfAngSep;               // stfs +0x24
        lpOut->mpDrive        = lpDrive;                // stw  +0x28
        lpOut->muTag          = lpDrive->GetTag();      // `lwz 0x18(r29)` -> stw +0x2C
        ++lpOut;                                        // addi r30, r30, 0x30

        ++m_DS_Count;                                   // +0x78
    }
}

// -------------------------------------------------------------------------------------
// Simulation::SimulationUpdate @ 0x82BC6B40   (79 instructions, no VMX) -- HOME AGAIN
// (2026-08-06). This body sat quarantined in Simulation_SimulationUpdate.cpp while the
// eight solver stages above were unreconstructed; that TU is deleted.
//
// One solver tick. Early-out when no bodies are active. Build the three jacobian batches,
// pick the solver pipeline from which batches are non-empty, integrate, then run any
// enabled jacobian spies.
//
// SIGNATURE: the time step arrives in f1 and is stored with `stfs` (0x82BC6B6C) -- a FLOAT,
// not the `double` Hex-Rays prints. The Xbox One build agrees (`vmovss [rcx+0E0h], xmm1`).
//
//   pipeline selector: bit0 = contacts present, bit1 = joints, bit2 = drives
//     1        -> Anubis   (contacts only)
//     2        -> Osiris   (joints only)
//     4        -> Isis     (drives only)
//     3, 5-7   -> Horus    (any MIX)
//   ⚠️ CORRECTION 2026-08-05: an older comment claimed "2, 3 -> Osiris"; the CODE below
//   (`else if (luPipeline < 3u)`) always sent 3 to Horus, and the Xbox One SimulationUpdate
//   (sub_1409B7240) confirms the code: its selector sends only v==2 to Osiris and 3 to
//   Horus. The comment was the bug.
//
// The spy block reads m_SpyFlag once for the gate, then RE-READS it for arms 2 and 3
// (X360 0x82BC6C30 / 0x82BC6C50) -- a spy is allowed to clear its own bit. Reading the
// member each time is faithful to arms 2/3 and harmless for arm 1.
// -------------------------------------------------------------------------------------
bool Simulation::SimulationUpdate(f32 lfTimeStep)
{
    if (m_ActiveRB_Count == 0u)
        return false;

    m_TimeStep = lfTimeStep;                 // stfs f1, +0xA0

    ContactBatchBuild();
    JointBatchBuild();
    DriveBatchBuild();

    u32 luPipeline = (m_CT_Count != 0u) ? 1u : 0u;
    if (m_JT_Count != 0u)
        luPipeline |= 2u;
    if (m_DR_Count != 0u)
        luPipeline |= 4u;

    if (luPipeline != 0u)
    {
        if (luPipeline == 1u)
            Anubis_Pipeline();
        else if (luPipeline < 3u)
            Osiris_Pipeline();
        else if (luPipeline == 4u)
            Isis_Pipeline();
        else
            Horus_Pipeline();
    }

    // [DIAG] NOT IN THE X360 BINARY -- b5-decomp#2 witness hook, inert when NULL (simulation.h).
    if (gpContactDiagHook != 0)
        gpContactDiagHook(this);

    BatchIntegrator();

    if (m_SpyFlag != SPY_NOTHING)
    {
        if (m_JT_Count != 0u && (static_cast<u32>(m_SpyFlag) & SPY_JOINTS) != 0u)
            SpyJointJacobians();
        if (m_DR_Count != 0u && (static_cast<u32>(m_SpyFlag) & SPY_DRIVES) != 0u)
            SpyDriveJacobians();
        if (m_CT_Count != 0u && (static_cast<u32>(m_SpyFlag) & SPY_CONTACTS) != 0u)
            SpyContactJacobians();
    }

    return true;
}

} // namespace physics
} // namespace rw
