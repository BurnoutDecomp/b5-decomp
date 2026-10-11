// Real nuclei, render records, integration, rotation, size and locator-weighting
// kernels. Only monitoring, random draws (zero variance), unused sub-emitter/wave
// paths and pool retirement cross fixture boundaries. No game is launched.
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "pc/gcm/renderengine/reflections/LionSimulation.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleMaterial.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBucketManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/RenderedParticle.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleWaveForm.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.h"

namespace CgsDev::PerfMonCpu { void StartMonitor(s32) {} void StopMonitor(s32) {} }
void cParticleRandomSeed::Update() { ++mSeed; }
void cParticleRandomSeed::Offset(u32 value) { mSeed += value; }
f32 cParticleRandomSeed::Build(f32 base, f32) { return base; }
s32 cParticleRandomSeed::Build(s32 base, s32) { return base; }
void cParticleRandomSeed::Build(cVector& out, const cVector& base, const cVector&) { out = base; }
rw::math::vpu::Vector3Plus cParticleRandomSeed::Build(rw::math::vpu::Vector4 base, rw::math::vpu::Vector4) { return {base.x, base.y, base.z, base.w}; }
rw::math::vpu::Vector3Plus cParticleRandomSeed::BuildLerp(rw::math::vpu::Vector4 base, rw::math::vpu::Vector4) { return {base.x, base.y, base.z, base.w}; }
f32 cParticleWaveForm::Evaluate(f32) const { return 1; }
static u32 suFrees = 0;
cParticleBucketManager& cParticleBucketManager::Instance() { static cParticleBucketManager pool = {}; return pool; }
void cParticleBucketManager::Free(cParticleBucket*) { ++suFrees; }
void cParticleEmitter::ParentMatrixCurrentBuild(cMatrix& out, const cTime&, f32, const cTime&) { out.BuildIdentity(); }

#include "pc_lion_simulation.inc"

static int siChecks = 0, siFailures = 0;
static void Check(bool value, const char* name)
{
    ++siChecks;
    if (!value) { ++siFailures; if (siFailures <= 12) std::printf("FAIL %s\n", name); }
}

struct Fixture
{
    cParticleEmitter mEmitter = {};
    cParticleDescriptor mDescriptor = {};
    cParticleBehaviour mBehaviour = {};
    cParticleLocator mLocator = {};
    cLionBindings mBindings = {};
    cParticleBucket mBucket = {};
    std::array<cMatrix, 16> maMatrices = {};
    std::array<cVector, 16> maVectors = {};
    cMatrix mLocatorMat = {};

    Fixture(cParticleMaterial* lpMaterial, u32 count)
    {
        mDescriptor.mpMaterial = lpMaterial;
        mDescriptor.mFlags = cParticleDescriptor::E_FLAG_NEEDS_BUCKET;
        mDescriptor.mShape = 3;
        mBehaviour.mTimeScale = 1;
        mBehaviour.mFlags = cParticleBehaviour::E_DO_EMITTER_WEIGHTING
            | cParticleBehaviour::E_BV_ROTVELACC | cParticleBehaviour::E_BV_SIZE_FULL
            | cParticleBehaviour::E_DO_OFFSETROT;
        mBehaviour.mEmitterStartWeight = 0;
        mBehaviour.mEmitterEndWeight = 1;
        mEmitter.mpDescriptor = &mDescriptor;
        mEmitter.mpCurrentBehaviour = &mBehaviour;
        mBindings.mpLocator = &mLocator;
        mEmitter.mpBindings = &mBindings;
        mEmitter.mDt = 0.0625f;
        mEmitter.mPrecalculatedParticleBuildData.mvScaleAndProportionalScaleYXAndZX = {1, 1, 1, 1};
        mEmitter.mPrecalculatedParticleBuildData.mvAlphaFadeInAndFadeOut = {0, 1, 0, 0};
        mEmitter.mPrecalculatedParticleBuildData.mvOrientStepAndDragFrameRateConstants = {0.125f, 0.0625f, 0, 0};
        mLocatorMat.BuildIdentity(); mLocatorMat.SetTrans(125, 10, -80);
        for (u32 i = 0; i < count; ++i)
        {
            mBucket.mActiveBits |= 1u << i;
            auto& p = mBucket.GetParticles()[i];
            p.LifeTime() = 8; p.BirthTime() = 0;
            p.mPos = {1 + static_cast<f32>(i), 2, 3, 1};
            p.mVel = {-8, 2, -16, 0}; p.mAcc = {0, 4, 0, 0};
            p.mLocatorVel = {12, -2, 8, 0};
            p.mSize = {1, 2, 3, 0}; p.mSizeVel = {2, 4, 6, 0}; p.mSizeAcc = {1, 2, 3, 0};
            p.mRotVel = {0.5f, 1, 2, 0}; p.mRotAcc = {1, 2, 3, 0};
            p.mOffsetRotVel = {0.25f, 0.5f, 1, 0}; p.mOffsetRotAcc = {1, 2, 3, 0};
            maMatrices[i] = mLocatorMat; maVectors[i] = mLocatorMat.wa;
        }
    }
};

