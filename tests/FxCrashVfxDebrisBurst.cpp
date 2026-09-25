// FX-CRASHVFX (crash parity 2026-09-25, item 3): ParticleModule::HandleFireDebrisBurstEvent @0x8229A660 -- the
// dispatch-thread consumer of the debris burst ParticleModule::FireDebrisBurst posts for a crashing / taken-down car.
// For each of the four burst arrays it spawns count * scale pieces, each placed in the emitter box, thrown at the
// camera or into a cone, sized, spun and coloured, straight into BrnDebrisArray::SpawnDebris.
//
// run_fxcrashvfx_debris_burst.py extracts the PRODUCTION region (the banner through the end of the handler) and the
// file-local constants and VMX idioms it names (the shower block of ParticleModule_SparkEvents.cpp) and compiles them
// onto this fixture: a ParticleModule carrying the real CgsNumeric::Random and the real BrnDebrisArray[5] (whose
// SpawnDebris is defined HERE as a recorder), the real FireDebrisBurstEvent (its debrisparams Instance pointed at the
// case's layout), and the production DebrisColourRandomiser.
//
// The expected values are the CONSOLE'S OWN OUTPUTS: FxCrashVfxDebrisBurstData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_fdb_data.py, which runs the handler's real instruction words on emu64
// (the CRT-initialised trig and colour splats filled by RUNNING their thunks) and records every SpawnDebris
// @0x82294DC8 at the call boundary. Seven checks per case: the pieces (count and array, in order), their positions,
// velocities, spin axes, colours, spin rate / size / time, and the Random + assert count afterwards. A console NaN
// is matched by any NaN (the x86 default NaN carries the other sign).
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Effects/Particles/BrnParticleModuleIO_EventTypes.h"
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"
#include "GameSource/Effects/BrnEffectsDebrisColourRandomiser.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <vector>

#include "FxCrashVfxDebrisBurstData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0, gNotReconstructed = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++gChecks;
    if (!lbPassed)
        ++gFailures;
    std::printf("%s  %s\n", lbPassed ? "pass" : "FAIL", lpcLabel);
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }

// A console word against a host float: equal bits, or both NaN.
static bool SameWord(u32 luConsole, f32 lfHost)
{
    const f32 lfConsole = Float(luConsole);
    if (lfConsole != lfConsole)
        return lfHost != lfHost;
    return luConsole == Bits(lfHost);
}

static bool SameVector(const u32 lau[4], const void* lpValue)
{
    f32 laf[4];
    std::memcpy(laf, lpValue, 16);
    for (u32 i = 0; i < 4; ++i)
        if (!SameWord(lau[i], laf[i]))
            return false;
    return true;
}

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    void WriteToLog(const char*) {}
}
}

// The production colour randomiser (BrnEffectsDebrisColourRandomiser.cpp, whole).
#include "fxcrashvfx_burst_colour.inc"

namespace BrnParticle
{
    struct DebrisRecord
    {
        const void* mpArray;
        Vector3     mvPosition, mvVelocity, mvAxis;
        Vector4     mvColour;
        f32         mfSpin, mfSize, mfTime;
    };
    static std::vector<DebrisRecord> gDebris;

    namespace Native
    {
        // The recorder: SpawnDebris' real body is the bucket write (another TU); the call is the boundary.
        void BrnDebrisArray::SpawnDebris(rw::math::vpu::Vector3 lPos, rw::math::vpu::Vector3 lLinearVelocity,
                                         rw::math::vpu::Vector3 lRotationAxis, rw::math::vpu::Vector4 lDiffuseColour,
                                         f32 lrRotationalVelocity, f32 lrSizeScale, f32 lrSpawnTime)
        {
            DebrisRecord lRecord;
            lRecord.mpArray    = this;
            lRecord.mvPosition = lPos;
            lRecord.mvVelocity = lLinearVelocity;
            lRecord.mvAxis     = lRotationAxis;
            lRecord.mvColour   = lDiffuseColour;
            lRecord.mfSpin     = lrRotationalVelocity;
            lRecord.mfSize     = lrSizeScale;
            lRecord.mfTime     = lrSpawnTime;
            gDebris.push_back(lRecord);
        }
    }

