// crash parity FX-NETCRASH (2026-09-25), item 3 -- CrashModule::HandleNetworkCrashingTraffic's [netcrash] witness
// (FLAG PC, BRN_NETCRASH_DIAG; NOT console code): the production block, compiled into a stand-in scope that holds
// the locals it reads. It must keep TWO budgets of KI_NETCRASH_DIAG_MAX_LINES: frames that posted at least one
// update, and frames whose updates were all contentious (posted=0). With one shared budget the 40 posted=0 lines
// of run fxnetcrash_pair/20260925_131154 (each half's own lockstep swerve crash, owned locally on both machines)
// silenced every later posted frame -- the line the pair case reads.
#include "types.hpp"
#include <cstdio>
#include <string>

static unsigned gChecks = 0, gFailures = 0;

// ---- the log stream: keeps the text so the checks can count and read the lines -------------------------
static std::string gLog;
namespace CgsDev
{
namespace Log
{
    struct DebugPrintFixture
    {
        DebugPrintFixture& operator<<(const char* lpcText) { gLog += lpcText; return *this; }
        DebugPrintFixture& operator<<(s32 liValue) { gLog += std::to_string(liValue); return *this; }
        DebugPrintFixture& operator<<(u32 luValue) { gLog += std::to_string(luValue); return *this; }
        DebugPrintFixture& operator<<(f32 lfValue)
        {
            char lacText[48];
            std::snprintf(lacText, sizeof(lacText), "%f", static_cast<double>(lfValue));
            gLog += lacText;
            return *this;
        }
    };
    DebugPrintFixture  gDebugPrint;
    DebugPrintFixture* gpDebugPrint = &gDebugPrint;
}
}

// ---- what the block reads: the network input's queue, the three per-player sets, the player index ------
namespace CrashIO
{
    struct Vector    { f32 x, y, z, w; };
    struct Transform { Vector xAxis, yAxis, zAxis, wAxis; };
    struct CrashingTrafficUpdateEvent
    {
        Transform mTransform;
        u16       muVehicleId;
    };
    struct CrashingTrafficUpdateQueue
    {
        s32                        miLength;
        CrashingTrafficUpdateEvent maEvents[2];
        s32 GetLength() const { return miLength; }
        const CrashingTrafficUpdateEvent& GetEvent(s32 liIndex) const { return maEvents[liIndex]; }
    };
}

struct FakeSet
{
    u32 muLength;
    u32 GetLength() const { return muLength; }
};

enum EActiveRaceCarIndex { E_ACTIVE_RACE_CAR_INDEX_0, E_ACTIVE_RACE_CAR_INDEX_1 };

static bool gbDiag = true;
bool NetCrashDiagEnabled() { return gbDiag; }

#include "budget_constant.inc"   // the production KI_NETCRASH_DIAG_MAX_LINES

// One HandleNetworkCrashingTraffic frame for one network player, as far as the witness can see it.
static void Frame(const CrashIO::CrashingTrafficUpdateQueue* lpCrashingTrafficQueue, u32 luPosted, u32 luNew,
                  u32 luCleared)
{
    const EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_1;
    const FakeSet lCrashingTrafficForPlayer = { luPosted };
    const FakeSet lNewCrashingTraffic       = { luNew };
    const FakeSet lClearedUpTraffic         = { luCleared };
#include "budget_block.inc"
}

static unsigned Count(const char* lpcNeedle)
{
    unsigned luCount = 0;
    for (std::string::size_type luAt = gLog.find(lpcNeedle); luAt != std::string::npos;
         luAt = gLog.find(lpcNeedle, luAt + 1))
    {
        ++luCount;
    }
    return luCount;
}

static void Check(bool lbPass, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", lpcWhat);
    }
}

int main()
{
    CrashIO::CrashingTrafficUpdateQueue lQueue = {};
    lQueue.miLength = 2;
    lQueue.maEvents[0].muVehicleId = 333;
    lQueue.maEvents[0].mTransform.wAxis = { 3380.0f, 0.5f, -1449.5f, 1.0f };
    lQueue.maEvents[1].muVehicleId = 148;

    const char* const lpcLine = "[netcrash] HandleNetworkCrashingTraffic player=";
    const unsigned luCap = static_cast<unsigned>(KI_NETCRASH_DIAG_MAX_LINES);
    Check(KI_NETCRASH_DIAG_MAX_LINES == 40, "the budget is still KI_NETCRASH_DIAG_MAX_LINES = 40 lines");

    // ---- the gates in front of both budgets -------------------------------------------------------------
    gbDiag = false;
    Frame(&lQueue, 1, 1, 0);
    Check(Count(lpcLine) == 0, "BRN_NETCRASH_DIAG off: no line");
    gbDiag = true;

    CgsDev::Log::gpDebugPrint = nullptr;
    Frame(&lQueue, 1, 1, 0);
    CgsDev::Log::gpDebugPrint = &CgsDev::Log::gDebugPrint;
    Check(Count(lpcLine) == 0, "no log stream: no line");

    CrashIO::CrashingTrafficUpdateQueue lEmpty = {};
    Frame(&lEmpty, 0, 0, 0);
    Check(Count(lpcLine) == 0, "a frame with no update from that player: no line");

    // ---- 45 frames whose updates were all contentious (the _131154 guest) ---------------------------------
    for (int liFrame = 0; liFrame < 45; ++liFrame)
    {
        Frame(&lQueue, 0, 0, 0);
    }
    Check(Count(lpcLine) == luCap && Count(" posted=0 ") == luCap,
          "posted=0 frames: exactly KI_NETCRASH_DIAG_MAX_LINES lines, then their budget is spent");

    // ---- then the other player's wreck is applied --------------------------------------------------------
    Frame(&lQueue, 1, 1, 0);
    Check(Count(" posted=1 ") == 1,
          "the first POSTED frame after 40 contentious ones still gets its line (its own budget)");
    Check(gLog.find("[netcrash] HandleNetworkCrashingTraffic player=1 updates=2 posted=1 new=1 cleared=0 first=333 "
                    "pos=(3380.000000, 0.500000, -1449.500000) [FLAG PC witness]\n") != std::string::npos,
          "the posted line reads player / updates / posted / new / cleared / the first event's id and position");

    for (int liFrame = 0; liFrame < 44; ++liFrame)
    {
        Frame(&lQueue, 2, 0, 1);
    }
    Check(Count(" posted=1 ") + Count(" posted=2 ") == luCap,
          "posted frames: exactly KI_NETCRASH_DIAG_MAX_LINES lines of their own");

    Frame(&lQueue, 0, 0, 0);
    Frame(&lQueue, 1, 0, 0);
    Check(Count(lpcLine) == 2 * luCap, "both budgets spent: no more lines of either kind");

    std::printf("FxNetcrashWitnessBudget: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
