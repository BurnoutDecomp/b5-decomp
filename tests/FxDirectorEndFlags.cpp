// FX-DIRECTOR (crash parity 2026-09-24): the PRODUCTION end-of-event arm of
// BrnGame::BrnGameModule::BridgeGameStateToDirector @0x823CD170 (live arm 0x823CD4A0..0x823CD510),
// extracted from src/GameSource/Game/BrnGameModule.cpp by run_fxdirector_end_flags.py and run
// against the REAL BrnGameState::GameStateModuleIO::ScoringOutputInterface (the member names the arm
// reads are the revision's own) and a stand-in director input carrying the two setters it calls.
//
// Checked against the ARTIST asm (r31 = gameStateOut, r23 = directorIn, r19 = 1 from 0x823CD1A8,
// r25 = ScoringOutputInterface::mePlayerRaceCarIndex, `lwzx +0x2AEEC` @0x823CD33C):
//   0x823CD4A0  `add. r11, r31, 0x2A4B8` -- the non-gating accessor assert (GameBridgeGameStateToX.cpp:256)
//   0x823CD4D8  `lbzx +0x2AEB8 + player` (ScoringOutputInterface +0xA00 mabPlayerEliminated[player])
//               -> `stb 0x7AD5`
//   0x823CD4E4  `lfsx +0x2AF4C` (+0xA94 mfModeTimeRemaining) `fcmpu` flt_82001CC0 (0.0f): `bgt` -> 0;
//               else `lbzx +0x2AF60` (+0xAA8 mbTimerActive) != 0 -> 1, else 0 -> `stb 0x7AD6`.
//               NaN is unordered, so it is not `gt` and takes the mbTimerActive arm.
// A revision without the arm compiles an empty body: every "raises" check then fails.
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Message
{
    u64 gxMessageFilterFlags = 0;
}
}

namespace FxEndFlags
{
    using ScoringOutputInterface = BrnGameState::GameStateModuleIO::ScoringOutputInterface;

    // The director input's two setters, counting their calls (the console stores both every frame).
    struct DirectorInputStandIn
    {
        bool mbPlayerEliminated;
        bool mbModeTimeExpired;
        int  miEliminatedStores;
        int  miExpiredStores;
        void SetPlayerEliminated(bool lbPlayerEliminated) { mbPlayerEliminated = lbPlayerEliminated; ++miEliminatedStores; }
        void SetModeTimeExpired(bool lbModeTimeExpired)   { mbModeTimeExpired  = lbModeTimeExpired;  ++miExpiredStores; }
    };

    // The game-state output: only the scoring snapshot the arm reads. Raw storage, because
    // CarScoreData's constructor (0x822A45A8) is declared-only on this build.
    struct GameStateOutputStandIn
    {
        alignas(16) unsigned char maStorage[sizeof(ScoringOutputInterface)];
        ScoringOutputInterface& Scoring() { return *reinterpret_cast<ScoringOutputInterface*>(maStorage); }
        const ScoringOutputInterface* GetScoringOutputInterface() const
        {
            return reinterpret_cast<const ScoringOutputInterface*>(maStorage);
        }
    };

    static void PublishEndOfEventPair(DirectorInputStandIn* lpDirectorInput, const GameStateOutputStandIn* lpGameStateOutput)
    {
        (void)lpDirectorInput;
        (void)lpGameStateOutput;
#include "bridge_end_flags.inc"
    }
}

using namespace FxEndFlags;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static GameStateOutputStandIn gOutput;

// A cleared snapshot (0xCD junk everywhere else, like a live buffer's unrelated members) with the
// three members the arm reads set.
static void Snapshot(s32 liPlayer, const bool (&labEliminated)[8], f32 lfTimeRemaining, bool lbTimerActive)
{
    std::memset(gOutput.maStorage, 0xCD, sizeof(gOutput.maStorage));
    ScoringOutputInterface& lrScoring = gOutput.Scoring();
    lrScoring.mePlayerRaceCarIndex = static_cast<EActiveRaceCarIndex>(liPlayer);
    for (int i = 0; i < 8; ++i)
    {
        lrScoring.mabPlayerEliminated[i] = labEliminated[i];
    }
    lrScoring.mfModeTimeRemaining = lfTimeRemaining;
    lrScoring.mbTimerActive       = lbTimerActive;
}

static DirectorInputStandIn Publish()
{
    DirectorInputStandIn lInput = { false, false, 0, 0 };
    PublishEndOfEventPair(&lInput, &gOutput);
    return lInput;
}

