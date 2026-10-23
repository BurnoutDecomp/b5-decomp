// Hosted-only functional route persistence regression. Actual production TUs
// supply every OptionsDataProfile, saved-event and live-event implementation.
// Four cache lookups below supply deterministic input records only.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#define private public
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"
#undef private
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"
#include "GameSource/Gui/BrnGuiCache.h"

namespace {
using Profile = BrnGui::OptionsDataProfile;
using Route = Profile::OnlineSaveRoute;
using SavedEvent = Route::OnlineSaveRouteEvent;
using Params = BrnGui::GuiEventNetworkGameParams;
using LandmarkIndex = BrnGameState::LandmarkIndex;
using Info = BrnGui::GuiEventUpdateSatNav::SatNavIconInfo;
unsigned checks = 0, failures = 0, assertions = 0;
unsigned from_index = 0, from_id = 0, from_trigger = 0, from_junction = 0;
bool mock_error = false, reload_mapping = false;
BrnGui::SatNavEventDisplayInfo starts[10];

void Check(const char* id, bool good) {
    ++checks; if (!good) ++failures;
    std::printf("CHECK %s %s\n", id, good ? "PASS" : "FAIL");
}
int Finish() {
    std::printf("ONLINE_SAVE_ROUTE checks=%u failures=%u assertions=%u\n", checks, failures, assertions);
    return failures ? 1 : 0;
}
void ResetLookups(bool reload) {
    from_index = from_id = from_trigger = from_junction = 0;
    mock_error = false; reload_mapping = reload;
}
CgsID LandmarkId(s32 item) {
    // Adjacent identities deliberately share all 32 low bits. A u32 truncation
    // cannot distinguish them, and all 160 identities have nonzero high halves.
    return (static_cast<u64>(0xC1000000u + item) << 32) | (0x89ABC000u + item / 2);
}
s32 SaveIndex(s32 item) { return 100 + item; }
s32 ReloadIndex(s32 item) { return 2000 + (159 - item) * 3; }
u32 SaveTrigger(s32 round) { return 0x00210000u + round * 37u; }
u32 Junction(s32 round) { return 0xAB120000u + round * 101u; }
u32 ReloadTrigger(s32 round) { return 0x00D30000u + round * 43u; }

void SeedRoute(Route& route) {
    route.meGameMode = static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(17);
    route.meBoostType = BrnNetwork::E_BOOST_TYPE_STUNT;
    route.meVehicleChoice = BrnNetwork::E_VEHICLE_CHOICE_DEFAULT;
    route.miTimeLimit = 777; route.miNumRounds = 7;
    route.miVehicleClass = 2; route.miNumRunnerCrashes = 22;
    route.mbInfiniteBoost = false; route.mbTrafficOn = false; route.mbTrafficCheckingOn = false;
    for (s32 r = 0; r < 10; ++r) {
        route.maEvents[r].miEventID = 700 + r;
        route.maEvents[r].mJunctionId = 0x98760000u + r;
        route.maEvents[r].miNumLandmarks = 16;
        for (s32 i = 0; i < 16; ++i) route.maEvents[r].maLandmarkIndices[i] = LandmarkId(r * 16 + i);
    }
}
bool DefaultEvents(const Route& route) {
    for (s32 r = 0; r < 10; ++r)
        if (route.maEvents[r].GetEventID() != 0 || route.maEvents[r].GetJunctionID() != 0 ||
            route.maEvents[r].GetNumLandmarks() != 0) return false;
    return true;
}
bool DefaultScalars(const Route& route) {
    return route.meGameMode == 10 && route.miNumRounds == 1 && route.miVehicleClass == 9 &&
        route.miNumRunnerCrashes == 3 && route.meBoostType == 0 && route.meVehicleChoice == 0 &&
        route.miTimeLimit == 0 && route.mbInfiniteBoost && route.mbTrafficOn && route.mbTrafficCheckingOn;
}
bool OriginalGuids(const Route& route) {
    for (s32 r = 0; r < 10; ++r) for (s32 i = 0; i < 16; ++i)
        if (route.maEvents[r].maLandmarkIndices[i] != LandmarkId(r * 16 + i)) return false;
    return true;
}
void TestDefaults() {
    Profile profile;
    std::memset(&profile, 0, sizeof(profile));
    for (s32 slot = 0; slot < 10; ++slot) {
        SeedRoute(profile.maCreatedOnlineGameOptions[slot]);
        SeedRoute(profile.maReceivedOnlineGameOptions[slot]);
    }
    profile.miNumCreatedOnlineGameOptions = 7; profile.miNumReceivedOnlineGameOptions = 8;
    profile.Construct();
    Check("default_version", profile.miVersionNumber == 12);
    Check("default_created_count", profile.GetNumCreatedOnlineGameOptions() == 0);
    Check("default_received_count", profile.GetNumReceivedOnlineGameOptions() == 0);
    bool ce = true, re = true, cs = true, rs = true, cg = true, rg = true;
    for (s32 slot = 0; slot < 10; ++slot) {
        ce = ce && DefaultEvents(profile.maCreatedOnlineGameOptions[slot]);
        re = re && DefaultEvents(profile.maReceivedOnlineGameOptions[slot]);
        cs = cs && DefaultScalars(profile.maCreatedOnlineGameOptions[slot]);
        rs = rs && DefaultScalars(profile.maReceivedOnlineGameOptions[slot]);
        cg = cg && OriginalGuids(profile.maCreatedOnlineGameOptions[slot]);
        rg = rg && OriginalGuids(profile.maReceivedOnlineGameOptions[slot]);
    }
    Check("default_all_created_event_triplets", ce);
    Check("default_all_received_event_triplets", re);
    Check("default_all_created_scalars", cs);
    Check("default_all_received_scalars", rs);
    Check("default_created_guids_untouched", cg);
    Check("default_received_guids_untouched", rg);
    Route route; std::memset(&route, 0, sizeof(route)); SeedRoute(route); route.Construct();
    Check("default_standalone_event_triplets", DefaultEvents(route));
    Check("default_standalone_scalars", DefaultScalars(route));
    Check("default_standalone_guids_untouched", OriginalGuids(route));
}
void FillParams(Params& params, s32 seed, s32 rounds, s32 landmarks) {
    std::memset(&params, 0, sizeof(params)); params.Construct();
    params.meGameMode = 10 + seed % 8; params.meBoostType = seed % 5;
    params.meVehicleChoice = 0; params.miTimeLimit = 60 + seed * 17;
    params.miNumRounds = rounds; params.miVehicleClass = 1 + seed % 9;
    params.miNumRunnerCrashes = seed % 6;
    params.mbInfiniteBoost = (seed & 1) != 0; params.mbTrafficOn = (seed & 2) != 0;
    params.mbTrafficCheckingOn = (seed & 4) != 0;
    params.mePreviousGameMode = 18; params.meSecurity = 2; params.mbRanked = (seed & 1) != 0;
    for (s32 r = 0; r < rounds; ++r) {
        LandmarkIndex indices[16];
        for (s32 i = 0; i < landmarks; ++i) indices[i] = LandmarkIndex(SaveIndex(r * 16 + i));
        params.maEvents[r].Construct(2000 + seed * 16 + r, SaveTrigger(r), indices, landmarks);
    }
}
void Destination(Params& params, const Params& source) {
    FillParams(params, 400, 10, 16);
    params.meGameMode = -91; params.meBoostType = -92; params.meVehicleChoice = -93;
    params.miTimeLimit = -94; params.miNumRounds = -95; params.miVehicleClass = -96;
    params.miNumRunnerCrashes = -97;
    params.mbInfiniteBoost = !source.mbInfiniteBoost; params.mbTrafficOn = !source.mbTrafficOn;
    params.mbTrafficCheckingOn = !source.mbTrafficCheckingOn;
    params.mePreviousGameMode = 6; params.meSecurity = 1; params.mbRanked = !source.mbRanked;
}
bool SameScalars(const Params& got, const Params& expected) {
    return got.meGameMode == expected.meGameMode && got.meBoostType == expected.meBoostType &&
        got.meVehicleChoice == expected.meVehicleChoice && got.miTimeLimit == expected.miTimeLimit &&
        got.miNumRounds == expected.miNumRounds && got.miVehicleClass == expected.miVehicleClass &&
        got.miNumRunnerCrashes == expected.miNumRunnerCrashes && got.mbInfiniteBoost == expected.mbInfiniteBoost &&
        got.mbTrafficOn == expected.mbTrafficOn && got.mbTrafficCheckingOn == expected.mbTrafficCheckingOn;
}
void SavedScalarChecks(const Route& route, const Params& input) {
    Check("max_saved_mode", route.meGameMode == input.meGameMode);
    Check("max_saved_boost", route.meBoostType == input.meBoostType);
    Check("max_saved_choice", route.meVehicleChoice == input.meVehicleChoice);
    Check("max_saved_time", route.miTimeLimit == input.miTimeLimit);
    Check("max_saved_rounds", route.miNumRounds == input.miNumRounds);
    Check("max_saved_class", route.miVehicleClass == input.miVehicleClass);
    Check("max_saved_crashes", route.miNumRunnerCrashes == input.miNumRunnerCrashes);
    Check("max_saved_infinite", route.mbInfiniteBoost == input.mbInfiniteBoost);
    Check("max_saved_traffic", route.mbTrafficOn == input.mbTrafficOn);
    Check("max_saved_checking", route.mbTrafficCheckingOn == input.mbTrafficCheckingOn);
}
void LoadedScalarChecks(const Params& got, const Params& input) {
    Check("max_loaded_mode", got.meGameMode == input.meGameMode);
    Check("max_loaded_boost", got.meBoostType == input.meBoostType);
    Check("max_loaded_choice", got.meVehicleChoice == input.meVehicleChoice);
    Check("max_loaded_time", got.miTimeLimit == input.miTimeLimit);
    Check("max_loaded_rounds", got.miNumRounds == input.miNumRounds);
    Check("max_loaded_class", got.miVehicleClass == input.miVehicleClass);
    Check("max_loaded_crashes", got.miNumRunnerCrashes == input.miNumRunnerCrashes);
    Check("max_loaded_infinite", got.mbInfiniteBoost == input.mbInfiniteBoost);
    Check("max_loaded_traffic", got.mbTrafficOn == input.mbTrafficOn);
    Check("max_loaded_checking", got.mbTrafficCheckingOn == input.mbTrafficCheckingOn);
}
void TestRoundTrip(BrnGui::GuiCache* cache) {
    Params input; FillParams(input, 5, 10, 16); const Params original = input;
    Route route; std::memset(&route, 0, sizeof(route)); SeedRoute(route); route.Construct();
    // Expected IDs all have bit 63 set; this poison has it clear in every slot.
    // The real save must replace all 160 values before the existing GUID checks.
    for (s32 r = 0; r < 10; ++r) for (s32 i = 0; i < 16; ++i)
        route.maEvents[r].maLandmarkIndices[i] = LandmarkId(r * 16 + i) ^ 0x8000000000000000ull;
    bool wide = true, colliding = true;
    for (s32 i = 0; i < 160; ++i) wide = wide && (LandmarkId(i) >> 32) != 0;
    for (s32 i = 0; i < 160; i += 2)
        colliding = colliding && static_cast<u32>(LandmarkId(i)) == static_cast<u32>(LandmarkId(i + 1)) &&
            LandmarkId(i) != LandmarkId(i + 1);
    Check("input_wide_guid_collision_witness", wide && colliding);
    ResetLookups(false); route.SetFromGameParams(cache, &input);
    Check("max_save_input_unchanged", std::memcmp(&input, &original, sizeof(input)) == 0);
    SavedScalarChecks(route, input);
    bool ids = true, events = true, junctions = true;
    for (s32 r = 0; r < 10; ++r) {
        const auto& saved = route.maEvents[r];
        events = events && saved.GetEventID() == input.maEvents[r].GetEventID() && saved.GetNumLandmarks() == 16;
        junctions = junctions && saved.GetJunctionID() == Junction(r) && saved.GetJunctionID() != SaveTrigger(r);
        for (s32 i = 0; i < 16; ++i) ids = ids && saved.GetLandmark(i) == LandmarkId(r * 16 + i);
    }
    Check("max_saved_160_full_guids", ids);
    Check("max_saved_10_event_ids_counts", events);
    Check("max_saved_triggers_become_junctions", junctions);
    Check("max_forward_lookup_counts", from_index == 160 && from_trigger == 10 && from_id == 0 && from_junction == 0 && !mock_error);
    const Route stored = route;
    Params output; Destination(output, input);
    ResetLookups(true); route.SetToGameParams(cache, &output);
    LoadedScalarChecks(output, input);
    Check("max_previous_mode_untouched", output.mePreviousGameMode == 6);
    Check("max_security_untouched", output.meSecurity == 1);
    Check("max_ranked_untouched", output.mbRanked == !input.mbRanked);
    bool indices = true, output_events = true, lights = true;
    for (s32 r = 0; r < 10; ++r) {
        output_events = output_events && output.maEvents[r].GetEventID() == input.maEvents[r].GetEventID() &&
            output.maEvents[r].GetNumLandmarks() == 16;
        lights = lights && output.maEvents[r].GetTrafficLightTriggerId() == ReloadTrigger(r) &&
            ReloadTrigger(r) != SaveTrigger(r);
        for (s32 i = 0; i < 16; ++i)
            indices = indices && static_cast<s32>(output.maEvents[r].GetLandmark(i)) == ReloadIndex(r * 16 + i) &&
                ReloadIndex(r * 16 + i) != SaveIndex(r * 16 + i);
    }
    Check("max_loaded_160_remapped_indices", indices);
    Check("max_loaded_10_event_ids_counts", output_events);
    Check("max_loaded_junctions_become_current_triggers", lights);
    Check("max_reverse_lookup_counts_full_ids", from_id == 160 && from_junction == 10 && from_index == 0 && from_trigger == 0 && !mock_error);
    Check("max_load_saved_route_unchanged", std::memcmp(&route, &stored, sizeof(route)) == 0);

    Params empty; FillParams(empty, 2, 0, 0);
    SavedEvent previous_events[10]; std::memcpy(previous_events, route.maEvents, sizeof(previous_events));
    ResetLookups(false); route.SetFromGameParams(cache, &empty);
    Check("zero_rounds_save_leaves_events", route.miNumRounds == 0 && std::memcmp(previous_events, route.maEvents, sizeof(previous_events)) == 0);
    Check("zero_rounds_save_no_lookups", !from_index && !from_id && !from_trigger && !from_junction && !mock_error);
    Destination(output, empty); Params previous = output;
    ResetLookups(true); route.SetToGameParams(cache, &output);
    Check("zero_rounds_load_scalars", SameScalars(output, empty));
    Check("zero_rounds_load_leaves_events", std::memcmp(output.maEvents, previous.maEvents, sizeof(output.maEvents)) == 0);
    Check("zero_rounds_load_preserves_unsaved_fields", output.mePreviousGameMode == previous.mePreviousGameMode && output.meSecurity == previous.meSecurity && output.mbRanked == previous.mbRanked);
    Check("zero_rounds_load_no_lookups", !from_index && !from_id && !from_trigger && !from_junction && !mock_error);

    Params no_landmarks; FillParams(no_landmarks, 12, 10, 0);
    ResetLookups(false); route.SetFromGameParams(cache, &no_landmarks);
    Check("zero_landmarks_save_160_guids_untouched", OriginalGuids(route));
    Check("zero_landmarks_save_only_start_lookups", from_index == 0 && from_trigger == 10 && from_id == 0 && from_junction == 0 && !mock_error);
    Destination(output, no_landmarks); previous = output;
    ResetLookups(true); route.SetToGameParams(cache, &output);
    bool empty_events = true, landmark_storage_untouched = true;
    for (s32 r = 0; r < 10; ++r) {
        empty_events = empty_events && output.maEvents[r].GetNumLandmarks() == 0 &&
            output.maEvents[r].GetEventID() == no_landmarks.maEvents[r].GetEventID() &&
            output.maEvents[r].GetTrafficLightTriggerId() == ReloadTrigger(r);
        // Compare the first 16 two-byte handles; no inactive handle is dereferenced.
        landmark_storage_untouched = landmark_storage_untouched && std::memcmp(&output.maEvents[r], &previous.maEvents[r], 16 * sizeof(LandmarkIndex)) == 0;
    }
    Check("zero_landmarks_load_all_event_metadata", empty_events);
    Check("zero_landmarks_load_handles_untouched", landmark_storage_untouched);
    Check("zero_landmarks_load_scalars", SameScalars(output, no_landmarks));
    Check("zero_landmarks_load_preserves_unsaved_fields", output.mePreviousGameMode == previous.mePreviousGameMode && output.meSecurity == previous.meSecurity && output.mbRanked == previous.mbRanked);
    Check("zero_landmarks_load_only_start_lookups", from_id == 0 && from_junction == 10 && from_index == 0 && from_trigger == 0 && !mock_error);
}
bool ReadSlot(Profile& profile, BrnGui::GuiCache* cache, bool created, s32 slot, s32 seed) {
    Params expected; FillParams(expected, seed, 1, 0);
    Params output; Destination(output, expected); ResetLookups(true);
    if (created) profile.GetCreatedOnlineGameOptions(slot, cache, &output);
    else profile.GetReceivedOnlineGameOptions(slot, cache, &output);
    return SameScalars(output, expected) && output.maEvents[0].GetEventID() == expected.maEvents[0].GetEventID() &&
        output.maEvents[0].GetNumLandmarks() == 0 && output.maEvents[0].GetTrafficLightTriggerId() == ReloadTrigger(0) &&
        output.mePreviousGameMode == 6 && output.meSecurity == 1 && output.mbRanked == !expected.mbRanked &&
        !from_index && !from_id && !from_trigger && from_junction == 1 && !mock_error;
}
void TestTables(BrnGui::GuiCache* cache) {
    Profile profile; std::memset(&profile, 0, sizeof(profile)); profile.Construct();
    bool counts = true, inputs = true;
    for (s32 slot = 0; slot < 10; ++slot) {
        Params input; FillParams(input, 1000 + slot, 1, 0); const Params before = input;
        ResetLookups(false); profile.SetCreatedOnlineGameOptions(slot, cache, &input);
        counts = counts && profile.GetNumCreatedOnlineGameOptions() == slot + 1;
        inputs = inputs && std::memcmp(&input, &before, sizeof(input)) == 0 && from_trigger == 1 && !mock_error;
        if (slot == 0) {
            FillParams(input, 2000, 1, 0); ResetLookups(false);
            profile.SetCreatedOnlineGameOptions(0, cache, &input);
            Check("created_partial_overwrite_keeps_count", profile.GetNumCreatedOnlineGameOptions() == 1);
            Check("created_partial_overwrite_replaces_payload", ReadSlot(profile, cache, true, 0, 2000));
        }
    }
    Check("created_all_10_append_counts", counts);
    Check("created_inputs_unchanged_and_forward_lookup", inputs);
    bool all = true;
    for (s32 slot = 0; slot < 10; ++slot) all = ReadSlot(profile, cache, true, slot, slot == 0 ? 2000 : 1000 + slot) && all;
    Check("created_all_10_slots_roundtrip", all);
    Params replacement; FillParams(replacement, 3000, 1, 0); ResetLookups(false);
    profile.SetCreatedOnlineGameOptions(4, cache, &replacement);
    Check("created_full_overwrite_keeps_count", profile.GetNumCreatedOnlineGameOptions() == 10);
    all = true;
    for (s32 slot = 0; slot < 10; ++slot) all = ReadSlot(profile, cache, true, slot, slot == 4 ? 3000 : slot == 0 ? 2000 : 1000 + slot) && all;
    Check("created_full_overwrite_preserves_other_9", all);
    Route created_before[10]; std::memcpy(created_before, profile.maCreatedOnlineGameOptions, sizeof(created_before));
    counts = true; bool windows = true, received_inputs = true;
    for (s32 insert = 0; insert < 12; ++insert) {
        Params input; FillParams(input, 4000 + insert, 1, 0); const Params before = input;
        ResetLookups(false); profile.AddReceivedOnlineGameOptions(cache, &input);
        const s32 count = insert < 10 ? insert + 1 : 10;
        counts = counts && profile.GetNumReceivedOnlineGameOptions() == count;
        received_inputs = received_inputs && std::memcmp(&input, &before, sizeof(input)) == 0 && from_trigger == 1 && !mock_error;
        for (s32 slot = 0; slot < count; ++slot)
            windows = ReadSlot(profile, cache, false, slot, 4000 + insert - slot) && windows;
    }
    Check("received_12_insert_counts_saturate_at_10", counts);
    Check("received_all_shifted_windows_and_oldest_eviction", windows);
    Check("received_inputs_unchanged_and_forward_lookup", received_inputs);
    Check("received_shifts_preserve_created_table", std::memcmp(created_before, profile.maCreatedOnlineGameOptions, sizeof(created_before)) == 0 && profile.GetNumCreatedOnlineGameOptions() == 10);
}

#if defined(ROUTE_SAVE_OLDTRAP_CONTROL)
// The exact original __debugbreak remains executable production code. SEH
// observes only breakpoint exceptions; access violations still fail the run.
DWORD ObserveOriginalTrap(Profile* profile, BrnGui::GuiCache* cache, Params* params, bool save) {
    __try {
        if (save) profile->SetCreatedOnlineGameOptions(0, cache, params);
        else profile->GetCreatedOnlineGameOptions(0, cache, params);
        return 0;
    } __except (GetExceptionCode() == EXCEPTION_BREAKPOINT ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return EXCEPTION_BREAKPOINT;
    }
}
#endif
} // namespace

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* expression, const char*, int) {
    ++assertions; std::printf("UNEXPECTED_ASSERT %s\n", expression); return 0;
}
void* EndAssert() { return nullptr; }
} }

