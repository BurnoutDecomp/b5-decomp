// FX-CRASHVFX (crash parity 2026-09-24): ParticleModule::HandleSpawnSparkShowerFromPointEvent @0x82299CC8 -- the
// dispatch-thread consumer of every spark shower EffectsModule::DoSparkShower posts (world grinding, vehicle
// grinding, the crash shower). It turns one SpawnSparkShowerFromPointEvent into luNumToSpawn
// SparkArray::SpawnSpark calls: a direction drawn in the record's frame (two angles through the TrigBaseFunctions5
// sin/cos polynomial), a speed scaled by the inherited velocity, the part heading INTO the surface reflected, a
// position jittered in the frame, and a birth time stepping back across one 60 Hz frame.
//
// run_fxcrashvfx_spark_shower.py extracts the PRODUCTION body (and the file-local constants and VMX idioms it
// names) out of ParticleModule_SparkEvents.cpp and compiles it onto this fixture: a ParticleModule carrying the real
// CgsNumeric::Random, the real SparkArray type (whose SpawnSpark is defined HERE as a recorder), the ring's newest
// timestamp and the render data's time / flags; the two randomiser bodies are the production ones.
//
// The expected values are the CONSOLE'S OWN OUTPUTS: FxCrashVfxSparkShowerData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_shower_data.py, which runs the handler's real instruction words
// on emu64 (fused vmaddfp rounded once, vnmsubfp negated, the CRT-initialised polynomial splats filled by RUNNING
// their thunks) and records every SpawnSpark @0x822955E0 at the call boundary. Compared bit for bit: every
// spark's array, position and velocity (4 lanes each), size, time, time-since-event, birth offset, height and bank;
// the ring / seed / cursor after the burst; the assert count.
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Effects/Particles/BrnParticleModuleIO_EventTypes.h"
#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"
#include "GameSource/Effects/Particles/ParticleCpuMonitors.h"
#include "GameSource/Effects/BrnEffectsUtils.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "FxCrashVfxSparkShowerData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0, gNotReconstructed = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
    else
    {
        std::printf("pass  %s\n", lpcLabel);
    }
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }

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
namespace PerfMonCpu
{
    void StartMonitor(s32) {}
    void StopMonitor(s32) {}
}
}

namespace BrnEffects
{
namespace Utils
{
#include "fxcrashvfx_shower_randomisers.inc"
}
}

namespace BrnParticle
{
    ParticleCpuMonitors gRaceCpuMonitors;
    ParticleCpuMonitors gCrashCpuMonitors;
    u32 gauSparkShowerSpawned = 0;

    struct SparkRecord
    {
        const void* mpArray;
        Vector3     mvPosition;
        Vector3     mvVelocity;
        f32         maf[5];
        bool        mbCrash;
    };
    static std::vector<SparkRecord> gSparks;

    namespace Native
    {
        // The recorder: SpawnSpark's real body is the bank write (another TU); the call is the boundary.
        void SparkArray::SpawnSpark(rw::math::vpu::Vector3::InParam lPosition,
                                    rw::math::vpu::Vector3::InParam lVelocity,
                                    f32 lfSize, f32 lfCurrentTime, f32 lfTimeSinceEvent, f32 lfBirthTimeOffset,
                                    f32 lfHeightAbovePlane, bool lbIsCrashSpark)
        {
            SparkRecord lRecord;
            lRecord.mpArray    = this;
            lRecord.mvPosition = lPosition;
            lRecord.mvVelocity = lVelocity;
            lRecord.maf[0] = lfSize; lRecord.maf[1] = lfCurrentTime; lRecord.maf[2] = lfTimeSinceEvent;
            lRecord.maf[3] = lfBirthTimeOffset; lRecord.maf[4] = lfHeightAbovePlane;
            lRecord.mbCrash = lbIsCrashSpark;
            gSparks.push_back(lRecord);
        }
    }

    struct FakeSparkFrame
    {
        f32 mfTimeStamp;
    };
    struct FakeSparkFrameDataSet
    {
        FakeSparkFrame maFrames[2];
        const FakeSparkFrame& GetFrame(u32 luFrameId) const { return maFrames[luFrameId]; }
    };

