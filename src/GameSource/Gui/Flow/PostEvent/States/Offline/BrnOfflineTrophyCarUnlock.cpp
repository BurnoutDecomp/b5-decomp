// ===================================================================================
// BrnGui::OfflineTrophyCarUnlock -- the offline "trophy car unlocked" post-event state
// (TRPHY_UNLOCK). GetResourcesToLoad is the header's inline; the resource table is below.
//
// THE PRESENTATION, in Update's order (EOfflineTrophyCarUnlockState):
//   NONE                 -> the GuiCache pointer (GUI event 64) arrives: LOADINGRESOURCES
//   LOADINGRESOURCES     -> the four apt banks + the car's own resources are in: play
//                           "BrnTrophyCarUnlock" at level 3, start the screen clock, load the
//                           car, register the five expected apt components: WAITINGFORCOMPONENTS
//   WAITINGFORCOMPONENTS -> they have all reported in: SetupComponents (car name, badge,
//                           "POSTRACE_NEW_CAR_DESC1", the car, "transin"): RUNNING
//   RUNNING              -> 3.0 s after the movie started: "fadeOutText"
//   (HandleAptTriggers)  -> ScreenAnim_mc finished its transition: SET_NEW_TEXT
//   SET_NEW_TEXT         -> "POSTRACE_NEW_CAR_INSTRUCTIONS", "fadeInText": SHOWING_NEW_TEXT
//   SHOWING_NEW_TEXT     -> 2.0 s later: "transout"
//   (HandleAptTriggers)  -> transition finished: TRANSOUT_COMPLETE
//   TRANSOUT_COMPLETE    -> SendStateEvent("ADVANCE"): FINISH
// OnEnter and OnLeave bracket the screen with GUI 470 (GuiEventCarbonCarSequence, entered 1 /
// 0), and OnLeave records the trophy's unlock sequence as seen on the profile.
// ===================================================================================
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineTrophyCarUnlock.h"

#include <cstdlib>                                                        // getenv (the opt-in witness)
#include <cstring>                                                        // strcmp

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SnPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // GuiEventControllerInputPressed
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTriggerPayload
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // E_FORMAT_ID_LOOKUP
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<18432,16>
#include "GameSource/GameState/Progression/BrnProfile.h"                  // Profile::SetSeenTrophyUnlockSequence
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventProgressionProfileData
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // GetVehicleList

namespace BrnGui
{
namespace
{
    // The state IN-queue is the 18KB variable event queue every GUI state reads.
    typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

    // ---- the six observed event ids (maiEventToObserve below) --------------------------------
    const s32 KI_EVENT_CONTROLLER_INPUT_PRESSED = 6;     // -> HandleControllerInput
    const s32 KI_EVENT_LOAD_NOTIFICATION        = 14;    // -> LargeCarComponent
    const s32 KI_EVENT_UNLOAD_NOTIFICATION      = 16;    // -> LargeCarComponent
    const s32 KI_EVENT_APT_TRIGGER              = 21;    // -> HandleAptTriggers
    const s32 KI_EVENT_GUI_CACHE                = 64;    // latch mpGuiCache
    const s32 KI_EVENT_PROGRESSION_PROFILE      = 350;   // latch mpProfile

    // ---- component names, read out of the image's string pool (each length + NUL is the
    //      declared array size of its constant).
    const char KAC_STATE_CPT_NAME[]        = "ScreenAnim_mc";         // char[14]
    const char KAC_SCREENANIM_NAME[]       = "ScreenAnim_cpt";        // char[15]
    const char KAC_SHUTDOWNCAR_COMPNAME[]  = "carLargeCarbon_cpt";    // char[19]
    const char KAC_MANU_ICON_NAME[]        = "ManufacturerIcon_mc";   // char[20]
    const char KAC_CONGRATTEXT_CAR_NAME[]  = "CongratTextCar_cpt";    // char[19]
    const char KAC_CONGRATTEXT_DESC_NAME[] = "CongratTextDesc_cpt";   // char[20]

    // ---- the two page durations (image floats 3.0f / 2.0f, the pooled literals Update loads).
    const f32 KF_SCREEN_DISPLAY_TIME  = 3.0f;
    const f32 KF_NEWTEXT_DISPLAY_TIME = 2.0f;

