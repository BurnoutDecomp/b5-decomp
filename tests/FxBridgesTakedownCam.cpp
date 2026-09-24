// FX-BRIDGES (crash parity 2026-09-24) CC-8: the PRODUCTION body of
// BrnGame::BrnGameModule::BridgeGameStateToDirector @0x823CD170 (src/GameSource/Game/BrnGameModule.cpp,
// extracted verbatim by run_fxbridges_takedown_cam.py) compiled against the REAL director InputBuffer
// (its inline SetPlayerKiller, DWARF :226, and the two members it writes), the REAL TakedownEvent and
// EventQueue<TakedownEvent, 8>, and small fixtures for the two game-state accessors and the two
// game-action queues.
//
// Checked against the ARTIST asm, live arm 0x823CD330..0x823CD3FC:
//   0x823CD33C  player index = `lwzx gameStateOut + 0x2AEEC` (ScoringOutputInterface::mePlayerRaceCarIndex)
//   0x823CD344  count = the takedown queue's miLength, read ONCE (`lwz r27, 8(r3)`)
//   0x823CD380  per event: `lwz 4` (meVictimIndex) == player -> the killer `lwz 0` (meAggressorIndex);
//   0x823CD394  the :228 assert (`li r5, 0xE4`) fires on a -1 killer and does NOT gate the stores;
//   0x823CD3B8  `stb 1, 0x7AC0` + `stw killer, 0x7AAC` (the inlined SetPlayerKiller) -- a later match
//               overwrites an earlier one; a non-matching event touches nothing;
//   0x823CD3FC  THEN the game-action queue Append.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"          // the REAL InputBuffer
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"    // the REAL TakedownEvent
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // the [takedown-cam] witness (silent here)
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }   // the [takedown-cam] witness stays silent
}

using BrnDirector::DirectorIO::InputBuffer;
typedef CgsModule::EventQueue<BrnGameState::TakedownEvent, 8> TakedownQueue;

static s32 giSequence = 0;   // orders the setter against the Append

struct ScoringOutputFixture
{
    EActiveRaceCarIndex mePlayerRaceCarIndex;
};

struct GameActionQueueFixture
{
    s32 miTag;
};

struct DirectorActionQueueFixture
{
    const GameActionQueueFixture* mpAppended;
    s32                           miAppends;
    s32                           miAppendSequence;
    void Append(const GameActionQueueFixture& lrQueue)
    {
        mpAppended = &lrQueue;
        ++miAppends;
        miAppendSequence = ++giSequence;
    }
};

struct GameStateOutputFixture
{
    ScoringOutputFixture   mScoring;
    TakedownQueue          mTakedowns;
    GameActionQueueFixture mActions;
    const ScoringOutputFixture*   GetScoringOutputInterface() const { return &mScoring; }
    const TakedownQueue*          GetTakedownEventOutputQueue() const { return &mTakedowns; }
    const GameActionQueueFixture* GetGameActionQueue() const { return &mActions; }
};

// The director input the body writes: the REAL InputBuffer behind it. SetPlayerKiller forwards to the
// buffer's own inline setter; it is a template so a revision whose InputBuffer has no such setter (and
// whose body never calls it) still compiles -- and then fails the checks below instead.
struct DirectorInputFixture
{
    InputBuffer*               mpReal;
    DirectorActionQueueFixture mQueue;
    s32                        miSetterCalls;
    s32                        miLastSetterSequence;

    template <class T = InputBuffer>
    void SetPlayerKiller(EActiveRaceCarIndex lePlayerKillerCarIndex)
    {
        static_cast<T*>(mpReal)->SetPlayerKiller(lePlayerKillerCarIndex);
        ++miSetterCalls;
        miLastSetterSequence = ++giSequence;
    }
    DirectorActionQueueFixture* GetGameActionQueue() { return &mQueue; }
};

// The body under test, wrapped under its production parameter names.
static void Bridge(DirectorInputFixture* lpDirectorInput, const GameStateOutputFixture* lpGameStateOutput)
#include "fxbridges_takedown_cam_body.inc"

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

alignas(16) static unsigned char gaInputStorage[sizeof(InputBuffer)];
static GameStateOutputFixture gOutput;
static DirectorInputFixture   gInput;

static InputBuffer& Real() { return *reinterpret_cast<InputBuffer*>(gaInputStorage); }

// A fresh director input, seeded as InputBuffer::Construct @0x822393D0 seeds the pair
// (mePlayerKillerCarIndex = -1, mbPlayerTakenDown = 0), and an empty takedown queue.
static void Reset(EActiveRaceCarIndex lePlayer)
{
    std::memset(gaInputStorage, 0, sizeof(gaInputStorage));
    Real().mePlayerKillerCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    Real().mbPlayerTakenDown      = false;
    gInput.mpReal                = &Real();
    gInput.mQueue.mpAppended     = nullptr;
    gInput.mQueue.miAppends      = 0;
    gInput.mQueue.miAppendSequence = 0;
    gInput.miSetterCalls         = 0;
    gInput.miLastSetterSequence  = 0;
    giSequence                   = 0;
    gAsserts                     = 0;
    gOutput.mScoring.mePlayerRaceCarIndex = lePlayer;
    gOutput.mTakedowns.Construct();
    gOutput.mActions.miTag = 0x5EED;
}

