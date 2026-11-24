// =================================================================================================
// GameSource/Effects/Particles/LionPerfMon.cpp   (X360 ARTIST)
//
//   LionPerfMon::Construct  @0x82279E88
//
// The Lion particle runtime's CPU perf-monitor set: twenty-three `CgsDev::PerfMonCpu::AddMonitor`
// registrations whose returned handles are the ints the Lion bodies bracket themselves with. The
// object is a single .bss instance at 0x82FAB638 -- which is why ParticleEmitter.cpp /
// ParticleRandomSeed.cpp name their handles "LionPerfMon + 0xNN".
//
// ⭐⭐ REWRITTEN 2026-09-06 (effects-path 1:1 audit, class 4: "an invented arm"). The previous
// text of this file was built on the HEX-RAYS SIX-ARGUMENT ARTEFACT and carried thirteen invented
// member names. Every correction below is one instruction in the console body; nothing here is
// inferred from a sibling.
//
//   1. THE SIGNATURE. The file re-declared `class CgsDev::PerfMonCpu` locally with
//          static int AddMonitor(const char*, int liColour, int liMin, double lfMax,
//                                int liParent, int liFlags);
//      and called it `AddMonitor(name, 21, 0, 100.0, liParent, 0)`. There is no such overload.
//      The real header has only the 5-arg form: on the PPC ABI an f32/f64 argument consumes
//      an INTEGER register slot as well as an FP register, so the real five-argument call
//      `AddMonitor(name, page, min, budget, tag)` is emitted as `r3, r4, r5, f1, r7` and **r6 is
//      never written** -- Hex-Rays sees the hole and invents a sixth parameter for it. This body is
//      twenty-three verbatim instances of exactly that emission:
//          lis/addi r3,<name> ; li r4,0x15 ; li r5,0 ; fmr f1,f31 ; li r7,0 ; bl AddMonitor
//      (f31 is loaded once at 0x82279EB8 from flt_820049E0 == 100.0). So the console's arguments
//      are page 21, min 0, budget 100.0f, tag 0 -- and the FIFTH is a literal 0, not `liParent`.
//      The local re-declaration also spelled `class` where the real home spells `struct`, the exact
//      kind-mismatch CgsPerfMonCpu.h's struct-kind note records as having produced an LNK2019. Both are
//      gone: this TU now includes the real header, as AGENTS.md's "reconstruct includes, don't fake
//      them" requires.
//
//   2. CONSTRUCT TAKES NO ARGUMENTS. The prologue is `mflr / stw / std r31 / stfd f31 / stwu /
//      mr r31, r3` -- r3 is `this`, and r4 / r5 are never read anywhere in the body. The old
//      `Construct(int liParent, int liUnused0, int liUnused1)` was three invented parameters, and
//      the first of them was being passed on to AddMonitor.
//
//   3. THIRTEEN MEMBER NAMES WERE INVENTED. Each handle's slot is named by the `stw r10, OFF(r31)`
//      that stores it, and each monitor's identity by the string its own call passes. Pairing the
//      two (the store for call N is emitted inside call N+1's argument setup) gives the console's
//      own map, reproduced verbatim in the member list below. From +0x1C onward the old names --
//      "ParticleBuild", "EmitterSortedRender", "EmitterSortedBuild", "EmitterBucketSort",
//      "EmitterBucketRender", "EmitterSub", "EmitterSubRender", "EmitterSubUpdate",
//      "EmitterSubGenerate", "EmitterSubParticleBuild", "ParticleManagerUpdate",
//      "ParticleManagerRender", "ParticleManagerWait" -- name monitors this build does not
//      register; not one of those strings appears in the body. The real ones are the eleven
//      *Behaviour* monitors plus SimulateLocalParticles and the two RandomSeed monitors.
//      ⭐ ParticleEmitter.cpp's own eleven handle names were derived from their READERS and are
//      independently CORRECT against this map (0x82FAB660 == "BaseColourVarianceBehaviour",
//      0x82FAB670 == "AlphaFadeBehaviour", 0x82FAB684 == "ParticleBehaviourRADIAL", ...); it was
//      only this file's member order that disagreed with the binary.
//
//   4. A MONITOR WAS MISSING. The console registers TWENTY-THREE and the last store is
//      `stw r3, 0x58(r31)` @0x8227A1D4 for "    RandomSeed::Update" (the four leading spaces are in
//      the console's own string literal). The old member list had twenty-two and ended at +0x54.
//
// =================================================================================================

#include "GameSource/Effects/Particles/LionPerfMon.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // CgsDev::PerfMonCpu

#include <cstddef>

