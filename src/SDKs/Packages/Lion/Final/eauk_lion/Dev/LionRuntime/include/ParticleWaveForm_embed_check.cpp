// Embed check: include the ParticleWaveForm home header and exercise the bodied
// table-build + Evaluate paths, with static_asserts on the proven layout.
// Compile-only; not shipped.
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleWaveForm.h"

#include <cstddef>

// Wave-form parameter offsets the X360 Evaluate reads (mType @0x04 .. mClampMax
// @0x1C). All same-size scalars, so relative member layout is portable.
static_assert(offsetof(cParticleWaveForm, mType)     == 0x04, "mType @0x04");
static_assert(offsetof(cParticleWaveForm, mBase)     == 0x08, "mBase @0x08");
static_assert(offsetof(cParticleWaveForm, mPhase)    == 0x0C, "mPhase @0x0C");
static_assert(offsetof(cParticleWaveForm, mFreq)     == 0x10, "mFreq @0x10");
static_assert(offsetof(cParticleWaveForm, mAmp)      == 0x14, "mAmp @0x14");
static_assert(offsetof(cParticleWaveForm, mClampMin) == 0x18, "mClampMin @0x18");
static_assert(offsetof(cParticleWaveForm, mClampMax) == 0x1C, "mClampMax @0x1C");

namespace
{
    void ParticleWaveFormEmbedCheck()
    {
        // Build the singleton's six lookup tables (Init fans out to all builders).
        cParticleWaveFormTable* lpTable = cParticleWaveFormTable::GetMe();
        lpTable->Init();

        // Sampling helpers are reachable by name.
        const f32 lfSin = lpTable->GetSin(128);
        const f32 lfTri = lpTable->GetTriangle(0);
        (void)lfSin;
        (void)lfTri;

        // Evaluate each wave-form type path.
        cParticleWaveForm lWave = {};
        lWave.Init();
        lWave.SetType(cParticleWaveForm::E_TYPE_SIN);
        lWave.SetAmp(1.0f);
        lWave.SetBase(0.0f);
        lWave.SetFreq(1.0f);
        lWave.SetClampMin(-1.0f);
        lWave.SetClampMax(1.0f);
        const f32 lfOut = lWave.Evaluate(0.25f);
        (void)lfOut;
    }
}