// Input-only world/cache boundary. No OptionsDataProfile method, saved-route
// field transfer or event construction is implemented by these providers.
namespace BrnGui {
GuiEventUpdateSatNav::SatNavIconInfo* GuiCache::GetLandmarkInfoFromIndex(LandmarkIndex index, Info* out) const {
    ++from_index; const s32 item = static_cast<s32>(index) - 100;
    std::memset(out, 0, sizeof(*out));
    if (reload_mapping || item < 0 || item >= 160) { mock_error = true; return out; }
    out->SetCgsId(LandmarkId(item)); out->SetLandmarkIndexHalf(static_cast<s16>(SaveIndex(item))); return out;
}
void GuiCache::GetLandmarkInfoFromID(CgsID id, Info* out) const {
    ++from_id; std::memset(out, 0, sizeof(*out));
    if (!reload_mapping) mock_error = true;
    for (s32 item = 0; item < 160; ++item) if (LandmarkId(item) == id) {
        out->SetCgsId(id); out->SetLandmarkIndexHalf(static_cast<s16>(ReloadIndex(item))); return;
    }
    mock_error = true;
}
const SatNavEventDisplayInfo* GuiCache::GetPresetEventDisplayInfo(u32 trigger) const {
    ++from_trigger;
    for (s32 r = 0; r < 10; ++r) if (!reload_mapping && SaveTrigger(r) == trigger) {
        starts[r].muLightTriggerId = trigger; starts[r].muJunctionId = Junction(r); return &starts[r];
    }
    mock_error = true; return &starts[0];
}
const SatNavEventDisplayInfo* GuiCache::GetEventStartInfoFromJunctionID(u32 junction) const {
    ++from_junction;
    for (s32 r = 0; r < 10; ++r) if (reload_mapping && Junction(r) == junction) {
        starts[r].muJunctionId = junction; starts[r].muLightTriggerId = ReloadTrigger(r); return &starts[r];
    }
    mock_error = true; return &starts[0];
}
} // namespace BrnGui