    // The apt clip every animation below is driven through.
    const char KAC_APT_TRANSITION[] = "apt_Transition";

    // The movie Update plays: gGuiResourceIdentifier[225] == "BrnTrophyCarUnlock", the same
    // id as maResourcesToLoad[0].
    const u32 KU_TROPHY_CAR_UNLOCK_MOVIE_RESOURCE = 225;
    const s32 KI_MOVIE_LEVEL                      = 3;   // and OnLeave's empty movie

    // The resource id-type the trophy car is shown with (the rival screen's car uses 123).
    const BrnGuiResourceId KU_TROPHY_CAR_RESOURCE_ID_TYPE = 124;

    // SetupComponents' "CAR_CAPS_<id>" buffer: SnPrintf is handed one byte less than the
    // buffer, then the last byte is cleared.
    const s32 KI_CAR_CAPS_TEXT_LENGTH = 64;

    // The state-output channel the GUI-out records are posted on.
    const s32 KI_CHANNEL_GUI_OUT = 40;

    // GUI 470, GuiEventCarbonCarSequence { bool mbEntered }: {1, 470, 12, entered} on channel
    // 40, 16 bytes. OnEnter posts entered = 1, OnLeave entered = 0; the GUI->game-state bridge
    // turns it into game signal 107 / 108 and the director's 100%-sequence latches.
    // FLAG: the type's home is BrnGuiEventTypeDefs.h, which does not carry it yet; it is
    // declared here until it is homed there (then this local copy goes).
    struct GuiEventCarbonCarSequence : public CgsGui::GuiEvent<470>
    {
        bool mbEntered;    // +0x0C
        u8   mau8Pad[3];

        explicit GuiEventCarbonCarSequence(bool lbEntered)
            : CgsGui::GuiEvent<470>(1, 12), mbEntered(lbEntered)
        {
            mau8Pad[0] = mau8Pad[1] = mau8Pad[2] = 0;
        }
    };
    static_assert(sizeof(GuiEventCarbonCarSequence) == 16,
                  "the console posts a 16-byte record for GUI 470");

    void PostCarbonCarSequence(CgsGui::StateInterface* lpStateInterface, bool lbEntered)
    {
        GuiEventCarbonCarSequence lEvent(lbEntered);
        lpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lEvent)));
    }

    // The console streams the fixed prefix, the offending value and a suffix into the global
    // assert message buffer and fires unconditionally; building into a stack buffer is the
    // committed OfflineRivalShutdown / CompletedGame precedent.
    void FireUnexpectedStateAssert(const char* lpacMessage, s32 liState, const char* lpacSuffix)
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << lpacMessage << liState << lpacSuffix;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    // [FLAG PC witness] NOT IN THE CONSOLE. Opt-in (BRN_TROPHY_DIAG), first 16 lines: which
    // presentation step the screen reached.
    void TrophyScreenWitness(const char* lpacStep, s32 liValue)
    {
        static const bool sbDiag = (getenv("BRN_TROPHY_DIAG") != 0);
        static s32 siLinesLeft = 16;
        if (sbDiag && siLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            --siLinesLeft;
            *CgsDev::Log::gpDebugPrint << "[trophy] screen " << lpacStep << " " << liValue << "\n";
        }
    }
}

// ---- observed events ----------------------------------------------------------------------
// Read out of the image: {6, 21, 14, 16, 64, 350}, followed by the count word 6 (OnEnter and
// OnLeave pass 6 as well). Each id is a live arm of HandleIncomingEvents.
const s32 OfflineTrophyCarUnlock::maiEventToObserve[] =
{
    KI_EVENT_CONTROLLER_INPUT_PRESSED,   // 6
    KI_EVENT_APT_TRIGGER,                // 21
    KI_EVENT_LOAD_NOTIFICATION,          // 14
    KI_EVENT_UNLOAD_NOTIFICATION,        // 16
    KI_EVENT_GUI_CACHE,                  // 64
    KI_EVENT_PROGRESSION_PROFILE,        // 350
};
const s32 OfflineTrophyCarUnlock::miNumEventsObserved = 6;