    class ParticleModule
    {
    public:
        CgsNumeric::Random     mRandom;
        Native::BrnDebrisArray maDebris[5];

        void HandleFireDebrisBurstEvent(const FireDebrisBurstEvent* lpEvent);
    };

    namespace
    {
        // The parent revision's handler announced itself instead of running; its announcement lands here.
        inline void LogSparkEventNotReconstructed(bool& lrbLogged, const char*)
        {
            lrbLogged = true;
            ++gNotReconstructed;
        }
    }

#include "fxcrashvfx_burst_consts.inc"
#include "fxcrashvfx_burst_body.inc"
}

int main()
{
    static BrnParticle::Native::BrnDebrisArrayParams saPresets[5];
    unsigned luPieces = 0;
    const u32 luNumCases = static_cast<u32>(sizeof(kaBurstCases) / sizeof(kaBurstCases[0]));
    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const BurstCase& lrCase = kaBurstCases[luCase];
        char lacLabel[400];

        void* lpStorage = _aligned_malloc(sizeof(BrnParticle::ParticleModule), 16);
        std::memset(lpStorage, 0, sizeof(BrnParticle::ParticleModule));
        BrnParticle::ParticleModule* lpModule = static_cast<BrnParticle::ParticleModule*>(lpStorage);
        for (u32 t = 0; t < 5; ++t)
        {
            saPresets[t].mColour.x = Float(lrCase.mauPresets[t][0]);
            saPresets[t].mColour.y = Float(lrCase.mauPresets[t][1]);
            saPresets[t].mColour.z = Float(lrCase.mauPresets[t][2]);
            saPresets[t].mColour.w = Float(lrCase.mauPresets[t][3]);
            lpModule->maDebris[t].mpParams = &saPresets[t];
        }
        for (u32 i = 0; i < 8; ++i)
            lpModule->mRandom.mauIntegerBuffer[i] = lrCase.mauRing[i];
        lpModule->mRandom.muSeed              = lrCase.muSeed;
        lpModule->mRandom.muOldestBufferIndex = lrCase.muIndex;

        alignas(16) static unsigned char saLayout[0x100];
        std::memset(saLayout, 0, sizeof(saLayout));
        for (u32 w = 0; w < lrCase.muNumLayoutWords; ++w)
            std::memcpy(saLayout + lrCase.mpLayout[w].muOffset, &lrCase.mpLayout[w].muValue, 4);

        // The record in raw storage: its debrisparams member is never constructed (that would reach the AttribSys
        // runtime); only its Instance's layout pointer is set, which is all the handler reads of it.
        alignas(16) static unsigned char saEvent[sizeof(BrnParticle::FireDebrisBurstEvent)];
        std::memset(saEvent, 0, sizeof(saEvent));
        BrnParticle::FireDebrisBurstEvent* lpEvent = reinterpret_cast<BrnParticle::FireDebrisBurstEvent*>(saEvent);
        std::memcpy(&lpEvent->mSpawnPosition, lrCase.mauSpawn, 16);
        std::memcpy(&lpEvent->mvEmitterHalfExtents, lrCase.mauHalf, 16);
        std::memcpy(&lpEvent->mvVelocityToInherit, lrCase.mauInherit, 16);
        std::memcpy(&lpEvent->mCameraPosition, lrCase.mauCamera, 16);
        std::memcpy(&lpEvent->mvCarColour, lrCase.mauColour, 16);
        ((Attrib::Instance&)lpEvent->mDebrisParams).mpAttributeData = saLayout;
        lpEvent->mfCurrentTime = Float(lrCase.muTime);
        lpEvent->mfScaleFactor = Float(lrCase.muScale);

        BrnParticle::gDebris.clear();
        const unsigned luAssertsBefore = gAsserts;
        lpModule->HandleFireDebrisBurstEvent(lpEvent);
        const std::vector<BrnParticle::DebrisRecord>& lrGot = BrnParticle::gDebris;

        // 1. the pieces: how many, and which array each went into, in order
        bool lbArrays = lrGot.size() == lrCase.muNumSpawns;
        for (u32 n = 0; lbArrays && n < lrCase.muNumSpawns; ++n)
            lbArrays = lrGot[n].mpArray == &lpModule->maDebris[lrCase.mpSpawns[n].muType];
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] %u piece(s), each into the console's array (got %u)",
                      lrCase.mpcName, lrCase.muNumSpawns, static_cast<unsigned>(lrGot.size()));
        Check(lbArrays, lacLabel);

        u32 luPos = 0, luVel = 0, luAxis = 0, luColour = 0, luScalars = 0;
        const u32 luCompared = (lrGot.size() < lrCase.muNumSpawns) ? static_cast<u32>(lrGot.size()) : lrCase.muNumSpawns;
        for (u32 n = 0; n < luCompared; ++n)
        {
            const BurstSpawnOut& lrWant = lrCase.mpSpawns[n];
            luPos     += SameVector(lrWant.mauPos, &lrGot[n].mvPosition) ? 1u : 0u;
            luVel     += SameVector(lrWant.mauVel, &lrGot[n].mvVelocity) ? 1u : 0u;
            luAxis    += SameVector(lrWant.mauAxis, &lrGot[n].mvAxis) ? 1u : 0u;
            luColour  += SameVector(lrWant.mauColour, &lrGot[n].mvColour) ? 1u : 0u;
            luScalars += (SameWord(lrWant.muSpin, lrGot[n].mfSpin) && SameWord(lrWant.muSize, lrGot[n].mfSize)
                          && SameWord(lrWant.muTime, lrGot[n].mfTime)) ? 1u : 0u;
        }
        luPieces += luCompared;
        const bool lbAll = luCompared == lrCase.muNumSpawns;
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] positions (emitter box, z drawn first): %u/%u",
                      lrCase.mpcName, luPos, lrCase.muNumSpawns);
        Check(lbAll && luPos == lrCase.muNumSpawns, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] velocities (camera throw or cone + inherited share): %u/%u",
                      lrCase.mpcName, luVel, lrCase.muNumSpawns);
        Check(lbAll && luVel == lrCase.muNumSpawns, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] spin axes (RandomUnitVector): %u/%u",
                      lrCase.mpcName, luAxis, lrCase.muNumSpawns);
        Check(lbAll && luAxis == lrCase.muNumSpawns, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] colours (grey-to-base 25%%..75%%, alpha 0.5..0.9): %u/%u",
                      lrCase.mpcName, luColour, lrCase.muNumSpawns);
        Check(lbAll && luColour == lrCase.muNumSpawns, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] spin rate, size and time: %u/%u",
                      lrCase.mpcName, luScalars, lrCase.muNumSpawns);
        Check(lbAll && luScalars == lrCase.muNumSpawns, lacLabel);

        bool lbRandom = lpModule->mRandom.muSeed == lrCase.muSeedOut
                     && lpModule->mRandom.muOldestBufferIndex == lrCase.muIndexOut
                     && (gAsserts - luAssertsBefore) == lrCase.muAsserts;
        for (u32 i = 0; i < 8; ++i)
            lbRandom = lbRandom && lpModule->mRandom.mauIntegerBuffer[i] == lrCase.mauRingOut[i];
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] the module Random (every draw, in order) and the assert count "
                      "are the console's", lrCase.mpcName);
        Check(lbRandom, lacLabel);

        _aligned_free(lpStorage);
    }

    std::printf("(%u pieces compared; %u NOT RECONSTRUCTED announcement(s))\n", luPieces, gNotReconstructed);
    std::printf("FxCrashVfxDebrisBurst: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
