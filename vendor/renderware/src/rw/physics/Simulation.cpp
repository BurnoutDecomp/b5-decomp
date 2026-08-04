// =====================================================================================
// rw::physics::Simulation -- definition home for the RenderWare physics simulation object.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is authoritative.
//
// ⚠️ CORRECTION 2026-08-04: the banner that used to stand here said "No Feb-2007 reference
// source and no DecFIGS DWARF exist for this TU". That was FALSE -- see the correction block
// in rw/physics/simulation.h. Names below are DWARF-authoritative.
//
// This TU has 22 X360 functions. SEVEN are reconstructed here:
//     GetResourceDescriptor  @ 0x82BC5090
//     SetWorkspace           @ 0x82BC54D8
//     BatchIntegrator        @ 0x82BC2FC8
//     ActivateRigidBody      @ 0x82BC29E8
//     FreezeRigidBody        @ 0x82BC2A58
//     RemoveRigidBody        @ 0x82BC2950
//     (JointBatchBuild / DriveBatchBuild live with the jacobian builders they call.)
//
// ⭐ THE BLOCK NOTE THIS FILE USED TO CARRY IS RETIRED. It said the list splice/walk
// functions were blocked because "these read the console RigidBody's intrusive node fields
// (next @+0x2C, prev @+0x3C, state @+0x8C) ... named as the pose-vector FLOAT LANES in the
// committed rw/physics/rigidbody.h, so naming them faithfully would require retyping that
// committed home". That retyping is exactly what happened: mRight / mLeft / mState / mCool
// are real named members now, and the four functions are transcribed instruction for
// instruction below.
//
// STILL BLOCKED, honestly (and this is why SimulationUpdate sits in its own unmounted TU):
//   * ContactBatchBuild and the four solver pipelines (Anubis/Osiris/Isis/Horus) -- heavy
//     VMX over the 272-byte contact batch record, not reconstructed.
//   * The three Spy* dumps -- debug-only, not reconstructed.
//   * AddRigidBody @0x82BC3318 (669 VMX instructions) and the Add/Remove Joint/Drive quartet.
//   * Initialize @0x82BC5158 -- builds the whole intrusive node graph inside the workspace.
// =====================================================================================

#include "rw/physics/simulation.h"

namespace rw
{
namespace physics
{

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

    // size = 240 * luCountA + 32 * (luCountB + luCountC + 32)
    const u32 luSize =
        0xF0u * static_cast<u32>(luCountA)
        + 0x20u * static_cast<u32>(luCountB + luCountC + 32);

    // entry[0] = { m_size = size, m_alignment = 64 }
    lpEntries[0].m_size      = luSize;
    lpEntries[0].m_alignment = 64u;
    return lpResult;
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
    char* lpJointBase   = static_cast<char*>(lpWorkspace);
    char* lpDriveBase   = lpJointBase + KU_JACOBIAN_STRIDE * static_cast<u32>(liMaxJoints);
    char* lpContactBase = lpDriveBase + KU_JACOBIAN_STRIDE * static_cast<u32>(liMaxDrives);

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

    m_JT_Stride = KU_JACOBIAN_STRIDE;   // +0x5C
    m_DR_Stride = KU_JACOBIAN_STRIDE;   // +0x60
    return true;
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

} // namespace physics
} // namespace rw
