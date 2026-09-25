// FX-CRASHVFX (crash parity 2026-09-25, item 4): EffectsModule::HandleCrashingTrail @0x82290D30 -- a crashing car
// sheds its debris trail (the five debris arrays and impact smoke) along the path it moved this step.
//
// run_fxcrashvfx_crash_trail.py extracts the PRODUCTION region (THE CRASHING TRAIL: its literals, layout words, piece
// helper, witness and the handler) and the helpers it names from EffectsModule.cpp (Dot3, Vnmsub, RefinedRsqrt,
// GuardedLength3, Scale4, DebrisParamsLayout, LayoutFloat and the glass region's Add4 / Sub4 / Mul4 / MaddSplat4 /
// MakeVector3 / RandomUnitVector) and compiles them onto this fixture: the real CgsNumeric::Random (with its real
// RandomVector / RandomFloat bodies), the real debrisparams Instance pointed at the case's layout, a RaceCarState
// carrying the three members the handler reads, and a ParticleModule recording SpawnDebris / SpawnSimple.
//
// The expected values are the CONSOLE'S OWN OUTPUTS: FxCrashVfxCrashTrailData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_trail_data.py, which runs 0x82290D30 on emu64 and records every
// SpawnDebris @0x8228A3E8 / SpawnSimple @0x82281A10 at the call boundary. Seven checks per case: the calls (kind and
// type, in order), positions, velocities, the debris' spin axes and colours, size / time / alpha, the car's six
// accumulators after, and mRandom + the assert count. A console NaN matches any NaN (the x86 default NaN carries
// the other sign).
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/AttribSys/Generated/classes/debrisparams.h"
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"
#include "rw/core/base/ostypes.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "FxCrashVfxCrashTrailData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0, gTrailLines = 0, gNotReconstructed = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++gChecks;
    if (!lbPassed)
        ++gFailures;
    std::printf("%s  %s\n", lbPassed ? "pass" : "FAIL", lpcLabel);
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool IsNanBits(u32 lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
static bool SameWord(u32 luGot, u32 luWant) { return luGot == luWant || (IsNanBits(luWant) && IsNanBits(luGot)); }

static bool SameVector(const void* lpValue, const u32 lau[4])
{
    for (u32 i = 0; i < 4; ++i)
    {
        u32 lu;
        std::memcpy(&lu, static_cast<const unsigned char*>(lpValue) + 4 * i, 4);
        if (!SameWord(lu, lau[i]))
            return false;
    }
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
    void WriteToLog(const char* lpcText)
    {
        if (lpcText != nullptr && std::strncmp(lpcText, "[crash-trail]", 13) == 0)
            ++gTrailLines;
    }
}
}

namespace BrnPhysics
{
namespace Vehicle
{
    // The three members HandleCrashingTrail reads (BrnVehicleEvents.h: @496, @816, @848), by name.
    struct alignas(16) RaceCarState
    {
        Matrix44Affine mTransform;
        Vector3        mLinearVelocity;
        Vector3        mHalfExtent;
    };
}
}

namespace BrnParticle
{
    struct TrailSpawnRecord
    {
        u32     muKind;   // 0 SpawnDebris, 1 SpawnSimple
        u32     muType;
        Vector3 mvPosition, mvVelocity, mvAxis;
        Vector4 mvColour;
        f32     mfSize, mfTime, mfAlpha;
    };

    class ParticleModule
    {
    public:
        std::vector<TrailSpawnRecord> mSpawns;
        void SpawnDebris(Native::EDebrisArrayID leDebrisType, Vector3 lvPosition, Vector3 lvVelocity,
                         Vector3 lvRotationAxis, Vector4 lvColour, f32 lfSize, f32 lfSpawnTime)
        {
            TrailSpawnRecord lRecord;
            std::memset(&lRecord, 0, sizeof(lRecord));
            lRecord.muKind     = 0u;
            lRecord.muType     = static_cast<u32>(leDebrisType);
            lRecord.mvPosition = lvPosition;
            lRecord.mvVelocity = lvVelocity;
            lRecord.mvAxis     = lvRotationAxis;
            lRecord.mvColour   = lvColour;
            lRecord.mfSize     = lfSize;
            lRecord.mfTime     = lfSpawnTime;
            mSpawns.push_back(lRecord);
        }
        void SpawnSimple(Vector3 lvPosition, Vector3 lvVelocity, Native::ENativeParticleType leParticleType,
                         f32 lfSizeScale, f32 lfSpawnTime, f32 lfAlpha)
        {
            TrailSpawnRecord lRecord;
            std::memset(&lRecord, 0, sizeof(lRecord));
            lRecord.muKind     = 1u;
            lRecord.muType     = static_cast<u32>(leParticleType);
            lRecord.mvPosition = lvPosition;
            lRecord.mvVelocity = lvVelocity;
            lRecord.mfSize     = lfSizeScale;
            lRecord.mfTime     = lfSpawnTime;
            lRecord.mfAlpha    = lfAlpha;
            mSpawns.push_back(lRecord);
        }
    };
}

