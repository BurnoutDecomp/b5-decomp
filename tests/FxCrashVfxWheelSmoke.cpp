// FX-CRASHVFX (crash parity 2026-09-25, C1): THE TYRE SMOKE -- BrnEffects::WheelStateMachine::Update @0x82293EB8,
// HandleSmokeLayer @0x82288E38, FireNativeParticle @0x82288C30 and BrnParticle::ParticleModule::SpawnWheelSmoke
// @0x82281AF0.
//
// run_fxcrashvfx_wheel_smoke.py hands this fixture the PRODUCTION translation unit -- the revision's
// GameSource/Effects/Wheel/WheelStateMachine.cpp compiled whole, followed by ParticleModule::SpawnWheelSmoke extracted
// from ParticleModule.cpp -- against the revision's own headers, and links the real CgsNumeric::Random. The fixture
// supplies what those bodies call outside themselves: BrnSimpleParticleArray::SpawnParticle (recorded; the array
// gives the particle type), the Attrib instance plumbing the surface lookup goes through (surfacelist Surfaces(id) ->
// surface -> the visualfxsurface ref 16 bytes into its layout), answered from the case's tables exactly as the emu64
// hooks answer them, the asserts (counted) and the log (captured).
//
// The EffectsModule, ParticleModule, RaceCarState and ActiveRaceCarData are zeroed storage with only what the bodies
// read set, BY NAME: the two rings, maSimpleParticles[t].mpStandardParams, the wheel, the transform's z row, mFlags.
// mID is filled so that its low halfword's bit 1 is the COMPLEMENT of the case's mFlags bit 1: a body that reads the
// console's byte offset +0x130 on x64 -- where mID lives; mFlags sits at +0x138 -- gets every crash gate backwards.
//
// The expected values are the CONSOLE'S OWN OUTPUTS: FxCrashVfxWheelSmokeData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_wheelsmoke_data.py, which runs 0x82293EB8 -- and the real
// HandleSmokeLayer, FireNativeParticle and SpawnWheelSmoke words -- on emu64. Per case, 5 checks: the spawn count, the
// types and whether the spawn loop was still running at the KU_WHEEL_SMOKE_HANG_SPAWNS-th record (a NaN accumulator
// never leaves the console's loop: `bge` at 0x82289034 is taken on unordered); the positions and spawn times; the
// velocities; size, rotation speed, crash bank and alpha; the two accumulators, both rings and the asserts. Plus one:
// the always-on NaN-accumulator line is written ONCE per process, before the first spawn of the first hanging case,
// with its NOT-IN-THE-X360-BINARY label. A console NaN matches any NaN (the x86 default NaN carries the other sign).
#include "fxcrashvfx_wheelsmoke_tu.inc"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "FxCrashVfxWheelSmokeData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

// A failure prints its whole label; a pass only its first 96 characters.
static void Check(bool lbPassed, const std::string& lrLabel)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lrLabel.c_str());
    }
    else
    {
        std::printf("pass  %.96s\n", lrLabel.c_str());
    }
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool IsNanBits(u32 lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
static bool SameWord(u32 luGot, u32 luWant) { return luGot == luWant || (IsNanBits(luWant) && IsNanBits(luGot)); }
static bool SameWords(const u32* lpauGot, const u32* lpauWant, u32 luCount)
{
    for (u32 i = 0; i < luCount; ++i)
    {
        if (!SameWord(lpauGot[i], lpauWant[i]))
            return false;
    }
    return true;
}

// ---- the asserts (counted) and the log (captured) ------------------------------------------------------------------
static std::vector<std::string> gLog;

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
    void WriteToLog(const char* lpcText) { gLog.push_back(lpcText ? lpcText : ""); }
}
}

