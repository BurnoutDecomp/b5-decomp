// ===================================================================================
// BrnGui::OfflineRivalShutdown  -- the offline "rival shut down" post-event state (POST_RIVAL)
//   class:BrnGui::OfflineRivalShutdown
//
//   Construct                     @0x824B94D0  ( 46)  (cpp:86)
//   OnEnter                       @0x824B9588  ( 70)  (cpp:104)
//   OnLeave                       @0x824D3060  ( 51)  (cpp:135)
//   Update                        @0x824DAFE0  (187)  (cpp:168)
//   HandleIncomingEvents          @0x824C23D0  (128)  (cpp:301)
//   AppendExpectedComponents      @0x824B96A0  ( 41)  (cpp:372)
//   SetupComponents               @0x824D3130  (193)  (cpp:392)
//   HandleAptTriggers             @0x824B9748  ( 91)  (cpp:475)
//   HandleControllerInput         @0x824BD100  ( 23)  (cpp:525)
//   HandleControllerInputPressed  @0x824B98B8  ( 65)  (cpp:556)
//   (GetResourcesToLoad @0x825008D0 is the header's inline.)
// Reconstructed store-for-store from the ARTIST asm; the DecFIGS PS3 twins of OnLeave and
// SetupComponents were read alongside (PS3 GUI ids 293/463/557/300 are X360 295/468/572/302,
// the +2/+5/+15/+2 id drift of those ranges; the X360 values are the ones below).
//
// ⭐ crash parity G13-X5 (2026-09-23). THIS STATE WAS A HOLLOW SHELL: the header carried only
// GetResourcesToLoad, so the screen flow could enter POST_RIVAL and never leave it (no Update
// ever sent "ADVANCE"). That is why the game-state translator's 120 -> GUI 373 arm (the rival
// SHUTDOWN) was deliberately held back (FX-RCEM, run_rcem_shutdown_gui_events.py): InGame's
// case 373 sends "TO_RVL_POST" straight into this state.
//
// THE PRESENTATION, in Update's order (EOfflineRivalShutdownState):
//   NONE                 -> the GuiCache pointer (GUI event 64) arrives: LOADINGRESOURCES
//   LOADINGRESOURCES     -> the four apt banks + the car's own resources are in: play
//                           "BrnRivalShutdown" at level 3, start the screen clock, load the car,
//                           register the five expected apt components: WAITINGFORCOMPONENTS
//   WAITINGFORCOMPONENTS -> they have all reported in: SetupComponents (car name, badge,
//                           "RVLSHUTDOWN_NEW_CAR_DESC1", the car, "transin", the rival-unlock
//                           audio, the training tip or the free-car sequence): RUNNING
//   RUNNING              -> 3.0 s after the movie started: "fadeOutText"
//   (HandleAptTriggers)  -> ScreenAnim_mc finished its transition: SET_NEW_TEXT
//   SET_NEW_TEXT         -> "POSTRACE_NEW_CAR_INSTRUCTIONS", "fadeInText": SHOWING_NEW_TEXT
//   SHOWING_NEW_TEXT     -> 2.0 s later: "transout"
//   (HandleAptTriggers)  -> transition finished: TRANSOUT_COMPLETE
//   TRANSOUT_COMPLETE    -> SendStateEvent("ADVANCE"): FINISH
// OnLeave then tells the game the display finished (GUI 295 -> game event 149,
// E_EVENT_RIVAL_SHUTDOWN_DISPLAY_FINSHED -- PS3 147 -- through the GUI->game-state bridge).
// ===================================================================================
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineRivalShutdown.h"

#include <cstring>                                                        // strcmp / memcpy

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SnPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // GuiEvent<N> / GuiEventWrapper
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // GuiEventControllerInputPressed
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTriggerPayload
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // E_FORMAT_ID_LOOKUP
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<18432,16>
#include "GameSource/GameState/Progression/BrnProfile.h"                  // Profile::HasPlayerSeenTrainingType
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // the three SetupComponents payloads
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // GetVehicleList
#include "SharedClasses/DataLists/VehicleList.h"                          // GetVehicleIndex / GetVehicleData
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // mRivalUnlockName / GetWonCarVoiceOverKeyHash
#include "SharedClasses/Progression/BrnTrainingTypes.h"                   // E_TRAINING_TYPE_RIVAL_SHUTDOWN