static_assert(sizeof(CgsID) == 8 && sizeof(LandmarkIndex) == 2, "persistent GUID and live handle widths");
static_assert(sizeof(Route) == 0x5C0 && sizeof(Params) == 480, "production persisted/wire layout");
static_assert(std::is_trivially_copyable<Route>::value && std::is_trivially_copyable<Params>::value,
              "the source uses fixed-record copies");
int main(int argc, char** argv) {
#if defined(ROUTE_SAVE_OLDTRAP_CONTROL)
    if (argc != 2 || (std::strcmp(argv[1], "save") && std::strcmp(argv[1], "load"))) return 90;
    Profile profile; std::memset(&profile, 0, sizeof(profile));
    Params params; FillParams(params, 5, 1, 0);
    BrnGui::GuiCache cache;
    const bool save = std::strcmp(argv[1], "save") == 0;
    const DWORD observed = ObserveOriginalTrap(&profile, &cache, &params, save);
    std::printf("ORIGINAL_BREAKPOINT mode=%s code=%08lX\n", argv[1], static_cast<unsigned long>(observed));
    Check(save ? "oldtrap_save_completes" : "oldtrap_load_completes", observed == 0);
    return Finish();
#else
    if (argc != 1) return 90;
    TestDefaults();
#if !defined(ROUTE_SAVE_OMIT_INIT_CONTROL)
    BrnGui::GuiCache cache;
    TestRoundTrip(&cache); TestTables(&cache);
#endif
    Check("no_unexpected_assertions", assertions == 0);
    return Finish();
#endif
}