static void AddTakedown(s32 liAggressor, s32 liVictim)
{
    BrnGameState::TakedownEvent lEvent;
    std::memset(&lEvent, 0, sizeof(lEvent));
    lEvent.meAggressorIndex = static_cast<EActiveRaceCarIndex>(liAggressor);
    lEvent.meVictimIndex    = static_cast<EActiveRaceCarIndex>(liVictim);
    gOutput.mTakedowns.AddEvent(lEvent);
}

int main()
{
    Check(offsetof(InputBuffer, mbPlayerTakenDown) == 0x7AC0 + InputBuffer::KU_HOOK_ENUMERATION_WIDENING
          && offsetof(InputBuffer, mePlayerKillerCarIndex) == 0x7AAC + InputBuffer::KU_HOOK_ENUMERATION_WIDENING,
          "the pair lives at the console's +0x7AC0 / +0x7AAC (plus the host's hook-enumeration widening)");

    // 1. No takedown this frame: nothing published, the queue still Appended once.
    Reset(E_ACTIVE_RACE_CAR_INDEX_0);
    Bridge(&gInput, &gOutput);
    Check(!Real().mbPlayerTakenDown && Real().mePlayerKillerCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID,
          "empty queue: the pair keeps its Construct seeds (0 / -1)");
    Check(gInput.mQueue.miAppends == 1 && gInput.mQueue.mpAppended == &gOutput.mActions,
          "the game-action queue is Appended exactly once, from the game-state output (0x823CD3FC)");

    // 2. The player (slot 0) is taken down by slot 2.
    Reset(E_ACTIVE_RACE_CAR_INDEX_0);
    AddTakedown(2, 0);
    Bridge(&gInput, &gOutput);
    Check(Real().mbPlayerTakenDown && Real().mePlayerKillerCarIndex == static_cast<EActiveRaceCarIndex>(2),
          "victim == player: mbPlayerTakenDown 1 and the killer is the aggressor (stb 1,0x7AC0 / stw 0x7AAC)");
    Check(gInput.miSetterCalls == 1 && gInput.miLastSetterSequence < gInput.mQueue.miAppendSequence,
          "the takedown pair is published BEFORE the game-action Append (0x823CD3B8 precedes 0x823CD3FC)");
    Check(gAsserts == 0, "a valid killer fires no :228 assert");

    // 3. The player takes someone ELSE down: not a player-taken-down frame.
    Reset(E_ACTIVE_RACE_CAR_INDEX_0);
    AddTakedown(0, 3);
    AddTakedown(4, 5);
    Bridge(&gInput, &gOutput);
    Check(!Real().mbPlayerTakenDown && Real().mePlayerKillerCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID
          && gInput.miSetterCalls == 0,
          "only an event whose VICTIM is the player publishes (`lwz 4` == player, 0x823CD380)");

    // 4. Two matching events in one frame: the later one wins.
    Reset(E_ACTIVE_RACE_CAR_INDEX_0);
    AddTakedown(2, 0);
    AddTakedown(6, 1);
    AddTakedown(5, 0);
    Bridge(&gInput, &gOutput);
    Check(Real().mbPlayerTakenDown && Real().mePlayerKillerCarIndex == static_cast<EActiveRaceCarIndex>(5)
          && gInput.miSetterCalls == 2,
          "every match stores; the LAST matching event's aggressor is the published killer");

    // 5. The player index is read from the scoring interface, not assumed to be slot 0.
    Reset(E_ACTIVE_RACE_CAR_INDEX_4);
    AddTakedown(1, 0);
    AddTakedown(7, 4);
    Bridge(&gInput, &gOutput);
    Check(Real().mbPlayerTakenDown && Real().mePlayerKillerCarIndex == static_cast<EActiveRaceCarIndex>(7)
          && gInput.miSetterCalls == 1,
          "the player is ScoringOutputInterface::mePlayerRaceCarIndex (lwzx +0x2AEEC), here slot 4");

    // 6. An invalid killer: the :228 tripwire fires, the stores still happen (the assert does not gate).
    Reset(E_ACTIVE_RACE_CAR_INDEX_0);
    AddTakedown(-1, 0);
    Bridge(&gInput, &gOutput);
    Check(gAsserts == 1 && Real().mbPlayerTakenDown
          && Real().mePlayerKillerCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID,
          "a -1 killer fires the :228 assert once and is still stored (0x823CD398 falls through to 0x823CD3B4)");

    std::printf("FxBridgesTakedownCam: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