int main()
{
    const bool KAB_NONE[8] = { false, false, false, false, false, false, false, false };
    const f32  KF_NAN      = std::numeric_limits<f32>::quiet_NaN();
    const f32  KF_INF      = std::numeric_limits<f32>::infinity();
    const f32  KF_DENORMAL = std::numeric_limits<f32>::denorm_min();

    // 1. Both stores happen every frame, even when both flags are 0.
    Snapshot(0, KAB_NONE, 30.0f, true);
    {
        const DirectorInputStandIn lInput = Publish();
        Check(lInput.miEliminatedStores == 1 && lInput.miExpiredStores == 1,
              "both flags are stored once per bridge call (stb 0x7AD5 / stb 0x7AD6, no condition around them)");
        Check(!lInput.mbPlayerEliminated && !lInput.mbModeTimeExpired,
              "nobody eliminated, 30 s left on an active clock -> @0x7AD5 = 0, @0x7AD6 = 0");
    }

    // 2. @0x7AD5 = mabPlayerEliminated[mePlayerRaceCarIndex] -- the PLAYER's slot, nobody else's.
    {
        bool lbOwnSlot = true, lbOtherSlots = true;
        for (int liPlayer = 0; liPlayer < 8; ++liPlayer)
        {
            bool labOnlyPlayer[8] = { false, false, false, false, false, false, false, false };
            labOnlyPlayer[liPlayer] = true;
            Snapshot(liPlayer, labOnlyPlayer, 30.0f, true);
            lbOwnSlot = lbOwnSlot && Publish().mbPlayerEliminated;

            bool labAllButPlayer[8] = { true, true, true, true, true, true, true, true };
            labAllButPlayer[liPlayer] = false;
            Snapshot(liPlayer, labAllButPlayer, 30.0f, true);
            lbOtherSlots = lbOtherSlots && !Publish().mbPlayerEliminated;
        }
        Check(lbOwnSlot, "0x823CD4D8: the player's own mabPlayerEliminated slot raises @0x7AD5 (players 0..7)");
        Check(lbOtherSlots, "0x823CD4D8: every other car eliminated leaves @0x7AD5 at 0 (players 0..7)");
    }
    {
        const bool labSome[8] = { false, true, false, false, true, false, false, false };
        Snapshot(4, labSome, 30.0f, true);
        const DirectorInputStandIn lInput = Publish();
        Check(lInput.mbPlayerEliminated && !lInput.mbModeTimeExpired,
              "player 4 eliminated with the clock running -> @0x7AD5 = 1, @0x7AD6 = 0 (independent)");
    }

    // 3. @0x7AD6 = mfModeTimeRemaining > 0.0f ? 0 : mbTimerActive (fcmpu / bgt, flt_82001CC0 == 0.0f).
    struct TimeCase { f32 mfRemaining; bool mbActive; bool mbExpected; const char* mpcName; };
    const TimeCase KA_TIME_CASES[] =
    {
        {  30.0f,       true,  false, "30 s left, clock active -> 0 (bgt taken)" },
        {  0.0f,        true,  true,  "0.0 left, clock active -> 1 (0.0 is not > 0.0: the clock ran out)" },
        { -0.0f,        true,  true,  "-0.0 left, clock active -> 1" },
        { -1.0f,        true,  true,  "a negative remainder, clock active -> 1" },
        {  KF_DENORMAL, true,  false, "the smallest positive remainder (denormal) is > 0.0 -> 0" },
        {  KF_INF,      true,  false, "+inf left -> 0" },
        {  KF_NAN,      true,  true,  "NaN left, clock active -> 1 (unordered is not gt: the mbTimerActive arm)" },
        {  0.0f,        false, false, "0.0 left, clock inactive -> 0 (lbzx +0x2AF60 == 0)" },
        {  KF_NAN,      false, false, "NaN left, clock inactive -> 0" },
        { -1.0f,        false, false, "a negative remainder, clock inactive -> 0" },
        {  30.0f,       false, false, "30 s left, clock inactive -> 0" },
    };
    for (const TimeCase& lrCase : KA_TIME_CASES)
    {
        Snapshot(2, KAB_NONE, lrCase.mfRemaining, lrCase.mbActive);
        const DirectorInputStandIn lInput = Publish();
        Check(lInput.mbModeTimeExpired == lrCase.mbExpected && !lInput.mbPlayerEliminated, lrCase.mpcName);
    }

    // 4. Both at once.
    {
        const bool labPlayer[8] = { false, false, false, true, false, false, false, false };
        Snapshot(3, labPlayer, 0.0f, true);
        const DirectorInputStandIn lInput = Publish();
        Check(lInput.mbPlayerEliminated && lInput.mbModeTimeExpired,
              "player 3 eliminated as the clock runs out -> both flags 1");
    }

    Check(gAsserts == 0, "no assert fired (the scoring snapshot is present)");

    std::printf("FxDirectorEndFlags: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