// ---- resources ----------------------------------------------------------------------------
// The four apt packages the screen loads (read from the image).
const CgsGui::sResourceTuple OfflineTrophyCarUnlock::maResourcesToLoad[] =
    { { 225, CgsGui::E_GUI_RESOURCETYPE_APT }, { 59, CgsGui::E_GUI_RESOURCETYPE_APT },
      {  29, CgsGui::E_GUI_RESOURCETYPE_APT }, { 55, CgsGui::E_GUI_RESOURCETYPE_APT } };
const u32 OfflineTrophyCarUnlock::muNumResourcesToLoad = 4;

// ---- Construct ----------------------------------------------------------------------------
// Assert the fsm pointer, run the base Construct, then zero the state word, the cache
// pointer and the profile pointer.
void OfflineTrophyCarUnlock::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
{
    CGS_ASSERT(lpFsm != 0, "Invalid ScriptedFsm ptr");

    CgsGui::State::Construct(liId, lpFsm);

    meOfflineTrophyCarUnlockState = E_OFFLINETROPHYCARUNLOCKSTATE_NONE;
    mpGuiCache                    = 0;
    mpProfile                     = 0;
}

// ---- OnEnter ------------------------------------------------------------------------------
// Register the six observed events, Construct the five embedded components (each through its
// vtable slot 0), reset the state word, both pointers and the screen clock, then tell the
// game the carbon-car sequence has started (GUI 470, entered).
void OfflineTrophyCarUnlock::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    mScreenAnim.Construct(KAC_SCREENANIM_NAME, mpStateInterface, 0);
    mManufacturerIcon.Construct(KAC_MANU_ICON_NAME, mpStateInterface, KAC_STATE_CPT_NAME);
    mCongratTextCar.Construct(KAC_CONGRATTEXT_CAR_NAME, mpStateInterface, KAC_STATE_CPT_NAME);
    mCongratTextDesc.Construct(KAC_CONGRATTEXT_DESC_NAME, mpStateInterface, KAC_STATE_CPT_NAME);
    mTrophyCarComponent.Construct(KAC_SHUTDOWNCAR_COMPNAME, mpStateInterface, 0);

    meOfflineTrophyCarUnlockState = E_OFFLINETROPHYCARUNLOCKSTATE_NONE;
    mpGuiCache                    = 0;
    mpProfile                     = 0;
    mfScreenStartTime             = 0.0f;

    PostCarbonCarSequence(mpStateInterface, true);

    TrophyScreenWitness("enter", 0);
}

// ---- OnLeave ------------------------------------------------------------------------------
// In the console's order: mark the unlock sequence seen (only once both the cache and the
// profile have arrived), drop the level-3 movie (PlayAptMovie("", 3)), hand the car's
// resources back, post GUI 470 (entered = 0), unregister, reset the state word, and clear the
// screen flow's expected-component list if the cache arrived.
void OfflineTrophyCarUnlock::OnLeave()
{
    if (mpGuiCache != 0 && mpProfile != 0)
    {
        mpProfile->SetSeenTrophyUnlockSequence(mpGuiCache->GetTrophyCarUnlockType());
    }

    mpStateInterface->PlayAptMovie("", KI_MOVIE_LEVEL);

    mTrophyCarComponent.ReleaseResources();

    PostCarbonCarSequence(mpStateInterface, false);

    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

    meOfflineTrophyCarUnlockState = E_OFFLINETROPHYCARUNLOCKSTATE_NONE;

    if (mpGuiCache != 0)
    {
        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        mpGuiCache = 0;
    }

    TrophyScreenWitness("leave", 0);
}

