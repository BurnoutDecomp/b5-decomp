#include <algorithm>
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>
#include "GameSource/Effects/Particles/Native/ParticleFramePC.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

using namespace BrnParticle::Native;
static int checks, failures;
static void Check(bool value, const char* name) {
    ++checks; if (!value) { ++failures; std::printf("FAIL %s\n", name); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++failures; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnParticle { namespace Native {
static const f32 KF_TWO_PI = 6.2831855f, KF_ZERO = 0.0f;
u32 gauSimpleParticleSpawned = 0;
#include "spawn_particle.inc"
} }

int main() {
    BrnSimpleParticleArray source[eParticleArray_Max] = {};
    CB4Particle records[32] = {}, crashRecords[16] = {};
    CB4ParticleArrayStandardParams params = {};
    source[1].mpStandardParams = &params;
    source[1].mbIsReady = true;
    source[1].mBankRegular = {records, 32, 31, -9999.0f};
    source[1].mBankCrash = {crashRecords, 16, 15, -9999.0f};
    params.mrLifeTime = 3.0f;
    source[1].SpawnParticle({1,2,3,0}, {4,5,6,0}, 10.0f, 2.0f, 0.5f, false, 0.75f);
    source[1].SpawnParticle({7,8,9,0}, {}, 11.0f, 1.0f, 0.0f, true, 1.0f);
    SimpleParticleFramePC frame;
    frame.Prepare(source);
    auto& frozen = frame.GetArrays()[1];
    const auto frozenRecord = frozen.mBankRegular.mpaParticles[31];
    Check(frozen.mBankRegular.mpaParticles != records, "regular bank owns its published storage");
    Check(frozen.mBankCrash.mpaParticles != crashRecords, "crash bank owns its published storage");
    Check(frozen.mBankRegular.mrLastSpawnTime == 10 && frozen.mBankCrash.mrLastSpawnTime == 11,
          "both bank timestamps belong to the published frame");
    Check(frozen.mpStandardParams != &params && frozen.mpStandardParams->mrLifeTime == 3,
          "authored parameters are copied with the bank");
    std::atomic<bool> start{false}, done{false};
    std::thread producer([&] {
        while (!start.load()) std::this_thread::yield();
        for (unsigned i=0;i<20000;++i)
            source[1].SpawnParticle({float(i),2,3,0}, {}, float(i+12), 2, 0, false, 1);
        params.mrLifeTime = 8;
        done = true;
    });
    bool unchanged = true; unsigned reads = 0;
    start = true;
    do {
        unchanged &= std::memcmp(&frozenRecord, &frozen.mBankRegular.mpaParticles[31], sizeof(frozenRecord)) == 0;
        unchanged &= frozen.mBankRegular.mrLastSpawnTime == 10 && frozen.mpStandardParams->mrLifeTime == 3;
        ++reads;
    } while (!done.load());
    producer.join();
    Check(unchanged && reads > 0, "next-frame ring overwrites cannot change dispatch's frame");
    auto* storage = frozen.mBankRegular.mpaParticles;
    frame.Publish(source);
    Check(frozen.mBankRegular.mpaParticles == storage, "steady-state publication reuses storage");
    Check(frozen.mBankRegular.mrLastSpawnTime == 20011 && frozen.mpStandardParams->mrLifeTime == 8,
          "joined publication exposes the next frame");

    CgsNumeric::Random random, reference; random.Construct(); reference.Construct();
    ParticleRandomAccessPC access;
    std::vector<u32> first(10000), second(10000), expected(20000);
    for (auto& value : expected) value = reference.RandomUInt();
    start = false;
    auto draw = [&](std::vector<u32>& output) {
        while (!start.load()) std::this_thread::yield();
        for (auto& value : output)
            value = access.Execute(random, [](CgsNumeric::Random& rng) { return rng.RandomUInt(); });
    };
    std::thread a(draw, std::ref(first)), b(draw, std::ref(second));
    start = true; a.join(); b.join();
    first.insert(first.end(), second.begin(), second.end());
    std::sort(first.begin(), first.end()); std::sort(expected.begin(), expected.end());
    Check(first == expected, "concurrent draws consume exactly the original RNG stream");
    Check(random.RandomUInt() == reference.RandomUInt(), "shared RNG retains its exact final state");
    std::printf("PCParticleFrame: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