// ---- the surface lookup, answered as the emu64 hooks answer it -----------------------------------------------------
// Surface n (the case's vfx block n): a 32-byte ref block, a surface layout (its visualfxsurface ref at +0x10) and a
// visualfxsurface block (the layer words at +0x20..+0x44, the enables at +0x4C / +0x4D). Anything else -- the
// DefaultDataArea fallback of a surface id the list does not hold -- resolves to nothing, and the generated ctors
// then take their own DefaultDataArea fallback: the zeroed block.
static const u32 KU_MAX_SURFACES = 4;
alignas(16) static unsigned char gaSurfaceRefs[KU_MAX_SURFACES][0x20];
alignas(16) static unsigned char gaLayouts[KU_MAX_SURFACES][0x100];
alignas(16) static unsigned char gaVfx[KU_MAX_SURFACES][0x100];
alignas(16) static unsigned char gaDefaultArea[0x100];
static const WheelSmokeCase* gpCase = nullptr;
static const void* gpSurfaceList = nullptr;
static u32 gBadLookups = 0;

namespace Attrib
{
    Instance::Instance(const RefSpec& lrRefSpec, void* lpOwner)
        : mpCollection(nullptr), mpAttributeData(nullptr), mpOwner(lpOwner), muFlags(0)
    {
        const unsigned char* lpRef = reinterpret_cast<const unsigned char*>(&lrRefSpec);
        for (u32 n = 0; n < KU_MAX_SURFACES; ++n)
        {
            if (lpRef == gaSurfaceRefs[n])
                mpAttributeData = gaLayouts[n];
            else if (lpRef == gaLayouts[n] + 0x10)
                mpAttributeData = gaVfx[n];
        }
    }
    Instance::~Instance() {}
    int Instance::GetClass() const { return 0; }
    u64 Instance::GetCollection() const { return 0; }
    void* Instance::GetAttributePointer(u64 luAttributeKey, u32 luIndex) const
    {
        // surfacelist::Surfaces(id): the "Surfaces" array (key 0x0ADCE56EF3DA7F1F) of the effects module's list.
        if (static_cast<const void*>(this) != gpSurfaceList || luAttributeKey != 0x0ADCE56EF3DA7F1Full)
            ++gBadLookups;
        for (u32 n = 0; n < gpCase->muNumSurfaces; ++n)
        {
            if (gpCase->mpSurfaces[n].muSurfaceId == luIndex)
                return gaSurfaceRefs[gpCase->mpSurfaces[n].muVfx];
        }
        return nullptr;
    }
    void AssertOnClassCheck(int, int, u64) { ++gAsserts; }
    void* DefaultDataArea(u32) { return gaDefaultArea; }
}

// ---- the particle records: SpawnParticle @0x8227B000's arguments, the array giving the type ------------------------
struct SpawnRecord { u32 muType; u32 mauPos[4], mauVel[4]; u32 muTime, muSize, muRot, muCrash, muAlpha; };
struct HangStop {};   // thrown at the KU_WHEEL_SMOKE_HANG_SPAWNS-th record, as the emu64 hook stops the console
static std::vector<SpawnRecord> gSpawns;
static const BrnParticle::Native::BrnSimpleParticleArray* gpArrays = nullptr;
static size_t gLogAtFirstSpawn = 0;

namespace BrnParticle
{
namespace Native
{
    bool BrnSimpleParticleArray::SpawnParticle(rw::math::vpu::Vector3 lSpawnPosition,
                                               rw::math::vpu::Vector3 lSpawnVelocity,
                                               f32 lrSpawnTime,
                                               f32 lrSizeScale,
                                               f32 lrRotationalVelocity,
                                               bool lbIsCrash,
                                               f32 lrAlpha)
    {
        if (gSpawns.empty())
            gLogAtFirstSpawn = gLog.size();
        SpawnRecord lRecord;
        lRecord.muType = static_cast<u32>(this - gpArrays);
        std::memcpy(lRecord.mauPos, &lSpawnPosition, 16);
        std::memcpy(lRecord.mauVel, &lSpawnVelocity, 16);
        lRecord.muTime  = Bits(lrSpawnTime);
        lRecord.muSize  = Bits(lrSizeScale);
        lRecord.muRot   = Bits(lrRotationalVelocity);
        lRecord.muCrash = lbIsCrash ? 1u : 0u;
        lRecord.muAlpha = Bits(lrAlpha);
        gSpawns.push_back(lRecord);
        if (gSpawns.size() >= KU_WHEEL_SMOKE_HANG_SPAWNS)
            throw HangStop();
        return true;
    }
}
}

