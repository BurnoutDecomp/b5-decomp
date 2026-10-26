#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "SharedClasses/Traffic/BrnTrafficFlowType.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleAsset.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>

int giChecks = 0, giFailures = 0, giAsserts = 0;
void Check(bool pass, const char* label) {
    ++giChecks;
    if (!pass) { ++giFailures; std::printf("FAIL %s\n", label); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* text, const char*, int) {
    ++giAsserts; std::printf("ASSERT %s\n", text); return 0;
}
void* EndAssert() { return nullptr; }
}}
namespace BrnTraffic {
struct RandomTape {
    u32 muSelection = 0, muBigRoll = 99, muCalls = 0;
    u32 RandomUInt() { return muCalls++ == 0 ? muSelection : muBigRoll; }
};
struct Picker {
    TrafficData* mpData = nullptr;
    s32 miDEBUGFlowtypeOverride = -1;
    s32 miBigVehicleAmount = 100;
    bool mbPlayingShowtimeMode = false;
    RandomTape mRand;
    u8 PickVehicleToSpawn(u32 luFlowTypeId);
};
#include "showtime_traffic_selection.inc"
}
int main() {
    using namespace BrnTraffic;
    TrafficData data{};
    FlowType flow{}, busFlow{};
    FlowType* flows[2] = {&flow, &busFlow};
    u16 typeIds[4] = {4, 9, 6, 11}, busType[1] = {9};
    u8 cumulative[4] = {63, 127, 191, 255}, busCumulative[1] = {255};
    VehicleTypeData types[12]{};
    VehicleAsset assets[4]{};
    data.muNumFlowTypes = 2;
    data.mpapFlowTypes = flows;
    data.mpaVehicleTypes = types;
    data.mpaVehicleAssets = assets;
    flow.muNumVehicleTypes = 4;
    flow.mpauVehicleTypeIds = typeIds;
    flow.mpauCumulativeProb = cumulative;
    busFlow.muNumVehicleTypes = 1;
    busFlow.mpauVehicleTypeIds = busType;
    busFlow.mpauCumulativeProb = busCumulative;
    for (int i = 0; i < 4; ++i) {
        types[typeIds[i]].muAssetId = static_cast<u8>(i);
        assets[i].mVehicleId = static_cast<CgsID>(100 + i);
    }
    types[4].muVehicleClass = static_cast<decltype(types[4].muVehicleClass)>(0);
    types[9].muVehicleClass = E_VEHICLECLASS_BUS;
    types[6].muVehicleClass = E_VEHICLECLASS_BIGRIG;
    types[11].muVehicleClass = static_cast<decltype(types[11].muVehicleClass)>(0);
    assets[3].mVehicleId = UINT64_C(0xBF2EA6A46A160000); // ARTIST's TUSRV01 exclusion.

    for (int showtime = 0; showtime < 2; ++showtime)
        for (int big = 0; big < 2; ++big) {
            Picker picker;
            picker.mpData = &data;
            picker.mbPlayingShowtimeMode = showtime != 0;
            picker.miBigVehicleAmount = big ? 100 : 0;
            int buses = 0, trucks = 0, rvs = 0;
            for (u32 roll = 0; roll < 256; ++roll) {
                picker.mRand = {roll, 99, 0};
                u8 expected = typeIds[roll / 64];
                if (showtime) {
                    if (roll >= 192) expected = typeIds[0];
                } else if (!big && roll >= 64 && roll < 192) {
                    expected = typeIds[0];
                }
                const u8 selected = picker.PickVehicleToSpawn(0);
                Check(selected == expected, "every authored probability interval retains its vehicle");
                buses += selected == 9; trucks += selected == 6; rvs += selected == 11;
            }
            const bool allowBig = showtime || big;
            Check(buses == (allowBig ? 64 : 0) && trucks == (allowBig ? 64 : 0) &&
                  rvs == (showtime ? 0 : 64), "bus/truck frequency and RV policy match ARTIST");
        }
    Picker overridePicker;
    overridePicker.mpData = &data;
    overridePicker.mbPlayingShowtimeMode = true;
    overridePicker.miDEBUGFlowtypeOverride = 1;
    Check(overridePicker.PickVehicleToSpawn(0) == 9, "valid flow override still selects its bus");
    overridePicker.miDEBUGFlowtypeOverride = 99;
    overridePicker.mRand = {64, 99, 0};
    Check(overridePicker.PickVehicleToSpawn(0) == 9 && giAsserts == 0,
          "invalid override falls back to requested flow without assertions");
    std::printf("ShowtimeTrafficSelection: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
