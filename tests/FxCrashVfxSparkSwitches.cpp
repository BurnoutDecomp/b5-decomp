// FX-CRASHVFX (crash parity 2026-09-24): the two spark "lbDisableThisEffect" switches -- EffectsModule::
// HandleSparkContacts @0x822906A8 (byte_82CDB40C) and EffectsModule::HandleRaceCarRaceCarSparks @0x82290A48
// (byte_82CDB40D). Both are initialised .data holding 0x01 in the ARTIST image and nothing writes them, so on the
// console both functions return at their first test and post nothing.
//
// run_fxcrashvfx_spark_switches.py extracts the PRODUCTION HandleSparkContacts body (and the constant it names) and
// compiles it onto a fixture carrying exactly what it touches: the real CgsNumeric::Random, four sparkeffect blocks,
// and a recording inter-thread queue. It is called with the very contact gen_switch_data.py hands the console's
// words -- a friction stress of 10 against a threshold of 7.5, which the body would otherwise turn into a
// 0x50-byte SpawnSparksAlongLine record after two random draws.
//
// The expected values are the CONSOLE'S OWN OUTPUTS (FxCrashVfxSparkSwitchesData.h, written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_switch_data.py from 0x822906A8's and 0x82290A48's real words with
// the image's data page): no record posted and the random stream untouched. The counterfactual (both bytes forced
// to 0) is recorded beside it -- one record each, types 3 and 1 -- to show the fixture's inputs DO spark when the
// switch is off.
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"
#include "GameSource/Effects/Particles/BrnParticleModuleIO_EventTypes.h"
#include "GameSource/AttribSys/Generated/classes/sparkeffect.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "FxCrashVfxSparkSwitchesData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

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
}

namespace
{
    alignas(16) unsigned char gaDefaultArea[0x100];
    alignas(16) unsigned char gaSparkLayouts[4][0x100];
}

namespace Attrib
{
    Instance::Instance(Collection* lpCollection, void* lpOwner)
        : mpCollection(lpCollection), mpAttributeData(nullptr), mpOwner(lpOwner), muFlags(0)
    {
    }
    Instance::~Instance() {}
    int  Instance::GetClass() const { return 0; }
    u64  Instance::GetCollection() const { return 0; }
    void* DefaultDataArea(u32) { return gaDefaultArea; }
    Collection* FindCollection(u64, u64) { return nullptr; }
}

namespace BrnParticle
{
    u32 gauSparkContactCalls    = 0;
    u32 gauSparkContactRejected = 0;
    u32 gauSparkContactPosted   = 0;

    struct RecordingQueue
    {
        std::vector<s32> mTypes;
        std::vector<s32> mSizes;
        template <class T>
        bool AddEventSafe(const T*, s32 liType, s32 liSize)
        {
            mTypes.push_back(liType);
            mSizes.push_back(liSize);
            return true;
        }
    };
    struct FakeParticleModule
    {
        RecordingQueue mInterThreadEventQueue;
    };
}

namespace BrnEffects
{
    namespace
    {
#include "fxcrashvfx_switch_consts.inc"
    }

    class EffectsModule
    {
    public:
        BrnParticle::FakeParticleModule mParticleModule;
        CgsNumeric::Random              mRandom;
        Attrib::Gen::sparkeffect        mSparkParams[4];

        void HandleSparkContacts(const BrnPhysics::ContactSpy::BaseContact& lrContact,
                                 Vector3 lvVelocity,
                                 BrnParticle::Native::ESparkArrayID leSparkType,
                                 f32 lfDt,
                                 f32 lfTime,
                                 f32 lfGroundPositionY,
                                 f32 lfMinFrictionStress,
                                 f32 lfSurfaceSparkScale,
                                 bool lbIsCrashing);
    };

#include "fxcrashvfx_switch_body.inc"
}

