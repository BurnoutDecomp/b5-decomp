#ifndef GAMESOURCE_EFFECTS_PARTICLES_PARTICLECPUMONITORS_H
#define GAMESOURCE_EFFECTS_PARTICLES_PARTICLECPUMONITORS_H

#include "types.hpp"

// ============================================================================
// GameSource/Effects/Particles/ParticleCpuMonitors.h
//
// BrnParticle::ParticleCpuMonitors - the set of CPU perf-monitor handles the
// particle module owns. Each int32_t member is a handle returned by
// CgsDev::PerfMonCpu::AddMonitor, naming one timed region of the particle
// frame (PreRenderUpdate, the dispatch-thread update, the per-stage render
// jobs, etc). The debug perfmon overlay reads these handles to draw the
// per-region bars.
//
// DWARF home: ParticleModule.cpp:160 (DecFIGS dwarfdump, struct
// BrnParticle::ParticleCpuMonitors). Member order + names + types are taken
// verbatim from that DWARF entry. Hoisted into its own header (mirrored at the
// DWARF source path) so the larger ParticleModule reconstruction can #include
// it without a forward redefinition.
// ============================================================================

namespace BrnParticle
{
    struct ParticleCpuMonitors
    {
        // 16 monitor handles, in the order Construct registers them
        // (DWARF ParticleModule.cpp:186..201).
        s32 miPreRenderUpdate;
        s32 miDispatchThreadUpdate;
        s32 miSpawnSpark;
        s32 miBeginParticleRenderJob;
        s32 miBuildLionVertexBuffers;
        s32 miLionUpdate;
        s32 miLionRender;
        s32 miWaitOnParticleRenderJob;
        s32 miRenderFullResParticles;
        s32 miTrailsRender;
        s32 miWaitOnDebrisUpdateJob;
        s32 miDebrisRender;
        s32 miSparksDispatch;
        s32 miRenderQuarterResParticles;
        s32 miSimpleParticlesDispatch;
        s32 miLionDispatch;

        // Register every monitor. lpcPrefix is prepended to each region name
        // (e.g. "Particles" -> "Particles: PreRenderUpdate"); pass an empty
        // (non-null) string for an unprefixed set. DWARF: Construct(const char*).
        void Construct(const char* lpcPrefix);

    private:
        // Format "<prefix><separator><name>" into a bounded buffer and register
        // the monitor, returning its handle. DWARF: private
        // AddMonitor(const char*, const char*). Inlined at every call site in
        // the X360 ARTIST build.
        s32 AddMonitor(const char* lpcName, const char* lpcPrefix);
    };
}

#endif // GAMESOURCE_EFFECTS_PARTICLES_PARTICLECPUMONITORS_H
