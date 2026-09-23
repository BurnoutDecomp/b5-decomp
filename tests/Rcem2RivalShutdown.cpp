// FX-RCEM2 (crash-parity 2026-09-23, G13-X5): the BrnGui::OfflineRivalShutdown state bodies
// (POST_RIVAL; ARTIST 0x824B94D0 Construct, 0x824B9588 OnEnter, 0x824D3060 OnLeave,
// 0x824DAFE0 Update, 0x824C23D0 HandleIncomingEvents, 0x824B96A0 AppendExpectedComponents,
// 0x824D3130 SetupComponents, 0x824B9748 HandleAptTriggers, 0x824BD100 HandleControllerInput,
// 0x824B98B8 HandleControllerInputPressed). run_rcem2_rival_shutdown.py extracts the WHOLE
// production `namespace BrnGui { ... }` body of BrnOfflineRivalShutdown.cpp VERBATIM (constants,
// statics, the ten bodies) and compiles it here inside `namespace Fixture`, whose recording mocks
// stand in for the state base, the state interface, the GuiCache and the four components. Every
// expectation below is read off the ARTIST asm (addresses beside each check).
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static std::vector<std::string> gaAsserts;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
}
}

namespace Fixture {
typedef u64 CgsID;
const s32 KI_CGSID_STRING_LEN = 13;
const CgsID KU_SHUTDOWN_CAR = 0x0000123456789ABCull;

static std::vector<std::string> gaLog;
static void Log(const char* lpcFormat, ...)
{
    char lacBuffer[512];
    va_list lArgs;
    va_start(lArgs, lpcFormat);
    std::vsnprintf(lacBuffer, sizeof(lacBuffer), lpcFormat, lArgs);
    va_end(lArgs);
    gaLog.push_back(lacBuffer);
}

void CgsIDConvertToString(CgsID lID, char* lpcString)
{
    std::snprintf(lpcString, KI_CGSID_STRING_LEN, "%s", lID == KU_SHUTDOWN_CAR ? "PUSMC01" : "UNKNOWN");
}

namespace CgsCore {
void SnPrintf(char* lpcBuffer, u32 luLength, const char* lpcFormat, ...)
{
    va_list lArgs;
    va_start(lArgs, lpcFormat);
    std::vsnprintf(lpcBuffer, luLength, lpcFormat, lArgs);
    va_end(lArgs);
    Log("SnPrintf(len=%u)", luLength);
}
}

namespace CgsLanguage { struct LanguageManager { enum ParameterFormatType { E_FORMAT_ID_LOOKUP = 9 }; }; }

namespace CgsModule {
struct Event {};
// One recorded GUI record: its channel/id and raw bytes.
struct Record { s32 miType; std::vector<u8> maBytes; };
// The state IN-queue: the production typedef names VariableEventQueue<18432,16>.
template <s32 SIZE, s32 ALIGN>
struct VariableEventQueue
{
    std::vector<Record> maRecords;
    void Push(s32 liType, const void* lpData, size_t luSize)
    {
        const u8* lpBytes = static_cast<const u8*>(lpData);
        maRecords.push_back({ liType, std::vector<u8>(lpBytes, lpBytes + luSize) });
    }
    s32 Find(const Event* lpEvent) const
    {
        for (size_t i = 0; i < maRecords.size(); ++i)
            if (reinterpret_cast<const Event*>(maRecords[i].maBytes.data()) == lpEvent) return static_cast<s32>(i);
        return -1;
    }
    s32 At(size_t luIndex, const Event** lppEvent, s32* lpiSize) const
    {
        if (luIndex >= maRecords.size()) { *lppEvent = 0; *lpiSize = 0; return -1; }
        *lppEvent = reinterpret_cast<const Event*>(maRecords[luIndex].maBytes.data());
        *lpiSize = static_cast<s32>(maRecords[luIndex].maBytes.size());
        return maRecords[luIndex].miType;
    }
    s32 GetFirstEvent(const Event** lppEvent, s32* lpiSize) const { return At(0, lppEvent, lpiSize); }
    s32 GetNextEvent(const Event* lpEvent, const Event** lppNext, s32* lpiSize) const
    {
        return At(static_cast<size_t>(Find(lpEvent) + 1), lppNext, lpiSize);
    }
};
}

namespace CgsFsm { struct ScriptedFsm { int miDummy; }; }

namespace CgsGui {
enum ResourceRequestTypes { E_GUI_RESOURCETYPE_APT = 4 };
struct sResourceTuple { u32 muId; ResourceRequestTypes meType; };

struct OutQueue
{
    std::vector<CgsModule::Record> maPosts;   // miType carries the CHANNEL here
    void AddEvent(const CgsModule::Event* lpEvent, s32 liChannel, s32 liSize)
    {
        const u8* lpBytes = reinterpret_cast<const u8*>(lpEvent);
        maPosts.push_back({ liChannel, std::vector<u8>(lpBytes, lpBytes + liSize) });
        Log("AddEvent(ch=%d,size=%d)", liChannel, liSize);
    }
};

struct StateInterface
{
    OutQueue mOutQueue;
    void RegisterForEvents(const s32* lpaiEvents, s32 liCount)
    {
        std::string lText;
        for (s32 i = 0; i < liCount; ++i) lText += (i ? "," : "") + std::to_string(lpaiEvents[i]);
        Log("RegisterForEvents(%s #%d)", lText.c_str(), liCount);
    }
    void UnRegisterForEvents(const s32* lpaiEvents, s32 liCount)
    {
        std::string lText;
        for (s32 i = 0; i < liCount; ++i) lText += (i ? "," : "") + std::to_string(lpaiEvents[i]);
        Log("UnRegisterForEvents(%s #%d)", lText.c_str(), liCount);
    }
    void PlayAptMovie(const char* lpacName, s32 liLevel) { Log("PlayAptMovie(%s,%d)", lpacName, liLevel); }
    OutQueue* GetOutputEventQueue() { return &mOutQueue; }
};

struct State
{
    void*           mpInGuiEventQueue = 0;
    StateInterface* mpStateInterface  = 0;
    virtual ~State() {}
    void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm) { Log("State::Construct(%llu,%s)", liId, lpFsm ? "fsm" : "null"); }
    void SendStateEvent(const char* lpacEvent) { Log("SendStateEvent(%s)", lpacEvent); }
};

template <s32 EventTypeId>
struct GuiEvent : public CgsModule::Event
{
    u32 muHeader0, muEventType, muHeader2;
    explicit GuiEvent(u32 luHeader0 = 0, u32 luHeader2 = 0)
        : muHeader0(luHeader0), muEventType(static_cast<u32>(EventTypeId)), muHeader2(luHeader2) {}
    s32 GetEventType() const { return EventTypeId; }
};

// Verbatim shape of CgsGuiEvent.h's wrapper: {sizeof(T), T::GetEventType(), offsetof(mOutEvent)} + T.
template <class T, s32 leEventType>
class GuiEventWrapper
{
public:
    s32 miOutEventSize;
    s32 miOutEventType;
    s32 miOutEventOffset;
    T   mOutEvent;
    explicit GuiEventWrapper(T& lrEvent)
        : miOutEventSize(static_cast<s32>(sizeof(T))), miOutEventType(lrEvent.GetEventType()),
          miOutEventOffset(static_cast<s32>(reinterpret_cast<size_t>(
              reinterpret_cast<char*>(&(reinterpret_cast<GuiEventWrapper*>(1)->mOutEvent)) - 1))),
          mOutEvent(lrEvent) {}
};

struct GuiEventAptTrigger
{
    enum AptEventType { E_APT_EVENT_START = 0, E_APT_EVENT_ONLOAD = 1, E_APT_EVENT_POSTFX = 2,
                        E_APT_EVENT_FRAME_TRIGGER = 3, E_APT_EVENT_TRANSITION_COMPLETE = 4 };
};
struct GuiEventAptTriggerPayload
{
    GuiEventAptTrigger::AptEventType meEventType;
    s32                              miUniqueId;
    const char*                      mpacComponentName;
    u32                              muComponentNameHash;
    void*                            mpComponentRef;
};
struct GuiEventControllerInputPressed { s32 miController; s32 miAction; };
struct GuiEventLoadNotification { s32 miId; };
struct GuiEventUnloadNotification { s32 miId; };
}

