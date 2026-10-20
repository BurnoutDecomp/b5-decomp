// Hosted-only selected-frame regression. Bodies and data declarations are extracted
// from production by the runner. Fixture boundaries: OS/VM/render/audio, large
// module collaborators, calendar/profile storage, and FSM entry scheduling.
// The guarded SetPlayerInfo boundary reports the original omission before allowing
// a NULL member call. The recovered unguarded body is unchanged and runs when bound.
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>
#include <memory>

using u8 = uint8_t; using s8 = int8_t; using u32 = uint32_t;
using s32 = int32_t; using u64 = uint64_t; using f32 = float;
using CgsID = u64;
static u32 guChecks = 0, guFailures = 0, guAsserts = 0;
static void Check(const char* label, bool ok) {
    ++guChecks; if (!ok) { ++guFailures; std::printf("FAIL %s\n", label); }
}
namespace CgsDev {
    enum { E_PRINTMODE_HEX = 1 };
    struct StrStream {
        char* mpText;
        StrStream(char* text, s32 size) : mpText(text) { if (size) text[0] = 0; }
        template<class T> StrStream& operator<<(const T&) { return *this; }
        const char* GetBuffer() const { return mpText; }
    };
    namespace Assert {
        const s32 KI_MESSAGEBUFFERSIZE = 1024;
        inline void BeginAssert() {} inline void EndAssert() {}
        inline void FireAssert(const char* text, const char*, s32) {
            ++guAsserts; std::printf("ASSERT %s\n", text);
        }
    }
    namespace Message { static u64 gxMessageFilterFlags = 0; }
    namespace Log {
        struct Stream { template<class T> Stream& operator<<(const T&) { return *this; } };
        static Stream* gpDebugPrint = nullptr;
    }
}
#define CGS_ASSERT(c,m) do { if (!(c)) CgsDev::Assert::FireAssert(m,__FILE__,__LINE__); } while (0)
namespace CgsCore {
    inline int SnPrintf(char* output, s32 count, const char* fmt, ...) {
        va_list args; va_start(args, fmt);
        int n = std::vsnprintf(output, static_cast<size_t>(count), fmt, args);
        va_end(args); return n;
    }
}
#include "license_handoff_queue.inc"
namespace BrnProgression { struct Profile; struct ProgressionData {}; }
namespace BrnGui { struct GuiCache; }
namespace CgsGui {
    struct EventObserver {};
    struct GuiEventTimeInfo {};
    enum { E_GUI_PRIORITY_REMOVAL = 2, E_GUI_PRIORITY_BLOCKING = 3 };
    enum ResourceType { E_GUI_RESOURCETYPE_START = 0, E_GUI_RESOURCETYPE_APT = 4 };
    struct sResourceTuple { u32 muId; ResourceType meType; };
}
#include "license_handoff_events.inc"
namespace CgsSystem {
    struct DateAndTime {
        s32 year = 2026, month = 10, day = 20;
        s32 GetYear() const { return year; }
        s32 GetMonth() const { return month; }
        s32 GetDay() const { return day; }
    };
}
namespace CgsLanguage {
    struct LanguageManager {
        enum ParameterFormatType : s32 { E_UNUSED = 0 };
        void FormatDateString(char* out, s32 day, s32 month, s32 year, s32 count) {
            std::snprintf(out, static_cast<size_t>(count), "%04d-%02d-%02d", year, month, day);
        }
    };
}
namespace BrnProgression {
    // A storage/calendar boundary; the production getter is compiled verbatim.
    struct Profile {
        bool mbHaveSet100PercentCompletedDate = false;
        CgsSystem::DateAndTime mIssued, mCompleted;
        #include "license_handoff_profile_getter.inc"
        CgsSystem::DateAndTime Get100PercentCompletedDate() const { return mCompleted; }
        CgsSystem::DateAndTime GetLicenceIssuedDate() const { return mIssued; }
        void SetLicenceIssuedDateAsNow() { mIssued = CgsSystem::DateAndTime{}; }
    };
}
static BrnProgression::Profile* gpExpectedProfile = nullptr;
namespace CgsGui {
    namespace CgsGuiModuleIO {
        struct InputBuffer {
            CgsModule::VariableEventQueue<32768,16> mEvents;
            InputBuffer() { mEvents.Construct(); }
            void LockForRead() const {} void UnlockForRead() const {}
            auto* GetGuiEvents() { return &mEvents; }
            const auto* GetGuiEvents() const { return &mEvents; }
        };
    }
    struct StateInterface {
        EventObserver* mpObserver = nullptr;
        CgsModule::VariableEventQueue<65536,16> mOutEventQueue;
        CgsLanguage::LanguageManager mLanguage;
        StateInterface() { mOutEventQueue.Construct(); }
        void RegisterForEvents(const s32*, s32);
        auto* GetOutputEventQueue() { return &mOutEventQueue; }
        auto* GetLanguageManager() { return &mLanguage; }
    };
    struct GuiEventAptTrigger {
        enum { E_APT_EVENT_ONLOAD = 1, E_APT_EVENT_TRANSITION_COMPLETE = 2 };
    };
    struct GuiEventAptTriggerPayload {
        s32 meEventType; const char* mpacComponentName;
    };
}
namespace BrnGui {
    enum { E_GUIFLOW_SCREEN=0, E_GUIFLOW_HUD=1, E_GUIFLOW_OVERLAY=2, E_GUIFLOW_COUNT=3 };
    struct WorldDataController {
        s32 GetRequiredWinsInRank(s32 rank) const { return 10 + 10*rank; }
    };
    struct GuiCache {
        WorldDataController mWorld;
        BrnProgression::Profile* mpProfile = nullptr;
        const BrnProgression::ProgressionData* mpData = nullptr;
        const char* GetPlayerName() const { return "PLAYER_NAME_STRING_ID"; }
        auto* GetWorldDataController() { return &mWorld; }
        s32 GetCamStatus() const { return 0; }
        void RecTimeInfo(const CgsGui::GuiEventTimeInfo*) {}
        void RecEvent(const CgsModule::Event* event, s32 id) {
            if (id == 350) {
                const auto* p = reinterpret_cast<const GuiEventProgressionProfileData*>(event);
                mpProfile = p->mpProfile; mpData = p->mpProgressionData;
            }
        }
    };
    struct IconComponent {
        CgsGui::StateInterface* mpStateInterface = nullptr;
        std::string mName;
        void Construct(const char* name, CgsGui::StateInterface* iface,
                       const char* const*, const char*) { mName=name; mpStateInterface=iface; }
        void Construct(const char* name, CgsGui::StateInterface* iface,
                       const char* parent) { Construct(name,iface,nullptr,parent); }
        const char* GetName() const { return mName.c_str(); }
        void AddOutputAptViewState(const char*, const char*, bool) {}
    };
    struct TextField : IconComponent {
        std::string mText;
        void SetText(const char* text) { mText = text ? text : ""; }
        const char* GetText() const { return mText.c_str(); }
        void ClearText() { mText.clear(); }
        void OutputAptData() {}
        void SetLocalisedText(const char* text, CgsLanguage::LanguageManager::ParameterFormatType,
                              s32 = 0, const char* = nullptr,
                              CgsLanguage::LanguageManager::ParameterFormatType =
                                  CgsLanguage::LanguageManager::E_UNUSED) { SetText(text); }
        void SetLocalisedText(s32 value, CgsLanguage::LanguageManager::ParameterFormatType) {
            mText = std::to_string(value);
        }
    };
    struct ButtonIconComponent { enum { E_PADBUTTON_BACK=5, E_PADBUTTON_SELECT=4 }; };
    struct PhotoBoothComponent {
        enum { E_PHOTOBOOTH_STYLE_DMV_FULL_PAGE=0,
               E_TAKEPHOTOSTRING_CONTINUE=2, E_BACKSTRING_CANCEL=3 };
        void Construct(const char*, CgsGui::StateInterface*, s32,s32,s32,s32,const char*) {}
        void SetVisualStyle(s32) {} void SetCachePointer(GuiCache*) {}
        void SetProfilePointer(BrnProgression::Profile*) {}
        void SetButtonPromptVisible(bool) {}
        void HandleCompressedStillImageEvent(const CgsModule::Event*) {}
    };
    struct LicenseComponent : IconComponent {
        #include "license_handoff_license_enum.inc"
        CgsGui::sResourceTuple maLicenseResourcesToLoad[2]{};
        s32 miNumResourcesToLoad=0;
        GuiCache* mpGuiCache=nullptr;
        BrnProgression::Profile* mpProfile=nullptr;
        TextField mPlayerNameTextField, mNextRankTextField, mDateIssuedTextField;
        TextField mCompletionMonthField, mCompletionDateField, mCompletionYearField;
        bool mbAtTopRank=false, mbElite=false, mbFinishedGame=false;
        s32 miCurrentRank=0, miWinsInCurrentRank=0, miPercentComplete=0;
        bool mbVisible=false, mbHiding=false;
        ELicenseStates meCurrentLicenseState=E_LICENSE_CONSTRUCTED;
        bool mbShowUpgradePending=false, mbShowPoints=false, mbForceCentred=false;
        f32 mfRequiredWinsTickUpRate=0, mfTimeToShowNextRank=0, mfTimeToNextWinIncrement=0;
        bool mbShowingProfilePicture=false;
        void Construct(const char*, CgsGui::StateInterface*, const char*);
        void SetCachePointer(GuiCache*);
        void SetProfilePointer(BrnProgression::Profile*);
        void SetPlayerInfoRecovered(const char*,bool,bool,s32,s32,bool,bool);
        void SetPlayerInfo(const char* n,bool e,bool f,s32 r,s32 p,bool u,bool s) {
            // TEST BOUNDARY ONLY: do not evaluate a NULL production Profile member.
            Check("license profile bound before cache use", mpProfile == gpExpectedProfile);
            if (mpProfile != gpExpectedProfile) return;
            SetPlayerInfoRecovered(n,e,f,r,p,u,s);
        }
        void UpdateDirt();
        void HandleAptLoadTriggers(const CgsGui::GuiEventAptTriggerPayload*) {}
        void HandleAptTransitionTriggers(const CgsGui::GuiEventAptTriggerPayload*) {}
    };
    struct Intro {
        #include "license_handoff_intro_enum.inc"
        static const s32 maiEventToObserve[8], miNumEventsObserved;
        static const char KAC_SCREENANIM_NAME[], KAC_LICENSECOMP_NAME[], KAC_PHOTOBOOTHCOMP_NAME[];
        static const char KAC_WELCOMETEXT1COMP_NAME[], KAC_WELCOMETEXT2COMP_NAME[];
        static const char KAC_MONTHTEXTFIELD_NAME[], KAC_DAYTEXTFIELD_NAME[], KAC_YEARTEXTFIELD_NAME[];
        CgsGui::StateInterface* mpStateInterface=nullptr;
        void* mpInGuiEventQueue=nullptr;
        GuiCache* mpGuiCache=nullptr;
        BrnProgression::Profile* mpProfile=nullptr;
        EIntroState meIntroState=E_INTROSTATE_NONE;
        bool mbVoiceOverPlaying=false;
        f32 mfPauseTimer=0;
        IconComponent mScreenAnim,mWelcomeText1Anim,mWelcomeText2Anim;
        LicenseComponent mLicenseComponent;
        PhotoBoothComponent mPhotoBoothComponent;
        TextField mMonthTextField,mDayTextField,mYearTextField;
        void OnEnter();
        void HandleIncomingEvents();
        void HandleControllerInput(const CgsModule::Event*,s32) {}
    };
    inline void LogIntroVoiceOver(bool,s32) {}
    struct GuiEventRoadRuleChangeMode { s32 meScoreType; };
    struct GuiEventRunFsm {}; struct GuiChallengeStartEvent {};
    struct GuiChallengeTriggerResponse {}; struct GuiChallengeUpdateEvent {};
    struct GuiEventFburnChallengeEveryPlayerStatus {}; struct GuiChallengeNotActiveStartEvent {};
    struct Collaborator {
        BrnProgression::Profile* mpProfile=nullptr;
        const BrnProgression::ProgressionData* mpData=nullptr;
        void RunFsm(const GuiEventRunFsm*) {} void HandleHudStateLoadComplete() {}
        void SetBlackBarSize(f32) {} void RecvEvent(const CgsModule::Event*,s32) {}
        void RecEvent(const CgsModule::Event*,s32,s32) {}
        void SetRoadRuleMode(s32) {} void Update() {} void SelectNext() {}
        void StartChallenge(const GuiChallengeStartEvent*) {}
        void TriggerChallenge(const GuiChallengeTriggerResponse*) {}
        void HandleNewData(const GuiChallengeUpdateEvent*) {}
        void StartResults() {} void FinishChallenge() {}
        void HandleCompletionStatus(const GuiEventFburnChallengeEveryPlayerStatus*) {}
        void StartNotActiveChallenge(const GuiChallengeNotActiveStartEvent*) {}
        void EndNotActiveChallenge() {}
        void SetProgressionProfile(BrnProgression::Profile* p,const BrnProgression::ProgressionData* d) {
            mpProfile=p;mpData=d;
        }
    };
    struct NotificationBuffer {
        CgsModule::VariableEventQueue<4096,16> mEvents;
        NotificationBuffer() { mEvents.Construct(); }
        void LockForWrite() {} void UnlockForWrite() {}
        auto* GetLoadNotificationsNonConst() { return &mEvents; }
    };
    struct FlowBoundary {
        Intro* mpIntro=nullptr; bool mbEntered=false;
        void Update() {
            if (!mpIntro) return;
            if (!mbEntered) { mpIntro->OnEnter(); mbEntered=true; }
            else mpIntro->HandleIncomingEvents();
        }
    };
    inline void FreeRoamRouteWitness(s32,const char*) {}
    struct GuiModule {
        #include "license_handoff_module_bounds.inc"
        struct PriorityClaim {
            bool mbActive=false; s32 miEventType=-1;
            bool mabOverriddenEventIds[KI_MAX_OBSERVED_EVENT_ID]{};
        };
        bool mabObservedEventIds[KI_NUM_EVENT_OBSERVERS][KI_MAX_OBSERVED_EVENT_ID]{};
        PriorityClaim maPriorityClaims[KI_NUM_EVENT_OBSERVERS][KI_MAX_PRIORITY_CLAIMS_PER_FLOW]{};
        bool mabPriorityBlocking[KI_NUM_EVENT_OBSERVERS]{};
        CgsModule::VariableEventQueue<18432,16> mScreenInQueue,mHudInQueue,mOverlayInQueue,mAlwaysAvailInQueue;
        CgsGui::CgsGuiModuleIO::InputBuffer* mpGuiEventInputBuffer=nullptr;
        GuiCache mGuiCache;
        Collaborator mFsmController,mHudMessageDirector,mMovieManager,mProfileManager;
        Collaborator mBurnoutSkillsManager,mFreeburnChallengeManager,mCustomRendererManager,mGuiTracker;
        NotificationBuffer mModelOutputBuffer;
        FlowBoundary mHudFlow,mScreenFlow,mOverlayFlow;
        bool mbForceProfileAutosave=false,mbProfileAutosavePending=false;
        GuiModule() {
            mScreenInQueue.Construct();mHudInQueue.Construct();
            mOverlayInQueue.Construct();mAlwaysAvailInQueue.Construct();
        }
        void RouteEventToFlow(const CgsModule::Event*,s32,s32);
        void DispatchInboundGuiEvents();
        void RecoveredFrameSlice();
        void RecoveredTailControl();
        void FixtureCommitRegistrationOutput(CgsGui::StateInterface*);
    };
}
namespace BrnGameState {
    struct ProgressionBoundary {
        BrnProgression::Profile* mpProfile=nullptr;
        const BrnProgression::ProgressionData* mpData=nullptr;
        bool mbAvailable=false;
        auto* GetProfile() { return mpProfile; }
        const auto* GetProgressionData() { return mpData; }
        bool AreRoadRulesAvailable() { return mbAvailable; }
    };
    void FixturePublishProfile(ProgressionBoundary&,
        CgsModule::VariableEventQueue<13312,16>*);
}
namespace BrnGame {
    void FixtureTranslateProfile(const CgsModule::Event*,s32,
        CgsGui::CgsGuiModuleIO::InputBuffer*);
}
#include "license_handoff_bodies.inc"