template<class Helper, class Transform>
static void Cameras(cParticleMaterial* material, u32 count, u32 frame)
{
    Fixture f(material, count);
    if constexpr (std::is_same_v<Helper, MatrixSimulationHelper>) f.mBucket.mpMatrices = f.maMatrices.data();
    if constexpr (std::is_same_v<Helper, VectorSimulationHelper>) f.mBucket.mpVectors = f.maVectors.data();
    std::array<RenderedParticle, 16> particles = {}, first = {};
    std::array<Transform, 16> transforms = {}, original = {};
    const cTime time(3000u);
    auto render = [&] { return f.mEmitter.SimulateParticlesInBucketGeneral(Helper(transforms.data()),
        particles.data(), &f.mBucket, time, time, f.mLocatorMat); };
    {
        CgsPC::Reflections::LionSimulation::Scope main(&f.mEmitter, frame, 1);
        Check(render() == count, "main view retains every live particle");
    }
    first = particles; original = transforms;
    const auto bucket = f.mBucket; const auto matrices = f.maMatrices; const auto vectors = f.maVectors;
    Check(f.mBucket.mParticles[0].mPos.z == 2, "position advances by exactly velocity times one frame");
    Check(f.mBucket.mParticles[0].mVel.y == 2.25f, "exhaust acceleration advances exactly once");
    Check(f.mBucket.mParticles[0].mSize.x == 1.125f, "size advances exactly once");
    for (u32 face = 0; face < 6; ++face)
    {
        particles = {}; transforms = {};
        CgsPC::Reflections::LionSimulation::Scope reflection(&f.mEmitter, frame, 1);
        Check(render() == count, "every camera receives the same live count");
        Check(std::memcmp(first.data(), particles.data(), count * sizeof(RenderedParticle)) == 0,
            "each camera receives identical simulation output before its vertex build");
        Check(std::memcmp(original.data(), transforms.data(), count * sizeof(Transform)) == 0,
            "each camera receives identical locator transforms");
        Check(std::memcmp(&bucket, &f.mBucket, sizeof(bucket)) == 0,
            "reflection cameras preserve all live nuclei, rotation, velocity and size");
        Check(std::memcmp(matrices.data(), f.maMatrices.data(), sizeof(matrices)) == 0
            && std::memcmp(vectors.data(), f.maVectors.data(), sizeof(vectors)) == 0,
            "reflection cameras do not accumulate locator drift");
    }
    {
        CgsPC::Reflections::LionSimulation::Scope next(&f.mEmitter, frame + 1, 1);
        Check(render() == count, "a new frame with a paused clock still evaluates particles");
        Check(f.mBucket.mParticles[0].mPos.z == 1, "next presentation advances one additional frame");
    }
    render();
    Check(f.mBucket.mParticles[0].mPos.z == 0, "outside capture scopes the original kernel remains active");
}

int main()
{
    auto* material = static_cast<cParticleMaterial*>(CgsMemory::LowMemory::Reserve(sizeof(cParticleMaterial)));
    if (!material || !CgsMemory::LowMemory::IsLowAddress(material)) return 2;
    *material = {};
    for (u32 count : {1u, 16u})
    {
        Cameras<MatrixSimulationHelper, cMatrix>(material, count, 10 + count);
        Cameras<VectorSimulationHelper, cVector>(material, count, 40 + count);
        Cameras<LocalSimulationHelper, cMatrix>(material, count, 70 + count);
    }
    Fixture f(material, 1);
    std::array<RenderedParticle, 16> particles = {};
    std::array<cMatrix, 16> transforms = {};
    auto render = [&](const cTime& time) { return f.mEmitter.SimulateParticlesInBucketGeneral(
        LocalSimulationHelper(transforms.data()), particles.data(), &f.mBucket, time, time, f.mLocatorMat); };
    f.mBucket.mParticles[0].BirthTime() = 2;
    {
        CgsPC::Reflections::LionSimulation::Scope late(&f.mEmitter, 100, 1);
        for (u32 face = 2; face < 6; ++face)
            Check(render(cTime(3000u)) == 0, "a particle not born yet stays absent in every face");
    }
    {
        CgsPC::Reflections::LionSimulation::Scope born(&f.mEmitter, 100, 2);
        Check(render(cTime(6000u)) == 1, "a changed presentation time invalidates empty samples");
        const auto nucleus = f.mBucket.mParticles[0];
        Check(render(cTime(6000u)) == 1 && std::memcmp(&nucleus, f.mBucket.GetParticles(), sizeof(nucleus)) == 0,
            "an emitter first seen by a reflection is also simulated once");
    }
    {
        CgsPC::Reflections::LionSimulation::Scope dead(&f.mEmitter, 101, 20);
        const u32 before = suFrees;
        for (u32 face = 0; face < 6; ++face) Check(render(cTime(60000u)) == 0, "dead buckets remain empty");
        Check(suFrees == before + 1 && f.mBucket.IsEmpty(), "the original retirement returns a dead bucket once");
    }
    CgsMemory::LowMemory::Release(material);
    std::printf("PCLionSimulation: %d checks, %d failures\n", siChecks, siFailures);
    return siFailures ? 1 : 0;
}