namespace BrnProgression {
enum ETrainingType { E_TRAINING_TYPE_RIVAL_SHUTDOWN = 32 };
class Profile
{
public:
    bool mbSeenRivalShutdownTip = false;
    bool HasPlayerSeenTrainingType(ETrainingType leType) const
    {
        Log("HasPlayerSeenTrainingType(%d)", static_cast<s32>(leType));
        return leType == E_TRAINING_TYPE_RIVAL_SHUTDOWN && mbSeenRivalShutdownTip;
    }
};
}

namespace BrnResource {
struct VehicleListEntry
{
    u32 mRivalUnlockName = 0xA1B2C3D4u;
    u64 muWonCarVoiceOver = 0x1122334455667788ull;
    u64 GetWonCarVoiceOverKeyHash() const { return muWonCarVoiceOver; }
};
struct VehicleList
{
    VehicleListEntry mEntry;
    s32 GetVehicleIndex(CgsID lId) const { Log("GetVehicleIndex"); return lId == KU_SHUTDOWN_CAR ? 7 : -1; }
    const VehicleListEntry* GetVehicleData(s32 liIndex) const { Log("GetVehicleData(%d)", liIndex); return &mEntry; }
};
}

namespace BrnGui {
typedef u32 BrnGuiResourceId;
enum GuiFlow { E_GUIFLOW_SCREEN = 0, E_GUIFLOW_HUD = 1 };

static const char* MakeTable(u32 luIndex) { return luIndex == 224 ? "BrnRivalShutdown" : "<other>"; }
struct ResourceNames { const char* operator[](u32 luIndex) const { return MakeTable(luIndex); } };
static const ResourceNames gGuiResourceIdentifier = {};

struct WorldDataController
{
    BrnResource::VehicleList* mpVehicleList = 0;
    const BrnResource::VehicleList* GetVehicleList() const { return mpVehicleList; }
};

class GuiCache
{
public:
    CgsID                    mShutdownCarID = KU_SHUTDOWN_CAR;
    f32                      mfTime = 10.0f;
    bool                     mbResourcesLoaded = false;
    bool                     mbComponentsInitialised = false;
    WorldDataController*     mpWorldDataController = 0;
    BrnProgression::Profile* mpProfile = 0;