namespace BrnGui
{
namespace
{
    // The state IN-queue is the 18KB variable event queue every GUI state reads (the house
    // typedef, as in BrnCompletedGame.cpp / BrnOfflineInstantResults.cpp).
    typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

    // ---- the five observed event ids (maiEventToObserve below; jpt_824C245C answers exactly
    //      these five and sends the other 54 slots of its 6..64 range to the default assert) --
    const s32 KI_EVENT_CONTROLLER_INPUT_PRESSED = 6;    // jt case 0  -> HandleControllerInput
    const s32 KI_EVENT_LOAD_NOTIFICATION        = 14;   // jt case 8  -> LargeCarComponent
    const s32 KI_EVENT_UNLOAD_NOTIFICATION      = 16;   // jt case 10 -> LargeCarComponent
    const s32 KI_EVENT_APT_TRIGGER              = 21;   // jt case 15 -> HandleAptTriggers
    const s32 KI_EVENT_GUI_CACHE                = 64;   // jt case 58 -> latch mpGuiCache

    // ---- component names (DWARF cpp:55..64). Read out of the image's string pool, which sits
    //      right after miNumEventsObserved in DWARF declaration order; every length + NUL is
    //      the DWARF's declared array size.
    const char KAC_STATE_CPT_NAME[]        = "ScreenAnim_mc";         // char[14] @0x820668B4
    const char KAC_SCREENANIM_NAME[]       = "ScreenAnim_cpt";        // char[15] @0x820668C4
    const char KAC_SHUTDOWNCAR_COMPNAME[]  = "carLarge_cpt";          // char[13] @0x820668D4
    const char KAC_MANU_ICON_NAME[]        = "ManufacturerIcon_mc";   // char[20] @0x820668E4
    const char KAC_CONGRATTEXT_CAR_NAME[]  = "CongratTextCar_cpt";    // char[19] @0x820668F8
    const char KAC_CONGRATTEXT_DESC_NAME[] = "CongratTextDesc_cpt";   // char[20] @0x8206690C

    // ---- the two page durations (DWARF cpp:66/67), .rdata floats read at the addresses
    //      Update loads them from (both are pooled literals the build shares).
    const f32 KF_SCREEN_DISPLAY_TIME  = 3.0f;   // flt_82065654 (Update case 3)
    const f32 KF_NEWTEXT_DISPLAY_TIME = 2.0f;   // flt_82065670 (Update case 6)

    // The apt clip every animation below is driven through.
    const char KAC_APT_TRANSITION[] = "apt_Transition";

    // The movie Update plays: `lwz r4, (off_82F27C60 - off_82F278E0)(gGuiResourceIdentifier)`
    // == gGuiResourceIdentifier[224] == "BrnRivalShutdown", the same id as maResourcesToLoad[0].
    const u32 KU_RIVAL_SHUTDOWN_MOVIE_RESOURCE = 224;
    const s32 KI_MOVIE_LEVEL                   = 3;    // `li r5, 3` (and OnLeave's empty movie)

    // The resource id-type the shut-down car is shown with: SetCarInfo's `li r5, 0x7B`.
    const BrnGuiResourceId KU_SHUTDOWN_CAR_RESOURCE_ID_TYPE = 123;

    // SetupComponents' "CAR_CAPS_<id>" buffer: SnPrintf `li r4, 0x40`, then `stb 0` at [63].
    const s32 KI_CAR_CAPS_TEXT_LENGTH = 64;

    // The state-output channel every OutputGuiEvent<T> record is posted on (`li r5, 0x28`).
    const s32 KI_CHANNEL_GUI_OUT = 40;

    // OnLeave's second post: {1, 295, 12} on channel 40, size 16 (@0x824D30E8..0x824D3108). The
    // one payload byte is never written by the console (its stack slot still holds the previous
    // record's words) -- it is the sizeof of an empty event struct, not a value; zeroed here, no
    // consumer reads it. The GUI->game-state bridge turns GUI 295 into game event 149, which is
    // E_EVENT_RIVAL_SHUTDOWN_DISPLAY_FINSHED (DWARF BrnGameEvents.h, PS3 id 147). The record NAME
    // is ours (the X360 inlines the OutputGuiEvent and keeps no type name); the id is the image's.
    struct GuiEventRivalShutdownDisplayFinished : public CgsGui::GuiEvent<295>
    {
        u8 mucPad;   // +0x0C (the 1-byte empty payload)

