// =====================================================================================
// rw::physics::Simulation -- definition home for the RenderWare physics simulation object.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is authoritative. No
// Feb-2007 reference source and no DecFIGS DWARF exist for this TU.
//
// This TU has 22 X360 functions. The three that touch only the Simulation object's own
// members (or nothing but a serialised descriptor) are reconstructed here faithfully:
//     GetResourceDescriptor  @ 0x82BC5090
//     SetWorkspace           @ 0x82BC54D8
//     SimulationUpdate       @ 0x82BC6B40
//
// The remaining 19 are BLOCKED (left declared-only in simulation.h) for honest reasons:
//
//   * RigidBody-list splice/walk -- ActivateRigidBody, FreezeRigidBody, RemoveRigidBody,
//     BatchIntegrator: these read the console RigidBody's intrusive node fields (next @+0x2C,
//     prev @+0x3C, state @+0x8C) and pointer links at +0x1C/+0x4C. Those offsets are named as
//     the pose-vector FLOAT LANES (mVel.w / mUp.w / ...) in the committed rw/physics/rigidbody.h,
//     so naming them faithfully would require retyping that committed home -- exactly why its
//     sibling RigidBody::DynamicUpdate is also left declared-only (see RigidBody.cpp).
//
//   * Joint / Drive node editing -- AddJoint, AddDrive, RemoveJoint, RemoveDrive: the Joint and
//     Drive node types (fields @+0x00..+0x1C) are un-homed; no faithful layout exists to name.
//
//   * Jacobian batch builders needing un-homed collaborators -- JointBatchBuild (calls
//     rw::physics::JointJacobian::Build), DriveBatchBuild (rw::physics::DriveJacobian::Build):
//     only rw::physics::Jacobian_RQD is homed; JointJacobian / DriveJacobian are not, and both
//     also walk the un-homed Joint/Drive nodes plus the RigidBody state @+0x8C.
//
//   * VMX solver / spy functions with guessed .rdata -- AddRigidBody, ContactBatchBuild,
//     Anubis_Pipeline, Horus_Pipeline, Isis_Pipeline, SpyContactJacobians, SpyDriveJacobians,
//     SpyJointJacobians: heavy VMX (vperm/vsel/vmaddfp) driven by unrecovered .rdata permute /
//     mask tables (unk_82CDA3xx, unk_8327F1xx, unk_82181650) that are NOT in the export. Guessing
//     a permute table mis-routes lanes at runtime, so these are not reconstructed.
//
//   * Initialize @ 0x82BC5158 -- builds the RigidBody/Joint/Drive intrusive node graph inside
//     the workspace (writes the same conflicting RigidBody node offsets as above), and seeds the
//     solver config vector from unrecovered float rodata (flt_82001CC0/8202CF3C/82001C98).
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
//   contact buffer base = lpWorkspace
//   joint   buffer base = lpWorkspace + 384 * liMaxContacts
//   drive   buffer base = joint base  + 384 * liMaxJoints
// -------------------------------------------------------------------------------------
bool Simulation::SetWorkspace(void* lpWorkspace, int liMaxContacts, int liMaxJoints, int liMaxDrives)
{
    char* lpContactBase = static_cast<char*>(lpWorkspace);
    char* lpJointBase   = lpContactBase + KU_JACOBIAN_STRIDE * static_cast<u32>(liMaxContacts);
    char* lpDriveBase   = lpJointBase   + KU_JACOBIAN_STRIDE * static_cast<u32>(liMaxJoints);

    miContactJacobianCount    = 0u;   // +0x64
    miContactJacobianOverflow = 0u;   // +0x68
    miJointJacobianCount      = 0u;   // +0x6C
    miJointJacobianOverflow   = 0u;   // +0x70
    miDriveJacobianCount      = 0u;   // +0x74
    miDriveJacobianOverflow   = 0u;   // +0x78

    mpContactJacobians = lpContactBase;   // +0x14
    mpDriveJacobians   = lpDriveBase;     // +0x10
    mpJointJacobians   = lpJointBase;     // +0x18

    miMaxContacts = static_cast<u32>(liMaxContacts);   // +0x80
    miMaxJoints   = static_cast<u32>(liMaxJoints);     // +0x84
    miMaxDrives   = static_cast<u32>(liMaxDrives);     // +0x7C

    miContactJacobianStride = KU_JACOBIAN_STRIDE;      // +0x5C
    miJointJacobianStride   = KU_JACOBIAN_STRIDE;      // +0x60
    return true;
}

// -------------------------------------------------------------------------------------
// Simulation::SimulationUpdate @ 0x82BC6B40
//
// One solver tick. Early-out when no bodies are active. Build the three jacobian batches,
// pick the solver pipeline from which batches are non-empty, integrate, then run any
// enabled jacobian spies.
//
//   pipeline selector: bit0 = contacts present, bit1 = joints, bit2 = drives
//     1        -> Anubis   (contacts only)
//     2        -> Osiris   (contacts + joints)
//     4        -> Isis     (drives only)
//     3,5,6,7  -> Horus    (mixed)
// -------------------------------------------------------------------------------------
bool Simulation::SimulationUpdate(float lfTimeStep)
{
    if (miActiveBodyCount == 0u)
        return false;

    mfTimeStep = lfTimeStep;                 // stfs f1, +0xA0

    ContactBatchBuild();
    JointBatchBuild();
    DriveBatchBuild();

    u32 luPipeline = (miContactJacobianCount != 0u) ? 1u : 0u;
    if (miJointJacobianCount != 0u)
        luPipeline |= 2u;
    if (miDriveJacobianCount != 0u)
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

    if (miSpyFlags != 0u)
    {
        if (miJointJacobianCount != 0u && (miSpyFlags & 1u) != 0u)
            SpyJointJacobians();
        if (miDriveJacobianCount != 0u && (miSpyFlags & 2u) != 0u)
            SpyDriveJacobians();
        if (miContactJacobianCount != 0u && (miSpyFlags & 4u) != 0u)
            SpyContactJacobians();
    }

    return true;
}

} // namespace physics
} // namespace rw