    CgsID GetShutdownCarID() const
    {
        CGS_ASSERT(mShutdownCarID != 0, "kCGSID_NULL != mShutdownCarID");
        return mShutdownCarID;
    }
    f32 GetTime() const { return mfTime; }
    bool EnsureResourcesAreLoaded(const CgsGui::sResourceTuple* lpResources, u32 luCount)
    {
        std::string lText;
        for (u32 i = 0; i < luCount; ++i)
            lText += (i ? "," : "") + std::to_string(lpResources[i].muId) + ":" + std::to_string(lpResources[i].meType);
        Log("cache.EnsureResourcesAreLoaded(%s #%u)", lText.c_str(), luCount);
        return mbResourcesLoaded;
    }
    void ClearExpectedAptComponentList(GuiFlow leFlow) { Log("cache.ClearExpectedAptComponentList(%d)", static_cast<s32>(leFlow)); }
    bool AreAllAptComponentsInitialised(GuiFlow) const { return mbComponentsInitialised; }
    void AppendExpectedAptComponent(GuiFlow leFlow, const char* lpacName) { Log("cache.AppendExpected(%d,%s)", static_cast<s32>(leFlow), lpacName); }
    WorldDataController* GetWorldDataController() const { return mpWorldDataController; }
    BrnProgression::Profile* GetProfile() { return mpProfile; }
};

struct GuiComponent
{
    char        macName[128] = {};
    const char* mpcTag = "?";
    void Construct(const char* lpacName, CgsGui::StateInterface*, const char* lpacParentName)
    {
        std::snprintf(macName, sizeof(macName), "%s", lpacName);
        Log("%s.Construct(%s,%s)", mpcTag, lpacName, lpacParentName ? lpacParentName : "null");
    }
    const char* GetName() const { return macName; }
    void AddOutputAptViewState(const char* lpacApt, const char* lpacState, bool lbImmediate)
    {
        Log("%s.AddOutputAptViewState(%s,%s,%d)", mpcTag, lpacApt, lpacState, lbImmediate ? 1 : 0);
    }
};
struct AnimationComponent : GuiComponent { AnimationComponent() { mpcTag = "anim"; } };
struct TextField : GuiComponent
{
    bool SetLocalisedText(const char* lpacText, CgsLanguage::LanguageManager::ParameterFormatType leFormat)
    {
        Log("%s.SetLocalisedText(%s,%d)", mpcTag, lpacText, static_cast<s32>(leFormat));
        return true;
    }
};
struct ManufacturersIcon : GuiComponent
{
    ManufacturersIcon() { mpcTag = "badge"; }
    void Set(const BrnResource::VehicleList* lpList, CgsID lId) { Log("badge.Set(%s,%llx)", lpList ? "list" : "null", lId); }
};
struct LargeCarComponent : GuiComponent
{
    bool mbReady = false;
    LargeCarComponent() { mpcTag = "car"; }
    void SetCachePointer(GuiCache* lpCache) { Log("car.SetCachePointer(%p)", static_cast<void*>(lpCache)); }
    void SetCarInfo(CgsID lId, BrnGuiResourceId luType) { Log("car.SetCarInfo(%llx,%u)", lId, luType); }
    void ShowCar() { Log("car.ShowCar"); }
    void OnLoad() { Log("car.OnLoad"); }
    void ReleaseResources() { Log("car.ReleaseResources"); }
    bool EnsureResourcesAreLoaded() { Log("car.EnsureResourcesAreLoaded"); return mbReady; }
    void HandleLoadNotification(const CgsGui::GuiEventLoadNotification* lp) { Log("car.HandleLoadNotification(%d)", lp->miId); }
    void HandleUnloadNotification(const CgsGui::GuiEventUnloadNotification* lp) { Log("car.HandleUnloadNotification(%d)", lp->miId); }
    bool HandleAptLoadTriggers(const CgsGui::GuiEventAptTriggerPayload* lp) { Log("car.HandleAptLoadTriggers(%s)", lp->mpacComponentName); return true; }
};

// the three SetupComponents payloads (BrnGuiDemangledEventTypes.h shapes)
struct GuiEventAudioGenericSequence { u8 maData[4]; s32 GetEventType() const { return 468; } };
struct alignas(8) GuiEventPostEventFreeCarSequenceStart { u8 maData[8]; s32 GetEventType() const { return 302; } };
struct GuiEventRequestTraining { s32 meTrainingType; s32 GetEventType() const { return 572; } };

// The production class shape (BrnOfflineRivalShutdown.h), members public so the test can drive them.
struct OfflineRivalShutdown : public CgsGui::State
{
    enum EOfflineRivalShutdownState
    {
        E_OFFLINERIVALSHUTDOWNSTATE_NONE = 0, E_OFFLINERIVALSHUTDOWNSTATE_LOADINGRESOURCES = 1,
        E_OFFLINERIVALSHUTDOWNSTATE_WAITINGFORCOMPONENTS = 2, E_OFFLINERIVALSHUTDOWNSTATE_RUNNING = 3,
        E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_FADEOUT_TEXT = 4, E_OFFLINERIVALSHUTDOWNSTATE_SET_NEW_TEXT = 5,
        E_OFFLINERIVALSHUTDOWNSTATE_SHOWING_NEW_TEXT = 6, E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_TRANSOUT = 7,
        E_OFFLINERIVALSHUTDOWNSTATE_TRANSOUT_COMPLETE = 8, E_OFFLINERIVALSHUTDOWNSTATE_FINISH = 9,
        E_OFFLINERIVALSHUTDOWNSTATE_COUNT = 10,
    };
    virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);
    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    void HandleIncomingEvents();
    void AppendExpectedComponents();
    void SetupComponents();
    void HandleAptTriggers(const CgsGui::GuiEventAptTriggerPayload* lpEvent);
    virtual void HandleControllerInput(const CgsModule::Event* lpEvent, s32 liEventType);
    void HandleControllerInputPressed(const CgsGui::GuiEventControllerInputPressed* lpEvent);

