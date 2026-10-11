#pragma once

#include "types.hpp"
#include <type_traits>

class cParticleEmitter;
struct cParticleBucket;
class MatrixSimulationHelper;
class VectorSimulationHelper;
class LocalSimulationHelper;
struct RenderedParticle;

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: Lion's render kernels integrate the live nuclei and
    // locator drift. Reuse their camera-independent output within one presentation;
    // each camera still builds its own billboard/quad vertices from those samples.
    class LionSimulation
    {
        enum class TransformKind { Matrix, Vector, Local };
        static bool Load(const cParticleEmitter*, const cParticleBucket*, TransformKind,
                         void*, RenderedParticle*, u32&);
        static void Save(const cParticleEmitter*, const cParticleBucket*, TransformKind,
                         const void*, const RenderedParticle*, u32);
        static void Begin(const void*, u32, f32);
        static bool SetActive(bool);

    public:
        class Scope
        {
            bool mbPrevious;
        public:
            Scope(const void* lpOwner, u32 luFrame, f32 lfTime);
            ~Scope();
            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;
        };

        template<class Helper>
        static bool Read(const cParticleEmitter* lpEmitter, const cParticleBucket* lpBucket,
                         const Helper& lrHelper, RenderedParticle* lpParticles, u32& luCount)
        {
            if constexpr (std::is_same_v<Helper, VectorSimulationHelper>)
                return Load(lpEmitter, lpBucket, TransformKind::Vector, lrHelper.mpVectors, lpParticles, luCount);
            else
                return Load(lpEmitter, lpBucket, std::is_same_v<Helper, LocalSimulationHelper>
                    ? TransformKind::Local : TransformKind::Matrix, lrHelper.mpMatrices, lpParticles, luCount);
        }

        template<class Helper>
        static void Write(const cParticleEmitter* lpEmitter, const cParticleBucket* lpBucket,
                          const Helper& lrHelper, const RenderedParticle* lpParticles, u32 luCount)
        {
            if constexpr (std::is_same_v<Helper, VectorSimulationHelper>)
                Save(lpEmitter, lpBucket, TransformKind::Vector, lrHelper.mpVectors, lpParticles, luCount);
            else
                Save(lpEmitter, lpBucket, std::is_same_v<Helper, LocalSimulationHelper>
                    ? TransformKind::Local : TransformKind::Matrix, lrHelper.mpMatrices, lpParticles, luCount);
        }
    };
}
