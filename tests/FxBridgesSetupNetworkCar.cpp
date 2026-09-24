// FX-BRIDGES (crash parity 2026-09-24) header request H-GS1: SetupNetworkCarAction carries the console's
// seventh member, the f32 base deformation at +0x38.
//
// X360 SetupNetworkCarAction::Construct @0x82355088 takes the value in f1 (`fmr f31, f1` @0x823550BC) and stores
// it at +0x38 (`stfs f31, 0x38(r31)` @0x82355140); ProcessGameEvents' case-7 arm passes the ChangeNetworkCarEvent's
// +0x18 float (`lfs f31, 0x18(r25)` @0x823A17D0) and posts the record as type 5, size 0x40 (@0x823A1878/0x823A187C);
// the world's action-5 arm HandleSetupNetworkCarAction reads +0x38 (`lfs f31, 0x38(r31)` @0x82305880) into the
// car's mfBaseDeformAmount (+0x7CC). The PC record had six members and a six-parameter Construct, so the value
// had nowhere to go. This TU compiles the PRODUCTION Construct (extracted from BrnGameActions.cpp by its
// signature) against the production header and checks every store, the new one included.
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace CgsDev
{
namespace Assert
{
    static int gAsserts = 0;
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnGameState
{
namespace GameStateModuleIO
{
#include "fxbridges_setup_network_car.inc"
}
}

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

static float FloatAt(const void* lpRecord, std::size_t luOffset)
{
    float lf;
    std::memcpy(&lf, static_cast<const unsigned char*>(lpRecord) + luOffset, sizeof(lf));
    return lf;
}

int main()
{
    using BrnGameState::GameStateModuleIO::SetupNetworkCarAction;
    alignas(16) unsigned char laRecord[sizeof(SetupNetworkCarAction)];
    std::memset(laRecord, 0xCD, sizeof(laRecord));
    SetupNetworkCarAction* lpAction = reinterpret_cast<SetupNetworkCarAction*>(laRecord);

    const Vector3 lPos = { 10.0f, 20.0f, 30.0f, 1.0f };
    const Vector3 lAt  = { 0.0f, 0.0f, 1.0f, 0.0f };
    lpAction->Construct(static_cast<BrnGameState::GameStateModuleIO::EPlayerScoringIndex>(2),
                        static_cast<EActiveRaceCarIndex>(3),
                        lPos, lAt, 0x1122334455667788ull, 0x99AABBCCDDEEFF00ull,
                        0.625f);

    Check(sizeof(SetupNetworkCarAction) == 0x40, "record size 0x40 (ProcessGameEvents li r6, 0x40 @0x823A1878)");
    Check(FloatAt(laRecord, 0x38) == 0.625f,
          "Construct stores the f1 argument at +0x38 (stfs f31, 0x38(r31) @0x82355140)");
    Check(FloatAt(laRecord, 0x00) == 10.0f && FloatAt(laRecord, 0x08) == 30.0f && FloatAt(laRecord, 0x10) == 0.0f
          && FloatAt(laRecord, 0x18) == 1.0f,
          "position at +0x00 (stvx128 v127 @0x82355144), at-vector at +0x10 (stvx128 v126 @0x82355158)");
    unsigned long long luModel = 0, luWheel = 0;
    std::memcpy(&luModel, laRecord + 0x20, 8);
    std::memcpy(&luWheel, laRecord + 0x28, 8);
    Check(luModel == 0x1122334455667788ull && luWheel == 0x99AABBCCDDEEFF00ull,
          "model id +0x20 (std r28 @0x82355150), wheel id +0x28 (std r26 @0x82355154)");
    int liActive = 0, liScoring = 0;
    std::memcpy(&liActive, laRecord + 0x30, 4);
    std::memcpy(&liScoring, laRecord + 0x34, 4);
    Check(liActive == 3 && liScoring == 2,
          "race-car index +0x30 (stw r29 @0x8235514C), scoring index +0x34 (stw r27 @0x82355148)");
    Check(CgsDev::Assert::gAsserts == 0, "valid arguments fire none of the three asserts (0x30F/0x310/0x311)");
    std::printf("FxBridgesSetupNetworkCar: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