static Vector3 V(f32 x, f32 y, f32 z, f32 w)
{
    Vector3 lv;
    lv.x = x; lv.y = y; lv.z = z; lv.w = w;
    return lv;
}

int main()
{
    Check(kuSparkSwitchImageBytes == 0x0101u,
          "the console's image holds 0x01 0x01 at byte_82CDB40C / byte_82CDB40D (both switches TRUE)");

    BrnEffects::EffectsModule* lpModule = new BrnEffects::EffectsModule();
    // The same stream gen_switch_data.py seeds the console's mRandom with.
    const u32 lauRing[8] = { 0x3F8A1B2Cu, 0x3FC00001u, 0x3F912345u, 0x3FF00000u,
                             0x3F800001u, 0x3FAAAAAAu, 0x3FD55555u, 0x3F999999u };
    for (u32 i = 0; i < 8; ++i)
        lpModule->mRandom.mauIntegerBuffer[i] = lauRing[i];
    lpModule->mRandom.muSeed              = 0x0123456789ABCDEFull;
    lpModule->mRandom.muOldestBufferIndex = 3;
    for (u32 i = 0; i < 4; ++i)
    {
        for (u32 luWord = 0; luWord < 0x40; ++luWord)
        {
            const f32 lf = 0.5f + 0.25f * static_cast<f32>(luWord % 5);
            std::memcpy(&gaSparkLayouts[i][4 * luWord], &lf, 4);
        }
        lpModule->mSparkParams[i].mpAttributeData = gaSparkLayouts[i];
    }

    alignas(16) BrnPhysics::ContactSpy::BaseContact lContact;
    std::memset(&lContact, 0, sizeof(lContact));
    lContact.mFrictionStress = V(6.0f, 0.0f, 8.0f, 0.0f);
    lContact.mNormalStress   = V(0.0f, 5.0f, 0.0f, 0.0f);
    lContact.mNormal         = V(0.0f, 1.0f, 0.0f, 0.0f);
    lContact.mPointOnA       = V(1.0f, 2.0f, 3.0f, 1.0f);
    lContact.mPointOnB       = V(1.1f, 2.0f, 3.0f, 1.0f);

    const u64 luSeedBefore = lpModule->mRandom.muSeed;
    const unsigned luAssertsBefore = gAsserts;
    lpModule->HandleSparkContacts(lContact, V(20.0f, 0.0f, 5.0f, 0.0f),
                                  BrnParticle::Native::eSparkArray_GrindingWorld,
                                  1.0f / 60.0f, 12.5f, 2.0f, 7.5f, 1.0f, true);

    const SparkSwitchRow& lrRow = kaSparkSwitchRows[0];
    char lacLabel[320];
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "HandleSparkContacts posts what the console posts for a sparking contact: %u record(s) (console "
                  "%u; with the switch forced off the console posts %u of type %u, 0x%X bytes)",
                  static_cast<unsigned>(lpModule->mParticleModule.mInterThreadEventQueue.mTypes.size()),
                  lrRow.muImagePosts, lrRow.muForcedPosts, lrRow.muForcedType, lrRow.muForcedSize);
    Check(lpModule->mParticleModule.mInterThreadEventQueue.mTypes.size() == lrRow.muImagePosts, lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "HandleSparkContacts leaves mRandom where the console does (moved %u, console %u)",
                  lpModule->mRandom.muSeed != luSeedBefore ? 1u : 0u, lrRow.muImageRngMoved);
    Check((lpModule->mRandom.muSeed != luSeedBefore ? 1u : 0u) == lrRow.muImageRngMoved, lacLabel);
    Check(gAsserts == luAssertsBefore, "no asserts");
    Check(kaSparkSwitchRows[1].muImagePosts == 0u,
          "HandleRaceCarRaceCarSparks posts nothing on the console (its body runs in run_fxcrashvfx_race_car_contacts)");

    delete lpModule;
    (void)Bits; (void)Float;
    std::printf("FxCrashVfxSparkSwitches: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