        GuiEventRivalShutdownDisplayFinished() : CgsGui::GuiEvent<295>(1, 12), mucPad(0) {}
    };
    static_assert(sizeof(GuiEventRivalShutdownDisplayFinished) == 16,
                  "the console posts a 16-byte record for GUI 295 (`li r6, 0x10`)");

    // The console streams the fixed prefix, the offending value and a suffix into the global
    // assert message buffer and fires unconditionally; building into a stack buffer is the
    // committed BrnCompletedGame / BrnOfflineInstantResults precedent.
    void FireUnexpectedStateAssert(const char* lpacMessage, s32 liState, const char* lpacSuffix)
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << lpacMessage << liState << lpacSuffix;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }
}

// ---- static resource list (DWARF cpp:32/40) ---------------------------------------------
// GetResourcesToLoad @0x825008D0 pins both addresses; the image reads {224,4},{59,4},{29,4},
// {55,4} and a count word of 4 (tools/re/x360rd.py 82F27318 32 / 82066898 4). Update case 1
// hands the same table to GuiCache::EnsureResourcesAreLoaded (`lis/addi unk_82F27318 ; li r5,4`).
// Moved here from BrnScreenStatesDataLinkStubs.cpp now that the class has its own TU.
const CgsGui::sResourceTuple OfflineRivalShutdown::maResourcesToLoad[] =
{
    { 224u, CgsGui::E_GUI_RESOURCETYPE_APT },   // BrnRivalShutdown
    {  59u, CgsGui::E_GUI_RESOURCETYPE_APT },
    {  29u, CgsGui::E_GUI_RESOURCETYPE_APT },
    {  55u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5ManufacturersIcon
};
const u32 OfflineRivalShutdown::muNumResourcesToLoad = 4;

// ---- observed events (DWARF cpp:43/52) --------------------------------------------------
// maiEventToObserve[5] @0x8206689C, READ OUT OF THE IMAGE: {6, 21, 14, 16, 64}; the very next
// word, @0x820668B0, is 5 == miNumEventsObserved (the DWARF's `const int32_t = 5`), and OnEnter /
// OnLeave pass `li r5, 5`. Each id is a live case of HandleIncomingEvents' switch.
const s32 OfflineRivalShutdown::maiEventToObserve[] =
{
    KI_EVENT_CONTROLLER_INPUT_PRESSED,   // 6
    KI_EVENT_APT_TRIGGER,                // 21
    KI_EVENT_LOAD_NOTIFICATION,          // 14
    KI_EVENT_UNLOAD_NOTIFICATION,        // 16
    KI_EVENT_GUI_CACHE,                  // 64
};
const s32 OfflineRivalShutdown::miNumEventsObserved = 5;

// ---- Construct  @0x824B94D0 -------------------------------------------------------------
// cpp:86 -- assert the fsm pointer (the StrStream "Invalid ScriptedFsm ptr", cpp:88 `li r5,0x58`),
// run the base Construct, then zero the state word (+0x38) and the cache pointer (+0x40).
void OfflineRivalShutdown::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
{
    CGS_ASSERT(lpFsm != 0, "Invalid ScriptedFsm ptr");   // cpp:88

    CgsGui::State::Construct(liId, lpFsm);

    meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_NONE;
    mpGuiCache                  = 0;
}

// ---- OnEnter  @0x824B9588 ---------------------------------------------------------------
// cpp:104 -- register the five observed events, Construct the five embedded components (each
// through its vtable slot 0), then reset the screen clock, the state word and the cache pointer.
// The animation clip and the car are top-level; the badge and both captions hang off the
// "ScreenAnim_mc" clip (r6 = aScreenanimMc for the three of them, 0 for the other two).
void OfflineRivalShutdown::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    mScreenAnim.Construct(KAC_SCREENANIM_NAME, mpStateInterface, 0);
    mManufacturerIcon.Construct(KAC_MANU_ICON_NAME, mpStateInterface, KAC_STATE_CPT_NAME);
    mCongratTextCar.Construct(KAC_CONGRATTEXT_CAR_NAME, mpStateInterface, KAC_STATE_CPT_NAME);
    mCongratTextDesc.Construct(KAC_CONGRATTEXT_DESC_NAME, mpStateInterface, KAC_STATE_CPT_NAME);
    mShutdownCarComponent.Construct(KAC_SHUTDOWNCAR_COMPNAME, mpStateInterface, 0);

    mfScreenStartTime           = 0.0f;                               // flt_82001CC0
    meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_NONE;
    mpGuiCache                  = 0;
}

