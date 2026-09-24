// FX-EMITTER (crash parity 2026-09-24): "is this StaticSoundEntity the one I am attached to?" --
// EmitterState::IsAttachedToThis against the ARTIST machine code.
//
//   BrnSound::Logic::World::EmitterState::IsAttachedToThis   0x826BADA0   (DWARF BrnEmitterState.cpp:112)
//
// EmitterStateManager::UpdateParams asks every live state this question for each queried entity; a
// "yes" suppresses a second attach. The console compares |test.xyz - mine.xyz| per lane against a
// splatted tolerance (`lvlx v0, unk_820AA0E0 ; vspltw128 v127, v0, 0` ; `vcmpgtfp128.`), the w lane
// masked out by `vrlimi128 v13, v0, 1, 1`, then the packed types (`srwi 16`). The tolerance at
// 0x820AA0E0 is 0x37800000 = 2^-16 = 1.52587890625e-05 (rwmath's SMALL_FLOAT); the PC carried an
// invented 0.01f "UNRECOVERED" placeholder, which merges distinct same-type entities within 1 cm.
//
// run_fxemitter_state_attached.py extracts the PRODUCTION IsAttachedToThis body and the file's
// anonymous-namespace constant block from BrnEmitterState.cpp and compiles them against a fixture
// EmitterState whose mEntity is the REAL StaticSoundEntity (BrnStaticSoundMap.h).
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Sound/World/BrnStaticSoundMap.h"

#include <cstdio>
#include <cstring>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnSound { namespace Logic { namespace World {

#include "fxemitter_state_constants.inc"

struct EmitterState
{
    BrnSound::World::StaticSoundEntity mEntity;
    bool mbAttached = true;
    bool IsAttached() const { return mbAttached; }
    bool IsAttachedToThis(void* lpvTestAttachment);
};

#include "fxemitter_state_body.inc"

} } }

namespace
{
unsigned guChecks = 0;
unsigned guFailures = 0;

void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass)
        ++guFailures;
    std::printf("%s %s\n", lbPass ? "ok   " : "FAIL ", lpcLabel);
}

BrnSound::World::StaticSoundEntity Entity(f32 lfX, f32 lfY, f32 lfZ, u32 luType, u32 luRadius)
{
    BrnSound::World::StaticSoundEntity lEntity;
    lEntity.mPosPlus.x = lfX;
    lEntity.mPosPlus.y = lfY;
    lEntity.mPosPlus.z = lfZ;
    const u32 luPacked = (luType << 16) | luRadius;   // ONE u32: type << 16 | radius
    std::memcpy(&lEntity.mPosPlus.w, &luPacked, sizeof(luPacked));
    return lEntity;
}

bool Ask(const BrnSound::World::StaticSoundEntity& lrMine, BrnSound::World::StaticSoundEntity lTest,
         bool lbAttached = true)
{
    BrnSound::Logic::World::EmitterState lState;
    lState.mEntity = lrMine;
    lState.mbAttached = lbAttached;
    return lState.IsAttachedToThis(&lTest);
}
} // namespace

int main()
{
    // Near 10 m the f32 ulp is ~9.5e-7, so offsets around 2^-16 are representable.
    const BrnSound::World::StaticSoundEntity lMine = Entity(10.0f, 2.0f, -3.0f, 29u, 173u);

    Check(Ask(lMine, lMine), "the same entity (bit-identical) is attached to this state");
    Check(Ask(lMine, Entity(10.0f + 1.0e-5f, 2.0f, -3.0f, 29u, 173u)),
          "x off by 1e-5 (< 2^-16): still this state");
    Check(!Ask(lMine, Entity(10.0f + 2.0e-5f, 2.0f, -3.0f, 29u, 173u)),
          "x off by 2e-5 (> 2^-16, the 0x820AA0E0 tolerance): a different entity");
    Check(!Ask(lMine, Entity(10.0f, 2.0f + 0.005f, -3.0f, 29u, 173u)),
          "y off by 5 mm: a different entity (the 0.01f placeholder merged it)");
    Check(!Ask(lMine, Entity(10.0f, 2.0f, -3.0f - 0.009f, 29u, 173u)),
          "z off by 9 mm: a different entity");
    Check(!Ask(lMine, Entity(10.0f, 2.0f, -3.0f + 0.02f, 29u, 173u)), "z off by 2 cm: a different entity");
    Check(!Ask(lMine, Entity(10.0f, 2.0f, -3.0f, 30u, 173u)), "same position, type 30 vs 29: a different entity");
    Check(Ask(lMine, Entity(10.0f, 2.0f, -3.0f, 29u, 12u)),
          "same position and type, other radius: this state (w lane masked, types compared)");

    guAsserts = 0;
    Ask(lMine, lMine, false);
    Check(guAsserts == 2, "not attached: both inlined GetSoundEntity() reads assert IsAttached() (2 asserts)");

    std::printf("FxEmitterStateAttached: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