// ---- Update -------------------------------------------------------------------------------
// Drain the in-queue, then run one step of the presentation ladder (cases 4, 7 and 9 share
// the plain return: HandleAptTriggers is what moves 4 and 7 on).
void OfflineTrophyCarUnlock::Update()
{
    HandleIncomingEvents();

    switch (meOfflineTrophyCarUnlockState)
    {
    case E_OFFLINETROPHYCARUNLOCKSTATE_NONE:
        // Nothing can load until event 64 has handed the cache over.
        if (mpGuiCache != 0)
        {
            meOfflineTrophyCarUnlockState = E_OFFLINETROPHYCARUNLOCKSTATE_LOADINGRESOURCES;
        }
        break;

    case E_OFFLINETROPHYCARUNLOCKSTATE_LOADINGRESOURCES:
        if (mpGuiCache != 0
            && mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad)
            && mTrophyCarComponent.EnsureResourcesAreLoaded())
        {
            mpStateInterface->PlayAptMovie(
                gGuiResourceIdentifier[KU_TROPHY_CAR_UNLOCK_MOVIE_RESOURCE], KI_MOVIE_LEVEL);
            mfScreenStartTime = mpGuiCache->GetTime();
            mTrophyCarComponent.OnLoad();
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            AppendExpectedComponents();
            meOfflineTrophyCarUnlockState = E_OFFLINETROPHYCARUNLOCKSTATE_WAITINGFORCOMPONENTS;
            TrophyScreenWitness("movie", static_cast<s32>(KU_TROPHY_CAR_UNLOCK_MOVIE_RESOURCE));
        }
        break;

    case E_OFFLINETROPHYCARUNLOCKSTATE_WAITINGFORCOMPONENTS:
        CGS_ASSERT(mpGuiCache, "mpGuiCache");
        if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            SetupComponents();
            meOfflineTrophyCarUnlockState = E_OFFLINETROPHYCARUNLOCKSTATE_RUNNING;
        }
        break;

    case E_OFFLINETROPHYCARUNLOCKSTATE_RUNNING:
        if (mpGuiCache->GetTime() > mfScreenStartTime + KF_SCREEN_DISPLAY_TIME)
        {
            mScreenAnim.AddOutputAptViewState(KAC_APT_TRANSITION, "fadeOutText", false);
            meOfflineTrophyCarUnlockState = E_OFFLINETROPHYCARUNLOCKSTATE_WAITING_FOR_FADEOUT_TEXT;
        }
        break;

    case E_OFFLINETROPHYCARUNLOCKSTATE_WAITING_FOR_FADEOUT_TEXT:
    case E_OFFLINETROPHYCARUNLOCKSTATE_WAITING_FOR_TRANSOUT:
    case E_OFFLINETROPHYCARUNLOCKSTATE_FINISH:
        break;

    case E_OFFLINETROPHYCARUNLOCKSTATE_SET_NEW_TEXT:
        mCongratTextDesc.SetLocalisedText("POSTRACE_NEW_CAR_INSTRUCTIONS",
                                          CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
        mScreenAnim.AddOutputAptViewState(KAC_APT_TRANSITION, "fadeInText", false);
        meOfflineTrophyCarUnlockState = E_OFFLINETROPHYCARUNLOCKSTATE_SHOWING_NEW_TEXT;
        mfNewTextStartTime = mpGuiCache->GetTime();
        break;

    case E_OFFLINETROPHYCARUNLOCKSTATE_SHOWING_NEW_TEXT:
        if (mpGuiCache->GetTime() > mfNewTextStartTime + KF_NEWTEXT_DISPLAY_TIME)
        {
            mScreenAnim.AddOutputAptViewState(KAC_APT_TRANSITION, "transout", false);
            meOfflineTrophyCarUnlockState = E_OFFLINETROPHYCARUNLOCKSTATE_WAITING_FOR_TRANSOUT;
        }
        break;

    case E_OFFLINETROPHYCARUNLOCKSTATE_TRANSOUT_COMPLETE:
        SendStateEvent("ADVANCE");
        meOfflineTrophyCarUnlockState = E_OFFLINETROPHYCARUNLOCKSTATE_FINISH;
        TrophyScreenWitness("advance", 0);
        break;

    default:
        FireUnexpectedStateAssert("Unhandled OfflineTrophyCarUnlockState ",
                                  static_cast<s32>(meOfflineTrophyCarUnlockState),
                                  " in OfflineTrophyCarUnlock::Update()\n");
        break;
    }
}

