#pragma once

// =====================================================================================
// rw::physics::Simulation -- the EATech RenderWare rigid-body physics simulation object.
// It owns the per-frame constraint solver: intrusive free/active/frozen/disabled lists of
// rigid bodies, joints and drives, plus the three jacobian scratch buffers carved out of
// the SimulationWorkspace. SimulationUpdate() drives one solver tick (contact/joint/drive
// batch build -> a solver pipeline -> the integrator -> optional jacobian "spy" dumps).
//
// EATech RenderWare physics. Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative. No Feb-2007 reference source and no DecFIGS DWARF exist for this TU.
//
// LAYOUT: the member set below is recovered from the union of the TU's functions' offset
// accesses (the X360 asm is authoritative for placement). Every member is accessed BY NAME
// -- the X360 byte offset in the trailing comment is decode documentation only; the PC/x64
// layout naturally differs (8-byte pointers), which is fine because nothing here relies on
// byte offsets (semantic parity, not byte matching).
//
// The rigid-body / joint / drive list nodes are threaded through this object by pointer but
// their concrete layouts are NOT modelled here: the node splice/walk functions of this TU
// (Add*/Remove*/Activate/Freeze/Initialize/*BatchBuild/BatchIntegrator) read the console
// RigidBody at offsets (+0x1C/+0x2C/+0x3C/+0x4C as POINTERS, state @+0x8C) that conflict
// with the committed rw/physics/rigidbody.h pose-vector layout, and the Joint/Drive node
// types are un-homed -- so those bodies are intentionally left un-reconstructed and the list
// heads are held as opaque `void*`. See Simulation.cpp for the block rationale.
// =====================================================================================

#include "types.hpp"                 // u32 / f32
#include "rw/rwcore_structs.h"       // rw::BaseResourceDescriptors<5>

namespace rw
{
namespace physics
{

class Simulation
{
public:
    // The stride, in bytes, of one solver jacobian block (contact/joint/drive share it).
    static const u32 KU_JACOBIAN_STRIDE = 0x180u;   // 384

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
    bool SetWorkspace(void* lpWorkspace, int liMaxContacts, int liMaxJoints, int liMaxDrives);

    // SimulationUpdate @ 0x82BC6B40 -- run one solver tick for the given time step. No-ops
    // (returns false) when no bodies are active. Builds the contact/joint/drive jacobian
    // batches, dispatches the matching solver pipeline, integrates, then dumps any enabled
    // jacobian spies. Returns true when a step ran.
    bool SimulationUpdate(float lfTimeStep);

    // -------------------------------------------------------------------------------------
    // Solver stages driven by SimulationUpdate. Declared here (the class owns them) and
    // defined by their own TUs; several are still un-reconstructed (heavy VMX solver math /
    // un-homed jacobian collaborators / guessed .rdata permute tables) -- see Simulation.cpp.
    // Return values are unused by SimulationUpdate, so they are declared `void`; a future
    // faithful body may widen the return type.
    // -------------------------------------------------------------------------------------
    void ContactBatchBuild();     // @ 0x82BC14C0
    void JointBatchBuild();       // @ 0x82BC6A30
    void DriveBatchBuild();       // @ 0x82BC6AB8
    void Anubis_Pipeline();       // @ 0x82BC11C0  (contacts only)
    void Osiris_Pipeline();       // @ (external)  (contacts + joints)
    void Isis_Pipeline();         // @ 0x82BC2218  (drives only)
    void Horus_Pipeline();        // @ 0x82BC1A20  (mixed)
    void BatchIntegrator();       // @ 0x82BC2FC8
    void SpyJointJacobians();     // @ 0x82BC24F8
    void SpyDriveJacobians();     // @ 0x82BC3010
    void SpyContactJacobians();   // @ 0x82BC4138