// ---- the rings ------------------------------------------------------------------------------------------------------
static void SetRandom(CgsNumeric::Random& lrRandom, const u32* lpauRing, u64 luSeed, u32 luIndex)
{
    for (u32 i = 0; i < 8; ++i)
        lrRandom.mauIntegerBuffer[i] = lpauRing[i];
    lrRandom.muSeed = luSeed;
    lrRandom.muOldestBufferIndex = luIndex;
}

static bool SameRandom(const CgsNumeric::Random& lrRandom, const u32* lpauRing, u64 luSeed, u32 luIndex)
{
    bool lbSame = lrRandom.muSeed == luSeed && lrRandom.muOldestBufferIndex == luIndex;
    for (u32 i = 0; i < 8; ++i)
        lbSame = lbSame && lrRandom.mauIntegerBuffer[i] == lpauRing[i];
    return lbSame;
}

int main()
{
    using namespace BrnEffects;
    using BrnPhysics::Vehicle::RaceCarState;
    using BrnPhysics::Vehicle::WheelLite;

    alignas(16) static unsigned char saEffects[sizeof(EffectsModule)];
    alignas(16) static unsigned char saParticles[sizeof(BrnParticle::ParticleModule)];
    alignas(16) static unsigned char saParams[BrnParticle::ParticleModule::KU_NUM_SIMPLE_ARRAYS]
                                            [sizeof(BrnParticle::Native::CB4ParticleArrayStandardParams)];
    alignas(16) static unsigned char saActive[sizeof(ActiveRaceCarData)];
    alignas(16) static unsigned char saState[sizeof(RaceCarState)];
    alignas(16) static unsigned char saCarState[sizeof(CarState)];

    const u32 luNumCases = static_cast<u32>(sizeof(kaWheelSmokeCases) / sizeof(kaWheelSmokeCases[0]));
    const char* const KPC_NAN_LINE = "[wheel-smoke] NaN ACCUMULATOR";
    u32  luNanLines = 0;
    s32  liFirstHangCase = -1;
    bool lbLineInFirstHangBeforeSpawn = false;
    bool lbLineLabelled = false;

    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const WheelSmokeCase& lrCase = kaWheelSmokeCases[luCase];
        gpCase = &lrCase;

        // The surfaces.
        std::memset(gaSurfaceRefs, 0, sizeof(gaSurfaceRefs));
        std::memset(gaLayouts, 0, sizeof(gaLayouts));
        std::memset(gaVfx, 0, sizeof(gaVfx));
        std::memset(gaDefaultArea, 0, sizeof(gaDefaultArea));
        for (u32 v = 0; v < lrCase.muNumVfx && v < KU_MAX_SURFACES; ++v)
        {
            const WheelSmokeVfx& lrVfx = lrCase.mpVfx[v];
            for (u32 w = 0; w < lrVfx.muNumWords; ++w)
                std::memcpy(gaVfx[v] + lrVfx.mpWords[w].muOffset, &lrVfx.mpWords[w].muValue, 4);
            gaVfx[v][0x4C] = lrVfx.mu8Enable0;
            gaVfx[v][0x4D] = lrVfx.mu8Enable1;
        }

        // The effects module: its ring and its surfacelist.
        std::memset(saEffects, 0, sizeof(saEffects));
        EffectsModule* lpEffects = reinterpret_cast<EffectsModule*>(saEffects);
        gpSurfaceList = &lpEffects->mSurfaceList;
        SetRandom(lpEffects->mRandom, lrCase.mauEffRing, lrCase.muEffSeed, lrCase.muEffIndex);

        // The particle module: its ring and each array's rotation-speed range.
        std::memset(saParticles, 0, sizeof(saParticles));
        std::memset(saParams, 0, sizeof(saParams));
        BrnParticle::ParticleModule* lpParticles = reinterpret_cast<BrnParticle::ParticleModule*>(saParticles);
        SetRandom(lpParticles->mRandom, lrCase.mauPmRing, lrCase.muPmSeed, lrCase.muPmIndex);
        gpArrays = &lpParticles->maSimpleParticles[0];
        for (u32 t = 0; t < BrnParticle::ParticleModule::KU_NUM_SIMPLE_ARRAYS; ++t)
        {
            BrnParticle::Native::CB4ParticleArrayStandardParams* lpParams =
                reinterpret_cast<BrnParticle::Native::CB4ParticleArrayStandardParams*>(saParams[t]);
            lpParams->mrRotationSpeedMin = Float(lrCase.mauRot[t][0]);
            lpParams->mrRotationSpeedMax = Float(lrCase.mauRot[t][1]);
            lpParticles->maSimpleParticles[t].mpStandardParams = lpParams;
        }

        // The race-car state: this wheel and the transform's z row.
        std::memset(saState, 0, sizeof(saState));
        RaceCarState* lpState = reinterpret_cast<RaceCarState*>(saState);
        WheelLite& lrWheel = lpState->maWheels[lrCase.muWheel];
        std::memcpy(&lrWheel.mRoadContact.mPosition, lrCase.mauPos, 16);
        std::memcpy(&lrWheel.mVelocity, lrCase.mauVel, 16);
        lrWheel.mRoadContact.mCollisionTag.muValue = lrCase.muTag;
        lrWheel.mRoadContact.mbIsOnGround = lrCase.mu8Ground != 0;
        lrWheel.mfRadiansPerSecond = Float(lrCase.muRadiansPerSecond);
        lrWheel.mfRadius           = Float(lrCase.muRadius);
        lrWheel.mfSkidFactor       = Float(lrCase.muSkid);
        lrWheel.mfRoadLatSpeed     = Float(lrCase.muLat);
        lrWheel.mbAttached         = lrCase.mu8Attached != 0;
        lrWheel.mbHasTraction      = lrCase.mu8Traction != 0;
        std::memcpy(&lpState->mTransform.zAxis, lrCase.mauZAxis, 16);

        // The active race car: mFlags by name; mID's low bit 1 the complement of mFlags' (see the banner).
        std::memset(saActive, 0, sizeof(saActive));
        ActiveRaceCarData* lpActive = reinterpret_cast<ActiveRaceCarData*>(saActive);
        lpActive->mFlags = static_cast<u16>(lrCase.muFlags);
        lpActive->mID = ((lrCase.muFlags & 2u) != 0) ? 0x5A5A5A5A5A5A5A58ull : 0x5A5A5A5A5A5A5A5Aull;

        // The car state: dt, time and the race-car state (+0x10 / +0x14 / +0x40).
        std::memset(saCarState, 0, sizeof(saCarState));
        CarState* lpCarState = reinterpret_cast<CarState*>(saCarState);
        lpCarState->mEffectsModuleParams.mDt   = Float(lrCase.muDt);
        lpCarState->mEffectsModuleParams.mTime = Float(lrCase.muTime);
        lpCarState->mpCarState = lpState;

        RwRGBAReal lColour;
        std::memset(&lColour, 0, sizeof(lColour));
        RaceCarParticleEffectHelper lHelper(*lpEffects, *lpActive, lpState, *lpParticles, nullptr, 0u, lColour,
                                            static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(0), nullptr);

        WheelStateMachine lMachine;
        std::memset(&lMachine, 0, sizeof(lMachine));
        lMachine.mWheelIndex = lrCase.muWheel;
        lMachine.mfSkidSmokeAccumulators[0] = Float(lrCase.mauAcc[0]);
        lMachine.mfSkidSmokeAccumulators[1] = Float(lrCase.mauAcc[1]);

        gSpawns.clear();
        gAsserts = 0;
        gBadLookups = 0;
        const size_t luLogBefore = gLog.size();
        gLogAtFirstSpawn = static_cast<size_t>(-1);
        bool lbHang = false;
        try
        {
            lMachine.Update(*lpCarState, lHelper);
        }
        catch (const HangStop&)
        {
            lbHang = true;
        }

        // The NaN-accumulator line(s) this case wrote.
        for (size_t i = luLogBefore; i < gLog.size(); ++i)
        {
            if (gLog[i].find(KPC_NAN_LINE) == std::string::npos)
                continue;
            ++luNanLines;
            if (lrCase.muHang != 0 && liFirstHangCase < 0)
            {
                lbLineInFirstHangBeforeSpawn = i < gLogAtFirstSpawn;
                lbLineLabelled = gLog[i].find("NOT IN THE X360 BINARY") != std::string::npos
                              && gLog[i].find("0x82289034") != std::string::npos;
            }
        }
        if (lrCase.muHang != 0 && liFirstHangCase < 0)
            liFirstHangCase = static_cast<s32>(luCase);

        const std::string lName = "[" + std::to_string(luCase) + "] " + lrCase.mpcName;
        const bool lbCount = gSpawns.size() == lrCase.muNumSpawns;
        bool lbTypes = lbCount && lbHang == (lrCase.muHang != 0);
        bool lbPositions = lbCount, lbVelocities = lbCount, lbScalars = lbCount;
        for (u32 i = 0; lbCount && i < lrCase.muNumSpawns; ++i)
        {
            const SpawnRecord&    lrGot  = gSpawns[i];
            const WheelSmokeSpawn& lrWant = lrCase.mpSpawns[i];
            lbTypes      = lbTypes && lrGot.muType == lrWant.muType;
            lbPositions  = lbPositions && SameWords(lrGot.mauPos, lrWant.mauPos, 4) && SameWord(lrGot.muTime, lrWant.muTime);
            lbVelocities = lbVelocities && SameWords(lrGot.mauVel, lrWant.mauVel, 4);
            lbScalars    = lbScalars && SameWord(lrGot.muSize, lrWant.muSize) && SameWord(lrGot.muRot, lrWant.muRot)
                        && lrGot.muCrash == lrWant.muCrash && SameWord(lrGot.muAlpha, lrWant.muAlpha);
        }
        char lacCounts[160];
        std::snprintf(lacCounts, sizeof(lacCounts), " (spawns %u want %u, hang %d want %u, asserts %u want %u)",
                      static_cast<u32>(gSpawns.size()), lrCase.muNumSpawns, lbHang ? 1 : 0, lrCase.muHang, gAsserts,
                      lrCase.muAsserts);
        Check(lbTypes && gBadLookups == 0, lName + " -- the spawn count, types and hang" + lacCounts);
        Check(lbPositions, lName + " -- the spawn positions and times");
        Check(lbVelocities, lName + " -- the velocities (spread, inheritance, kick, backward bias)");
        Check(lbScalars, lName + " -- size, rotation speed, crash bank, alpha");
        char lacState[160];
        std::snprintf(lacState, sizeof(lacState), " (acc %08X/%08X want %08X/%08X)",
                      Bits(lMachine.mfSkidSmokeAccumulators[0]), Bits(lMachine.mfSkidSmokeAccumulators[1]),
                      lrCase.mauAccOut[0], lrCase.mauAccOut[1]);
        Check(SameWord(Bits(lMachine.mfSkidSmokeAccumulators[0]), lrCase.mauAccOut[0])
              && SameWord(Bits(lMachine.mfSkidSmokeAccumulators[1]), lrCase.mauAccOut[1])
              && SameRandom(lpEffects->mRandom, lrCase.mauEffRingOut, lrCase.muEffSeedOut, lrCase.muEffIndexOut)
              && SameRandom(lpParticles->mRandom, lrCase.mauPmRingOut, lrCase.muPmSeedOut, lrCase.muPmIndexOut)
              && gAsserts == lrCase.muAsserts,
              lName + " -- the accumulators, both rings, the asserts" + lacState);
    }

    char lacLine[200];
    std::snprintf(lacLine, sizeof(lacLine),
                  " (%u lines; first hanging case %d; before its first spawn %d; labelled %d)", luNanLines,
                  liFirstHangCase, lbLineInFirstHangBeforeSpawn ? 1 : 0, lbLineLabelled ? 1 : 0);
    Check(luNanLines == 1 && liFirstHangCase >= 0 && lbLineInFirstHangBeforeSpawn && lbLineLabelled,
          std::string("the NaN-accumulator line: ONE per process, written before the first hanging case's first spawn, "
                      "labelled NOT IN THE X360 BINARY with the loop's address") + lacLine);

    std::printf("FxCrashVfxWheelSmoke: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures != 0 ? 1 : 0;
}