// ---- HandleIncomingEvents -----------------------------------------------------------------
// Drain the state's in-queue through the six observed ids. The cache arm latches ONCE and
// immediately hands the car component the cache and the trophy car's id (the car component's
// SetCachePointer carries the "NULL != lpGuiCache" assert, GetTrophyCarID the
// "kCGSID_NULL != mTrophyCarID" one). The profile arm also latches once, asserting after the
// store.
void OfflineTrophyCarUnlock::HandleIncomingEvents()
{
    StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;

    for (s32 liEventType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
         lpEvent != 0;
         liEventType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        switch (liEventType)
        {
        case KI_EVENT_CONTROLLER_INPUT_PRESSED:
            // Through the vtable, with the event id as the second argument.
            HandleControllerInput(lpEvent, liEventType);
            break;

        case KI_EVENT_LOAD_NOTIFICATION:
            mTrophyCarComponent.HandleLoadNotification(
                reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent));
            break;

        case KI_EVENT_UNLOAD_NOTIFICATION:
            mTrophyCarComponent.HandleUnloadNotification(
                reinterpret_cast<const CgsGui::GuiEventUnloadNotification*>(lpEvent));
            break;

        case KI_EVENT_APT_TRIGGER:
            HandleAptTriggers(reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent));
            break;

        case KI_EVENT_GUI_CACHE:
            if (mpGuiCache == 0)
            {
                mpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
                mTrophyCarComponent.SetCachePointer(mpGuiCache);
                mTrophyCarComponent.SetCarInfo(mpGuiCache->GetTrophyCarID(),
                                               KU_TROPHY_CAR_RESOURCE_ID_TYPE);
            }
            break;

        case KI_EVENT_PROGRESSION_PROFILE:
            if (mpProfile == 0)
            {
                mpProfile =
                    reinterpret_cast<const GuiEventProgressionProfileData*>(lpEvent)->mpProfile;
                CGS_ASSERT(mpProfile, "mpProfile");
            }
            break;

        default:
            FireUnexpectedStateAssert("Unhandled event ", liEventType,
                                      " in OfflineTrophyCarUnlock::Update()\n");
            break;
        }
    }
}

// ---- AppendExpectedComponents -------------------------------------------------------------
// Register the five components whose apt counterparts must report in before the screen
// counts as initialised, each by its own composed name, on the screen flow, in the console's
// order.
void OfflineTrophyCarUnlock::AppendExpectedComponents()
{
    CGS_ASSERT(mpGuiCache, "mpGuiCache");

    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mScreenAnim.GetName());
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mManufacturerIcon.GetName());
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mCongratTextCar.GetName());
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mCongratTextDesc.GetName());
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTrophyCarComponent.GetName());
}

