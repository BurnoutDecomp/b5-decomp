// FX-CRASHVFX (crash parity 2026-09-25, item 2): THE GLASS SMASH -- EffectsModule::HandleGlassSmashEventsForAllCars
// @0x82297420 (the deformation system's smashed panes -> glass debris + 'Glass_shattering' LION effects) and
// EffectsModule::BurstAreaEmitParticles @0x82292160 (the debris burst over a pane, also the showtime bounce's).
//
// run_fxcrashvfx_glass.py extracts the PRODUCTION bodies -- the glass region of EffectsModule.cpp (its constants,
// four-lane helpers, RandomUnitVector, both bodies), the shared vector idioms it names (Dot3, Vnmsub, RefinedRsqrt,
// RefinedRecip, GuardedLength3, Scale4, CrossPermuted, AxisY, IsReplayPlayback / IsReplayRecording), and from
// BrnEffectsUtils.cpp the TrigBaseFunctions5 sin/cos and the Vector3Randomiser bodies -- and compiles them onto this
// fixture: the real CgsNumeric::Random, the real DeformationOutputInterface glass queue, the real ActiveRaceCarData
// flags, and recording stand-ins for ParticleModule::SpawnDebris, the glass manager (FireGlassEffect /
// UpdateVehicleEffectPositions) and the replay layout's two glass accessors.
//
// The expected values are the CONSOLE'S OWN OUTPUTS: FxCrashVfxGlassData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_glass_data.py, which runs 0x82297420 / 0x82292160 (and the real
// RandomiseXYZ / GetEvent) on emu64 and records the same five boundaries. Compared bit for bit per case: every
// SpawnDebris (type, position, velocity, spin axis, colour, size, time), every FireGlassEffect (the effect matrix,
// the car transform, id, time), the replay appends and reads, UpdateVehicleEffectPositions, mRandom's ring / seed /
// cursor after, and the assert count.
//
// TWO DOCUMENTED EXCLUSIONS:
//  * The replay append's pane normal and velocity. The console passes SetGlassEventData six vectors; the declared
//    accessor (Replays/**, another lane's) takes four corner pointers, so the drain has no slot for the other two.
//    The id, transform and four corners are compared; FOLLOWUPS.md carries the accessor gap.
//  * The SIGN of a NaN generated from ordinary operands. The PowerPC's default NaN is 0x7FC00000; x86's is
//    0xFFC00000. A pane facing straight up gives the console a NaN effect frame (up x normal = 0, no zero guard);
//    a console NaN word matches any NaN word here. Every non-NaN word is compared exactly.
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"
#include "GameSource/Effects/ActiveRaceCarData.h"
#include "GameSource/Effects/BrnEffectsUtils.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "FxCrashVfxGlassData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0, gGlassLines = 0, gNotReconstructed = 0;

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
    void WriteToLog(const char* lpcText)
    {
        if (lpcText != nullptr && std::strncmp(lpcText, "[glass]", 7) == 0)
            ++gGlassLines;
    }
}
}

// ---- the particle module: SpawnDebris recorded ----------------------------------------------------------------
namespace BrnParticle
{
    struct DebrisSpawnRecord
    {
        u32     muType;
        Vector3 mvPosition, mvVelocity, mvAxis;
        Vector4 mvColour;
        f32     mfSize, mfTime;
    };

    class ParticleModule
    {
    public:
        std::vector<DebrisSpawnRecord> mSpawns;
        void SpawnDebris(Native::EDebrisArrayID leDebrisType, Vector3 lvPosition, Vector3 lvVelocity,
                         Vector3 lvRotationAxis, Vector4 lvColour, f32 lfSize, f32 lfSpawnTime)
        {
            DebrisSpawnRecord lRecord;
            lRecord.muType     = static_cast<u32>(leDebrisType);
            lRecord.mvPosition = lvPosition;
            lRecord.mvVelocity = lvVelocity;
            lRecord.mvAxis     = lvRotationAxis;
            lRecord.mvColour   = lvColour;
            lRecord.mfSize     = lfSize;
            lRecord.mfTime     = lfSpawnTime;
            mSpawns.push_back(lRecord);
        }
    };
}

namespace BrnEffects
{
namespace Utils
{
#include "fxcrashvfx_glass_utils.inc"
}

namespace EffectsIO
{
    struct InputBuffer
    {
        const BrnPhysics::Deformation::DeformationOutputInterface* mpDeformation;
        const BrnPhysics::Deformation::DeformationOutputInterface* GetDeformationInterface() const
        {
            return mpDeformation;
        }
    };
}