// ---- OnLeave  @0x824D3060 ---------------------------------------------------------------
// cpp:135 -- in asm order:
//   0x824D30B8  {8, 18, 12, "", 3} on channel 41, size 20: StateInterface::PlayAptMovie("", 3)
//               inlined (its body @0x82436F10 builds exactly that record; "" is unk_820046A7,
//               the NUL of "%s%s%s" reused) -- the movie Update played at level 3 is dropped.
//   0x824D30C0  the car hands its resources back.
//   0x824D30D4  unregister the five events; `stw 0, 0x38` (the state word, NONE).
//   0x824D3108  GUI 295 {1, 295, 12} on channel 40 -- the "display finished" signal.
//   0x824D311C  if the cache arrived: clear the screen flow's expected-component list, drop it.
void OfflineRivalShutdown::OnLeave()
{
    mpStateInterface->PlayAptMovie("", KI_MOVIE_LEVEL);

    mShutdownCarComponent.ReleaseResources();

    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

    meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_NONE;

    GuiEventRivalShutdownDisplayFinished lDisplayFinished;
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lDisplayFinished), KI_CHANNEL_GUI_OUT,
        static_cast<s32>(sizeof(lDisplayFinished)));

    if (mpGuiCache != 0)
    {
        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        mpGuiCache = 0;
    }
}

// ---- Update  @0x824DAFE0 ----------------------------------------------------------------
// cpp:168 -- drain the in-queue, then run one step of the presentation ladder (jpt_824DB014:
// cases 4, 7 and 9 share the plain return -- HandleAptTriggers is what moves 4 and 7 on).
void OfflineRivalShutdown::Update()
{
    HandleIncomingEvents();

    switch (meOfflineRivalShutdownState)
    {
    case E_OFFLINERIVALSHUTDOWNSTATE_NONE:
        // Nothing can load until event 64 has handed the cache over.
        if (mpGuiCache != 0)
        {
            meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_LOADINGRESOURCES;
        }
        break;

    case E_OFFLINERIVALSHUTDOWNSTATE_LOADINGRESOURCES:
        if (mpGuiCache != 0
            && mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad)
            && mShutdownCarComponent.EnsureResourcesAreLoaded())
        {
            mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_RIVAL_SHUTDOWN_MOVIE_RESOURCE],
                                           KI_MOVIE_LEVEL);
            mfScreenStartTime = mpGuiCache->GetTime();
            mShutdownCarComponent.OnLoad();
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            AppendExpectedComponents();
            meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_WAITINGFORCOMPONENTS;
        }
        break;

    case E_OFFLINERIVALSHUTDOWNSTATE_WAITINGFORCOMPONENTS:
        CGS_ASSERT(mpGuiCache, "mpGuiCache");   // cpp:212
        if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            SetupComponents();
            meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_RUNNING;
        }
        break;

    case E_OFFLINERIVALSHUTDOWNSTATE_RUNNING:
        if (mpGuiCache->GetTime() > mfScreenStartTime + KF_SCREEN_DISPLAY_TIME)
        {
            mScreenAnim.AddOutputAptViewState(KAC_APT_TRANSITION, "fadeOutText", false);
            meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_FADEOUT_TEXT;
        }
        break;

    case E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_FADEOUT_TEXT:
    case E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_TRANSOUT:
    case E_OFFLINERIVALSHUTDOWNSTATE_FINISH:
        break;

    case E_OFFLINERIVALSHUTDOWNSTATE_SET_NEW_TEXT:
        mCongratTextDesc.SetLocalisedText("POSTRACE_NEW_CAR_INSTRUCTIONS",
                                          CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
        mScreenAnim.AddOutputAptViewState(KAC_APT_TRANSITION, "fadeInText", false);
        meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_SHOWING_NEW_TEXT;
        mfNewTextStartTime = mpGuiCache->GetTime();
        break;

    case E_OFFLINERIVALSHUTDOWNSTATE_SHOWING_NEW_TEXT:
        if (mpGuiCache->GetTime() > mfNewTextStartTime + KF_NEWTEXT_DISPLAY_TIME)
        {
            mScreenAnim.AddOutputAptViewState(KAC_APT_TRANSITION, "transout", false);
            meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_TRANSOUT;
        }
        break;

    case E_OFFLINERIVALSHUTDOWNSTATE_TRANSOUT_COMPLETE:
        SendStateEvent("ADVANCE");
        meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_FINISH;
        break;

    default:
        FireUnexpectedStateAssert("Unhandled OfflineRivalShutdownState ",
                                  static_cast<s32>(meOfflineRivalShutdownState),
                                  " in OfflineRivalShutdown::Update()\n");   // cpp:287
        break;
    }
}

