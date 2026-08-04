// =====================================================================================
// rw::physics::Simulation::SimulationUpdate @ 0x82BC6B40 -- SPLIT OUT of Simulation.cpp
// on 2026-08-04. BUILD-MECHANICS SPLIT ONLY: the body below is the one that used to sit at
// the tail of that file, and its declared home is still rw/physics/simulation.h.
//
// ⛔ THIS TU IS DELIBERATELY **NOT MOUNTED** in tools/build/build_game_exe.bat, and it is
// the ONLY thing keeping the rest of the Simulation TU out of the link. SimulationUpdate
// calls eleven solver stages; eight of them are not reconstructed:
//     ContactBatchBuild, Anubis_Pipeline, Osiris_Pipeline, Isis_Pipeline, Horus_Pipeline,
//     SpyJointJacobians, SpyDriveJacobians, SpyContactJacobians.
// Mounting this file is therefore an immediate 8x LNK2019, and `/OPT:REF` does not suppress
// those (it is a *reference* the linker must resolve, not a dead COMDAT it may drop).
// The other three stages -- BatchIntegrator, JointBatchBuild, DriveBatchBuild -- ARE bodied
// and ARE mounted, so this file is the whole of the remaining gap.
//
// ⚠️ DO NOT "unblock" this by stubbing the eight. A stub here is the exact silent-drop shape
// this project keeps getting bitten by: the solver would run, build no constraints, emit no
// impulses, and every car would fall through the world with nothing asserting.
//
// TO MOUNT: reconstruct ContactBatchBuild (343 X360 insn over the 272-byte contact record)
// and the four pipelines, then move this body back into Simulation.cpp and delete this TU.
// =====================================================================================

#include "rw/physics/simulation.h"

namespace rw
{
namespace physics
{

// -------------------------------------------------------------------------------------
// Simulation::SimulationUpdate @ 0x82BC6B40   (79 instructions, no VMX)
//
// One solver tick. Early-out when no bodies are active. Build the three jacobian batches,
// pick the solver pipeline from which batches are non-empty, integrate, then run any
// enabled jacobian spies.
//
// SIGNATURE: the time step arrives in f1 and is stored with `stfs` (0x82BC6B6C), i.e. it is
// a FLOAT, not the `double` Hex-Rays prints. Recovered from the asm; the Xbox One build
// agrees (`vmovss dword ptr [rcx+0E0h], xmm1`).
//
//   pipeline selector: bit0 = contacts present, bit1 = joints, bit2 = drives
//     1        -> Anubis   (contacts only)
//     2, 3     -> Osiris   (joints, or joints + contacts)
//     4        -> Isis     (drives only)
//     5, 6, 7  -> Horus    (anything mixed with drives)
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