    // -------------------------------------------------------------------------------------
    // Rigid-body / joint / drive membership editing and the frame constructor. Declared for
    // the class shape; NOT defined yet -- each reads the console RigidBody/Joint/Drive node
    // layout that has no faithful home (see the layout note above), so the node argument is
    // held opaque. A future wave that homes those node types will body these.
    // -------------------------------------------------------------------------------------
    void  ActivateRigidBody(void* lpBody);   // @ 0x82BC29E8
    void  FreezeRigidBody(void* lpBody);     // @ 0x82BC2A58
    void  RemoveRigidBody(void* lpBody);     // @ 0x82BC2950
    void* AddRigidBody(void* lpDesc, int liState, int liArg);  // @ 0x82BC3318 (VMX)
    void* AddJoint(int liA, int liB, void* lpBodyA, void* lpBodyB);  // @ 0x82BC3D90
    void* AddDrive(int liA, int liB, void* lpBodyA, void* lpBodyB);  // @ 0x82BC3E28
    void  RemoveJoint(void* lpJoint);        // @ 0x82BC2AC8
    void  RemoveDrive(void* lpDrive);        // @ 0x82BC2B20
    int   Initialize(void** lpMemory);       // @ 0x82BC5158

private:
    // Intrusive list heads (opaque node pointers -- node layouts are un-homed).
    void* mpEnabledBodies;          // X360 +0x00
    void* mpJointList;              // X360 +0x04
    void* mpDriveList;              // X360 +0x08
    void* mpBodyPoses;              // X360 +0x0C  (pose-block base)
    void* mpDriveJacobians;         // X360 +0x10
    void* mpContactJacobians;       // X360 +0x14
    void* mpJointJacobians;         // X360 +0x18
    void* mpFreeBodies;             // X360 +0x1C
    void* mpDisabledBodies;         // X360 +0x20
    void* mpFrozenBodies;           // X360 +0x24
    void* mpActiveBodies;           // X360 +0x28
    void* mpFreeJoints;             // X360 +0x2C
    void* mpActiveJoints;           // X360 +0x30
    void* mpFreeDrives;             // X360 +0x34
    void* mpActiveDrives;           // X360 +0x38

    u32   miFreeBodyCount;          // X360 +0x3C
    u32   miDisabledBodyCount;      // X360 +0x40
    u32   miFrozenBodyCount;        // X360 +0x44
    u32   miActiveBodyCount;        // X360 +0x48
    u32   miFreeJointCount;         // X360 +0x4C
    u32   miActiveJointCount;       // X360 +0x50
    u32   miFreeDriveCount;         // X360 +0x54
    u32   miActiveDriveCount;       // X360 +0x58

    u32   miContactJacobianStride;  // X360 +0x5C  (= KU_JACOBIAN_STRIDE)
    u32   miJointJacobianStride;    // X360 +0x60  (= KU_JACOBIAN_STRIDE)
    u32   miContactJacobianCount;   // X360 +0x64  (built this frame)
    u32   miContactJacobianOverflow;// X360 +0x68
    u32   miJointJacobianCount;     // X360 +0x6C  (built this frame)
    u32   miJointJacobianOverflow;  // X360 +0x70
    u32   miDriveJacobianCount;     // X360 +0x74  (built this frame)
    u32   miDriveJacobianOverflow;  // X360 +0x78

    u32   miMaxDrives;              // X360 +0x7C
    u32   miMaxContacts;            // X360 +0x80
    u32   miMaxJoints;              // X360 +0x84
    u32   miNumBodies;              // X360 +0x88
    u32   muUnknown8C;              // X360 +0x8C

    f32   mv4Config[4];             // X360 +0x90  (16-byte solver config vector)
    f32   mfTimeStep;               // X360 +0xA0  (current step, set by SimulationUpdate)
    u32   miMaxIterations;          // X360 +0xA4  (default 30)
    f32   mfConfigA8;               // X360 +0xA8
    u32   miConfigAC;               // X360 +0xAC  (default 50)
    u32   miSpyFlags;               // X360 +0xB0  (bit0 joints, bit1 drives, bit2 contacts)
};

} // namespace physics
} // namespace rw
