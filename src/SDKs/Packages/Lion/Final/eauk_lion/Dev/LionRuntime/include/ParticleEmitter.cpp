// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.cpp
//
// cParticleEmitter::Init @0x82913228 -- the one emitter body this build actually RUNS.
//
// ⭐ WHY IT MATTERS: cParticleEmitterManager::AppInit @0x82913470 calls Init(nullptr) on every
// pooled emitter, and AppInit runs inside cLionFX::Init, which ParticleModule::Prepare now
// calls on every boot. So this is not a "for later" body -- it executes 256 times before the
// first frame.
//
// The rest of the emitter -- Update (the simulation core), DeInit, Bind, BucketRemove -- is not
// reconstructed and is announced in LionRuntimeLinkStubs.cpp rather than faked here.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffectManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdio>

namespace
{
// mBlendLast's initial value: `lfs f0, -0xF30(r10)` at 0x829132B4 resolves to flt_820FF0D0,
// read out of the image (tools/re/x360rd.py 0x820FF0D0) as 0x42C84027 == 100.12529754638672.
// It is a "no blend in progress" sentinel, not a tunable: the blend path compares against it,
// it is never interpolated toward. The exact bits are kept so the comparison behaves.
const f32 KF_BLEND_LAST_NONE = 100.12529754638672f;   // flt_820FF0D0
}  // namespace

// ----------------------------------------------------------------------------
// cParticleEmitter::Init  @ 0x82913228
//
// Store for store, in the console's own order. The descriptor branch at the tail is what makes
// a REGISTERED emitter (apDescriptor != nullptr) different from a POOLED one: a descriptor with
// a single behaviour binds that behaviour directly and precalculates its build data, while one
// with several allocates a blend behaviour from the effect manager and starts a blend.
//
// ⛔ THAT TAIL IS NOT RECONSTRUCTED, AND IT IS ANNOUNCED, NOT DROPPED. Both of its arms need
// cParticleEmitter bodies this build does not have (PrecalculateParticleBuildData @0x8290E018
// and Blend @0x8290F730), and mPrecalculatedParticleBuildData is still a reserved span. It is
// unreachable on this build -- nothing registers an emitter, because cLionFX::EffectCreate is
// not reconstructed -- and the only caller that DOES run, cParticleEmitterManager::AppInit,
// passes nullptr and never enters it.
// ----------------------------------------------------------------------------
void cParticleEmitter::Init(cParticleDescriptor* apDescriptor)
{
    mForce.x = 0.0f;
    mForce.y = 0.0f;
    mForce.z = 0.0f;
    mForce.w = 0.0f;

    mEmissionCount     = 0;
    mFlags             = 0;
    mNextEmissionTime  = 0;
    mpDescriptor       = apDescriptor;
    mpBindings         = nullptr;
    mpBucket           = nullptr;
    mpNext             = nullptr;
    mPhysicsHandle     = 0;
    mLastTime          = 0;
    mParentIndex       = 0;
    mBucketsUsed       = 0;

    mEmitterSeed.Init();

    m_age              = 0.0f;
    mUpdateLastTime    = 0;
    mpParentEmitter    = nullptr;
    mpCurrentBehaviour = nullptr;
    mpTempBehaviour    = nullptr;
    mBlendLast         = KF_BLEND_LAST_NONE;

    if (apDescriptor == nullptr)
    {
        return;
    }

    static bool sbLogged = false;
    if (!sbLogged)
    {
        sbLogged = true;
        CgsDev::Log::WriteToLog(
            "[effects] NOT RECONSTRUCTED: cParticleEmitter::Init's descriptor tail @0x82913228 "
            "(the single-behaviour bind + PrecalculateParticleBuildData @0x8290E018 arm, and the "
            "multi-behaviour CreateBehaviour + Blend @0x8290F730 arm). Reached only by a "
            "REGISTERED emitter; the pooling path (Init(nullptr)) above is complete.\n");
    }
}