// ---- HandleIncomingEvents  @0x824C23D0 --------------------------------------------------
// cpp:301 -- drain the state's in-queue through the five observed ids (jpt_824C245C, indexed
// by id - 6). The cache arm latches ONCE (`if (!mpGuiCache)`, no null assert on the payload)
// and immediately hands the car component the cache and the shut-down car's id.
void OfflineRivalShutdown::HandleIncomingEvents()
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
            // Through the vtable (`lwz r11, 0x24(vtbl) ; bctrl`), with the event id as r5.
            HandleControllerInput(lpEvent, liEventType);
            break;

        case KI_EVENT_GUI_CACHE:
            if (mpGuiCache == 0)
            {
                mpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
                mShutdownCarComponent.SetCachePointer(mpGuiCache);
                mShutdownCarComponent.SetCarInfo(mpGuiCache->GetShutdownCarID(),
                                                 KU_SHUTDOWN_CAR_RESOURCE_ID_TYPE);
            }
            break;

        case KI_EVENT_APT_TRIGGER:
            HandleAptTriggers(reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent));
            break;

        case KI_EVENT_LOAD_NOTIFICATION:
            mShutdownCarComponent.HandleLoadNotification(
                reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent));
            break;

        case KI_EVENT_UNLOAD_NOTIFICATION:
            mShutdownCarComponent.HandleUnloadNotification(
                reinterpret_cast<const CgsGui::GuiEventUnloadNotification*>(lpEvent));
            break;

        default:
            FireUnexpectedStateAssert("Unhandled event ", liEventType,
                                      " in OfflineRivalShutdown::Update()\n");   // cpp:356
            break;
        }
    }
}

// ---- AppendExpectedComponents  @0x824B96A0 ----------------------------------------------
// cpp:372 -- register the five components whose apt counterparts must report in before the
// screen counts as initialised, each by its OWN name buffer (component + 4 == its macName, the
// composed "<parent>_<name>"), on the screen flow (r4 = 0), in the console's order.
void OfflineRivalShutdown::AppendExpectedComponents()
{
    CGS_ASSERT(mpGuiCache, "mpGuiCache");   // cpp:374

    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mScreenAnim.GetName());
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mManufacturerIcon.GetName());
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mCongratTextCar.GetName());
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mCongratTextDesc.GetName());
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mShutdownCarComponent.GetName());
}