static void RunScenario(bool completed) {
    BrnProgression::Profile profile;
    profile.mbHaveSet100PercentCompletedDate=completed;
    BrnProgression::ProgressionData data;
    gpExpectedProfile=&profile;
    CgsGui::StateInterface iface;
    BrnGui::Intro intro;
    auto module=std::make_unique<BrnGui::GuiModule>();
    CgsGui::CgsGuiModuleIO::InputBuffer input;
    module->mpGuiEventInputBuffer=&input;
    intro.mpStateInterface=&iface;intro.mpInGuiEventQueue=&module->mScreenInQueue;
    module->mScreenFlow.mpIntro=&intro;
    module->RecoveredFrameSlice();
    module->FixtureCommitRegistrationOutput(&iface);
    module->RecoveredTailControl();
    Check("entry leaves Intro profile unbound until real 350",intro.mpProfile==nullptr); // 1
    Check("entry leaves License profile unbound until real 350",intro.mLicenseComponent.mpProfile==nullptr); // 2
    Check("entry leaves Intro cache unbound until real 64",intro.mpGuiCache==nullptr); // 3
    Check("entry leaves License cache unbound until real 64",intro.mLicenseComponent.mpGuiCache==nullptr); // 4
    Check("real License Construct sets constructed state",
          intro.mLicenseComponent.meCurrentLicenseState==BrnGui::LicenseComponent::E_LICENSE_CONSTRUCTED); // 5
    Check("real License Construct clears supplied rank",intro.mLicenseComponent.miCurrentRank==-1); // 6
    Check("actual register output subscribes profile 350",module->mabObservedEventIds[0][350]); // 7
    Check("actual register output subscribes cache 64",module->mabObservedEventIds[0][64]); // 8
    s32 registered=0;for(s32 id=0;id<module->KI_MAX_OBSERVED_EVENT_ID;++id)
        if(module->mabObservedEventIds[0][id])++registered;
    Check("actual Intro registers its eight event ids",registered==8); // 9
    Check("entry does not seed stale cache into next observer frame",module->mScreenInQueue.GetLength()==0); // 10
    CgsModule::VariableEventQueue<13312,16> actions;actions.Construct();
    BrnGameState::ProgressionBoundary progression;
    progression.mpProfile=&profile;progression.mpData=&data;progression.mbAvailable=completed;
    BrnGameState::FixturePublishProfile(progression,&actions);
    Check("real PreWorld provider posts one action",actions.GetLength()==1); // 11
    const CgsModule::Event* event=nullptr;s32 bytes=0;
    s32 id=actions.GetFirstEvent(&event,&bytes);
    Check("real PreWorld provider posts native action 193",id==193); // 12
    Check("provider retains native-width full payload",
          bytes==sizeof(BrnGameState::GameStateModuleIO::ProgressionProfileDataAction)); // 13
    const auto* a=reinterpret_cast<const BrnGameState::GameStateModuleIO::ProgressionProfileDataAction*>(event);
    Check("provider uses owned Profile pointer",a->mpProgressionProfile==&profile); // 14
    Check("provider uses owned ProgressionData pointer",a->mpProgressionData==&data); // 15
    Check("provider retains road-rule availability",a->mbAreRoadRulesAvailable==completed); // 16
    BrnGame::FixtureTranslateProfile(event,id,&input);
    Check("real action arm emits one GUI record",input.mEvents.GetLength()==1); // 17
    id=input.mEvents.GetFirstEvent(&event,&bytes);
    Check("real action arm emits GUI 350",id==350); // 18
    const auto* g=reinterpret_cast<const BrnGui::GuiEventProgressionProfileData*>(event);
    Check("typed GUI payload retains Profile pointer",g->mpProfile==&profile); // 19
    Check("typed GUI payload retains ProgressionData pointer",g->mpProgressionData==&data); // 20
    Check("typed GUI payload retains availability",g->mbRoadRulesAvailable==completed); // 21
    module->DispatchInboundGuiEvents();
    Check("actual dispatch forwards owned profile to manager",module->mProfileManager.mpProfile==&profile); // 22
    Check("actual dispatch forwards owned profile to cache",module->mGuiCache.mpProfile==&profile); // 23
    id=module->mScreenInQueue.GetFirstEvent(&event,&bytes);
    Check("current observer frame starts with profile 350",id==350); // 24
    module->RecoveredFrameSlice(); // actual Intro/License call contributes check 25
    module->FixtureCommitRegistrationOutput(&iface);
    module->RecoveredTailControl();
    Check("real Intro profile handler binds owned profile",intro.mpProfile==&profile); // 26
    Check("real License SetProfilePointer binds owned profile",intro.mLicenseComponent.mpProfile==&profile); // 27
    Check("real unguarded License body reaches supplied state",
          intro.mLicenseComponent.meCurrentLicenseState==BrnGui::LicenseComponent::E_LICENSE_DATA_SUPPLIED); // 28
    Check("real unguarded License body supplies player name",
          intro.mLicenseComponent.mPlayerNameTextField.mText=="PLAYER_NAME_STRING_ID"); // 29
    bool dateOk=completed ?
        intro.mLicenseComponent.mCompletionMonthField.mText=="MONTH_SHORT_10" &&
        intro.mLicenseComponent.mCompletionDateField.mText=="20" &&
        intro.mLicenseComponent.mCompletionYearField.mText=="2026" :
        intro.mLicenseComponent.mCompletionMonthField.mText.empty() &&
        intro.mLicenseComponent.mCompletionDateField.mText.empty() &&
        intro.mLicenseComponent.mCompletionYearField.mText.empty();
    Check("real License completion branch supplies or clears date fields",dateOk); // 30
    Check("real License body supplies rank resources",
          intro.mLicenseComponent.miNumResourcesToLoad==2 &&
          intro.mLicenseComponent.maLicenseResourcesToLoad[0].muId==97 &&
          intro.mLicenseComponent.maLicenseResourcesToLoad[1].muId==98); // 31
    Check("selected production paths preserve native assertions",guAsserts==0); // 32
}
int main() {
    RunScenario(false);RunScenario(true);
    if(guChecks!=64) {
        std::printf("ERROR expected exactly 64 executed numeric checks, got %u\n",guChecks);
        return 2;
    }
    std::printf("GUI_LICENSE_INTRO_HANDOFF: %u checks, %u failures\n",guChecks,guFailures);
    return guFailures?1:0;
}
