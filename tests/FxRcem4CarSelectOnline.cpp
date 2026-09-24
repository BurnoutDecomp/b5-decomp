// FX-RCEM4 (crash parity 2026-09-24, G60-D6): ActiveRaceCar::UpdateCarSelectStateOnline (ARTIST
// 0x822BFA30), the InputBuffer_PreScene car-select accessors (header inlines, h:856/:857 and
// h:866/:867) and PreSceneUpdate's per-slot car-select leg (0x8230E118..0x8230E170), extracted VERBATIM
// by run_fxrcem4_car_select_online.py. A piece the source lacks is replayed as a no-op.
//   0x822BFA44  !IsAttached -> stb 0, 0x79A ; return
//   0x822BFA88  lbz 0xA4 != 2 -> assert "Trying to update a non-network car car select state" (:1763)
//   0x822BFB04  (0x79A != 0) != (new != 0) -> stb 1, 0x79B ; stb new, 0x79A
//   0x8230E118  lbz +0x52+i (valid) ; beq ; lbz +0x4A+i (status) ; bl UpdateCarSelectStateOnline
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <string>

static unsigned guAssertions = 0;
static std::string gsLastAssertion;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; gsLastAssertion = lpcMessage; return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace Fixture {
enum ERaceCarType : u8 { E_RACE_CAR_TYPE_PLAYER = 0, E_RACE_CAR_TYPE_AI = 1, E_RACE_CAR_TYPE_NETWORK = 2,
                         E_RACE_CAR_TYPE_INACTIVE = 3 };
struct RaceCar {
    ERaceCarType muType = E_RACE_CAR_TYPE_NETWORK;
    ERaceCarType GetType() const { return muType; }
};
struct ActiveRaceCar {
    RaceCar* mpRaceCar = nullptr;
    bool mbIsInCarSelectOnline = false, mbCarSelectOnlineStateChanged = false;
    bool IsAttached() const { return mpRaceCar != nullptr; }
    RaceCar* GetGlobalRaceCar() const { CGS_ASSERT(IsAttached(), "IsAttached()"); return mpRaceCar; }
    void UpdateCarSelectStateOnline(bool lbInCarSelect);
};
#include "fxrcem4_cs_update.inc"

struct InputBuffer_PreScene {
    bool mabCarSelectStatus[8] = {};
    bool mabCarSelectStatusValid[8] = {};
#include "fxrcem4_cs_accessors.inc"
};

static void Pass(ActiveRaceCar* lpCars, const InputBuffer_PreScene* lpInput) {
    for (s32 liActivateSlot = 0; liActivateSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liActivateSlot)
    {
        const EActiveRaceCarIndex leActivateSlot = static_cast<EActiveRaceCarIndex>(liActivateSlot);
        ActiveRaceCar* lpActivateCar = &lpCars[liActivateSlot];
#include "fxrcem4_cs_leg.inc"
    }
}
}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main() {
    using namespace Fixture;

    // ---- the accessors ------------------------------------------------------------------------------
    {
        InputBuffer_PreScene in; in.mabCarSelectStatus[5] = true; in.mabCarSelectStatusValid[5] = true;
        guAssertions = 0;
        Check(in.IsCarSelectStatusValid(static_cast<EActiveRaceCarIndex>(5)) && in.GetCarSelectStatus(static_cast<EActiveRaceCarIndex>(5))
              && !in.IsCarSelectStatusValid(static_cast<EActiveRaceCarIndex>(4)) && guAssertions == 0,
              "IsCarSelectStatusValid reads +0x52+i, GetCarSelectStatus +0x4A+i");
        (void)in.GetCarSelectStatus(E_ACTIVE_RACE_CAR_INDEX_INVALID);
        Check(guAssertions == 1 && gsLastAssertion == "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0",
              "a -1 slot fires the >= INDEX_0 assert (h:856)");
        guAssertions = 0;
        (void)in.IsCarSelectStatusValid(E_ACTIVE_RACE_CAR_INDEX_COUNT);
        Check(guAssertions == 1 && gsLastAssertion == "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT",
              "slot 8 fires the < INDEX_COUNT assert (h:867)");
    }

    // ---- UpdateCarSelectStateOnline -----------------------------------------------------------------
    {
        RaceCar lNetwork; ActiveRaceCar car; car.mpRaceCar = &lNetwork;
        guAssertions = 0;
        car.UpdateCarSelectStateOnline(true);
        Check(car.mbIsInCarSelectOnline && car.mbCarSelectOnlineStateChanged && guAssertions == 0,
              "a network car entering the junkyard: latch 1 and arm the change (stb 1, 0x79B)");
        car.mbCarSelectOnlineStateChanged = false;
        car.UpdateCarSelectStateOnline(true);
        Check(car.mbIsInCarSelectOnline && !car.mbCarSelectOnlineStateChanged, "no change -> the change flag is not armed");
        car.UpdateCarSelectStateOnline(false);
        Check(!car.mbIsInCarSelectOnline && car.mbCarSelectOnlineStateChanged, "leaving the junkyard arms it again");

        ActiveRaceCar loose; loose.mbIsInCarSelectOnline = true;
        loose.UpdateCarSelectStateOnline(true);
        Check(!loose.mbIsInCarSelectOnline && !loose.mbCarSelectOnlineStateChanged && guAssertions == 0,
              "an unattached slot only clears the latch (0x822BFB44 stb 0, 0x79A)");

        RaceCar lAi; lAi.muType = E_RACE_CAR_TYPE_AI; ActiveRaceCar ai; ai.mpRaceCar = &lAi;
        ai.UpdateCarSelectStateOnline(true);
        Check(guAssertions == 1 && gsLastAssertion == "Trying to update a non-network car car select state" && ai.mbIsInCarSelectOnline,
              "a non-network car fires :1763 and still latches");
        guAssertions = 0;
    }

    // ---- PreSceneUpdate's per-slot leg -----------------------------------------------------------------
    {
        RaceCar laCars[8]; ActiveRaceCar laSlots[8];
        for (s32 i = 0; i < 8; ++i) laSlots[i].mpRaceCar = (i == 6) ? nullptr : &laCars[i];
        laSlots[6].mbIsInCarSelectOnline = true;
        InputBuffer_PreScene in;
        in.mabCarSelectStatusValid[2] = true; in.mabCarSelectStatus[2] = true;
        in.mabCarSelectStatusValid[6] = true; in.mabCarSelectStatus[6] = true;
        in.mabCarSelectStatus[3] = true;   // not valid: must be ignored
        Pass(laSlots, &in);
        Check(laSlots[2].mbIsInCarSelectOnline && laSlots[2].mbCarSelectOnlineStateChanged,
              "a slot with a valid status gets it (slot 2 enters the junkyard)");
        Check(!laSlots[3].mbIsInCarSelectOnline && !laSlots[3].mbCarSelectOnlineStateChanged,
              "a status without its valid byte is ignored (beq @0x8230E120)");
        Check(!laSlots[6].mbIsInCarSelectOnline, "every slot is visited: an unattached valid slot has its latch cleared");
        Check(guAssertions == 0, "no assertion on the pass");
    }

    std::printf("FxRcem4CarSelectOnline: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