// ---- SetupComponents  @0x824D3130 -------------------------------------------------------
// cpp:392 -- fill the page once every component is live:
//   * the car's localised name "CAR_CAPS_<id>" and its manufacturer badge,
//   * the first caption "RVLSHUTDOWN_NEW_CAR_DESC1", the car itself, the clip's "transin",
//   * then three channel-40 posts decided by the vehicle-list entry and the profile:
//       GUI 468 GuiEventAudioGenericSequence  {4, 468, 12, entry.mRivalUnlockName (+0xC8)}
//       and EITHER (the player has already seen training tip 32, RIVAL_SHUTDOWN)
//       GUI 302 GuiEventPostEventFreeCarSequenceStart {8, 302, 16, won-car voice-over hash}
//       OR     GUI 572 GuiEventRequestTraining {4, 572, 12, 32} -- ask for that tip.
// The console reads the tip bit inline (`ldx profile+0x1CCC0 & (1<<32)`, i.e. word 0 bit 32
// of Profile::maHasPlayerSeenTraining); HasPlayerSeenTrainingType is that test out of line
// (its two range asserts hold for 32), the committed InstantResultsState::UpdateCarUnlock
// precedent for the identical three-post block.
void OfflineRivalShutdown::SetupComponents()
{
    CGS_ASSERT(mpGuiCache, "mpGuiCache");   // cpp:394

    // Inlined on the console with its own "kCGSID_NULL != mShutdownCarID" assert
    // (BrnGuiCache.h:3337, `li r5, 0xD09`) -- the out-of-line accessor carries the same one.
    const CgsID lShutdownCarID = mpGuiCache->GetShutdownCarID();

    char lacCarId[KI_CGSID_STRING_LEN];
    CgsIDConvertToString(lShutdownCarID, lacCarId);

    char lacCarCapsText[KI_CAR_CAPS_TEXT_LENGTH];
    CgsCore::SnPrintf(lacCarCapsText, KI_CAR_CAPS_TEXT_LENGTH, "CAR_CAPS_%s", lacCarId);
    lacCarCapsText[KI_CAR_CAPS_TEXT_LENGTH - 1] = '\0';

    mManufacturerIcon.Set(mpGuiCache->GetWorldDataController()->GetVehicleList(), lShutdownCarID);
    mCongratTextCar.SetLocalisedText(lacCarCapsText, CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
    mCongratTextDesc.SetLocalisedText("RVLSHUTDOWN_NEW_CAR_DESC1",
                                      CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
    mShutdownCarComponent.ShowCar();
    mScreenAnim.AddOutputAptViewState(KAC_APT_TRANSITION, "transin", false);

    CGS_ASSERT(mpGuiCache, "mpGuiCache");   // cpp:435
    const BrnResource::VehicleList* lpVehicleList =
        mpGuiCache->GetWorldDataController()->GetVehicleList();
    CGS_ASSERT(lpVehicleList, "lpVehicleList");   // cpp:437

    const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lShutdownCarID);
    const BrnResource::VehicleListEntry* lpVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);
    CGS_ASSERT(lpVehicleListEntry, "lpVehicleListEntry");   // cpp:439

    GuiEventAudioGenericSequence lAudio;
    std::memcpy(lAudio.maData, &lpVehicleListEntry->mRivalUnlockName, sizeof(lAudio.maData));
    CgsGui::GuiEventWrapper<GuiEventAudioGenericSequence, KI_CHANNEL_GUI_OUT> lAudioRecord(lAudio);
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lAudioRecord), KI_CHANNEL_GUI_OUT,
        static_cast<s32>(sizeof(lAudioRecord)));

    if (mpGuiCache->GetProfile()->HasPlayerSeenTrainingType(
            BrnProgression::E_TRAINING_TYPE_RIVAL_SHUTDOWN))
    {
        GuiEventPostEventFreeCarSequenceStart lSequence;
        const u64 luVoiceOver = lpVehicleListEntry->GetWonCarVoiceOverKeyHash();
        std::memcpy(lSequence.maData, &luVoiceOver, sizeof(lSequence.maData));
        CgsGui::GuiEventWrapper<GuiEventPostEventFreeCarSequenceStart, KI_CHANNEL_GUI_OUT>
            lSequenceRecord(lSequence);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lSequenceRecord), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lSequenceRecord)));
    }
    else
    {
        GuiEventRequestTraining lTraining;
        lTraining.meTrainingType = BrnProgression::E_TRAINING_TYPE_RIVAL_SHUTDOWN;
        CgsGui::GuiEventWrapper<GuiEventRequestTraining, KI_CHANNEL_GUI_OUT> lTrainingRecord(lTraining);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lTrainingRecord), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lTrainingRecord)));
    }
}