// The one budget every monitor in this set is registered with: f31 is loaded once at 0x82279EB8
// from flt_820049E0 and re-used by all twenty-three calls.
static const f32 KF_LION_PERFMON_BUDGET_MS = 100.0f;   // flt_820049E0

LionPerfMon gLionPerfMon;

CGS_STATIC_ASSERT((offsetof(LionPerfMon, miRandomSeedBuild) == 0x54), "Lion build monitor offset");
CGS_STATIC_ASSERT((offsetof(LionPerfMon, miRandomSeedUpdate) == 0x58), "Lion update monitor offset");
CGS_STATIC_ASSERT((sizeof(LionPerfMon) == 0x5C), "ARTIST Lion monitor set size");

// X360 0x82279E88. Twenty-three registrations IN THE CONSOLE'S CALL ORDER -- which is not the
// member order. AddMonitor allocates handles in call order.
void LionPerfMon::Construct()
{
    const f32 lfBudget = KF_LION_PERFMON_BUDGET_MS;
    const CgsDev::PerfMonCpuPage lePage = CgsDev::E_PMP_21;   // li r4, 0x15

    miLionRender                   = CgsDev::PerfMonCpu::AddMonitor("Render",                      lePage, false, lfBudget, false);
    miEmitterRender                = CgsDev::PerfMonCpu::AddMonitor("EmitterRender",               lePage, false, lfBudget, false);
    miEmitterCubeRender            = CgsDev::PerfMonCpu::AddMonitor("EmitterCubeRender",           lePage, false, lfBudget, false);
    miParticleEmitterBlend         = CgsDev::PerfMonCpu::AddMonitor("EmitterBlend",                lePage, false, lfBudget, false);
    miParticleEmitterUpdate        = CgsDev::PerfMonCpu::AddMonitor("EmitterUpdate",               lePage, false, lfBudget, false);
    miParticleEmitterGenerate      = CgsDev::PerfMonCpu::AddMonitor("EmitterGenerate",             lePage, false, lfBudget, false);
    miParticleEmitterParticleBuild = CgsDev::PerfMonCpu::AddMonitor("EmitterParticleBuild",        lePage, false, lfBudget, false);
    miSimulateMatrixParticles      = CgsDev::PerfMonCpu::AddMonitor("SimulateMatrixParticles",     lePage, false, lfBudget, false);
    miSimulateVectorParticles      = CgsDev::PerfMonCpu::AddMonitor("SimulateVectorParticles",     lePage, false, lfBudget, false);
    miSimulateLocalParticles       = CgsDev::PerfMonCpu::AddMonitor("SimulateLocalParticles",      lePage, false, lfBudget, false);
    miBaseColourVarianceBehaviour = CgsDev::PerfMonCpu::AddMonitor("BaseColourVarianceBehaviour", lePage, false, lfBudget, false);
    miColourStepsBehaviour         = CgsDev::PerfMonCpu::AddMonitor("ColourStepsBehaviour",        lePage, false, lfBudget, false);
    miWaveRgbBehaviour             = CgsDev::PerfMonCpu::AddMonitor("WaveRgbBehaviour",            lePage, false, lfBudget, false);
    miWaveBehaviour                = CgsDev::PerfMonCpu::AddMonitor("WaveBehaviour",               lePage, false, lfBudget, false);
    miAlphaFadeBehaviour           = CgsDev::PerfMonCpu::AddMonitor("AlphaFadeBehaviour",          lePage, false, lfBudget, false);
    miRotationBehaviour            = CgsDev::PerfMonCpu::AddMonitor("RotationBehaviour",           lePage, false, lfBudget, false);
    miSizeBehaviour                = CgsDev::PerfMonCpu::AddMonitor("SizeBehaviour",               lePage, false, lfBudget, false);
    miDragBehaviour                = CgsDev::PerfMonCpu::AddMonitor("DragBehaviour",               lePage, false, lfBudget, false);
    miMultiFrameBehaviour          = CgsDev::PerfMonCpu::AddMonitor("MultiFrameBehaviour",         lePage, false, lfBudget, false);
    miParticleBehaviourRADIAL      = CgsDev::PerfMonCpu::AddMonitor("ParticleBehaviourRADIAL",     lePage, false, lfBudget, false);
    miParticleBehaviourOFFSETROT   = CgsDev::PerfMonCpu::AddMonitor("ParticleBehaviourOFFSETROT",  lePage, false, lfBudget, false);
    miRandomSeedBuild              = CgsDev::PerfMonCpu::AddMonitor("RandomSeed::Build",           lePage, false, lfBudget, false);
    miRandomSeedUpdate             = CgsDev::PerfMonCpu::AddMonitor("    RandomSeed::Update",      lePage, false, lfBudget, false);

}