    typedef BrnPhysics::Deformation::DeformationOutputInterface DeformationOutputInterface;
    struct RCEntityActiveRaceCarOutputInterface {};

    // The effects serialiser's static layout as the drain sees it: the recorded glass count at +0x30 (read inline)
    // and the two glass accessors with their DECLARED signatures (BrnReplayEffectsSerialiser.h). The read answers
    // what the console's GetGlassEventData hands back (id, transform, corners, normal, velocity) -- the case's
    // recorded pane -- so the drain's use of each out-parameter is what is measured.
    struct GlassSetRecord
    {
        u32 muId;
        u32 mauTransform[16];
        u32 mauCorners[4][4];
    };

    struct FakeGlassLayout
    {
        static const s32 KI_OFF_NUM_GLASS_EVENTS = 0x30;
        alignas(16) u8 mau8Raw[0x40];
        const GlassEventIn*          mpReplay;
        u32                          muNumReplay;
        std::vector<GlassSetRecord>  mSets;
        std::vector<u32>             mGets;

        int SetGlassEventData(int liEventId, const void* lpMatrix, const void* lpVec1, const void* lpVec2,
                              const void* lpVec3, const void* lpVec4)
        {
            GlassSetRecord lRecord;
            lRecord.muId = static_cast<u32>(liEventId);
            std::memcpy(lRecord.mauTransform, lpMatrix, 64);
            std::memcpy(lRecord.mauCorners[0], lpVec1, 16);
            std::memcpy(lRecord.mauCorners[1], lpVec2, 16);
            std::memcpy(lRecord.mauCorners[2], lpVec3, 16);
            std::memcpy(lRecord.mauCorners[3], lpVec4, 16);
            mSets.push_back(lRecord);
            return 0;
        }

        void GetGlassEventData(u8 luEventIndex, u32* lpuField, void* lpBlock0, void* lpBlock1, void* lpBlock2,
                               void* lpBlock3, void* lpBlock4, void* lpVpermPacked, void* lpVec260)
        {
            mGets.push_back(luEventIndex);
            if (luEventIndex >= muNumReplay)
                return;
            const GlassEventIn& lr = mpReplay[luEventIndex];
            *lpuField = lr.muId;
            std::memcpy(lpBlock0, lr.mauTransform, 64);
            std::memcpy(lpBlock1, lr.mauCorners[0], 16);
            std::memcpy(lpBlock2, lr.mauCorners[1], 16);
            std::memcpy(lpBlock3, lr.mauCorners[2], 16);
            std::memcpy(lpBlock4, lr.mauCorners[3], 16);
            std::memcpy(lpVpermPacked, lr.mauNormal, 16);
            std::memcpy(lpVec260, lr.mauVel, 16);
        }
    };

    struct FakeEffectsSerialiser
    {
        BrnReplays::BaseSerialiser::EMode meMode;
        FakeGlassLayout*                  mpLayout;
        BrnReplays::BaseSerialiser::EMode GetMode() const { return meMode; }
        FakeGlassLayout* GetStaticLayout() const { return mpLayout; }
    };

    struct GlassFireRecord
    {
        Matrix44Affine mEffect;
        Matrix44Affine mVehicle;
        u32            muId;
        f32            mfTime;
    };

    struct FakeGlassManager
    {
        std::vector<GlassFireRecord> mFires;
        std::vector<f32>             mUpdateTimes;
        const EffectsIO::InputBuffer* mpUpdateInput = nullptr;
        void FireGlassEffect(const Matrix44Affine& lEffectTransform, const Matrix44Affine& lVehicleTransform,
                             EntityId lVehicleEntity, f32 lfCurrentTime)
        {
            GlassFireRecord lRecord;
            lRecord.mEffect  = lEffectTransform;
            lRecord.mVehicle = lVehicleTransform;
            lRecord.muId     = lVehicleEntity.muValue;
            lRecord.mfTime   = lfCurrentTime;
            mFires.push_back(lRecord);
        }
        void UpdateVehicleEffectPositions(const EffectsIO::InputBuffer* lpInput, f32 lfCurrentTime)
        {
            mpUpdateInput = lpInput;
            mUpdateTimes.push_back(lfCurrentTime);
        }
    };

    namespace
    {
        typedef FakeGlassLayout EffectsStaticLayout;

        // The parent revision's drain announced itself instead of running; its announcement lands here.
        inline void LogNotReconstructed(bool& lrbLogged, const char*)
        {
            lrbLogged = true;
            ++gNotReconstructed;
        }

#include "fxcrashvfx_glass_helpers.inc"
    }