// ---- HandleAptTriggers  @0x824B9748 -----------------------------------------------------
// cpp:475 -- the apt callbacks. An ONLOAD for the car component's own name is forwarded to it;
// a TRANSITION_COMPLETE from the "ScreenAnim_mc" clip is what moves the two waiting states on
// (fadeOutText finished -> SET_NEW_TEXT, transout finished -> TRANSOUT_COMPLETE). Any other
// state at that moment is the console's streamed assert (cpp:506). The name tests are the
// inlined strcmp loops (component name vs payload name for ONLOAD, payload name vs the literal
// for TRANSITION_COMPLETE -- operand order kept).
void OfflineRivalShutdown::HandleAptTriggers(const CgsGui::GuiEventAptTriggerPayload* lpEvent)
{
    CGS_ASSERT(lpEvent, "lpEvent");   // cpp:477

    if (lpEvent->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
    {
        if (std::strcmp(mShutdownCarComponent.GetName(), lpEvent->mpacComponentName) == 0)
        {
            mShutdownCarComponent.HandleAptLoadTriggers(lpEvent);
        }
    }
    else if (lpEvent->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE)
    {
        if (std::strcmp(lpEvent->mpacComponentName, KAC_STATE_CPT_NAME) == 0)
        {
            if (meOfflineRivalShutdownState == E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_FADEOUT_TEXT)
            {
                meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_SET_NEW_TEXT;
            }
            else if (meOfflineRivalShutdownState == E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_TRANSOUT)
            {
                meOfflineRivalShutdownState = E_OFFLINERIVALSHUTDOWNSTATE_TRANSOUT_COMPLETE;
            }
            else
            {
                FireUnexpectedStateAssert("Unhandled offline rival shutdown state ",
                                          static_cast<s32>(meOfflineRivalShutdownState),
                                          " in OfflineRivalShutdown::HandleAptTriggers().\n");   // cpp:506
            }
        }
    }
}

// ---- HandleControllerInput  @0x824BD100 -------------------------------------------------
// cpp:525 -- only the "pressed" record (id 6) is answered.
void OfflineRivalShutdown::HandleControllerInput(const CgsModule::Event* lpEvent, s32 liEventType)
{
    CGS_ASSERT(lpEvent, "lpEvent");   // cpp:527

    if (liEventType == KI_EVENT_CONTROLLER_INPUT_PRESSED)
    {
        HandleControllerInputPressed(
            reinterpret_cast<const CgsGui::GuiEventControllerInputPressed*>(lpEvent));
    }
}

// ---- HandleControllerInputPressed  @0x824B98B8 ------------------------------------------
// cpp:556 -- the screen is not skippable: jpt_824B9914's ten entries ALL point at the return
// (0x824B99D8, read out of the image), so every legal state ignores the pad; only an
// out-of-range state word reaches the streamed assert (cpp:584).
void OfflineRivalShutdown::HandleControllerInputPressed(
    const CgsGui::GuiEventControllerInputPressed* lpControllerPressedEvent)
{
    CGS_ASSERT(lpControllerPressedEvent, "lpControllerPressedEvent");   // cpp:558

    switch (meOfflineRivalShutdownState)
    {
    case E_OFFLINERIVALSHUTDOWNSTATE_NONE:
    case E_OFFLINERIVALSHUTDOWNSTATE_LOADINGRESOURCES:
    case E_OFFLINERIVALSHUTDOWNSTATE_WAITINGFORCOMPONENTS:
    case E_OFFLINERIVALSHUTDOWNSTATE_RUNNING:
    case E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_FADEOUT_TEXT:
    case E_OFFLINERIVALSHUTDOWNSTATE_SET_NEW_TEXT:
    case E_OFFLINERIVALSHUTDOWNSTATE_SHOWING_NEW_TEXT:
    case E_OFFLINERIVALSHUTDOWNSTATE_WAITING_FOR_TRANSOUT:
    case E_OFFLINERIVALSHUTDOWNSTATE_TRANSOUT_COMPLETE:
    case E_OFFLINERIVALSHUTDOWNSTATE_FINISH:
        break;

    default:
        FireUnexpectedStateAssert("Unhandled OfflineRivalShutdown state ",
                                  static_cast<s32>(meOfflineRivalShutdownState),
                                  " in OfflineRivalShutdown::HandleControllerInputPressed()\n");   // cpp:584
        break;
    }
}
}
