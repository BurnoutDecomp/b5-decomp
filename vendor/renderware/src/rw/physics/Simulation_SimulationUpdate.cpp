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
//
// ⭐ 2026-08-04 (task #138) -- THE EIGHT ARE NOW SIZED, and one "hole" turned out not to be
// one. Suggested order (cheapest first, and Osiris is on the joints-only path a single car
// with wheel joints actually takes):
//     Osiris_Pipeline        179 insn   ⚠️ ABSENT FROM .ida-exports; body recovered headless
//                                       off the .i64 -- range 0x82BC2680..0x82BC294C, sole
//                                       xref SimulationUpdate @0x82BC6BE8. "No export" is
//                                       NOT "no body" (see [[ida-export-set-has-holes]]).
//     Isis_Pipeline          184 insn
//     Anubis_Pipeline        192 insn
//     SpyJointJacobians       97 insn / SpyContactJacobians 107 / SpyDriveJacobians 193
//     ContactBatchBuild      343 insn
//     Horus_Pipeline         510 insn
//   ------------------------------------
//     1,805 instructions total for the eight.
//
// ⚠️⚠️ AND MOUNTING THIS TU STILL DOES NOT STEP A CAR. Landing all eight makes
// SimulationUpdate callable; nothing calls it. Its caller PhysicsSimulationModule::Update
// @0x828A74D0 is not declared, and ITS caller BrnPhysics::PhysicsModule::Update @0x825B0640
// is a link stub whose own depth-1 closure measures ~15,000 instructions. Full measured map
// in GameShared/GameClasses/Physics/CgsPhysicsSimulationModule.h.
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
//     2        -> Osiris   (joints only)
//     4        -> Isis     (drives only)
//     3, 5-7   -> Horus    (any MIX)
//   ⚠️ CORRECTION 2026-08-05: this comment used to claim "2, 3 -> Osiris"; the CODE below
//   (`else if (luPipeline < 3u)`) always sent 3 to Horus, and the Xbox One SimulationUpdate
//   (sub_1409B7240, located this wave) confirms the code: its selector chain sends only
//   v==2 to Osiris (sub_1409B5E80) and 3 to Horus (sub_1409B3CD0). The comment was the bug.
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