    static const CgsGui::sResourceTuple maResourcesToLoad[];
    static const u32                    muNumResourcesToLoad;
    static const s32                    maiEventToObserve[];
    static const s32                    miNumEventsObserved;

    EOfflineRivalShutdownState meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_COUNT;
    BrnProgression::Profile*   mpProfile = 0;
    GuiCache*                  mpGuiCache = reinterpret_cast<GuiCache*>(1);
    AnimationComponent         mScreenAnim;
    LargeCarComponent          mShutdownCarComponent;
    ManufacturersIcon          mManufacturerIcon;
    TextField                  mCongratTextCar;
    TextField                  mCongratTextDesc;
    f32                        mfScreenStartTime = -1.0f;
    f32                        mfNewTextStartTime = -1.0f;
};

#include "rcem2_rival_shutdown.inc"
}
}

// ---------------------------------------------------------------------------------------------
static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool LogHas(const std::string& lrText)
{
    for (const std::string& lrLine : Fixture::gaLog) if (lrLine == lrText) return true;
    return false;
}
// Index of the first log line equal to lrText at or after liFrom (-1 when absent).
static int LogAt(const std::string& lrText, int liFrom = 0)
{
    for (size_t i = static_cast<size_t>(liFrom); i < Fixture::gaLog.size(); ++i) if (Fixture::gaLog[i] == lrText) return static_cast<int>(i);
    return -1;
}
static bool InOrder(const std::vector<std::string>& lrLines)
{
    int liAt = 0;
    for (const std::string& lrLine : lrLines)
    {
        const int liFound = LogAt(lrLine, liAt);
        if (liFound < 0) { std::fprintf(stderr, "  (missing in order: %s)\n", lrLine.c_str()); return false; }
        liAt = liFound + 1;
    }
    return true;
}
template <class T> static T Word(const std::vector<u8>& lrBytes, size_t luOffset)
{
    T lValue; std::memcpy(&lValue, lrBytes.data() + luOffset, sizeof(T)); return lValue;
}
static void DumpLog()
{
    for (const std::string& lrLine : Fixture::gaLog) std::fprintf(stderr, "    log: %s\n", lrLine.c_str());
}