    class ParticleModule
    {
    public:
        struct ParticleRenderData
        {
            static const u16 eRenderDataFlagReducedFrameRate = 64;   // the console's `rlwinm ..., 0, 0x19, 0x19`
            f32 mfCurrentTime;
            u16 muFlags;
        };

        CgsNumeric::Random     mRandom;
        Native::SparkArray*    maSparks;
        FakeSparkFrameDataSet  mSparkFrameDataSetUpdate;

        void HandleSpawnSparkShowerFromPointEvent(const SpawnSparkShowerFromPointEvent* lpEvent,
                                                  const ParticleRenderData& lrRenderData);
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

#include "fxcrashvfx_shower_consts.inc"
#include "fxcrashvfx_shower_body.inc"
}

static bool SameBits(const void* lpValue, const u32* lpau, u32 luWords)
{
    for (u32 i = 0; i < luWords; ++i)
    {
        u32 lu;
        std::memcpy(&lu, static_cast<const unsigned char*>(lpValue) + 4 * i, 4);
        if (lu != lpau[i])
            return false;
    }
    return true;
}

static void Load(Vector4& lrv, const u32 lau[4])
{
    lrv.x = Float(lau[0]); lrv.y = Float(lau[1]); lrv.z = Float(lau[2]); lrv.w = Float(lau[3]);
}

int main()
{
    alignas(16) static unsigned char saSparkStorage[4 * sizeof(BrnParticle::Native::SparkArray)];
    unsigned luTotal = 0;
    const u32 luNumCases = static_cast<u32>(sizeof(kaShowerCases) / sizeof(kaShowerCases[0]));
    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const ShowerCase& lrCase = kaShowerCases[luCase];
        char lacLabel[320];

        BrnParticle::ParticleModule* lpModule = new BrnParticle::ParticleModule();
        for (u32 i = 0; i < 8; ++i)
            lpModule->mRandom.mauIntegerBuffer[i] = lrCase.maRing[i];
        lpModule->mRandom.muSeed              = lrCase.muSeed;
        lpModule->mRandom.muOldestBufferIndex = lrCase.muIndex;
        std::memset(saSparkStorage, 0, sizeof(saSparkStorage));
        lpModule->maSparks = reinterpret_cast<BrnParticle::Native::SparkArray*>(saSparkStorage);
        lpModule->mSparkFrameDataSetUpdate.maFrames[0].mfTimeStamp = Float(lrCase.muRingTime);
        lpModule->mSparkFrameDataSetUpdate.maFrames[1].mfTimeStamp = 0.0f;

        BrnParticle::ParticleModule::ParticleRenderData lRenderData;
        lRenderData.mfCurrentTime = Float(lrCase.muRenderTime);
        lRenderData.muFlags       = static_cast<u16>(lrCase.muRenderFlags);

        alignas(16) BrnParticle::SpawnSparkShowerFromPointEvent lEvent;
        std::memset(&lEvent, 0, sizeof(lEvent));
        std::memcpy(&lEvent.mTransform, lrCase.maMatrix, 64);
        Load(lEvent.mLateralAngleMinMaxForwardAngleMinMax, lrCase.maLateral);
        Load(lEvent.mVelocityMinMaxInheritanceMinMax, lrCase.maVelArgs);
        Vector4 lvInherit;
        Load(lvInherit, lrCase.maInherit);
        std::memcpy(&lEvent.mVelocityToInherit, &lvInherit, 16);
        Load(lEvent.mSparkSizeMinMaxSpawnRadiusXSpawnRadiusYZ, lrCase.maSizeArgs);
        lEvent.mfCurrentTime                 = Float(lrCase.maF[0]);
        lEvent.mfGroundPositionY             = Float(lrCase.maF[1]);
        lEvent.mfVelocityScaleSpeedThreshold = Float(lrCase.maF[2]);
        lEvent.mfReflectionAmount            = Float(lrCase.maF[3]);
        lEvent.muNumToSpawn                  = lrCase.muCount;
        lEvent.meSparkType                   = static_cast<BrnParticle::Native::ESparkArrayID>(lrCase.muSpark);

        BrnParticle::gSparks.clear();
        const unsigned luAssertsBefore = gAsserts;
        lpModule->HandleSpawnSparkShowerFromPointEvent(&lEvent, lRenderData);

        u32 luSame = 0;
        for (u32 n = 0; n < lrCase.muNumSparks && n < BrnParticle::gSparks.size(); ++n)
        {
            const ShowerSpark& lrWant = lrCase.mpSparks[n];
            const BrnParticle::SparkRecord& lrGot = BrnParticle::gSparks[n];
            const u32 luArray = static_cast<u32>((static_cast<const unsigned char*>(lrGot.mpArray) - saSparkStorage)
                                                 / sizeof(BrnParticle::Native::SparkArray));
            const bool lbSame = luArray == lrWant.muArray && SameBits(&lrGot.mvPosition, lrWant.maPos, 4)
                             && SameBits(&lrGot.mvVelocity, lrWant.maVel, 4) && SameBits(lrGot.maf, lrWant.maF, 5)
                             && (lrGot.mbCrash ? 1u : 0u) == lrWant.muCrash;
            if (!lbSame && luSame + 3 > n)
            {
                std::printf("      spark %u: got array %u pos %08X %08X %08X %08X vel %08X %08X %08X %08X f %08X %08X %08X %08X %08X\n"
                            "               want array %u pos %08X %08X %08X %08X vel %08X %08X %08X %08X f %08X %08X %08X %08X %08X\n",
                            n, luArray, Bits(lrGot.mvPosition.x), Bits(lrGot.mvPosition.y), Bits(lrGot.mvPosition.z),
                            Bits(lrGot.mvPosition.w), Bits(lrGot.mvVelocity.x), Bits(lrGot.mvVelocity.y),
                            Bits(lrGot.mvVelocity.z), Bits(lrGot.mvVelocity.w), Bits(lrGot.maf[0]), Bits(lrGot.maf[1]),
                            Bits(lrGot.maf[2]), Bits(lrGot.maf[3]), Bits(lrGot.maf[4]),
                            lrWant.muArray, lrWant.maPos[0], lrWant.maPos[1], lrWant.maPos[2], lrWant.maPos[3],
                            lrWant.maVel[0], lrWant.maVel[1], lrWant.maVel[2], lrWant.maVel[3],
                            lrWant.maF[0], lrWant.maF[1], lrWant.maF[2], lrWant.maF[3], lrWant.maF[4]);
            }
            luSame += lbSame ? 1u : 0u;
        }
        luTotal += lrCase.muNumSparks;
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u (%s): every SpawnSpark is the console's (array, position, velocity, size, the three "
                      "times, height, bank): %u/%u (spawned %u)",
                      luCase, lrCase.mpcName, luSame, lrCase.muNumSparks,
                      static_cast<unsigned>(BrnParticle::gSparks.size()));
        Check(luSame == lrCase.muNumSparks && BrnParticle::gSparks.size() == lrCase.muNumSparks, lacLabel);

        bool lbRing = lpModule->mRandom.muSeed == lrCase.muSeedOut
                   && lpModule->mRandom.muOldestBufferIndex == lrCase.muIndexOut;
        for (u32 i = 0; i < 8; ++i)
            lbRing = lbRing && lpModule->mRandom.mauIntegerBuffer[i] == lrCase.maRingOut[i];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "case %u: mRandom's ring, seed and cursor after the burst are the console's (8 draws per spark)",
                      luCase);
        Check(lbRing, lacLabel);

        std::snprintf(lacLabel, sizeof(lacLabel), "case %u: asserts fired %u (console %u)", luCase,
                      gAsserts - luAssertsBefore, lrCase.muAsserts);
        Check(gAsserts - luAssertsBefore == lrCase.muAsserts, lacLabel);

        delete lpModule;
    }

    Check(gNotReconstructed == 0, "the handler runs: no NOT RECONSTRUCTED announcement");
    std::printf("(%u console sparks compared)\n", luTotal);
    std::printf("FxCrashVfxSparkShower: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
