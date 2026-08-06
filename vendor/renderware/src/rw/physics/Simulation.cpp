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
        return nullptr;

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
        return nullptr;

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
// ⚠️ THE POOL BODIES GET mId = THEIR INDEX, NOT A POINTER. The console stores the resolved
// address `m_RF_Stack + 64*i` into the +0x1C slot; on x64 that slot is the 32-bit index
// (see the banner in rw/physics/rigidbody.h -- the shipping x64 builds index too). Same
// reaction-force block, different representation.
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
            lpNode->mId     = luId;                              // stw r7, 0x1C(r11)
            lpNode->mState  = FREE_BODY;                         // stw r31,0x8C(r11)
            ++luId;
            lpNode->mStasis = lpSim;                             // stw r3, 0x4C(r11)
            ++lpNode;                                            // addi r11,r11,0xB0
            lpPrev->mRight  = lpNode;                            // stw r11,0x2C(r8)
        }

        lpNode->mState  = FREE_BODY;
        lpNode->mLeft   = lpPrev;
        lpNode->mStasis = lpSim;
        lpNode->mId     = static_cast<u32>(liNumBodies - 1);     // `slwi r10,r4,6 ; add r10,r10,r9`
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
    lpSim->m_Gravity.x    = 0.0f;                                        // flt_82001CC0
    lpSim->m_Gravity.y    = -600.0f;                                     // flt_8202CF3C
    lpSim->m_Gravity.z    = 0.0f;                                        // flt_82001CC0
    lpSim->m_Gravity.w    = 0.0f;                                        // stw r31, 0(r11)
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
        return nullptr;

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
        lpBodyInertia = nullptr;                        // ⚠️ `stw r29(=0),0x5C` -- drops lpInertia
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
    // (w lane from the current value), store. On the PC the w payloads are their own
    // members, so this is a plain .xyz assignment of four rows.
    //     mRi (+0x40) <- r4+0x00     mUp  (+0x50) <- r4+0x10
    //     mAt (+0x60) <- r4+0x20     mCom (+0x10) <- r4+0x30
    lpBody->mRi.x  = lrFrame.xAxis.x;  lpBody->mRi.y  = lrFrame.xAxis.y;  lpBody->mRi.z  = lrFrame.xAxis.z;
    lpBody->mUp.x  = lrFrame.yAxis.x;  lpBody->mUp.y  = lrFrame.yAxis.y;  lpBody->mUp.z  = lrFrame.yAxis.z;
    lpBody->mAt.x  = lrFrame.zAxis.x;  lpBody->mAt.y  = lrFrame.zAxis.y;  lpBody->mAt.z  = lrFrame.zAxis.z;
    lpBody->mCom.x = lrFrame.wAxis.x;  lpBody->mCom.y = lrFrame.wAxis.y;  lpBody->mCom.z = lrFrame.wAxis.z;

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
    if (lpBody->mInertia != nullptr)
        lpBody->InertiaUpdate(lpBody->mInertia);        // mIfull/+0x70, mIsplt/+0x80, mInvm/+0x7C

    // ---- 5. the common tail ---------------------------------------------------------------
    // 0x82BC3660..0x82BC36B8. mVel/mOmega/mTorque are zeroed .xyz-only; mKine takes
    // flt_821815B0, re-read from the image this wave as 0x7F7FFFFF == FLT_MAX.
    lpBody->mVel.x    = 0.0f;  lpBody->mVel.y    = 0.0f;  lpBody->mVel.z    = 0.0f;
    lpBody->mOmega.x  = 0.0f;  lpBody->mOmega.y  = 0.0f;  lpBody->mOmega.z  = 0.0f;
    lpBody->mTorque.x = 0.0f;  lpBody->mTorque.y = 0.0f;  lpBody->mTorque.z = 0.0f;
    lpBody->mKine     = FLT_MAX;                        // +0x9C

    // ⚠️⚠️ mForce IS SEEDED WITH GRAVITY, NOT ZEROED. `lwz r10,0x4C(r11)` reads the body's own
    // mStasis and `lvx128 v12,r10,r6(=0x90)` loads m_Gravity through it -- so the source
    // really is `ResetForces(GetSimulation()->GetGravity())` on the body, not `this`. Same
    // object (Initialize threads mStasis when it builds the free list), kept faithful anyway.
    // ⚠️ The DEFAULT gravity in this build is -600.0f (flt_8202CF3C), not -9.81f.
    {
        const rw::math::vpu::Vector3& lrG = lpBody->mStasis->GetGravity();
        lpBody->mForce.x = lrG.x;  lpBody->mForce.y = lrG.y;  lpBody->mForce.z = lrG.z;
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
// Newton-Raphson refinements of 1/(m_TimeStep^2) -- a plain divide here (more precise,
// same intent; the spy path is diagnostic).
//
// ⭐ DECODE NOTE that unblocked this body (banked for the seven remaining stages): in this
// IDA listing plain `vmaddfp vD,vA,vB,vC` is the VA-form with the ADDEND PRINTED THIRD
// (vD = vA*vC + vB -- the brief's standing rule), while `vmaddfp128 vD,vA,vB,vC` prints the
// addend FOURTH (vD = vA*vB + vC). Both occur in this one function and each decodes to
// algebra the XB1 oracle (sub_1409B7B80) confirms only under its own rule.
//
// ⚠️ w LANES, named divergence: the console's emitted w lanes fold in mRows[12].w -- which on
// the CONSOLE is the low half of the packed node POINTER (Jacobian.hpp's +0xCC note), i.e.
// deterministic junk. On the host mRows[12].w is a real (zero-filled by Build) float lane, so
// the emitted w differs from the console's junk. The consumer (AddJointSpiesToOutputQueue
// @0x828A58E0) forwards rows whole into OutJointSpy events without reading w -- dead cargo,
// same class as Contact::mBodyA at drain time.
//
// The M33 rows contribute xyz only (Matrix33 rows; GetMatIBT gathers nine scalar lanes).
// -------------------------------------------------------------------------------------
void Simulation::SpyJointJacobians()
{
    if (m_JT_Count == 0u)                               // +0x6C (`ble` after the recip setup)
        return;

    const f32 lfInvTsSq = 1.0f / (m_TimeStep * m_TimeStep);   // +0xA0

    char*             lpArray = static_cast<char*>(m_JJ_Stack);            // +0x14, read cursor
    JointJacobianSpy* lpOut   = reinterpret_cast<JointJacobianSpy*>(m_JJ_Stack);  // emit cursor

    for (u32 luIndex = 0u; luIndex < m_JT_Count; ++luIndex, lpArray += JacobianStride())
    {
        const Jacobian* lpJac = reinterpret_cast<const Jacobian*>(lpArray);
        if (lpJac->mSpy == 0u)                          // +0x4C
            continue;

        Joint* const lpJoint = static_cast<Joint*>(lpJac->mpNode);         // +0xCC

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

        rw::math::vpu::Vector4 lForce;                  // R12*r2.x + R16*r2.y + R20*r2.z
        lForce.x = (lrR12.x * lrR2.x + lrR16.x * lrR2.y + lrR20.x * lrR2.z) * lfInvTsSq;
        lForce.y = (lrR12.y * lrR2.x + lrR16.y * lrR2.y + lrR20.y * lrR2.z) * lfInvTsSq;
        lForce.z = (lrR12.z * lrR2.x + lrR16.z * lrR2.y + lrR20.z * lrR2.z) * lfInvTsSq;
        lForce.w = (lrR12.w * lrR2.x + lrR16.w * lrR2.y + lrR20.w * lrR2.z) * lfInvTsSq;

        rw::math::vpu::Vector4 lTorque;                 // M^T-gather + R14/R18/R22 trilinear
        lTorque.x = (lMat.xAxis.x * lrR3.x + lMat.yAxis.x * lrR3.y + lMat.zAxis.x * lrR3.z
                   + lrR14.x * lrR3.x + lrR18.x * lrR3.y + lrR22.x * lrR3.z) * lfInvTsSq;
        lTorque.y = (lMat.xAxis.y * lrR3.x + lMat.yAxis.y * lrR3.y + lMat.zAxis.y * lrR3.z
                   + lrR14.y * lrR3.x + lrR18.y * lrR3.y + lrR22.y * lrR3.z) * lfInvTsSq;
        lTorque.z = (lMat.xAxis.z * lrR3.x + lMat.yAxis.z * lrR3.y + lMat.zAxis.z * lrR3.z
                   + lrR14.z * lrR3.x + lrR18.z * lrR3.y + lrR22.z * lrR3.z) * lfInvTsSq;
        lTorque.w = (lrR14.w * lrR3.x + lrR18.w * lrR3.y + lrR22.w * lrR3.z) * lfInvTsSq;

        lpOut->mForce  = lForce;                        // stvx128 @r30+0x00
        lpOut->mTorque = lTorque;                       // stvx128 @r30+0x10
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
//   * THE REACTION-FORCE BLOCK is four Vector4 rows per body at m_RF_Stack + 64*mId
//     (DynamicUpdate's consumer view, RigidBody.cpp): row0 = linear velocity impulse,
//     row1 = direct linear position correction, row2 = angular velocity impulse,
//     row3 = angular position correction. The console's mBodyA/mIdA slots hold the RESOLVED
//     block address (Initialize stores m_RF_Stack + 64*i into mId there); the host holds
//     the INDEX and resolves it here -- exactly the XB1 move (`shl rdx,6; add rdx,RF`).
//   * ALL RF ACCUMULATION IS xyz-ONLY on the host. The console's full-row vector adds drag
//     deterministic junk through the w lanes; DynamicUpdate reads xyz only (vLoad3) and
//     zeroes whole rows, so the lanes are dead on both targets.
//   * NO BODY-STATE BRANCHES. X360 solves every pair branchlessly: ContactBatchBuild and
//     the two jacobian builders zero-mask invm and I^-1 for non-ACTIVE bodies, so a static
//     side receives and contributes exactly nothing (its RF block provably stays zero:
//     zeroed at Initialize, only ever incremented by zero-scaled rows, and DynamicUpdate
//     never runs on it). XB1 instead BRANCHES on a flags bit -- and its batch even swaps
//     the pair so the dynamic body is always in seat A -- reaching the same fixed point.
//     The host follows X360.
//   * `vmaddfp vD,vA,vB,vC == vA*vC + vB` / `vnmsubfp == vB - vA*vC` (JacobianMath.hpp's
//     proven operand rules) -- every expression below was transposed through them.
//   * Effective-mass reciprocals: the console batch uses a SINGLE UNREFINED `vrefp`
//     (~12-bit) on the denominators, and the spies refine `vrefp`/`vrsqrtefp` with two
//     Newton-Raphson steps; XB1 uses exact divides everywhere. The host divides exactly:
//     behaviourally identical, not bit-identical (the committed GenerateFromCollision
//     precedent).
// =====================================================================================

// The scalar lane helpers the builders already use (JacobianMath.hpp) -- V3/M33/Quat,
// Add/Sub/Scale/Cross/Dot3, Transform, UnpackInverseInertia, QuatMul, Sqrt, Min3/Max3.
using namespace jacobian_detail;

namespace
{
    inline rw::math::vpu::Vector4* ReactionRows(void* lpRFStack, s32 liId)
    {
        return reinterpret_cast<rw::math::vpu::Vector4*>(
            static_cast<u8*>(lpRFStack) + static_cast<u32>(liId) * KU_REACTION_FORCE_STRIDE);
    }

    inline V3 U3(const Contact::Vector3U_32& lrU) { return MakeV3(lrU.x, lrU.y, lrU.z); }

    inline void PutU3(Contact::Vector3U_32& lrDst, const V3& lrSrc)
    { lrDst.x = lrSrc.x; lrDst.y = lrSrc.y; lrDst.z = lrSrc.z; }

    // xyz-only accumulate into an RF row (see the w-lane convention in the banner).
    inline void AddXyz(rw::math::vpu::Vector4& lrRow, const V3& lrD)
    { lrRow.x += lrD.x; lrRow.y += lrD.y; lrRow.z += lrD.z; }

    inline void SubXyz(rw::math::vpu::Vector4& lrRow, const V3& lrD)
    { lrRow.x -= lrD.x; lrRow.y -= lrD.y; lrRow.z -= lrD.z; }

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
        const V3 lvRA = Xyz(lrJ.mRows[0]);
        const V3 lvRB = Xyz(lrJ.mRows[1]);
        lrVel = Sub(Add(Xyz(lpB[0]), Cross(Xyz(lpB[2]), lvRB)),
                    Add(Xyz(lpA[0]), Cross(Xyz(lpA[2]), lvRA)));
        lrOmg = Sub(Xyz(lpB[2]), Xyz(lpA[2]));
    }

    // J . v through the transposed, PRE-DIVIDED rows the builders store (linear rows 4/6/8,
    // angular rows 5/7/9): lane i of the result = dot(axis_i, v) / mEff_i.
    inline V3 ProjectRows(const rw::math::vpu::Vector4& lrRx,
                          const rw::math::vpu::Vector4& lrRy,
                          const rw::math::vpu::Vector4& lrRz, const V3& lrV)
    {
        return Add(Add(Scale(Xyz(lrRx), lrV.x), Scale(Xyz(lrRy), lrV.y)),
                   Scale(Xyz(lrRz), lrV.z));
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
        const V3 lvWorld = ProjectRows(lrJ.mRows[12], lrJ.mRows[16], lrJ.mRows[20], lrDLin);
        AddXyz(lpA[0], Scale(lvWorld, lrJ.mRows[16].w));      // +0x10C invmA
        SubXyz(lpB[0], Scale(lvWorld, lrJ.mRows[20].w));      // +0x14C invmB

        const V3 lvAngA = Add(ProjectRows(lrJ.mRows[13], lrJ.mRows[17], lrJ.mRows[21], lrDLin),
                              ProjectRows(lrJ.mRows[14], lrJ.mRows[18], lrJ.mRows[22], lrDAng));
        AddXyz(lpA[2], lvAngA);

        const V3 lvAxB0 = MakeV3(lrJ.mRows[13].w, lrJ.mRows[14].w, lrJ.mRows[15].w);
        const V3 lvAxB1 = MakeV3(lrJ.mRows[17].w, lrJ.mRows[18].w, lrJ.mRows[19].w);
        const V3 lvAxB2 = MakeV3(lrJ.mRows[21].w, lrJ.mRows[22].w, lrJ.mRows[23].w);
        const V3 lvAngB = Add(ProjectRows(lrJ.mRows[15], lrJ.mRows[19], lrJ.mRows[23], lrDLin),
                              Add(Add(Scale(lvAxB0, lrDAng.x), Scale(lvAxB1, lrDAng.y)),
                                  Scale(lvAxB2, lrDAng.z)));
        SubXyz(lpB[2], lvAngB);
    }

    // ---------------------------------------------------------------------------------
    // One CONTACT record, one iteration -- the per-record body of Anubis_Pipeline
    // @0x82BC11C0 (and Horus's contact segment 0x82BC1B48..0x82BC1D58, fused copy).
    // Lane-verified against the X360 asm; algorithm cross-read on XB1 sub_1409ADBF0.
    // ---------------------------------------------------------------------------------
    inline void SolveContactRecord(rw::physics::ContactJacobian& lrJ, void* lpRFStack)
    {
        rw::math::vpu::Vector4* const lpA = ReactionRows(lpRFStack, lrJ.mBodyA);
        rw::math::vpu::Vector4* const lpB = ReactionRows(lpRFStack, lrJ.mBodyB);

        const V3 lvRA = U3(lrJ.mRA);
        const V3 lvRB = U3(lrJ.mRB);

        // Velocity and positional halves, B minus A (X360 0x82BC12E8..0x82BC1394):
        //   vel = (RF_B0 + cross(RF_B2,rB)) - (RF_A0 + cross(RF_A2,rA))
        //   pos = (RF_B1 + cross(RF_B3,rB)) - (RF_A1 + cross(RF_A3,rA))
        // The xyz residual lanes use vel+pos; the split positional lane uses pos ALONE
        // (X360 builds the splat pairs {tot.s, tot.s, tot.s, pos.s} with the
        // 0x821816F0/0x82181700/0x82181710 tables; XB1 keeps xmm5/xmm11 apart -- same).
        const V3 lvVel = Sub(Add(Xyz(lpB[0]), Cross(Xyz(lpB[2]), lvRB)),
                             Add(Xyz(lpA[0]), Cross(Xyz(lpA[2]), lvRA)));
        const V3 lvPos = Sub(Add(Xyz(lpB[1]), Cross(Xyz(lpB[3]), lvRB)),
                             Add(Xyz(lpA[1]), Cross(Xyz(lpA[3]), lvRA)));
        const V3 lvTot = Add(lvVel, lvPos);

        // Raw updates: L + bias + J.v through the pre-divided columns; the positional lane
        // re-uses the J x-lanes (= the normal column) with the positional velocity.
        const V3  lvJx = U3(lrJ.mJx), lvJy = U3(lrJ.mJy), lvJz = U3(lrJ.mJz);
        const V3  lvRaw = Add(Add(U3(lrJ.mLambda), U3(lrJ.mBias)),
                              Add(Add(Scale(lvJx, lvTot.x), Scale(lvJy, lvTot.y)),
                                  Scale(lvJz, lvTot.z)));
        const f32 lfRawP = lrJ.mLambdaPos + lrJ.mBiasPos
                         + lvJx.x * lvPos.x + lvJy.x * lvPos.y + lvJz.x * lvPos.z;

        // The four-lane clamp (X360 0x82BC13BC..0x82BC1410, lane-verified: bounds
        // {inf, +mus*Lx, +mus*Lx, inf} / {0, -mus*Lx, -mus*Lx, 0} built from the
        // {0, Lx, Lx, 0} perm and the 0x821817A0 = {FLT_MAX,0,0,FLT_MAX} corners;
        // replacement values use mud). Both friction tests compare the RAW value; the
        // TEST cone is static friction, the REPLACEMENT is dynamic friction.
        const f32 lfLxOld = lrJ.mLambda.x;                       // the PRE-update normal impulse
        const f32 lfMusLx = lrJ.mMus * lfLxOld;
        const f32 lfMudLx = lrJ.mMud * lfLxOld;

        f32 lfN  = (lvRaw.x >= 0.0f) ? lvRaw.x : 0.0f;
        f32 lfT1 = lvRaw.y;
        if (lvRaw.y < -lfMusLx) lfT1 = -lfMudLx;
        if (lvRaw.y >  lfMusLx) lfT1 =  lfMudLx;
        f32 lfT2 = lvRaw.z;
        if (lvRaw.z < -lfMusLx) lfT2 = -lfMudLx;
        if (lvRaw.z >  lfMusLx) lfT2 =  lfMudLx;
        const f32 lfP = (lfRawP >= 0.0f) ? lfRawP : 0.0f;

        const f32 lfDN  = lfN  - lrJ.mLambda.x;
        const f32 lfDT1 = lfT1 - lrJ.mLambda.y;
        const f32 lfDT2 = lfT2 - lrJ.mLambda.z;
        const f32 lfDP  = lfP  - lrJ.mLambdaPos;

        lrJ.mLambda.x = lfN; lrJ.mLambda.y = lfT1; lrJ.mLambda.z = lfT2;   // stvx +0x50
        lrJ.mLambdaPos = lfP;

        // Apply (X360 0x82BC141C..0x82BC14AC, all eight RF rows, both bodies,
        // unconditionally -- the zero-masked invm/T rows make the static side a no-op):
        const V3 lvN  = U3(lrJ.mRi);
        const V3 lvUp = U3(lrJ.mUp);
        const V3 lvAt = U3(lrJ.mAt);
        const V3 lvWorld = Add(Add(Scale(lvN, lfDN), Scale(lvUp, lfDT1)), Scale(lvAt, lfDT2));
        const V3 lvNP    = Scale(lvN, lfDP);

        AddXyz(lpA[0], Scale(lvWorld, lrJ.mInvmA));
        AddXyz(lpA[1], Scale(lvNP,    lrJ.mInvmA));
        AddXyz(lpA[2], Add(Add(Scale(U3(lrJ.mTnA), lfDN), Scale(U3(lrJ.mTupA), lfDT1)),
                           Scale(U3(lrJ.mTatA), lfDT2)));
        AddXyz(lpA[3], Scale(U3(lrJ.mTnA), lfDP));

        SubXyz(lpB[0], Scale(lvWorld, lrJ.mInvmB));
        SubXyz(lpB[1], Scale(lvNP,    lrJ.mInvmB));
        SubXyz(lpB[2], Add(Add(Scale(U3(lrJ.mTnB), lfDN), Scale(U3(lrJ.mTupB), lfDT1)),
                           Scale(U3(lrJ.mTatB), lfDT2)));
        SubXyz(lpB[3], Scale(U3(lrJ.mTnB), lfDP));
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
        rw::math::vpu::Vector4* const lpA = ReactionRows(lpRFStack, static_cast<s32>(lrJ.mIdA));
        rw::math::vpu::Vector4* const lpB = ReactionRows(lpRFStack, static_cast<s32>(lrJ.mIdB));

        V3 lvVel, lvOmg;
        JacobianRelVel(lrJ, lpA, lpB, lvVel, lvOmg);

        const V3 lvXLin = Add(Xyz(lrJ.mRows[2]),
                              ProjectRows(lrJ.mRows[4], lrJ.mRows[6], lrJ.mRows[8], lvVel));
        const V3 lvXAng = Add(Xyz(lrJ.mRows[3]),
                              ProjectRows(lrJ.mRows[5], lrJ.mRows[7], lrJ.mRows[9], lvOmg));

        const V3 lvLinLo = Xyz(lrJ.mRows[10]);                               // +0xA0
        const V3 lvLinHi = Xyz(lrJ.mRows[11]);                               // +0xB0
        const V3 lvAngLo = MakeV3(lrJ.mRows[6].w, lrJ.mRows[7].w, lrJ.mRows[10].w);   // {6C,7C,AC}
        const V3 lvAngHi = MakeV3(lrJ.mRows[8].w, lrJ.mRows[9].w, lrJ.mRows[11].w);   // {8C,9C,BC}

        const V3 lvZero = MakeV3(0.0f, 0.0f, 0.0f);
        const V3 lvNewLin = Add(Max3(Add(lvXLin, lvLinLo), lvZero),
                                Min3(Add(lvXLin, lvLinHi), lvZero));
        const V3 lvNewAng = Add(Max3(Add(lvXAng, lvAngLo), lvZero),
                                Min3(Add(lvXAng, lvAngHi), lvZero));

        const V3 lvDLin = Sub(lvNewLin, Xyz(lrJ.mRows[2]));
        const V3 lvDAng = Sub(lvNewAng, Xyz(lrJ.mRows[3]));

        // xyz stores; the console's whole-row stvx writes a dead computed w over the
        // builder's zero -- the host leaves the zero (both dead, the host's deterministic).
        lrJ.mRows[2].x = lvNewLin.x; lrJ.mRows[2].y = lvNewLin.y; lrJ.mRows[2].z = lvNewLin.z;
        lrJ.mRows[3].x = lvNewAng.x; lrJ.mRows[3].y = lvNewAng.y; lrJ.mRows[3].z = lvNewAng.z;

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
        rw::math::vpu::Vector4* const lpA = ReactionRows(lpRFStack, static_cast<s32>(lrJ.mIdA));
        rw::math::vpu::Vector4* const lpB = ReactionRows(lpRFStack, static_cast<s32>(lrJ.mIdB));

        V3 lvVel, lvOmg;
        JacobianRelVel(lrJ, lpA, lpB, lvVel, lvOmg);

        const V3 lvXLin = Add(Scale(Add(Xyz(lrJ.mRows[2]),
                                        ProjectRows(lrJ.mRows[4], lrJ.mRows[6], lrJ.mRows[8], lvVel)),
                                    lrJ.mRows[2].w),
                              Xyz(lrJ.mRows[10]));
        const V3 lvXAng = Add(Scale(Add(Xyz(lrJ.mRows[3]),
                                        ProjectRows(lrJ.mRows[5], lrJ.mRows[7], lrJ.mRows[9], lvOmg)),
                                    lrJ.mRows[3].w),
                              Xyz(lrJ.mRows[11]));

        const V3 lvClampL = MakeV3(lrJ.mRows[6].w, lrJ.mRows[7].w, lrJ.mRows[10].w);
        const V3 lvClampA = MakeV3(lrJ.mRows[8].w, lrJ.mRows[9].w, lrJ.mRows[11].w);
        const V3 lvNegL   = Sub(MakeV3(0.0f, 0.0f, 0.0f), lvClampL);
        const V3 lvNegA   = Sub(MakeV3(0.0f, 0.0f, 0.0f), lvClampA);

        const V3 lvNewLin = Max3(Min3(lvXLin, lvClampL), lvNegL);
        const V3 lvNewAng = Max3(Min3(lvXAng, lvClampA), lvNegA);

        const V3 lvDLin = Sub(lvNewLin, Xyz(lrJ.mRows[2]));
        const V3 lvDAng = Sub(lvNewAng, Xyz(lrJ.mRows[3]));

        lrJ.mRows[2].x = lvNewLin.x; lrJ.mRows[2].y = lvNewLin.y; lrJ.mRows[2].z = lvNewLin.z;
        lrJ.mRows[3].x = lvNewAng.x; lrJ.mRows[3].y = lvNewAng.y; lrJ.mRows[3].z = lvNewAng.z;

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
// ~12-bit. The host divides exactly (the XB1 spelling); documented, not reproduced.
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
    const u32 luCount = m_CT_Count;                     // +0x64
    if (luCount == 0u)
        return;

    const f32 lfTs = m_TimeStep;                        // lfs +0xA0 at the function head

    Contact* lpContact = static_cast<Contact*>(m_CJ_Stack);
    for (u32 luI = 0u; luI < luCount; ++luI, ++lpContact)
    {
        Contact& lrC = *lpContact;

        // ---- read the whole drain record into locals (the rewrite below overlaps it) ----
        const V3  lvPosA = U3(lrC.mPosA),   lvPosB = U3(lrC.mPosB);
        const V3  lvN    = U3(lrC.mRi),     lvUp   = U3(lrC.mUp),  lvAt = U3(lrC.mAt);
        const V3  lvVel  = U3(lrC.mVel);
        const f32 lfRes  = lrC.mRes;
        const f32 lfMus  = lrC.mMus,        lfMud  = lrC.mMud;
        const u32 luTag  = lrC.mTag;
        const V3  lvComA = U3(lrC.mComA),   lvComB = U3(lrC.mComB);
        const s32 liIdA  = static_cast<s32>(lrC.mIdA);
        const s32 liIdB  = static_cast<s32>(lrC.mIdB);
        const u32 luStA  = lrC.mStateA,     luStB  = lrC.mStateB;
        RigidBody* const lpBodyA = lrC.mpBodyA;
        RigidBody* const lpBodyB = lrC.mpBodyB;

        rw::math::vpu::Vector4 lIfullA, lIspltA, lIfullB, lIspltB;
        lIfullA.x = lrC.mIfullA.x; lIfullA.y = lrC.mIfullA.y; lIfullA.z = lrC.mIfullA.z; lIfullA.w = 0.0f;
        lIspltA.x = lrC.mIspltA.x; lIspltA.y = lrC.mIspltA.y; lIspltA.z = lrC.mIspltA.z; lIspltA.w = 0.0f;
        lIfullB.x = lrC.mIfullB.x; lIfullB.y = lrC.mIfullB.y; lIfullB.z = lrC.mIfullB.z; lIfullB.w = 0.0f;
        lIspltB.x = lrC.mIspltB.x; lIspltB.y = lrC.mIspltB.y; lIspltB.z = lrC.mIspltB.z; lIspltB.w = 0.0f;
        const V3 lvForceA  = U3(lrC.mForceA),  lvForceB  = U3(lrC.mForceB);
        const V3 lvTorqueA = U3(lrC.mTorqueA), lvTorqueB = U3(lrC.mTorqueB);

        // ---- the branchless ACTIVE masking (X360 `vand 4` + `vcmpequw` + vsel) -----------
        const bool lbActiveA = (luStA & static_cast<u32>(ACTIVE_BODY)) != 0u;
        const bool lbActiveB = (luStB & static_cast<u32>(ACTIVE_BODY)) != 0u;
        const f32  lfInvmA = lbActiveA ? lrC.mInvmA : 0.0f;
        const f32  lfInvmB = lbActiveB ? lrC.mInvmB : 0.0f;
        const M33  lInvIA  = lbActiveA ? UnpackInverseInertia(lIfullA, lIspltA) : ZeroMatrix33();
        const M33  lInvIB  = lbActiveB ? UnpackInverseInertia(lIfullB, lIspltB) : ZeroMatrix33();

        const V3 lvRA = Sub(lvPosA, lvComA);            // vsubfp @0x82BC15C4
        const V3 lvRB = Sub(lvPosB, lvComB);            // vsubfp @0x82BC15C0
        const V3 lvD  = Sub(lvPosB, lvPosA);            // vsubfp @0x82BC1714

        // ---- the six I^-1.(r x frame-row) products and the three denominators ------------
        const V3 lvCnA = Cross(lvRA, lvN),  lvCuA = Cross(lvRA, lvUp), lvCaA = Cross(lvRA, lvAt);
        const V3 lvCnB = Cross(lvRB, lvN),  lvCuB = Cross(lvRB, lvUp), lvCaB = Cross(lvRB, lvAt);
        const V3 lvTnA = Transform(lInvIA, lvCnA), lvTupA = Transform(lInvIA, lvCuA),
                 lvTatA = Transform(lInvIA, lvCaA);
        const V3 lvTnB = Transform(lInvIB, lvCnB), lvTupB = Transform(lInvIB, lvCuB),
                 lvTatB = Transform(lInvIB, lvCaB);

        const f32 lfDenN  = lfInvmA + lfInvmB + Dot3(lvCnA, lvTnA) + Dot3(lvCnB, lvTnB);
        const f32 lfDenUp = lfInvmA + lfInvmB + Dot3(lvCuA, lvTupA) + Dot3(lvCuB, lvTupB);
        const f32 lfDenAt = lfInvmA + lfInvmB + Dot3(lvCaA, lvTatA) + Dot3(lvCaB, lvTatB);
        const f32 lfIDenN = 1.0f / lfDenN;              // console: ONE unrefined vrefp
        const f32 lfIDenU = 1.0f / lfDenUp;
        const f32 lfIDenA = 1.0f / lfDenAt;

        // ---- the bias: predicted displacement u = d + ts*vel + ts^2*(accB - accA), with
        //      the acceleration terms ACTIVE-masked (X360 vsel @0x82BC1828/0x82BC18A4) ------
        const V3 lvAccA = lbActiveA ? Add(lvForceA, Cross(lvTorqueA, lvRA)) : MakeV3(0.0f, 0.0f, 0.0f);
        const V3 lvAccB = lbActiveB ? Add(lvForceB, Cross(lvTorqueB, lvRB)) : MakeV3(0.0f, 0.0f, 0.0f);
        const V3 lvU = Add(Add(lvD, Scale(lvVel, lfTs)),
                           Scale(Sub(lvAccB, lvAccA), lfTs * lfTs));

        f32       lfEN     = Dot3(lvN, lvU);
        const f32 lfEUp    = Dot3(lvUp, lvU);
        const f32 lfEAt    = Dot3(lvAt, lvU);
        const f32 lfP      = Dot3(lvN, lvD);
        const f32 lfBounce = -(lfRes * lfTs * Dot3(lvN, lvVel));

        if (lfEN > lfBounce)
        {
            if (lfP >= 0.0f)
                lfEN -= lfBounce;
            else if (lfBounce < 0.0f)
                lfEN -= (lfP + lfBounce);
        }

        // ---- the in-place rewrite (contact.h's ContactJacobian slot map) ------------------
        ContactJacobian& lrJ = *reinterpret_cast<ContactJacobian*>(&lrC);
        PutU3(lrJ.mRA, lvRA);   lrJ.mBodyA = liIdA;     // the vsel {r.xyz, com.w} mint
        PutU3(lrJ.mRB, lvRB);   lrJ.mBodyB = liIdB;
        PutU3(lrJ.mJx, MakeV3(lvN.x * lfIDenN, lvUp.x * lfIDenU, lvAt.x * lfIDenA));
        lrJ.mFlags = (luStA | luStB) & static_cast<u32>(SPY_BODY);
        PutU3(lrJ.mJy, MakeV3(lvN.y * lfIDenN, lvUp.y * lfIDenU, lvAt.y * lfIDenA));
        lrJ.mMus = lfMus;                               // preserved drain w lanes
        PutU3(lrJ.mJz, MakeV3(lvN.z * lfIDenN, lvUp.z * lfIDenU, lvAt.z * lfIDenA));
        lrJ.mMud = lfMud;
        PutU3(lrJ.mLambda, MakeV3(0.0f, 0.0f, 0.0f));   // stvx v0 @0x82BC19B4
        lrJ.mLambdaPos = 0.0f;
        PutU3(lrJ.mBias, MakeV3(lfEN * lfIDenN, lfEUp * lfIDenU, lfEAt * lfIDenA));
        lrJ.mBiasPos = lfP * lfIDenN;
        PutU3(lrJ.mRi, lvN);    lrJ.mDeadA = 0.0f;      // console: the parked 4-byte pointers
        PutU3(lrJ.mTnA, lvTnA); lrJ.mInvmA = lfInvmA;   // zero-masked, the spy's weights
        PutU3(lrJ.mTnB, lvTnB); lrJ.mInvmB = lfInvmB;
        PutU3(lrJ.mUp, lvUp);   lrJ.mDeadB = 0.0f;
        PutU3(lrJ.mTupA, lvTupA); lrJ.mDead0 = 0.0f;
        PutU3(lrJ.mTupB, lvTupB); lrJ.mDead1 = 0.0f;
        PutU3(lrJ.mAt, lvAt);   lrJ.mTag = luTag;       // preserved for the spy's emit
        PutU3(lrJ.mTatA, lvTatA); lrJ.mDead2 = 0.0f;
        PutU3(lrJ.mTatB, lvTatB); lrJ.mDead3 = 0.0f;
        lrJ.mpBodyA = lpBodyA;                          // the tail rides through
        lrJ.mpBodyB = lpBodyB;
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
    static_assert(sizeof(ContactJacobianSpy) <= sizeof(Contact),
                  "the in-place emit must stay behind the read cursor");

    const f32 lfInvTsSq = 1.0f / (m_TimeStep * m_TimeStep);         // +0xA0

    const ContactJacobian* lpJ   = static_cast<const ContactJacobian*>(m_CJ_Stack);
    ContactJacobianSpy*    lpOut = static_cast<ContactJacobianSpy*>(m_CJ_Stack);

    for (u32 luI = 0u; luI < m_CT_Count; ++luI, ++lpJ)              // +0x64
    {
        if ((lpJ->mFlags & static_cast<u32>(SPY_BODY)) == 0u)       // `lwz 0x2C` + `& 8`
            continue;
        if (!(lpJ->mLambda.x > 0.0f))                               // fcmpu vs 0.0f
            continue;

        const f32 lfWeight = 1.0f / (lpJ->mInvmB + lpJ->mInvmA);    // fdivs, +0x9C + +0x8C
        const V3 lvPointA = Add(Xyz(lpJ->mpBodyA->mCom), U3(lpJ->mRA));
        const V3 lvPointB = Add(Xyz(lpJ->mpBodyB->mCom), U3(lpJ->mRB));
        const V3 lvPoint  = Scale(Add(Scale(lvPointB, lpJ->mInvmB),
                                      Scale(lvPointA, lpJ->mInvmA)), lfWeight);

        const V3 lvN  = U3(lpJ->mRi);
        const V3 lvUp = U3(lpJ->mUp);
        const V3 lvAt = U3(lpJ->mAt);
        const V3 lvForceN = Scale(lvN, lpJ->mLambda.x * lfInvTsSq);
        const V3 lvForceT = Scale(Add(Scale(lvUp, lpJ->mLambda.y),
                                      Scale(lvAt, lpJ->mLambda.z)), lfInvTsSq);

        // The console emits full rows whose w lanes carry the parked pointer/tag junk;
        // host w lanes are 0 (dead cargo either way -- the consumer reads by name).
        lpOut->mRi.x  = lvN.x;  lpOut->mRi.y  = lvN.y;  lpOut->mRi.z  = lvN.z;  lpOut->mRi.w  = 0.0f;
        lpOut->mUp.x  = lvUp.x; lpOut->mUp.y  = lvUp.y; lpOut->mUp.z  = lvUp.z; lpOut->mUp.w  = 0.0f;
        lpOut->mAt.x  = lvAt.x; lpOut->mAt.y  = lvAt.y; lpOut->mAt.z  = lvAt.z; lpOut->mAt.w  = 0.0f;
        lpOut->mPoint.x  = lvPoint.x;  lpOut->mPoint.y  = lvPoint.y;  lpOut->mPoint.z  = lvPoint.z;  lpOut->mPoint.w  = 0.0f;
        lpOut->mForceN.x = lvForceN.x; lpOut->mForceN.y = lvForceN.y; lpOut->mForceN.z = lvForceN.z; lpOut->mForceN.w = 0.0f;
        lpOut->mForceT.x = lvForceT.x; lpOut->mForceT.y = lvForceT.y; lpOut->mForceT.z = lvForceT.z; lpOut->mForceT.w = 0.0f;
        lpOut->mpBodyA = lpJ->mpBodyA;                  // console stw +0x60
        lpOut->mpBodyB = lpJ->mpBodyB;                  // console stw +0x64
        lpOut->muTag   = lpJ->mTag;                     // console `lwz 0xDC` -> stw +0x68
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
    const f32 lfInvTsSq = 1.0f / (m_TimeStep * m_TimeStep);

    char*             lpArray = static_cast<char*>(m_DJ_Stack);     // +0x18, read cursor
    DriveJacobianSpy* lpOut   = static_cast<DriveJacobianSpy*>(m_DJ_Stack);   // emit cursor

    for (u32 luI = 0u; luI < m_DR_Count; ++luI, lpArray += JacobianStride())  // +0x74
    {
        const Jacobian* lpJ = reinterpret_cast<const Jacobian*>(lpArray);
        if (lpJ->mSpy == 0u)                            // `lwz 0x4C` @0x82BC30C4
            continue;

        Drive* const lpDrive = static_cast<Drive*>(lpJ->mpNode);    // `lwz 0xCC` @0x82BC30D0
        const DriveFrames& lrF = *lpDrive->GetFrames();             // `lwz 0(r29)`
        RigidBody* const lpA = lpDrive->GetChild();                 // `lwz 0x10(r29)`
        RigidBody* const lpB = lpDrive->GetParent();                // `lwz 0x14(r29)`

        // anchor separation (Build block 2's r = axis*component + com, per body)
        const V3 lvPA = Xyz(lrF.GetChildPosition());                // skel +0x10
        const V3 lvPB = Xyz(lrF.GetParentPosition());               // skel +0x30
        const V3 lvAnchorA = Add(Xyz(lpA->mCom),
                                 Add(Add(Scale(Xyz(lpA->mRi), lvPA.x), Scale(Xyz(lpA->mUp), lvPA.y)),
                                     Scale(Xyz(lpA->mAt), lvPA.z)));
        const V3 lvAnchorB = Add(Xyz(lpB->mCom),
                                 Add(Add(Scale(Xyz(lpB->mRi), lvPB.x), Scale(Xyz(lpB->mUp), lvPB.y)),
                                     Scale(Xyz(lpB->mAt), lvPB.z)));
        const V3  lvSep = Sub(lvAnchorB, lvAnchorA);
        const f32 lfSep = Sqrt(Dot3(lvSep, lvSep));     // 0-guarded, the console's vsel

        // orientation separation (Build block 1's two Hamilton products)
        const Quat lqA = QuatMul(lpA->mQuat, lrF.GetChildOrientation());    // skel +0x00
        const Quat lqB = QuatMul(lpB->mQuat, lrF.GetParentOrientation());   // skel +0x20
        const f32  lfDot4 = lqA.x * lqB.x + lqA.y * lqB.y + lqA.z * lqB.z + lqA.w * lqB.w;
        const f32  lfAngSep = std::fabs(1.0f - lfDot4); // fsubs + fabs @0x82BC3290/0x82BC32A8

        M33 lMat;
        DriveJacobian::GetMatIBT(&lMat, lpJ);           // bl @0x82BC3240

        const V3 lvLam = Xyz(lpJ->mRows[2]);            // +0x20  accumulated linear impulses
        const V3 lvAng = Xyz(lpJ->mRows[3]);            // +0x30  accumulated angular impulses

        const V3 lvForce = Scale(ProjectRows(lpJ->mRows[12], lpJ->mRows[16], lpJ->mRows[20], lvLam),
                                 lfInvTsSq);
        const V3 lvTorque = Scale(
            Add(Add(Add(Scale(Xyz(lMat.xAxis), lvAng.x), Scale(Xyz(lMat.yAxis), lvAng.y)),
                    Scale(Xyz(lMat.zAxis), lvAng.z)),
                ProjectRows(lpJ->mRows[14], lpJ->mRows[18], lpJ->mRows[22], lvAng)),
            lfInvTsSq);

        // ⚠️ w lanes: the console's full-row stores fold the record's w detritus (for a
        // DRIVE record mRows[12].w is builder-untouched heap) into the emitted w -- dead on
        // the console, NONDETERMINISTIC here; the host emits 0 instead (consumer reads xyz).
        lpOut->mForce.x  = lvForce.x;  lpOut->mForce.y  = lvForce.y;  lpOut->mForce.z  = lvForce.z;  lpOut->mForce.w  = 0.0f;
        lpOut->mTorque.x = lvTorque.x; lpOut->mTorque.y = lvTorque.y; lpOut->mTorque.z = lvTorque.z; lpOut->mTorque.w = 0.0f;
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