int main()
{
    using namespace Fixture;
    using namespace Fixture::BrnGui;
    typedef OfflineRivalShutdown S;

    CgsModule::VariableEventQueue<18432, 16> lInQueue;
    CgsGui::StateInterface lInterface;
    BrnResource::VehicleList lVehicles;
    WorldDataController lWorld; lWorld.mpVehicleList = &lVehicles;
    BrnProgression::Profile lProfile;
    GuiCache lCache; lCache.mpWorldDataController = &lWorld; lCache.mpProfile = &lProfile;
    GuiCache* lpCache = &lCache;
    CgsFsm::ScriptedFsm lFsm = {};

    S lState;
    lState.mpInGuiEventQueue = &lInQueue;
    lState.mpStateInterface  = &lInterface;
    lState.mCongratTextCar.mpcTag  = "cartext";
    lState.mCongratTextDesc.mpcTag = "desc";

    // ---- static data ------------------------------------------------------------------
    Check(S::muNumResourcesToLoad == 4 && S::maResourcesToLoad[0].muId == 224 && S::maResourcesToLoad[1].muId == 59
          && S::maResourcesToLoad[2].muId == 29 && S::maResourcesToLoad[3].muId == 55
          && S::maResourcesToLoad[3].meType == CgsGui::E_GUI_RESOURCETYPE_APT,
          "maResourcesToLoad == {224,59,29,55} x APT, count 4 (image 0x82F27318 / 0x82066898)");
    Check(S::miNumEventsObserved == 5 && S::maiEventToObserve[0] == 6 && S::maiEventToObserve[1] == 21
          && S::maiEventToObserve[2] == 14 && S::maiEventToObserve[3] == 16 && S::maiEventToObserve[4] == 64,
          "maiEventToObserve == {6,21,14,16,64}, count 5 (image 0x8206689C / 0x820668B0)");

    // ---- Construct @0x824B94D0 ---------------------------------------------------------
    lState.Construct(0x42ull, &lFsm);
    Check(LogHas("State::Construct(66,fsm)"), "Construct runs CgsGui::State::Construct(id, fsm) (0x824B9574)");
    Check(lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_NONE && lState.mpGuiCache == 0,
          "Construct zeroes the state word (+0x38) and mpGuiCache (+0x40) (0x824B9578/0x824B957C)");
    Check(gaAsserts.empty(), "Construct with a valid fsm fires no assert");
    lState.Construct(0x42ull, 0);
    Check(gaAsserts.size() == 1 && gaAsserts[0] == "Invalid ScriptedFsm ptr", "Construct asserts \"Invalid ScriptedFsm ptr\" on a null fsm (cpp:88)");
    gaAsserts.clear();

    // ---- OnEnter @0x824B9588 ----------------------------------------------------------
    lState.mpGuiCache = lpCache; lState.mfScreenStartTime = 5.0f;
    lState.meOfflineRivalShutdownState = S::E_OFFLINERIVALSHUTDOWNSTATE_FINISH;
    gaLog.clear();
    lState.OnEnter();
    Check(InOrder({ "RegisterForEvents(6,21,14,16,64 #5)",
                    "anim.Construct(ScreenAnim_cpt,null)",
                    "badge.Construct(ManufacturerIcon_mc,ScreenAnim_mc)",
                    "cartext.Construct(CongratTextCar_cpt,ScreenAnim_mc)",
                    "desc.Construct(CongratTextDesc_cpt,ScreenAnim_mc)",
                    "car.Construct(carLarge_cpt,null)" }),
          "OnEnter: RegisterForEvents(table,5) then the five component Constructs in console order with their parents");
    Check(lState.mfScreenStartTime == 0.0f && lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_NONE
          && lState.mpGuiCache == 0, "OnEnter resets mfScreenStartTime=0, state NONE, mpGuiCache=0 (0x824B967C..0x824B9684)");

    // ---- Update: NONE stays until the cache arrives ------------------------------------
    gaLog.clear();
    lState.Update();
    Check(lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_NONE, "Update in NONE without a cache stays NONE (0x824DB048)");

    // ---- event 64: latch the cache once, hand the car its id ---------------------------
    lInQueue.Push(64, &lpCache, sizeof(lpCache));
    lState.Update();
    lInQueue.maRecords.clear();
    char lacCacheLine[64]; std::snprintf(lacCacheLine, sizeof(lacCacheLine), "car.SetCachePointer(%p)", static_cast<void*>(lpCache));
    Check(lState.mpGuiCache == lpCache, "event 64 latches mpGuiCache (0x824C2584)");
    Check(InOrder({ lacCacheLine, "car.SetCarInfo(123456789abc,123)" }),
          "event 64: car.SetCachePointer(cache) then car.SetCarInfo(GetShutdownCarID(), 0x7B) (0x824C2588..0x824C25A0)");
    Check(lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_LOADINGRESOURCES,
          "the same Update then moves NONE -> LOADINGRESOURCES (events drain first, 0x824DAFF0)");
    GuiCache lOtherCache;
    GuiCache* lpOtherCache = &lOtherCache;
    lInQueue.Push(64, &lpOtherCache, sizeof(lpOtherCache));
    gaLog.clear();
    lState.Update();
    lInQueue.maRecords.clear();
    Check(lState.mpGuiCache == lpCache && LogAt("car.SetCarInfo(123456789abc,123)") < 0,
          "a second event 64 is ignored (the arm is guarded on !mpGuiCache, 0x824C2574)");

    // ---- LOADINGRESOURCES ---------------------------------------------------------------
    gaLog.clear();
    lState.Update();
    Check(LogHas("cache.EnsureResourcesAreLoaded(224:4,59:4,29:4,55:4 #4)") && lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_LOADINGRESOURCES
          && LogAt("car.EnsureResourcesAreLoaded") < 0,
          "LOADINGRESOURCES waits on GuiCache::EnsureResourcesAreLoaded(table,4) and short-circuits the car (0x824DB074)");
    lCache.mbResourcesLoaded = true;
    gaLog.clear();
    lState.Update();
    Check(LogHas("car.EnsureResourcesAreLoaded") && lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_LOADINGRESOURCES
          && LogAt("PlayAptMovie(BrnRivalShutdown,3)") < 0,
          "LOADINGRESOURCES also waits on LargeCarComponent::EnsureResourcesAreLoaded (0x824DB08C)");
    lState.mShutdownCarComponent.mbReady = true;
    lCache.mfTime = 20.0f;
    gaLog.clear();
    lState.Update();
    Check(InOrder({ "PlayAptMovie(BrnRivalShutdown,3)", "car.OnLoad", "cache.ClearExpectedAptComponentList(0)",
                    "cache.AppendExpected(0,ScreenAnim_cpt)", "cache.AppendExpected(0,ManufacturerIcon_mc)",
                    "cache.AppendExpected(0,CongratTextCar_cpt)", "cache.AppendExpected(0,CongratTextDesc_cpt)",
                    "cache.AppendExpected(0,carLarge_cpt)" }),
          "resources in: PlayAptMovie(gGuiResourceIdentifier[224],3), car.OnLoad, ClearExpected(0), the five AppendExpected in order (0x824DB0B0..0x824DB0D8, 0x824B96EC..)");
    Check(lState.mfScreenStartTime == 20.0f, "mfScreenStartTime = GuiCache::GetTime() when the movie starts (0x824DB0C0)");
    Check(lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_WAITINGFORCOMPONENTS, "-> WAITINGFORCOMPONENTS (0x824DB0E0)");

    // ---- WAITINGFORCOMPONENTS -> SetupComponents ---------------------------------------
    gaLog.clear();
    lState.Update();
    Check(lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_WAITINGFORCOMPONENTS && lInterface.mOutQueue.maPosts.empty(),
          "WAITINGFORCOMPONENTS waits on AreAllAptComponentsInitialised(0) (0x824DB120)");
    lCache.mbComponentsInitialised = true;
    lInterface.mOutQueue.maPosts.clear();
    gaLog.clear();
    lState.Update();
    Check(InOrder({ "SnPrintf(len=64)", "badge.Set(list,123456789abc)", "cartext.SetLocalisedText(CAR_CAPS_PUSMC01,9)",
                    "desc.SetLocalisedText(RVLSHUTDOWN_NEW_CAR_DESC1,9)", "car.ShowCar",
                    "anim.AddOutputAptViewState(apt_Transition,transin,0)", "GetVehicleIndex", "GetVehicleData(7)",
                    "AddEvent(ch=40,size=16)", "HasPlayerSeenTrainingType(32)", "AddEvent(ch=40,size=16)" }),
          "SetupComponents: CAR_CAPS_<id> (64-byte SnPrintf), badge, both captions (format 9), ShowCar, transin, the entry lookup, then the two channel-40 posts (0x824D31D4..0x824D33D4)");
    const std::vector<CgsModule::Record>& lrPosts = lInterface.mOutQueue.maPosts;
    Check(lrPosts.size() == 2 && lrPosts[0].miType == 40 && Word<s32>(lrPosts[0].maBytes, 0) == 4 && Word<s32>(lrPosts[0].maBytes, 4) == 468
          && Word<s32>(lrPosts[0].maBytes, 8) == 12 && Word<u32>(lrPosts[0].maBytes, 12) == 0xA1B2C3D4u,
          "post 1 = {4, 468, 12, entry.mRivalUnlockName (+0xC8)} (0x824D3354..0x824D3368)");
    Check(lrPosts.size() == 2 && Word<s32>(lrPosts[1].maBytes, 0) == 4 && Word<s32>(lrPosts[1].maBytes, 4) == 572
          && Word<s32>(lrPosts[1].maBytes, 8) == 12 && Word<s32>(lrPosts[1].maBytes, 12) == 32,
          "tip 32 unseen: post 2 = GuiEventRequestTraining {4, 572, 12, 32} (0x824D33A8..0x824D33D4)");
    Check(lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_RUNNING, "-> RUNNING (0x824DB138)");

    // ---- RUNNING -> fadeOutText at > start + 3.0 s --------------------------------------
    lCache.mfTime = 23.0f;
    gaLog.clear();
    lState.Update();
    Check(lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_RUNNING, "RUNNING holds at exactly start + 3.0 (fcmpu/ble: strictly greater, flt_82065654 = 3.0)");
    lCache.mfTime = 23.01f;
    lState.Update();
    Check(LogHas("anim.AddOutputAptViewState(apt_Transition,fadeOutText,0)")
          && lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_FADEOUT_TEXT,
          "RUNNING past start + 3.0: \"fadeOutText\" -> WAITING_FOR_FADEOUT_TEXT (0x824DB180)");
    gaLog.clear();
    lState.Update();
    Check(gaLog.empty() && lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_FADEOUT_TEXT,
          "WAITING_FOR_FADEOUT_TEXT does nothing on its own (jt cases 4/7/9 -> return)");

    // ---- apt callbacks -------------------------------------------------------------------
    CgsGui::GuiEventAptTriggerPayload lTrigger = { CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE, 0, "SomethingElse_mc", 0, 0 };
    lInQueue.Push(21, &lTrigger, sizeof(lTrigger));
    lState.Update();
    lInQueue.maRecords.clear();
    Check(lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_FADEOUT_TEXT,
          "a TRANSITION_COMPLETE from another clip is ignored (strcmp vs \"ScreenAnim_mc\")");
    lTrigger.mpacComponentName = "ScreenAnim_mc";
    lCache.mfTime = 30.0f;
    lInQueue.Push(21, &lTrigger, sizeof(lTrigger));
    gaLog.clear();
    lState.Update();
    lInQueue.maRecords.clear();
    Check(InOrder({ "desc.SetLocalisedText(POSTRACE_NEW_CAR_INSTRUCTIONS,9)", "anim.AddOutputAptViewState(apt_Transition,fadeInText,0)" })
          && lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_SHOWING_NEW_TEXT && lState.mfNewTextStartTime == 30.0f,
          "ScreenAnim_mc transition done in WAITING_FOR_FADEOUT_TEXT -> SET_NEW_TEXT; that Update shows the instructions, fadeInText, SHOWING_NEW_TEXT, mfNewTextStartTime=now (0x824B98A4, 0x824DB194..0x824DB1D4)");
    lCache.mfTime = 32.0f;
    gaLog.clear();
    lState.Update();
    Check(lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_SHOWING_NEW_TEXT, "SHOWING_NEW_TEXT holds at exactly +2.0 (flt_82065670 = 2.0)");
    lCache.mfTime = 32.01f;
    lState.Update();
    Check(LogHas("anim.AddOutputAptViewState(apt_Transition,transout,0)")
          && lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_TRANSOUT,
          "SHOWING_NEW_TEXT past +2.0: \"transout\" -> WAITING_FOR_TRANSOUT (0x824DB218)");
    lInQueue.Push(21, &lTrigger, sizeof(lTrigger));
    gaLog.clear();
    lState.Update();
    lInQueue.maRecords.clear();
    Check(LogHas("SendStateEvent(ADVANCE)") && lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_FINISH,
          "transout done -> TRANSOUT_COMPLETE; that Update sends \"ADVANCE\" and FINISHes (0x824B9894, 0x824DB238)");
    gaLog.clear();
    lState.Update();
    Check(gaLog.empty() && gaAsserts.empty(), "FINISH is quiet, and the whole ladder fired no assert");

    // ---- ONLOAD forwarding, load/unload notifications, pad input ------------------------
    CgsGui::GuiEventAptTriggerPayload lOnLoad = { CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD, 0, "carLarge_cpt", 0, 0 };
    CgsGui::GuiEventAptTriggerPayload lOnLoadOther = { CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD, 0, "ScreenAnim_mc", 0, 0 };
    CgsGui::GuiEventLoadNotification lLoad = { 77 };
    CgsGui::GuiEventUnloadNotification lUnload = { 78 };
    CgsGui::GuiEventControllerInputPressed lPad = { 0, 49 };
    lInQueue.Push(21, &lOnLoad, sizeof(lOnLoad));
    lInQueue.Push(21, &lOnLoadOther, sizeof(lOnLoadOther));
    lInQueue.Push(14, &lLoad, sizeof(lLoad));
    lInQueue.Push(16, &lUnload, sizeof(lUnload));
    lInQueue.Push(6, &lPad, sizeof(lPad));
    gaLog.clear();
    lState.Update();
    lInQueue.maRecords.clear();
    Check(LogHas("car.HandleAptLoadTriggers(carLarge_cpt)") && LogAt("car.HandleAptLoadTriggers(ScreenAnim_mc)") < 0,
          "ONLOAD is forwarded to the car only for its own name (strcmp vs this+0xD4, 0x824B97D0)");
    Check(InOrder({ "car.HandleLoadNotification(77)", "car.HandleUnloadNotification(78)" }),
          "events 14/16 -> LargeCarComponent::HandleLoadNotification / HandleUnloadNotification (0x824C25C0/0x824C25D0)");
    Check(gaAsserts.empty() && lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_FINISH,
          "event 6 -> HandleControllerInput -> ...Pressed: every legal state ignores the pad (jpt_824B9914 all -> return)");

    // ---- the console's own diagnostics --------------------------------------------------
    s32 liUnknown = 0;
    lInQueue.Push(7, &liUnknown, sizeof(liUnknown));
    lState.Update();
    lInQueue.maRecords.clear();
    Check(gaAsserts.size() == 1 && gaAsserts[0] == "Unhandled event 7 in OfflineRivalShutdown::Update()\n",
          "an unobserved event id hits the streamed default assert (cpp:356)");
    gaAsserts.clear();
    lState.meOfflineRivalShutdownState = S::E_OFFLINERIVALSHUTDOWNSTATE_RUNNING;
    lCache.mfTime = 0.0f;
    lInQueue.Push(21, &lTrigger, sizeof(lTrigger));
    lState.Update();
    lInQueue.maRecords.clear();
    Check(gaAsserts.size() == 1 && gaAsserts[0] == "Unhandled offline rival shutdown state 3 in OfflineRivalShutdown::HandleAptTriggers().\n"
          && lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_RUNNING,
          "a ScreenAnim_mc transition in any other state is the streamed assert and changes nothing (cpp:506)");
    gaAsserts.clear();
    lState.meOfflineRivalShutdownState = S::E_OFFLINERIVALSHUTDOWNSTATE_COUNT;
    lState.Update();
    Check(gaAsserts.size() == 1 && gaAsserts[0] == "Unhandled OfflineRivalShutdownState 10 in OfflineRivalShutdown::Update()\n",
          "an out-of-range state word is Update's streamed default (cpp:287)");
    gaAsserts.clear();
    lInQueue.Push(6, &lPad, sizeof(lPad));
    lState.Update();
    lInQueue.maRecords.clear();
    Check(std::count(gaAsserts.begin(), gaAsserts.end(),
                     std::string("Unhandled OfflineRivalShutdown state 10 in OfflineRivalShutdown::HandleControllerInputPressed()\n")) == 1,
          "...and the pad handler's own streamed default (cpp:584)");
    gaAsserts.clear();

    // ---- SetupComponents with the tip already seen: the free-car sequence ---------------
    lProfile.mbSeenRivalShutdownTip = true;
    lInterface.mOutQueue.maPosts.clear();
    lState.SetupComponents();
    Check(lrPosts.size() == 2 && Word<s32>(lrPosts[1].maBytes, 0) == 8 && Word<s32>(lrPosts[1].maBytes, 4) == 302
          && Word<s32>(lrPosts[1].maBytes, 8) == 16 && Word<u64>(lrPosts[1].maBytes, 16) == 0x1122334455667788ull
          && lrPosts[1].maBytes.size() == 24,
          "tip 32 seen: post 2 = GuiEventPostEventFreeCarSequenceStart {8, 302, 16, voice-over hash}, 24 bytes (0x824D33E8..0x824D3428)");

    // ---- OnLeave @0x824D3060 ------------------------------------------------------------
    lState.meOfflineRivalShutdownState = S::E_OFFLINERIVALSHUTDOWNSTATE_FINISH;
    lInterface.mOutQueue.maPosts.clear();
    gaLog.clear();
    lState.OnLeave();
    Check(InOrder({ "PlayAptMovie(,3)", "car.ReleaseResources", "UnRegisterForEvents(6,21,14,16,64 #5)",
                    "AddEvent(ch=40,size=16)", "cache.ClearExpectedAptComponentList(0)" }),
          "OnLeave: empty movie at level 3, car resources back, unregister, GUI 295, clear the expected list (0x824D30B8..0x824D311C)");
    Check(lrPosts.size() == 1 && Word<s32>(lrPosts[0].maBytes, 0) == 1 && Word<s32>(lrPosts[0].maBytes, 4) == 295
          && Word<s32>(lrPosts[0].maBytes, 8) == 12,
          "OnLeave posts {1, 295, 12} (GUI 295 -> game event 149 RIVAL_SHUTDOWN_DISPLAY_FINSHED) (0x824D30FC..0x824D3108)");
    Check(lState.meOfflineRivalShutdownState == S::E_OFFLINERIVALSHUTDOWNSTATE_NONE && lState.mpGuiCache == 0,
          "OnLeave leaves the state NONE and drops mpGuiCache (0x824D30F8/0x824D3120)");
    gaLog.clear();
    lState.OnLeave();
    Check(LogAt("cache.ClearExpectedAptComponentList(0)") < 0, "OnLeave without a cache skips the clear (0x824D3114)");
    Check(gaAsserts.empty(), "no stray asserts");

    if (giFailures) DumpLog();
    std::printf("Rcem2RivalShutdown: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
