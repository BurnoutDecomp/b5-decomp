// Selected production cache handler/getter with only the surrounding cache omitted.
// Event 81 is a four-byte car-select start; event 77 is the ticker's one-byte signal.
#include <cstdint>
#include <cstdio>
#include <cstring>
using s32 = int32_t;
static unsigned guAsserts;
#define CGS_ASSERT(c,m) do { if (!(c)) ++guAsserts; } while (0)
namespace CgsModule { struct Event {}; }
struct Cache {
    s32 meCarSelectType = 1;
    void RecEvent(const CgsModule::Event* lpEvent, s32 liEventId) {
        switch (liEventId) {
            #include "junkyard_car_select_type.inc"
            default: break;
        }
    }
    #include "junkyard_car_select_getter.inc"
};
int main() {
    unsigned luChecks = 0, luFailures = 0;
    auto Check = [&](bool lbGood, const char* lpcName) {
        ++luChecks;
        if (!lbGood) { ++luFailures; std::printf("FAIL %s\n", lpcName); }
    };
    Cache lCache;
    s32 liPayload = 0; // Four-byte backing keeps the old wrong handler's read in-bounds.
    lCache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&liPayload), 77);
    Check(lCache.GetCurrentCarSelectType() == 1, "unlock ticker cannot clear the junkyard type");
    Check(guAsserts == 0, "vehicle screen can open after the unlock ticker closes");
    liPayload = 2;
    lCache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&liPayload), 81);
    Check(lCache.GetCurrentCarSelectType() == 2, "online selection start replaces the cached type");
    liPayload = 0;
    lCache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&liPayload), 77);
    Check(lCache.GetCurrentCarSelectType() == 2, "ticker cannot replace an online selection type");
    liPayload = 1;
    lCache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&liPayload), 81);
    Check(lCache.GetCurrentCarSelectType() == 1, "next junkyard entry restores the offline type");
    liPayload = 0;
    lCache.RecEvent(reinterpret_cast<const CgsModule::Event*>(&liPayload), 77);
    Check(lCache.GetCurrentCarSelectType() == 1, "later unlocks keep livery acceptance enabled");
    Check(guAsserts == 0, "all valid starts and ticker closures preserve the getter contract");
    std::printf("JunkyardCarSelectType: %u checks, %u failures\n", luChecks, luFailures);
    return luFailures ? 1 : 0;
}