namespace BrnEffects
{
    class ActiveRaceCarData;

    namespace
    {
        typedef ::EActiveRaceCarIndex             EActiveRaceCarIndex;
        typedef BrnPhysics::Vehicle::RaceCarState RaceCarState;
        const u32 KU_EFFECTS_DIAG_MAX_LINES = 128u;

        // The parent revision's handler announced itself instead of running; its announcement lands here.
        inline void LogNotReconstructed(bool& lrbLogged, const char*)
        {
            lrbLogged = true;
            ++gNotReconstructed;
        }

#include "fxcrashvfx_trail_helpers.inc"
    }

    class EffectsModule
    {
    public:
        BrnParticle::ParticleModule mParticleModule;
        CgsNumeric::Random          mRandom;
        f32                         mafCrashingTrailAccumulators[8][6];
        Matrix44Affine              maRaceCarPreviousTransforms[8];

        void HandleCrashingTrail(ActiveRaceCarData& lrActiveRaceCar, f32 lfDt, f32 lfTime,
                                 const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState,
                                 EActiveRaceCarIndex leIndex,
                                 const RwRGBAReal& lrCarColour,
                                 const Attrib::Gen::debrisparams& lrDebrisParameters);
    };

#include "fxcrashvfx_trail_body.inc"
}

static void LoadVector(Vector3& lrv, const u32 lau[4])
{
    lrv.x = Float(lau[0]); lrv.y = Float(lau[1]); lrv.z = Float(lau[2]); lrv.w = Float(lau[3]);
}

