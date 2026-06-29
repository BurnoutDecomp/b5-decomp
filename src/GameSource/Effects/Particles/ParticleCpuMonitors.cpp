// ============================================================================
// GameSource/Effects/Particles/ParticleCpuMonitors.cpp
//
// BrnParticle::ParticleCpuMonitors::Construct - register the 16 CPU perf
// monitors for the particle frame. Reconstructed from X360 0x822812D0
// (ARTIST build) + the DecFIGS DWARF (ParticleModule.cpp struct
// ParticleCpuMonitors). The X360 build inlined the private AddMonitor helper
// at every call site; it is restored here as the helper the DWARF declares.
//
// Grounded facts from the ASM (0x822812D0):
//   * SnPrintf buffer length r4 = 0x20 (32) - the perfmon max string length.
//   * Each name is formatted "%s%s%s" = <prefix><separator><region name>,
//     where the separator is ": " when the prefix is non-empty and "" (empty)
//     when *prefix == 0 (the lbz r11,0(r31) / cmplwi test on the prefix byte).
//   * AddMonitor is called as (name, r4=0x14=20, r5=0=min, f1=100.0=budget,
//     r6=parent, r7=0=flags). Colour 20, min 0, budget 100.0 and flags 0 are
//     immediates / the loaded float (flt_820049E0 = 100.0).
//   * The 16 region names are the rodata string literals, in order.
// ============================================================================

#include "GameSource/Effects/Particles/ParticleCpuMonitors.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"            // CgsCore::SnPrintf
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::AddMonitor

namespace BrnParticle
{
    // Colour token + budget the X360 stamps on every particle monitor
    // (r4 = 0x14, the loaded f32 flt_820049E0 = 100.0).
    static const s32 KI_PARTICLE_MONITOR_COLOUR = 20;
    static const f32 KF_PARTICLE_MONITOR_BUDGET = 100.0f;

    // FLAG (not recovered): the X360 passes r6 (liParentHandle) without ever
    // loading it before the AddMonitor call - the register is left holding the
    // prefix pointer from the preceding SnPrintf, i.e. it is a stale,
    // uncontrolled value rather than a deliberate handle. The conventional
    // "no parent" sentinel is used here; this is the one value below that is
    // NOT grounded in a stored immediate.
    static const s32 KI_PARTICLE_MONITOR_NO_PARENT = -1;

    s32 ParticleCpuMonitors::AddMonitor(const char* lpcName, const char* lpcPrefix)
    {
        // ": " when a prefix is present, empty string otherwise (ASM tests the
        // first byte of the prefix; unk_820046A7 is the empty-string literal).
        const char* lpcSeparator = (*lpcPrefix != '\0') ? ": " : "";

        char lacName[40];
        CgsCore::SnPrintf(lacName, CgsDev::KI_PERFMONCPU_MAXSTRINGLENGTH, "%s%s%s",
                          lpcPrefix, lpcSeparator, lpcName);

        return CgsDev::PerfMonCpu::AddMonitor(lacName,
                                              KI_PARTICLE_MONITOR_COLOUR,
                                              0,                              // minimum
                                              KF_PARTICLE_MONITOR_BUDGET,
                                              KI_PARTICLE_MONITOR_NO_PARENT,
                                              0);                             // flags
    }

    void ParticleCpuMonitors::Construct(const char* lpcPrefix)
    {
        miPreRenderUpdate            = AddMonitor("PreRenderUpdate", lpcPrefix);
        miDispatchThreadUpdate       = AddMonitor("DispatchThreadUpdate", lpcPrefix);
        miSpawnSpark                 = AddMonitor("    SpawnSpark", lpcPrefix);
        miBeginParticleRenderJob     = AddMonitor("BeginParticleRenderJob", lpcPrefix);
        miBuildLionVertexBuffers     = AddMonitor("BuildLionVertexBuffers", lpcPrefix);
        miLionUpdate                 = AddMonitor("    LionUpdate", lpcPrefix);
        miLionRender                 = AddMonitor("    LionRender", lpcPrefix);
        miWaitOnParticleRenderJob    = AddMonitor("WaitOnParticleRenderJob", lpcPrefix);
        miRenderFullResParticles     = AddMonitor("RenderFullResParticles", lpcPrefix);
        miTrailsRender               = AddMonitor("    TrailsRender", lpcPrefix);
        miWaitOnDebrisUpdateJob      = AddMonitor("    WaitOnDebrisJob", lpcPrefix);
        miDebrisRender               = AddMonitor("    DebrisRender", lpcPrefix);
        miSparksDispatch             = AddMonitor("    SparksDispatch", lpcPrefix);
        miRenderQuarterResParticles  = AddMonitor("RenderQuarterResPtcls", lpcPrefix);
        miSimpleParticlesDispatch    = AddMonitor("    SimplePtclsDispatch", lpcPrefix);
        miLionDispatch               = AddMonitor("    LionDispatch", lpcPrefix);
    }
}
