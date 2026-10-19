// Production login/parser/reset bodies with controlled service config and time.
// Native award/save/restart cases separately prove gameplay and on-disk persistence.
#include "types.hpp"
#include "lobbytagfield.h"
#include "platform.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

static u32 gClock = 1791996687u;
extern "C" u32 ds_timeinsecs() { return gClock; }
extern "C" s32 NetPrintfCode(const char*, ...) { return 0; }
extern "C" s32 ds_snzprintf(char* output, s32 length, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const s32 result = std::vsnprintf(output, length, format, args);
    va_end(args);
    return result;
}
extern "C" struct tm* ds_secstotime(struct tm* output, u32 epoch)
{
    const __time64_t value = epoch;
    return _gmtime64_s(output, &value) == 0 ? output : nullptr;
}
extern "C" u32 ds_timetosecs(const struct tm* value)
{
    struct tm copy = *value;
    return static_cast<u32>(_mkgmtime64(&copy));
}
static int gChecks = 0, gFailures = 0, gAsserts = 0;
static void Check(bool value, const char* label)
{
    ++gChecks;
    if (!value) { ++gFailures; std::printf("FAIL %s\n", label); }
}
#define CGS_ASSERT(value, text) do { if (!(value)) ++gAsserts; } while (0)
#define strnicmp _strnicmp
namespace CgsDev { namespace Log { static std::ostream* gpDebugPrint = nullptr; } }
namespace CgsModule { struct Event {}; }
namespace BrnGameState { namespace GameStateModuleIO {
enum EGameModeType { E_MODE_OFFLINE = 0, E_MODE_ONLINE_MODE_START = 10,
                    E_MODE_ONLINE_FREE_BURN_LOBBY = 15, E_MODE_ONLINE_SHOWTIME = 16 };
struct OnlineRoadRulesConnectInfoEvent { u32 muLastRoadRulesResetTime; };
} }
namespace BrnNetworkModuleIO {
struct NetworkOutRoadRulesConnectedOnlineEvent : CgsModule::Event {
    u32 muLastRoadRulesResetTime;
    s32 GetEventType() const { return 99; }
};
}
namespace CgsNetwork {
const s32 KI_MESSAGE_BUFFER_SIZE = 2048;
struct BufferOwner {
    char data[2048] = {};
    char* GetMessageBuffer() { return data; }
};
struct ServerInterfaceServerInfo {
    BufferOwner owner;
    BufferOwner* mpServerInterface = &owner;
    std::string resetDate;
    int timestampQueries = 0;
    void GetStringFromClientConfig(const char* key, char* output, s32 size)
    {
        const char* value = std::strcmp(key, "ROAD_RULES_RESET_DATE") == 0 ? resetDate.c_str() : "key";
        std::snprintf(output, size, "%s", value);
    }
    u32 GetTimeStampFromClientConfig(const char* key);
};
#include "road_rules_parser.inc"
}
struct BrnServerInterface {
    CgsNetwork::ServerInterfaceServerInfo info;
    CgsNetwork::ServerInterfaceServerInfo* GetServerInfoComponent() { return &info; }
};
namespace BrnNetwork {
struct Events {
    std::vector<u32> dates;
    void AddEvent(const CgsModule::Event* event, s32, usize)
    {
        dates.push_back(reinterpret_cast<const BrnNetworkModuleIO::NetworkOutRoadRulesConnectedOnlineEvent*>(event)->muLastRoadRulesResetTime);
    }
};
struct NetworkManager {
    int completions = 0;
    void OnAutoLoginProcessComplete(s32) { ++completions; }
};
struct GameStateToNetworkInterface {
    BrnGameState::GameStateModuleIO::EGameModeType mode = BrnGameState::GameStateModuleIO::E_MODE_OFFLINE;
    BrnGameState::GameStateModuleIO::EGameModeType GetCurrentGameMode() const { return mode; }
};
struct BrnNetworkModule {
    NetworkManager manager;
    GameStateToNetworkInterface game;
    Events events;
    NetworkManager* GetNetworkManager() { return &manager; }
    GameStateToNetworkInterface* GetGameStateToNetworkInterface() { return &game; }
    Events* GetNetworkEventQueue() { return &events; }
};
struct NetworkRoadRulesManager {
    enum { E_ROAD_RULES_STATE_BOOTING = 0, KI_STATE_AUTO_LOGIN_PRIMED = 4, KI_SERVER_KEY_LENGTH = 64 };
    s32 meState = E_ROAD_RULES_STATE_BOOTING;
    bool mbDownloadedLocalScores = false;
    char macRoadRulesServerKey[KI_SERVER_KEY_LENGTH] = {};
    BrnServerInterface server;
    BrnNetworkModule network;
    BrnServerInterface* mpServerInterface = &server;
    BrnNetworkModule* mpNetworkModule = &network;
    int localDownloads = 0, downloads = 0, uploads = 0;
    void AttemptToDownloadLocalRoadRulesHighScores() { ++localDownloads; }
    void AttemptToDownloadRoadRulesHighScores() { ++downloads; }
    void AttemptToUploadNewRoadRulesScores() { ++uploads; }
    void OnAutoLogin();
};
#include "road_rules_login.inc"
}
namespace BrnProgression {
struct Profile {
    u32 resetDate = 1725834000u;
    u32 GetLastRoadRulesResetTime() const { return resetDate; }
    void SetLastRoadRulesResetTime(u32 value) { resetDate = value; }
    u64 GetRoadRulesID() const { return 7; }
    u32 GetNetworkChallengeDownloadTimestamp() const { return 11; }
};
}
namespace BrnGameState {
using Profile = BrnProgression::Profile;
struct Progression {
    Profile profile;
    Profile* GetProfile() { return &profile; }
};
struct GuiQueue {
    int notifications = 0;
    void AddEvent(const CgsModule::Event*, s32, usize) { ++notifications; }
};
namespace GameStateModuleIO {
struct OutputBuffer {
    GuiQueue queue;
    GuiQueue* GetGuiOutputQueue() { return &queue; }
};
}
struct StreetManager {
    Progression progression;
    Progression* mpProgressionManager = &progression;
    bool hasTimeRecord = true, hasShowtimeRecord = true;
    int clearCalls = 0;
    void ClearAllChallengeData() { ++clearCalls; hasTimeRecord = hasShowtimeRecord = false; }
    void ProcessConnectedOnlineEvent(GameStateModuleIO::OutputBuffer*, const GameStateModuleIO::OnlineRoadRulesConnectInfoEvent*);
};
struct RoadRulesScoreSummaryPayload {
    u64 mu64RoadRulesID;
    u32 muDownloadTimestamp;
    StreetManager* mpStreetManager;
};
#include "road_rules_reset.inc"
}
static void Deliver(BrnNetwork::NetworkRoadRulesManager& login, BrnGameState::StreetManager& street)
{
    BrnGameState::GameStateModuleIO::OutputBuffer output;
    for (u32 date : login.network.events.dates) {
        BrnGameState::GameStateModuleIO::OnlineRoadRulesConnectInfoEvent event{date};
        street.ProcessConnectedOnlineEvent(&output, &event);
    }
}
int main()
{
    Check(TagFieldGetEpoch("", 0) == gClock, "original DirtySDK empty/default0 means current time");
    Check(TagFieldGetEpoch(nullptr, 0) == gClock, "missing date also means current time in original parser");
    Check(TagFieldGetEpoch("1700000000", 0) == 1700000000u, "configured epoch parses unchanged");
    BrnNetwork::NetworkRoadRulesManager empty;
    BrnGameState::StreetManager retained;
    empty.OnAutoLogin();
    Deliver(empty, retained);
    Check(empty.network.events.dates.empty(), "absent PC config emits no reset epoch");
    Check(retained.hasTimeRecord && retained.hasShowtimeRecord, "both stored score types survive missing config");
    Check(retained.progression.profile.resetDate == 1725834000u, "imported nonzero saved epoch is retained");
    Check(empty.localDownloads == 1 && empty.downloads == 1 && empty.uploads == 1, "missing config keeps score download/upload work");
    Check(empty.meState == empty.KI_STATE_AUTO_LOGIN_PRIMED, "missing config keeps login state transition");
    for (const char* text : {"0", "1700000000", "$6553F100", "2023.11.14-22:13:20"}) {
        BrnNetwork::NetworkRoadRulesManager configured;
        configured.server.info.resetDate = text;
        configured.OnAutoLogin();
        const u32 expected = std::strcmp(text, "0") == 0 ? 0 : 1700000000u;
        Check(configured.network.events.dates.size() == 1 && configured.network.events.dates[0] == expected,
              "configured zero/decimal/hex/calendar date emits its original epoch");
        BrnGameState::StreetManager reset;
        Deliver(configured, reset);
        Check(reset.clearCalls == 1 && !reset.hasTimeRecord && !reset.hasShowtimeRecord,
              "original reset mismatch still clears both tables for an authoritative date");
        Check(reset.progression.profile.resetDate == expected, "original reset stores the configured epoch");
        Deliver(configured, reset);
        Check(reset.clearCalls == 1, "unchanged configured epoch causes no second reset");
        Check(configured.localDownloads == 1 && configured.downloads == 1 && configured.uploads == 1,
              "configured reset preserves download/upload work");
    }
    BrnNetwork::NetworkRoadRulesManager already;
    already.meState = 1;
    already.OnAutoLogin();
    Check(already.network.manager.completions == 1 && already.network.events.dates.empty(), "nonboot login callback is unchanged");
    BrnNetwork::NetworkRoadRulesManager downloaded;
    downloaded.mbDownloadedLocalScores = true;
    downloaded.OnAutoLogin();
    Check(downloaded.localDownloads == 0 && downloaded.downloads == 1 && downloaded.uploads == 1,
          "previous local download suppresses only the original local download");
    Check(gAsserts == 0, "production bodies raise no fixture assertions");
    std::printf("RoadRulesReset: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
