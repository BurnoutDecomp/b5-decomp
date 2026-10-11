#include "pc/gcm/renderengine/reflections/LionSimulation.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBucket.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/RenderedParticle.h"
#include <array>
#include <cstring>
#include <unordered_map>

namespace CgsPC::Reflections
{
    namespace
    {
        struct Sample
        {
            const cParticleEmitter* mpEmitter;
            u32 muKind, muCount;
            std::array<RenderedParticle, cParticleBucket::KU_MAX_PARTICLES> maParticles;
            // Matrix-sized storage also holds the vector arm, copied as bytes.
            std::array<cMatrix, cParticleBucket::KU_MAX_PARTICLES> maTransforms;
        };
        // The render thread owns this cache. Keep the bucket slots allocated across
        // frames; generation checks invalidate samples without per-frame allocation.
        struct Entry { u64 muGeneration = 0; Sample mSample; };
        std::unordered_map<const cParticleBucket*, Entry> saSamples;
        const void* spOwner = nullptr;
        u32 suFrame = 0;
        f32 sfTime = 0;
        u64 suGeneration = 0;
        bool sbActive = false;
    }

    void LionSimulation::Begin(const void* lpOwner, u32 luFrame, f32 lfTime)
    {
        if (!suGeneration || spOwner != lpOwner || suFrame != luFrame || sfTime != lfTime)
        {
            if (spOwner != lpOwner) saSamples.clear();
            spOwner = lpOwner;
            suFrame = luFrame;
            sfTime = lfTime;
            ++suGeneration;
        }
    }

    bool LionSimulation::SetActive(bool lbActive)
    {
        const bool lbPrevious = sbActive;
        sbActive = lbActive;
        return lbPrevious;
    }

    LionSimulation::Scope::Scope(const void* lpOwner, u32 luFrame, f32 lfTime)
        : mbPrevious(SetActive(true)) { Begin(lpOwner, luFrame, lfTime); }

    LionSimulation::Scope::~Scope() { SetActive(mbPrevious); }

    bool LionSimulation::Load(const cParticleEmitter* lpEmitter, const cParticleBucket* lpBucket,
        TransformKind leKind, void* lpTransforms, RenderedParticle* lpParticles, u32& luCount)
    {
        if (!sbActive) return false;
        const auto lFound = saSamples.find(lpBucket);
        if (lFound == saSamples.end() || lFound->second.muGeneration != suGeneration) return false;
        const auto& lrSample = lFound->second.mSample;
        if (lrSample.mpEmitter != lpEmitter || lrSample.muKind != static_cast<u32>(leKind)) return false;
        luCount = lrSample.muCount;
        if (luCount)
        {
            std::memcpy(lpParticles, lrSample.maParticles.data(), luCount * sizeof(RenderedParticle));
            std::memcpy(lpTransforms, lrSample.maTransforms.data(),
                luCount * (leKind == TransformKind::Vector ? sizeof(cVector) : sizeof(cMatrix)));
        }
        return true;
    }

    void LionSimulation::Save(const cParticleEmitter* lpEmitter, const cParticleBucket* lpBucket,
        TransformKind leKind, const void* lpTransforms, const RenderedParticle* lpParticles, u32 luCount)
    {
        if (!sbActive) return;
        auto& lrEntry = saSamples[lpBucket];
        auto& lrSample = lrEntry.mSample;
        lrSample.mpEmitter = lpEmitter;
        lrSample.muKind = static_cast<u32>(leKind);
        lrSample.muCount = luCount;
        if (luCount)
        {
            std::memcpy(lrSample.maParticles.data(), lpParticles, luCount * sizeof(RenderedParticle));
            std::memcpy(lrSample.maTransforms.data(), lpTransforms,
                luCount * (leKind == TransformKind::Vector ? sizeof(cVector) : sizeof(cMatrix)));
        }
        lrEntry.muGeneration = suGeneration;
    }
}