    class EffectsModule
    {
    public:
        BrnParticle::ParticleModule mParticleModule;
        CgsNumeric::Random          mRandom;
        ActiveRaceCarData*          maActiveRaceCarData;
        FakeEffectsSerialiser       mEffectsSerialiser;
        FakeGlassManager            mGlassSmashManager;

        void BurstAreaEmitParticles(Vector3* lpCorners,
                                    Vector3 lvNormal,
                                    Vector3 lvInheritedVelocity,
                                    BrnParticle::Native::EDebrisArrayID leDebrisType,
                                    f32 lfCurrentTime,
                                    f32 lfSizeMin,
                                    f32 lfSizeMax,
                                    f32 lfParticleDensity);
        void HandleGlassSmashEventsForAllCars(const EffectsIO::InputBuffer* lpInputBuffer,
                                              const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                              f32 lfDt, f32 lfTime);
    };

#include "fxcrashvfx_glass_body.inc"
}

// ---- comparison helpers ---------------------------------------------------------------------------------------
static bool IsNanBits(u32 lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }

// One word against the console's: exact, except that a console NaN matches any NaN (the default NaN's sign is
// the one platform difference -- see the banner).
static bool SameWord(u32 luGot, u32 luWant)
{
    return luGot == luWant || (IsNanBits(luWant) && IsNanBits(luGot));
}

static bool SameWords(const void* lpValue, const u32* lpau, u32 luWords, bool lbVerbose, const char* lpcWhat)
{
    bool lbSame = true;
    for (u32 i = 0; i < luWords; ++i)
    {
        u32 lu;
        std::memcpy(&lu, static_cast<const unsigned char*>(lpValue) + 4 * i, 4);
        if (!SameWord(lu, lpau[i]))
            lbSame = false;
    }
    if (!lbSame && lbVerbose)
    {
        std::printf("        %s got:", lpcWhat);
        for (u32 i = 0; i < luWords; ++i)
        {
            u32 lu;
            std::memcpy(&lu, static_cast<const unsigned char*>(lpValue) + 4 * i, 4);
            std::printf(" %08X", lu);
        }
        std::printf("\n        %s want:", lpcWhat);
        for (u32 i = 0; i < luWords; ++i)
            std::printf(" %08X", lpau[i]);
        std::printf("\n");
    }
    return lbSame;
}

static Vector3 VectorFromBits(const u32 lau[4])
{
    Vector3 lv;
    lv.x = Float(lau[0]); lv.y = Float(lau[1]); lv.z = Float(lau[2]); lv.w = Float(lau[3]);
    return lv;
}

static void FillEvent(BrnPhysics::Deformation::GlassSmashOrCrackEvent& lrEvent, const GlassEventIn& lr)
{
    std::memset(&lrEvent, 0, sizeof(lrEvent));
    for (u32 i = 0; i < 4; ++i)
        lrEvent.maCorners[i] = VectorFromBits(lr.mauCorners[i]);
    lrEvent.mNormal         = VectorFromBits(lr.mauNormal);
    lrEvent.mLinearVelocity = VectorFromBits(lr.mauVel);
    std::memcpy(&lrEvent.mTransform, lr.mauTransform, 64);
    lrEvent.mVehicleEntityId.muValue = lr.muId;
    lrEvent.meGlassPart   = static_cast<decltype(lrEvent.meGlassPart)>(lr.muPart);
    lrEvent.meNewState    = static_cast<BrnPhysics::Deformation::EGlassState>(lr.muState);
    lrEvent.mfCrackAmount = Float(lr.muCrack);
    lrEvent.mbDontPlaySmashEffect = lr.mu8Dont != 0;
}