int main()
{
    unsigned luPieces = 0;
    const u32 luNumCases = static_cast<u32>(sizeof(kaTrailCases) / sizeof(kaTrailCases[0]));
    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const TrailCase& lrCase = kaTrailCases[luCase];
        char lacLabel[400];

        BrnEffects::EffectsModule* lpModule = new BrnEffects::EffectsModule();
        std::memset(lpModule->mafCrashingTrailAccumulators, 0, sizeof(lpModule->mafCrashingTrailAccumulators));
        std::memset(lpModule->maRaceCarPreviousTransforms, 0, sizeof(lpModule->maRaceCarPreviousTransforms));
        for (u32 i = 0; i < 8; ++i)
            lpModule->mRandom.mauIntegerBuffer[i] = lrCase.mauRing[i];
        lpModule->mRandom.muSeed              = lrCase.muSeed;
        lpModule->mRandom.muOldestBufferIndex = lrCase.muIndex;
        for (u32 t = 0; t < 6; ++t)
            lpModule->mafCrashingTrailAccumulators[lrCase.muCar][t] = Float(lrCase.mauAcc[t]);
        std::memcpy(&lpModule->maRaceCarPreviousTransforms[lrCase.muCar], lrCase.mauPrevious, 64);

        alignas(16) static BrnPhysics::Vehicle::RaceCarState sState;
        std::memcpy(&sState.mTransform, lrCase.mauTransform, 64);
        LoadVector(sState.mLinearVelocity, lrCase.mauVel);
        LoadVector(sState.mHalfExtent, lrCase.mauHalf);

        RwRGBAReal lColour;
        lColour.red = Float(lrCase.mauColour[0]); lColour.green = Float(lrCase.mauColour[1]);
        lColour.blue = Float(lrCase.mauColour[2]); lColour.alpha = Float(lrCase.mauColour[3]);

        alignas(16) static unsigned char saLayout[0x100];
        std::memset(saLayout, 0, sizeof(saLayout));
        for (u32 w = 0; w < lrCase.muNumLayoutWords; ++w)
            std::memcpy(saLayout + lrCase.mpLayout[w].muOffset, &lrCase.mpLayout[w].muValue, 4);
        // The debrisparams in raw storage: never constructed (that reaches the AttribSys runtime); its Instance's
        // layout pointer is all the handler reads.
        alignas(16) static unsigned char saParams[sizeof(Attrib::Gen::debrisparams)];
        std::memset(saParams, 0, sizeof(saParams));
        Attrib::Gen::debrisparams* lpParams = reinterpret_cast<Attrib::Gen::debrisparams*>(saParams);
        ((Attrib::Instance&)*lpParams).mpAttributeData = saLayout;

        const unsigned luAssertsBefore = gAsserts;
        lpModule->HandleCrashingTrail(*reinterpret_cast<BrnEffects::ActiveRaceCarData*>(saParams), Float(lrCase.muDt),
                                      Float(lrCase.muTime), &sState,
                                      static_cast<EActiveRaceCarIndex>(lrCase.muCar), lColour, *lpParams);
        const std::vector<BrnParticle::TrailSpawnRecord>& lrGot = lpModule->mParticleModule.mSpawns;

        bool lbCalls = lrGot.size() == lrCase.muNumSpawns;
        for (u32 n = 0; lbCalls && n < lrCase.muNumSpawns; ++n)
            lbCalls = lrGot[n].muKind == lrCase.mpSpawns[n].muKind && lrGot[n].muType == lrCase.mpSpawns[n].muType;
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] %u call(s), each the console's kind and array (got %u)",
                      lrCase.mpcName, lrCase.muNumSpawns, static_cast<unsigned>(lrGot.size()));
        Check(lbCalls, lacLabel);

        const u32 luCompared = (lrGot.size() < lrCase.muNumSpawns) ? static_cast<u32>(lrGot.size()) : lrCase.muNumSpawns;
        u32 luPos = 0, luVel = 0, luAxisColour = 0, luScalars = 0;
        for (u32 n = 0; n < luCompared; ++n)
        {
            const TrailSpawnOut& lrWant = lrCase.mpSpawns[n];
            const BrnParticle::TrailSpawnRecord& lrHave = lrGot[n];
            luPos += SameVector(&lrHave.mvPosition, lrWant.mauPos) ? 1u : 0u;
            luVel += SameVector(&lrHave.mvVelocity, lrWant.mauVel) ? 1u : 0u;
            luAxisColour += (lrWant.muKind == 1u
                             || (SameVector(&lrHave.mvAxis, lrWant.mauAxis) && SameVector(&lrHave.mvColour, lrWant.mauColour)))
                            ? 1u : 0u;
            luScalars += (SameWord(Bits(lrHave.mfSize), lrWant.muSize) && SameWord(Bits(lrHave.mfTime), lrWant.muTime)
                          && (lrWant.muKind == 0u || SameWord(Bits(lrHave.mfAlpha), lrWant.muAlpha))) ? 1u : 0u;
        }
        luPieces += luCompared;
        const bool lbAll = luCompared == lrCase.muNumSpawns;
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] positions (the car's box turned by last step's transform): %u/%u",
                      lrCase.mpcName, luPos, lrCase.muNumSpawns);
        Check(lbAll && luPos == lrCase.muNumSpawns, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] velocities (spread + inherited share, fused): %u/%u",
                      lrCase.mpcName, luVel, lrCase.muNumSpawns);
        Check(lbAll && luVel == lrCase.muNumSpawns, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] debris spin axes and colours: %u/%u",
                      lrCase.mpcName, luAxisColour, lrCase.muNumSpawns);
        Check(lbAll && luAxisColour == lrCase.muNumSpawns, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] sizes, times (stepped by dt / n) and the smoke's alpha: %u/%u",
                      lrCase.mpcName, luScalars, lrCase.muNumSpawns);
        Check(lbAll && luScalars == lrCase.muNumSpawns, lacLabel);

        bool lbAcc = true;
        for (u32 t = 0; t < 6; ++t)
            lbAcc = lbAcc && SameWord(Bits(lpModule->mafCrashingTrailAccumulators[lrCase.muCar][t]), lrCase.mauAccOut[t]);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] the car's six accumulators carry the console's fractions",
                      lrCase.mpcName);
        Check(lbAcc, lacLabel);

        bool lbRandom = lpModule->mRandom.muSeed == lrCase.muSeedOut
                     && lpModule->mRandom.muOldestBufferIndex == lrCase.muIndexOut
                     && (gAsserts - luAssertsBefore) == lrCase.muAsserts;
        for (u32 i = 0; i < 8; ++i)
            lbRandom = lbRandom && lpModule->mRandom.mauIntegerBuffer[i] == lrCase.mauRingOut[i];
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] mRandom (every draw, in order) and the assert count are the "
                      "console's", lrCase.mpcName);
        Check(lbRandom, lacLabel);

        delete lpModule;
    }

    std::printf("(%u pieces compared; %u NOT RECONSTRUCTED announcement(s); %u [crash-trail] line(s))\n", luPieces,
                gNotReconstructed, gTrailLines);
    std::printf("FxCrashVfxCrashTrail: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