// ---- SetupComponents ----------------------------------------------------------------------
// Fill the page once every component is live: the car's localised name "CAR_CAPS_<id>", its
// manufacturer badge, the first caption "POSTRACE_NEW_CAR_DESC1", the car itself and the
// clip's "transin". Unlike the rival screen there are no audio / training posts here.
void OfflineTrophyCarUnlock::SetupComponents()
{
    CGS_ASSERT(mpGuiCache, "mpGuiCache");

    const CgsID lTrophyCarID = mpGuiCache->GetTrophyCarID();

    char lacCarId[KI_CGSID_STRING_LEN];
    CgsIDConvertToString(lTrophyCarID, lacCarId);

    char lacCarCapsText[KI_CAR_CAPS_TEXT_LENGTH];
    CgsCore::SnPrintf(lacCarCapsText, KI_CAR_CAPS_TEXT_LENGTH - 1, "CAR_CAPS_%s", lacCarId);
    lacCarCapsText[KI_CAR_CAPS_TEXT_LENGTH - 1] = '\0';

    mManufacturerIcon.Set(mpGuiCache->GetWorldDataController()->GetVehicleList(), lTrophyCarID);
    mCongratTextCar.SetLocalisedText(lacCarCapsText, CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
    mCongratTextDesc.SetLocalisedText("POSTRACE_NEW_CAR_DESC1",
                                      CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
    mTrophyCarComponent.ShowCar();
    mScreenAnim.AddOutputAptViewState(KAC_APT_TRANSITION, "transin", false);
}

// ---- HandleAptTriggers --------------------------------------------------------------------
// The apt callbacks. An ONLOAD for the car component's own name is forwarded to it; a
// TRANSITION_COMPLETE from the "ScreenAnim_mc" clip moves the two waiting states on
// (fadeOutText finished -> SET_NEW_TEXT, transout finished -> TRANSOUT_COMPLETE). Any other
// state at that moment is the console's streamed assert. The name tests are the inlined
// strcmp loops (operand order kept).
void OfflineTrophyCarUnlock::HandleAptTriggers(const CgsGui::GuiEventAptTriggerPayload* lpEvent)
{
    CGS_ASSERT(lpEvent, "lpEvent");

    if (lpEvent->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
    {
        if (std::strcmp(mTrophyCarComponent.GetName(), lpEvent->mpacComponentName) == 0)
        {
            mTrophyCarComponent.HandleAptLoadTriggers(lpEvent);
        }
    }
    else if (lpEvent->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE)
    {
        if (std::strcmp(lpEvent->mpacComponentName, KAC_STATE_CPT_NAME) == 0)
        {
            if (meOfflineTrophyCarUnlockState == E_OFFLINETROPHYCARUNLOCKSTATE_WAITING_FOR_FADEOUT_TEXT)
            {
                meOfflineTrophyCarUnlockState = E_OFFLINETROPHYCARUNLOCKSTATE_SET_NEW_TEXT;
            }
            else if (meOfflineTrophyCarUnlockState == E_OFFLINETROPHYCARUNLOCKSTATE_WAITING_FOR_TRANSOUT)
            {
                meOfflineTrophyCarUnlockState = E_OFFLINETROPHYCARUNLOCKSTATE_TRANSOUT_COMPLETE;
            }
            else
            {
                FireUnexpectedStateAssert("Unhandled offline trophy car unlock state ",
                                          static_cast<s32>(meOfflineTrophyCarUnlockState),
                                          " in OfflineTrophyCarUnlock::HandleAptTriggers().\n");
            }
        }
    }
}

// ---- HandleControllerInput ----------------------------------------------------------------
// Only the "pressed" record (id 6) is answered.
void OfflineTrophyCarUnlock::HandleControllerInput(const CgsModule::Event* lpEvent, s32 liEventType)
{
    CGS_ASSERT(lpEvent, "lpEvent");

    if (liEventType == KI_EVENT_CONTROLLER_INPUT_PRESSED)
    {
        HandleControllerInputPressed(
            reinterpret_cast<const CgsGui::GuiEventControllerInputPressed*>(lpEvent));
    }
}

// ---- HandleControllerInputPressed ---------------------------------------------------------
// The screen is not skippable: the jump table's ten entries all point at the return, so
// every legal state ignores the pad; only an out-of-range state word reaches the streamed
// assert.
void OfflineTrophyCarUnlock::HandleControllerInputPressed(
    const CgsGui::GuiEventControllerInputPressed* lpControllerPressedEvent)
{
    CGS_ASSERT(lpControllerPressedEvent, "lpControllerPressedEvent");

    switch (meOfflineTrophyCarUnlockState)
    {
    case E_OFFLINETROPHYCARUNLOCKSTATE_NONE:
    case E_OFFLINETROPHYCARUNLOCKSTATE_LOADINGRESOURCES:
    case E_OFFLINETROPHYCARUNLOCKSTATE_WAITINGFORCOMPONENTS:
    case E_OFFLINETROPHYCARUNLOCKSTATE_RUNNING:
    case E_OFFLINETROPHYCARUNLOCKSTATE_WAITING_FOR_FADEOUT_TEXT:
    case E_OFFLINETROPHYCARUNLOCKSTATE_SET_NEW_TEXT:
    case E_OFFLINETROPHYCARUNLOCKSTATE_SHOWING_NEW_TEXT:
    case E_OFFLINETROPHYCARUNLOCKSTATE_WAITING_FOR_TRANSOUT:
    case E_OFFLINETROPHYCARUNLOCKSTATE_TRANSOUT_COMPLETE:
    case E_OFFLINETROPHYCARUNLOCKSTATE_FINISH:
        break;

    default:
        FireUnexpectedStateAssert("Unhandled OfflineTrophyCarUnlock state ",
                                  static_cast<s32>(meOfflineTrophyCarUnlockState),
                                  " in OfflineTrophyCarUnlock::HandleControllerInputPressed()\n");
        break;
    }
}
}
