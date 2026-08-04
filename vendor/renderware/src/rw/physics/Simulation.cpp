// =====================================================================================
// rw::physics::Simulation -- definition home for the RenderWare physics simulation object.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is authoritative.
//
// ⚠️ CORRECTION 2026-08-04: the banner that used to stand here said "No Feb-2007 reference
// source and no DecFIGS DWARF exist for this TU". That was FALSE -- see the correction block
// in rw/physics/simulation.h. Names below are DWARF-authoritative.
//
// This TU has 22 X360 functions. EIGHT are reconstructed here:
//     GetResourceDescriptor  @ 0x82BC5090
//     SetWorkspace           @ 0x82BC54D8
//     BatchIntegrator        @ 0x82BC2FC8
//     ActivateRigidBody      @ 0x82BC29E8
//     FreezeRigidBody        @ 0x82BC2A58
//     RemoveRigidBody        @ 0x82BC2950
//     JointBatchBuild        @ 0x82BC6A30
//     DriveBatchBuild        @ 0x82BC6AB8
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
// STILL BLOCKED, honestly (and this is why SimulationUpdate sits in its own unmounted TU):
//   * ContactBatchBuild and the four solver pipelines (Anubis/Osiris/Isis/Horus) -- heavy
//     VMX over the 272-byte contact batch record, not reconstructed.
//   * The three Spy* dumps -- debug-only, not reconstructed.
//   * AddRigidBody @0x82BC3318 (669 VMX instructions) and the Add/Remove Joint/Drive quartet.
// =====================================================================================

#include "rw/physics/simulation.h"

#include "vendor/renderware/physics/Jacobian.hpp"   // the 384-byte record + JacobianStride()
#include "vendor/renderware/physics/Joint.hpp"
#include "vendor/renderware/physics/Drive.hpp"
#include "vendor/renderware/physics/JointFrames.hpp" // rw::math::vpu::QuaternionFromMatrix33 (AddRigidBody)

#include <string.h>   // memset -- the `vspltisb v0,0` + four stvx128 block-clear
#include <cfloat>     // FLT_MAX -- AddRigidBody's mKine seed (X360 flt_821815B0 = 0x7F7FFFFF)

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

} // namespace physics
} // namespace rw