int main()
{
    alignas(16) static unsigned char saCarDataStorage[8 * sizeof(BrnEffects::ActiveRaceCarData)];
    BrnPhysics::Deformation::DeformationOutputInterface* lpDeformation =
        new BrnPhysics::Deformation::DeformationOutputInterface();

    unsigned luTotalSpawns = 0, luTotalFires = 0;
    const u32 luNumCases = static_cast<u32>(sizeof(kaGlassCases) / sizeof(kaGlassCases[0]));
    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const GlassCase& lrCase = kaGlassCases[luCase];
        char lacLabel[320];

        BrnEffects::EffectsModule* lpModule = new BrnEffects::EffectsModule();
        for (u32 i = 0; i < 8; ++i)
            lpModule->mRandom.mauIntegerBuffer[i] = lrCase.mauRing[i];
        lpModule->mRandom.muSeed              = lrCase.muSeed;
        lpModule->mRandom.muOldestBufferIndex = lrCase.muIndex;

        std::memset(saCarDataStorage, 0, sizeof(saCarDataStorage));
        lpModule->maActiveRaceCarData = reinterpret_cast<BrnEffects::ActiveRaceCarData*>(saCarDataStorage);
        for (u32 i = 0; i < 8; ++i)
            lpModule->maActiveRaceCarData[i].mFlags = lrCase.mau16Flags[i];

        BrnEffects::FakeGlassLayout* lpLayout = new BrnEffects::FakeGlassLayout();
        std::memset(lpLayout->mau8Raw, 0, sizeof(lpLayout->mau8Raw));
        const s32 liRecorded = static_cast<s32>(lrCase.muNumReplay);
        std::memcpy(lpLayout->mau8Raw + BrnEffects::FakeGlassLayout::KI_OFF_NUM_GLASS_EVENTS, &liRecorded, 4);
        lpLayout->mpReplay    = lrCase.mpReplay;
        lpLayout->muNumReplay = lrCase.muNumReplay;
        lpModule->mEffectsSerialiser.meMode   = static_cast<BrnReplays::BaseSerialiser::EMode>(lrCase.muMode);
        lpModule->mEffectsSerialiser.mpLayout = lpLayout;

        gAsserts = 0;
        BrnEffects::EffectsIO::InputBuffer lInput;
        lInput.mpDeformation = lpDeformation;
        if (lrCase.muKind == 0u)
        {
            lpDeformation->mGlassSmashOrCrackQueue.Construct();
            for (u32 k = 0; k < lrCase.muNumEvents; ++k)
            {
                BrnPhysics::Deformation::GlassSmashOrCrackEvent lEvent;
                FillEvent(lEvent, lrCase.mpEvents[k]);
                lpDeformation->mGlassSmashOrCrackQueue.AddEvent(lEvent);
            }
            lpModule->HandleGlassSmashEventsForAllCars(&lInput, nullptr, Float(lrCase.muDt), Float(lrCase.muTime));
        }
        else
        {
            Vector3 laCorners[4];
            for (u32 i = 0; i < 4; ++i)
                laCorners[i] = VectorFromBits(lrCase.mauBurstCorners[i]);
            lpModule->BurstAreaEmitParticles(laCorners, VectorFromBits(lrCase.mauBurstNormal),
                                             VectorFromBits(lrCase.mauBurstVel),
                                             static_cast<BrnParticle::Native::EDebrisArrayID>(lrCase.muBurstType),
                                             Float(lrCase.muTime), Float(lrCase.muBurstMin), Float(lrCase.muBurstMax),
                                             Float(lrCase.muBurstDensity));
        }

        // ---- 1. every SpawnDebris ----
        const std::vector<BrnParticle::DebrisSpawnRecord>& lrSpawns = lpModule->mParticleModule.mSpawns;
        bool lbSpawns = lrSpawns.size() == lrCase.muNumSpawns;
        u32 luShown = 0;
        for (u32 k = 0; lbSpawns && k < lrCase.muNumSpawns; ++k)
        {
            const GlassSpawnOut& lrWant = lrCase.mpSpawns[k];
            const BrnParticle::DebrisSpawnRecord& lrGot = lrSpawns[k];
            const bool lbVerbose = luShown < 2u;
            bool lbSame = lrGot.muType == lrWant.muType;
            lbSame = SameWords(&lrGot.mvPosition, lrWant.mauPos, 4, lbVerbose, "position") && lbSame;
            lbSame = SameWords(&lrGot.mvVelocity, lrWant.mauVel, 4, lbVerbose, "velocity") && lbSame;
            lbSame = SameWords(&lrGot.mvAxis, lrWant.mauAxis, 4, lbVerbose, "axis") && lbSame;
            lbSame = SameWords(&lrGot.mvColour, lrWant.mauColour, 4, lbVerbose, "colour") && lbSame;
            lbSame = SameWord(Bits(lrGot.mfSize), lrWant.muSize) && SameWord(Bits(lrGot.mfTime), lrWant.muTime) && lbSame;
            if (!lbSame)
            {
                if (lbVerbose)
                    std::printf("        spawn %u: type %u/%u size %08X/%08X time %08X/%08X\n", k, lrGot.muType,
                                lrWant.muType, Bits(lrGot.mfSize), lrWant.muSize, Bits(lrGot.mfTime), lrWant.muTime);
                ++luShown;
                lbSpawns = false;
            }
        }
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] every SpawnDebris is the console's (%u pieces; got %u)",
                      lrCase.mpcName, lrCase.muNumSpawns, static_cast<unsigned>(lrSpawns.size()));
        Check(lbSpawns, lacLabel);
        luTotalSpawns += lrCase.muNumSpawns;

        // ---- 2. every FireGlassEffect ----
        const std::vector<BrnEffects::GlassFireRecord>& lrFires = lpModule->mGlassSmashManager.mFires;
        bool lbFires = lrFires.size() == lrCase.muNumFires;
        for (u32 k = 0; lbFires && k < lrCase.muNumFires; ++k)
        {
            const GlassFireOut& lrWant = lrCase.mpFires[k];
            const BrnEffects::GlassFireRecord& lrGot = lrFires[k];
            bool lbSame = SameWords(&lrGot.mEffect, lrWant.mauMatrix, 16, true, "effect matrix");
            lbSame = SameWords(&lrGot.mVehicle, lrWant.mauTransform, 16, true, "car transform") && lbSame;
            lbSame = lrGot.muId == lrWant.muId && SameWord(Bits(lrGot.mfTime), lrWant.muTime) && lbSame;
            lbFires = lbFires && lbSame;
        }
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] every FireGlassEffect is the console's (%u shatters; got %u)",
                      lrCase.mpcName, lrCase.muNumFires, static_cast<unsigned>(lrFires.size()));
        Check(lbFires, lacLabel);
        luTotalFires += lrCase.muNumFires;

        // ---- 3. the replay appends (id, transform, four corners) ----
        bool lbSets = lpLayout->mSets.size() == lrCase.muNumSets;
        for (u32 k = 0; lbSets && k < lrCase.muNumSets; ++k)
        {
            const GlassSetOut& lrWant = lrCase.mpSets[k];
            const BrnEffects::GlassSetRecord& lrGot = lpLayout->mSets[k];
            bool lbSame = lrGot.muId == lrWant.muId && SameWords(lrGot.mauTransform, lrWant.mauTransform, 16, true, "set transform");
            for (u32 i = 0; i < 4; ++i)
                lbSame = SameWords(lrGot.mauCorners[i], lrWant.mauVecs[i], 4, true, "set corner") && lbSame;
            lbSets = lbSets && lbSame;
        }
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "[%s] the replay appends are the console's: id, transform, corners (%u; got %u)",
                      lrCase.mpcName, lrCase.muNumSets, static_cast<unsigned>(lpLayout->mSets.size()));
        Check(lbSets, lacLabel);

        // ---- 4. the replay reads ----
        bool lbGets = lpLayout->mGets.size() == lrCase.muNumGets;
        for (u32 k = 0; lbGets && k < lrCase.muNumGets; ++k)
            lbGets = lpLayout->mGets[k] == lrCase.mpGets[k];
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] the replay reads are the console's (%u; got %u)",
                      lrCase.mpcName, lrCase.muNumGets, static_cast<unsigned>(lpLayout->mGets.size()));
        Check(lbGets, lacLabel);

        // ---- 5. UpdateVehicleEffectPositions ----
        const std::vector<f32>& lrUpdates = lpModule->mGlassSmashManager.mUpdateTimes;
        const bool lbUpdates = lrUpdates.size() == lrCase.muNumUpdates
            && (lrCase.muNumUpdates == 0u
                || (Bits(lrUpdates[0]) == lrCase.muUpdateTime && lpModule->mGlassSmashManager.mpUpdateInput == &lInput));
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] UpdateVehicleEffectPositions runs as on the console (%u)",
                      lrCase.mpcName, lrCase.muNumUpdates);
        Check(lbUpdates, lacLabel);

        // ---- 6. mRandom after ----
        bool lbRandom = lpModule->mRandom.muSeed == lrCase.muSeedOut
                     && lpModule->mRandom.muOldestBufferIndex == lrCase.muIndexOut;
        for (u32 i = 0; i < 8; ++i)
            lbRandom = lbRandom && lpModule->mRandom.mauIntegerBuffer[i] == lrCase.mauRingOut[i];
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] mRandom's ring / seed / cursor match the console's",
                      lrCase.mpcName);
        Check(lbRandom, lacLabel);

        // ---- 7. asserts ----
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] %u assert(s), as on the console (got %u)", lrCase.mpcName,
                      lrCase.muAsserts, gAsserts);
        Check(gAsserts == lrCase.muAsserts, lacLabel);

        delete lpLayout;
        delete lpModule;
    }

    std::printf("(%u pieces and %u shatters compared; %u NOT RECONSTRUCTED announcement(s); %u [glass] line(s))\n",
                luTotalSpawns, luTotalFires, gNotReconstructed, gGlassLines);
    std::printf("FxCrashVfxGlass: %u checks, %u failures\n", gChecks, gFailures);
    delete lpDeformation;
    return gFailures == 0 ? 0 : 1;
}
