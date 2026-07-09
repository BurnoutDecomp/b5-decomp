// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/CgsGuiModule_AddGuiEvent_Inst.cpp
//
// The single shared body of CgsGui::GuiModule::AddGuiEvent<T> plus the 270
// X360-attested explicit instantiations (one per queued GUI event type). The X360
// build emits each instantiation out-of-line (0x823CEB70..0x823DAE68); every one
// compiles to the identical three-step body below, differing only in the two
// per-T compile-time literals AddEvent(&event, id, size) -- id == T::GetEventType()
// (the GuiEvent<N> id) and size == sizeof(T). Verified store-for-store against the
// asm (e.g. AddGuiEvent<BrnGui::GuiAftertouchEvent> @0x823D4490 -> AddEvent(q, ev,
// 403, 32)). See CgsGuiModule.h for the declaration.
//
// sub_8284F238(buffer) is CgsGui::CgsGuiModuleIO::InputBuffer::GetGuiEvents() -- the
// write handle that lock-checks the module input buffer and returns its inbound
// GuiEventInputQueue (GuiEventQueueBase<32768,16>, whose base is the queue the asm
// names, VariableEventQueue<32768,16>). Confirmed by GameBridgeNetworkToX.cpp /
// GameBridgeReplayToX.h, which both equate sub_8284F238 with this +4 queue accessor.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiModule.h"        // CgsGui::GuiModule (declaration)
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"      // CgsGuiModuleIO::InputBuffer::GetGuiEvents()
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"         // CgsGui::GuiEvent<N>, GuiEventQueueBase
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT

// ---- GUI-event payload TYPE homes (each T must be a complete type with GetEventType) ----
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"       // 212 BrnGui payload homes (this task)
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h" // CgsGui controller/lang/time + GuiEventNetworkLaunching
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"             // JunctionInfo/SatNav/OverlayRequest/LiveRevenge/Skillz/HudMessage/RoadRuleEnter/RoadRuleUpcomingRoads
#include "GameSource/Gui/Events/BrnGuiChallengeEvents.h"    // GuiChallengeStartEvent/TriggerResponse/UpdateEvent
#include "GameSource/Gui/Events/BrnGuiEventRankProgressResponse.h" // GuiEventRankProgressResponse
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"    // GuiEventNetworkGameParams
#include "GameSource/Gui/Events/BrnGuiEventOnlinePostEvent.h"      // GuiEventOnlinePostEvent
#include "GameSource/Gui/BrnGuiRaceCarInfoEvent.h"          // GuiRaceCarInfoEvent

// ---- the one shared template body -----------------------------------------------
template <class T>
int CgsGui::GuiModule::AddGuiEvent(const T* lpEvent, CgsGui::CgsGuiModuleIO::InputBuffer* lpBuffer)
{
    // X360: the buffer (r5, null-checked) must have been write-locked by the producer.
    CGS_ASSERT(lpBuffer != 0, "Input hasn't been locked for write");
    // queue = sub_8284F238(lpBuffer); return queue->AddEvent(&event, event->GetEventType(), sizeof(T));
    return lpBuffer->GetGuiEvents()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(lpEvent),
        lpEvent->GetEventType(),
        static_cast<s32>(sizeof(T)));
}

// ---- the 270 explicit instantiations (X360 addresses in trailing comments) -------
namespace CgsGui
{
    template int GuiModule::AddGuiEvent<BrnGui::GuiAftertouchEvent>(const BrnGui::GuiAftertouchEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4490
    template int GuiModule::AddGuiEvent<BrnGui::GuiAttackScoreUpdate>(const BrnGui::GuiAttackScoreUpdate*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1188
    template int GuiModule::AddGuiEvent<BrnGui::GuiAutosaveRequestEvent>(const BrnGui::GuiAutosaveRequestEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D03E0
    template int GuiModule::AddGuiEvent<BrnGui::GuiBHRCheckpointReachedEvent>(const BrnGui::GuiBHRCheckpointReachedEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5968
    template int GuiModule::AddGuiEvent<BrnGui::GuiBlueTeamIsBehindYouEvent>(const BrnGui::GuiBlueTeamIsBehindYouEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4C78
    template int GuiModule::AddGuiEvent<BrnGui::GuiBlueTeamIsEscapingEvent>(const BrnGui::GuiBlueTeamIsEscapingEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4BC0
    template int GuiModule::AddGuiEvent<BrnGui::GuiCarSelectAbortEvent>(const BrnGui::GuiCarSelectAbortEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D88D8
    template int GuiModule::AddGuiEvent<BrnGui::GuiCarSelectOnlineTimeLeftEvent>(const BrnGui::GuiCarSelectOnlineTimeLeftEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1800
    template int GuiModule::AddGuiEvent<BrnGui::GuiCarSelectReadyToExitEvent>(const BrnGui::GuiCarSelectReadyToExitEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D86B0
    template int GuiModule::AddGuiEvent<BrnGui::GuiCarSelectStartEvent>(const BrnGui::GuiCarSelectStartEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1690
    template int GuiModule::AddGuiEvent<BrnGui::GuiCarSelectionChangedDropIn>(const BrnGui::GuiCarSelectionChangedDropIn*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8768
    template int GuiModule::AddGuiEvent<BrnGui::GuiCarSelectionChangedOnline>(const BrnGui::GuiCarSelectionChangedOnline*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8820
    template int GuiModule::AddGuiEvent<BrnGui::GuiCarSelectionEvent>(const BrnGui::GuiCarSelectionEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4998
    template int GuiModule::AddGuiEvent<BrnGui::GuiCarUnlockEvent>(const BrnGui::GuiCarUnlockEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7A78
    template int GuiModule::AddGuiEvent<BrnGui::GuiCarUnlockNewCarEvent>(const BrnGui::GuiCarUnlockNewCarEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7850
    template int GuiModule::AddGuiEvent<BrnGui::GuiCarUnlockStartEvent>(const BrnGui::GuiCarUnlockStartEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D15D8
    template int GuiModule::AddGuiEvent<BrnGui::GuiCarUnlockedLiveryEvent>(const BrnGui::GuiCarUnlockedLiveryEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D48E0
    template int GuiModule::AddGuiEvent<BrnGui::GuiChallengeEndEvent>(const BrnGui::GuiChallengeEndEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5F28
    template int GuiModule::AddGuiEvent<BrnGui::GuiChallengeNotActiveStartEvent>(const BrnGui::GuiChallengeNotActiveStartEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D09A0
    template int GuiModule::AddGuiEvent<BrnGui::GuiChallengeStartEvent>(const BrnGui::GuiChallengeStartEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5DB8
    template int GuiModule::AddGuiEvent<BrnGui::GuiChallengeTriggerResponse>(const BrnGui::GuiChallengeTriggerResponse*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5E70
    template int GuiModule::AddGuiEvent<BrnGui::GuiChallengeUpdateEvent>(const BrnGui::GuiChallengeUpdateEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6208
    template int GuiModule::AddGuiEvent<BrnGui::GuiChangeCarEvent>(const BrnGui::GuiChangeCarEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D79C0
    template int GuiModule::AddGuiEvent<BrnGui::GuiCompletedStuntEvent>(const BrnGui::GuiCompletedStuntEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2888
    template int GuiModule::AddGuiEvent<BrnGui::GuiCrashComboEvent>(const BrnGui::GuiCrashComboEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7798
    template int GuiModule::AddGuiEvent<BrnGui::GuiCrashScoreUpdate>(const BrnGui::GuiCrashScoreUpdate*, CgsGuiModuleIO::InputBuffer*);  // 0x823D10D0
    template int GuiModule::AddGuiEvent<BrnGui::GuiDeveloperChallengesCompleted>(const BrnGui::GuiDeveloperChallengesCompleted*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9230
    template int GuiModule::AddGuiEvent<BrnGui::GuiDirtyTrickEndedEvent>(const BrnGui::GuiDirtyTrickEndedEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9CF8
    template int GuiModule::AddGuiEvent<BrnGui::GuiDirtyTrickNewEvent>(const BrnGui::GuiDirtyTrickNewEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9B88
    template int GuiModule::AddGuiEvent<BrnGui::GuiDirtyTrickTriggerEvent>(const BrnGui::GuiDirtyTrickTriggerEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9C40
    template int GuiModule::AddGuiEvent<BrnGui::GuiDriftingEvent>(const BrnGui::GuiDriftingEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D40F8
    template int GuiModule::AddGuiEvent<BrnGui::GuiDriveThroughEvent>(const BrnGui::GuiDriveThroughEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3070
    template int GuiModule::AddGuiEvent<BrnGui::GuiEnteredJunkyard>(const BrnGui::GuiEnteredJunkyard*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1520
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventAllJunctionsDiscoveredOfType>(const BrnGui::GuiEventAllJunctionsDiscoveredOfType*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3408
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventAllOfRivalsShutdown>(const BrnGui::GuiEventAllOfRivalsShutdown*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3298
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventAllOfTypeComplete>(const BrnGui::GuiEventAllOfTypeComplete*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3350
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventBoostBarStuntInfo>(const BrnGui::GuiEventBoostBarStuntInfo*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3A80
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventBoostInfo>(const BrnGui::GuiEventBoostInfo*, CgsGuiModuleIO::InputBuffer*);  // 0x823DA510
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventBuddyNotification>(const BrnGui::GuiEventBuddyNotification*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF638
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventCamPicCompressed>(const BrnGui::GuiEventCamPicCompressed*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0778
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventCanSkipCrash>(const BrnGui::GuiEventCanSkipCrash*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3630
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventCantPaintCar>(const BrnGui::GuiEventCantPaintCar*, CgsGuiModuleIO::InputBuffer*);  // 0x823D37A0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventCarJoinedEvent>(const BrnGui::GuiEventCarJoinedEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6710
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventChallengedEventDataResponse>(const BrnGui::GuiEventChallengedEventDataResponse*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7120
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventChangeDistrict>(const BrnGui::GuiEventChangeDistrict*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2CD8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventCurrentStatus>(const BrnGui::GuiEventCurrentStatus*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0DF0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventDirectorSettings>(const BrnGui::GuiEventDirectorSettings*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF2A0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventDriveThruDiscovered>(const BrnGui::GuiEventDriveThruDiscovered*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3128
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventEnterEventStartLocation>(const BrnGui::GuiEventEnterEventStartLocation*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1E78
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventEnterLandmarkArea>(const BrnGui::GuiEventEnterLandmarkArea*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3ED0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventEventStateResponse>(const BrnGui::GuiEventEventStateResponse*, CgsGuiModuleIO::InputBuffer*);  // 0x823D85F8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventFailedToStartEvent>(const BrnGui::GuiEventFailedToStartEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6430
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventFburnChallengeEveryPlayerStatus>(const BrnGui::GuiEventFburnChallengeEveryPlayerStatus*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8B00
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventFinishedModeResults>(const BrnGui::GuiEventFinishedModeResults*, CgsGuiModuleIO::InputBuffer*);  // 0x823D24F0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventGameCompleted>(const BrnGui::GuiEventGameCompleted*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3E18
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventGameCompletedOnline>(const BrnGui::GuiEventGameCompletedOnline*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3D60
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventHideDriveThru>(const BrnGui::GuiEventHideDriveThru*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2D90
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventInviteComplete>(const BrnGui::GuiEventInviteComplete*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5460
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventInviteFailed>(const BrnGui::GuiEventInviteFailed*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF6F0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventJumpStarted>(const BrnGui::GuiEventJumpStarted*, CgsGuiModuleIO::InputBuffer*);  // 0x823D39C8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventJunctionInfo>(const BrnGui::GuiEventJunctionInfo*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1AE0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventLiveRevengeProfileData>(const BrnGui::GuiEventLiveRevengeProfileData*, CgsGuiModuleIO::InputBuffer*);  // 0x823D01B8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventLoadImageFiles>(const BrnGui::GuiEventLoadImageFiles*, CgsGuiModuleIO::InputBuffer*);  // 0x823D81A8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventMedalUpdate>(const BrnGui::GuiEventMedalUpdate*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1A28
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventMiniMapSwitch>(const BrnGui::GuiEventMiniMapSwitch*, CgsGuiModuleIO::InputBuffer*);  // 0x823D74B8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventMustFixCarFirst>(const BrnGui::GuiEventMustFixCarFirst*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3858
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkGameParams>(const BrnGui::GuiEventNetworkGameParams*, CgsGuiModuleIO::InputBuffer*);  // 0x823CFA88
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkLeftGame>(const BrnGui::GuiEventNetworkLeftGame*, CgsGuiModuleIO::InputBuffer*);  // 0x823CFB40
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkLobbyPlayerList>(const BrnGui::GuiEventNetworkLobbyPlayerList*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF4C8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkPlayerImage>(const BrnGui::GuiEventNetworkPlayerImage*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7B30
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkPlayerJoinedLobby>(const BrnGui::GuiEventNetworkPlayerJoinedLobby*, CgsGuiModuleIO::InputBuffer*);  // 0x823D76E0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkPlayerLeftLobby>(const BrnGui::GuiEventNetworkPlayerLeftLobby*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0830
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkPlayerList>(const BrnGui::GuiEventNetworkPlayerList*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF358
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkPlayerStatus>(const BrnGui::GuiEventNetworkPlayerStatus*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF410
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkPostGameProcessingFinished>(const BrnGui::GuiEventNetworkPostGameProcessingFinished*, CgsGuiModuleIO::InputBuffer*);  // 0x823CFBF8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkShowFreeBurnIntro>(const BrnGui::GuiEventNetworkShowFreeBurnIntro*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2AB0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkSplashEvent>(const BrnGui::GuiEventNetworkSplashEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2C20
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventOfflinePostEvent>(const BrnGui::GuiEventOfflinePostEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D20A0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventOnlineAccountSettings>(const BrnGui::GuiEventOnlineAccountSettings*, CgsGuiModuleIO::InputBuffer*);  // 0x823D06C0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventOnlineNumFriendsCount>(const BrnGui::GuiEventOnlineNumFriendsCount*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF918
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventOnlinePostEvent>(const BrnGui::GuiEventOnlinePostEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1240
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventOnlinePostEventScalps>(const BrnGui::GuiEventOnlinePostEventScalps*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF7A8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventOnlineReceiveFriendInfo>(const BrnGui::GuiEventOnlineReceiveFriendInfo*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF9D0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventOnlineTimeout>(const BrnGui::GuiEventOnlineTimeout*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0328
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventPlayerReachedRoadRageTarget>(const BrnGui::GuiEventPlayerReachedRoadRageTarget*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1DC0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventPlayerWrecked>(const BrnGui::GuiEventPlayerWrecked*, CgsGuiModuleIO::InputBuffer*);  // 0x823DA680
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventPreRaceMessages>(const BrnGui::GuiEventPreRaceMessages*, CgsGuiModuleIO::InputBuffer*);  // 0x823D29F8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventPrepareForInvite>(const BrnGui::GuiEventPrepareForInvite*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5238
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventPrepareForModeStart>(const BrnGui::GuiEventPrepareForModeStart*, CgsGuiModuleIO::InputBuffer*);  // 0x823D27D0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventPreraceTrigger>(const BrnGui::GuiEventPreraceTrigger*, CgsGuiModuleIO::InputBuffer*);  // 0x823CEC28
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventProgressionProfileData>(const BrnGui::GuiEventProgressionProfileData*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4548
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRaceDistanceRemaining>(const BrnGui::GuiEventRaceDistanceRemaining*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0C80
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRaceDistanceToCheckpoint>(const BrnGui::GuiEventRaceDistanceToCheckpoint*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0D38
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRacePositionInfo>(const BrnGui::GuiEventRacePositionInfo*, CgsGuiModuleIO::InputBuffer*);  // 0x823D92E8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRankProgressResponse>(const BrnGui::GuiEventRankProgressResponse*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7290
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRequestCollisionWorldEvent>(const BrnGui::GuiEventRequestCollisionWorldEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823DA230
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventReturnDistrict>(const BrnGui::GuiEventReturnDistrict*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5518
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRivalInfoResponse>(const BrnGui::GuiEventRivalInfoResponse*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7400
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRivalryFullInfoResponse>(const BrnGui::GuiEventRivalryFullInfoResponse*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7348
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRagePlayerDamage>(const BrnGui::GuiEventRoadRagePlayerDamage*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2210
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRageTimeExtended>(const BrnGui::GuiEventRoadRageTimeExtended*, CgsGuiModuleIO::InputBuffer*);  // 0x823D64E8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleBatchDataResponse>(const BrnGui::GuiEventRoadRuleBatchDataResponse*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8BB8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleBegin>(const BrnGui::GuiEventRoadRuleBegin*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6C18
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleChangeMode>(const BrnGui::GuiEventRoadRuleChangeMode*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7068
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleData>(const BrnGui::GuiEventRoadRuleData*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6AA8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleEnd>(const BrnGui::GuiEventRoadRuleEnd*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6CD0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleEnter>(const BrnGui::GuiEventRoadRuleEnter*, CgsGuiModuleIO::InputBuffer*);  // 0x823D69F0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleLeave>(const BrnGui::GuiEventRoadRuleLeave*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6D88
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleNewHighScore>(const BrnGui::GuiEventRoadRuleNewHighScore*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6FB0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleNewRulers>(const BrnGui::GuiEventRoadRuleNewRulers*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7EC8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleTickerScoreResponse>(const BrnGui::GuiEventRoadRuleTickerScoreResponse*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7E10
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleUpcomingRoads>(const BrnGui::GuiEventRoadRuleUpcomingRoads*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6E40
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleUpdate>(const BrnGui::GuiEventRoadRuleUpdate*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6EF8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleUpdateTargetScores>(const BrnGui::GuiEventRoadRuleUpdateTargetScores*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6B60
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventRunFsm>(const BrnGui::GuiEventRunFsm*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2438
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventSaveImageFileAndAutosave>(const BrnGui::GuiEventSaveImageFileAndAutosave*, CgsGuiModuleIO::InputBuffer*);  // 0x823D80F0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventScoreUpdate>(const BrnGui::GuiEventScoreUpdate*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0EA8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventScoreboardDownloadedChallengeable>(const BrnGui::GuiEventScoreboardDownloadedChallengeable*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8DE0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventScoreboardResponseCategoryEvent>(const BrnGui::GuiEventScoreboardResponseCategoryEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823CFED8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventScoreboardResponseEvScoreTarget>(const BrnGui::GuiEventScoreboardResponseEvScoreTarget*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8D28
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventScoreboardResponseIndexEvent>(const BrnGui::GuiEventScoreboardResponseIndexEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823CFF90
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventScoreboardResponseTableEvent>(const BrnGui::GuiEventScoreboardResponseTableEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0100
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventScoreboardResponseVariationEvent>(const BrnGui::GuiEventScoreboardResponseVariationEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0048
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventSetAvailablePresetRaces>(const BrnGui::GuiEventSetAvailablePresetRaces*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2E48
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventSetBlackBars>(const BrnGui::GuiEventSetBlackBars*, CgsGuiModuleIO::InputBuffer*);  // 0x823CEE50
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventSetRoadRuleScoreMode>(const BrnGui::GuiEventSetRoadRuleScoreMode*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7F80
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventShowFreeburnChallenge>(const BrnGui::GuiEventShowFreeburnChallenge*, CgsGuiModuleIO::InputBuffer*);  // 0x823D62C0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventShowHideHud>(const BrnGui::GuiEventShowHideHud*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1C50
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventSpecificPresetRaces>(const BrnGui::GuiEventSpecificPresetRaces*, CgsGuiModuleIO::InputBuffer*);  // 0x823D13B0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventStatsResponse>(const BrnGui::GuiEventStatsResponse*, CgsGuiModuleIO::InputBuffer*);  // 0x823D71D8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventStopMode>(const BrnGui::GuiEventStopMode*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2380
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventStuntAllComplete>(const BrnGui::GuiEventStuntAllComplete*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3CA8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventStuntAreaComplete>(const BrnGui::GuiEventStuntAreaComplete*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3BF0
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventStuntInfo>(const BrnGui::GuiEventStuntInfo*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3B38
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventSuperJumpFailed>(const BrnGui::GuiEventSuperJumpFailed*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3910
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventTickerClearMessages>(const BrnGui::GuiEventTickerClearMessages*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2718
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventTickerCustomMessage>(const BrnGui::GuiEventTickerCustomMessage*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1D08
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventTimeUp>(const BrnGui::GuiEventTimeUp*, CgsGuiModuleIO::InputBuffer*);  // 0x823D36E8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventToggleChangeCarMessage>(const BrnGui::GuiEventToggleChangeCarMessage*, CgsGuiModuleIO::InputBuffer*);  // 0x823DAA18
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventTogglePictureParadise>(const BrnGui::GuiEventTogglePictureParadise*, CgsGuiModuleIO::InputBuffer*);  // 0x823CEB70
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventTriggerOnlinePostEvent>(const BrnGui::GuiEventTriggerOnlinePostEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1F30
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventTrophyCarUnlock>(const BrnGui::GuiEventTrophyCarUnlock*, CgsGuiModuleIO::InputBuffer*);  // 0x823D22C8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventUpdateEventCountdown>(const BrnGui::GuiEventUpdateEventCountdown*, CgsGuiModuleIO::InputBuffer*);  // 0x823D25A8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventUpdateEventStarts>(const BrnGui::GuiEventUpdateEventStarts*, CgsGuiModuleIO::InputBuffer*);  // 0x823D12F8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventUpdateHud>(const BrnGui::GuiEventUpdateHud*, CgsGuiModuleIO::InputBuffer*);  // 0x823DA5C8
    template int GuiModule::AddGuiEvent<BrnGui::GuiEventUpdateSatNav>(const BrnGui::GuiEventUpdateSatNav*, CgsGuiModuleIO::InputBuffer*);  // 0x823D93A0
    template int GuiModule::AddGuiEvent<BrnGui::GuiFinishRaceEvent>(const BrnGui::GuiFinishRaceEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9E68
    template int GuiModule::AddGuiEvent<BrnGui::GuiGameModeStarted>(const BrnGui::GuiGameModeStarted*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4A50
    template int GuiModule::AddGuiEvent<BrnGui::GuiGamePausedEvent>(const BrnGui::GuiGamePausedEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1B98
    template int GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageBHRRunnerCrashed>(const BrnGui::GuiHUDMessageBHRRunnerCrashed*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5A20
    template int GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageComboPerformed>(const BrnGui::GuiHUDMessageComboPerformed*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5B90
    template int GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageCrushCombo>(const BrnGui::GuiHUDMessageCrushCombo*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9738
    template int GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageShowtimeMultiplier>(const BrnGui::GuiHUDMessageShowtimeMultiplier*, CgsGuiModuleIO::InputBuffer*);  // 0x823D97F0
    template int GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageSignSmashed>(const BrnGui::GuiHUDMessageSignSmashed*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9960
    template int GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageStuntPerformed>(const BrnGui::GuiHUDMessageStuntPerformed*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5AD8
    template int GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageStuntTimeUp>(const BrnGui::GuiHUDMessageStuntTimeUp*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5C48
    template int GuiModule::AddGuiEvent<BrnGui::GuiHitVehicleEvent>(const BrnGui::GuiHitVehicleEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9680
    template int GuiModule::AddGuiEvent<BrnGui::GuiImageGalleryCollectedCountEvent>(const BrnGui::GuiImageGalleryCollectedCountEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7CA0
    template int GuiModule::AddGuiEvent<BrnGui::GuiImageGalleryCollectedDataEvent>(const BrnGui::GuiImageGalleryCollectedDataEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7D58
    template int GuiModule::AddGuiEvent<BrnGui::GuiImageGalleryImageInfoEvent>(const BrnGui::GuiImageGalleryImageInfoEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7BE8
    template int GuiModule::AddGuiEvent<BrnGui::GuiImpactEvent>(const BrnGui::GuiImpactEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823DA7F0
    template int GuiModule::AddGuiEvent<BrnGui::GuiInAirEvent>(const BrnGui::GuiInAirEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4268
    template int GuiModule::AddGuiEvent<BrnGui::GuiInEventFinisher>(const BrnGui::GuiInEventFinisher*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5740
    template int GuiModule::AddGuiEvent<BrnGui::GuiInEventLeaderSplit>(const BrnGui::GuiInEventLeaderSplit*, CgsGuiModuleIO::InputBuffer*);  // 0x823D55D0
    template int GuiModule::AddGuiEvent<BrnGui::GuiInEventNeckAndNeck>(const BrnGui::GuiInEventNeckAndNeck*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5688
    template int GuiModule::AddGuiEvent<BrnGui::GuiInEventRivalProgress>(const BrnGui::GuiInEventRivalProgress*, CgsGuiModuleIO::InputBuffer*);  // 0x823D57F8
    template int GuiModule::AddGuiEvent<BrnGui::GuiInProgressStuntEvent>(const BrnGui::GuiInProgressStuntEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2940
    template int GuiModule::AddGuiEvent<BrnGui::GuiLastBlueTeamMemberEvent>(const BrnGui::GuiLastBlueTeamMemberEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5010
    template int GuiModule::AddGuiEvent<BrnGui::GuiLeaderPassedKMBoundaryEvent>(const BrnGui::GuiLeaderPassedKMBoundaryEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4DE8
    template int GuiModule::AddGuiEvent<BrnGui::GuiLeaderPassedMileBoundaryEvent>(const BrnGui::GuiLeaderPassedMileBoundaryEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4D30
    template int GuiModule::AddGuiEvent<BrnGui::GuiLeaptVehicleEvent>(const BrnGui::GuiLeaptVehicleEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D95C8
    template int GuiModule::AddGuiEvent<BrnGui::GuiLiveRevengeUpdateEvent>(const BrnGui::GuiLiveRevengeUpdateEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF580
    template int GuiModule::AddGuiEvent<BrnGui::GuiLocalPlayerEliminatedEvent>(const BrnGui::GuiLocalPlayerEliminatedEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4EA0
    template int GuiModule::AddGuiEvent<BrnGui::GuiMugshotControlEvent>(const BrnGui::GuiMugshotControlEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D08E8
    template int GuiModule::AddGuiEvent<BrnGui::GuiNearMissEvent>(const BrnGui::GuiNearMissEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4040
    template int GuiModule::AddGuiEvent<BrnGui::GuiNetworkLastStunRunEvent>(const BrnGui::GuiNetworkLastStunRunEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D90C0
    template int GuiModule::AddGuiEvent<BrnGui::GuiNetworkPlayerCrashingEvent>(const BrnGui::GuiNetworkPlayerCrashingEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D58B0
    template int GuiModule::AddGuiEvent<BrnGui::GuiNetworkPlayerOnTailEvent>(const BrnGui::GuiNetworkPlayerOnTailEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9FD8
    template int GuiModule::AddGuiEvent<BrnGui::GuiNetworkRemotePlayerDisconnectEvent>(const BrnGui::GuiNetworkRemotePlayerDisconnectEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6938
    template int GuiModule::AddGuiEvent<BrnGui::GuiNetworkStuntRunEliminationEvent>(const BrnGui::GuiNetworkStuntRunEliminationEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8F50
    template int GuiModule::AddGuiEvent<BrnGui::GuiNetworkStuntRunLeadingEvent>(const BrnGui::GuiNetworkStuntRunLeadingEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9008
    template int GuiModule::AddGuiEvent<BrnGui::GuiNetworkStuntRunVictoryEvent>(const BrnGui::GuiNetworkStuntRunVictoryEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8E98
    template int GuiModule::AddGuiEvent<BrnGui::GuiNetworkSuntRunInfoMessageEvent>(const BrnGui::GuiNetworkSuntRunInfoMessageEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9178
    template int GuiModule::AddGuiEvent<BrnGui::GuiNewBurnoutHudMessageEvent>(const BrnGui::GuiNewBurnoutHudMessageEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8318
    template int GuiModule::AddGuiEvent<BrnGui::GuiNewBurnoutSkillzEvent>(const BrnGui::GuiNewBurnoutSkillzEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8260
    template int GuiModule::AddGuiEvent<BrnGui::GuiOncomingEvent>(const BrnGui::GuiOncomingEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4320
    template int GuiModule::AddGuiEvent<BrnGui::GuiOnlineCarStatusEvent>(const BrnGui::GuiOnlineCarStatusEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0A58
    template int GuiModule::AddGuiEvent<BrnGui::GuiOnlineTeamChangeEvent>(const BrnGui::GuiOnlineTeamChangeEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5D00
    template int GuiModule::AddGuiEvent<BrnGui::GuiOverlayRequest>(const BrnGui::GuiOverlayRequest*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5180
    template int GuiModule::AddGuiEvent<BrnGui::GuiOverlayWaitFinishRequest>(const BrnGui::GuiOverlayWaitFinishRequest*, CgsGuiModuleIO::InputBuffer*);  // 0x823CFD68
    template int GuiModule::AddGuiEvent<BrnGui::GuiOvertakeEvent>(const BrnGui::GuiOvertakeEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9DB0
    template int GuiModule::AddGuiEvent<BrnGui::GuiPFXHookEvent>(const BrnGui::GuiPFXHookEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF078
    template int GuiModule::AddGuiEvent<BrnGui::GuiPFXHookStopEvent>(const BrnGui::GuiPFXHookStopEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823CEFC0
    template int GuiModule::AddGuiEvent<BrnGui::GuiPFXStartBackgroundHookEvent>(const BrnGui::GuiPFXStartBackgroundHookEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF130
    template int GuiModule::AddGuiEvent<BrnGui::GuiPFXStopBackgroundHookEvent>(const BrnGui::GuiPFXStopBackgroundHookEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF1E8
    template int GuiModule::AddGuiEvent<BrnGui::GuiPaybackReceivedEvent>(const BrnGui::GuiPaybackReceivedEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2F00
    template int GuiModule::AddGuiEvent<BrnGui::GuiPlayerCarColourResponse>(const BrnGui::GuiPlayerCarColourResponse*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1970
    template int GuiModule::AddGuiEvent<BrnGui::GuiPlayerCrashingStateChangeEvent>(const BrnGui::GuiPlayerCrashingStateChangeEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3578
    template int GuiModule::AddGuiEvent<BrnGui::GuiPlayerDrivableFromCrash>(const BrnGui::GuiPlayerDrivableFromCrash*, CgsGuiModuleIO::InputBuffer*);  // 0x823DA2E8
    template int GuiModule::AddGuiEvent<BrnGui::GuiPlayerEliminatedEvent>(const BrnGui::GuiPlayerEliminatedEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4F58
    template int GuiModule::AddGuiEvent<BrnGui::GuiPlayerEngineEvent>(const BrnGui::GuiPlayerEngineEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823DA3A0
    template int GuiModule::AddGuiEvent<BrnGui::GuiPlayerInShortcutEvent>(const BrnGui::GuiPlayerInShortcutEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8038
    template int GuiModule::AddGuiEvent<BrnGui::GuiPlayerInfoResponse>(const BrnGui::GuiPlayerInfoResponse*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4828
    template int GuiModule::AddGuiEvent<BrnGui::GuiPlayerRaceCarIdEvent>(const BrnGui::GuiPlayerRaceCarIdEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823DA458
    template int GuiModule::AddGuiEvent<BrnGui::GuiPowerParkResult>(const BrnGui::GuiPowerParkResult*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8488
    template int GuiModule::AddGuiEvent<BrnGui::GuiPursuitScoreUpdate>(const BrnGui::GuiPursuitScoreUpdate*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1018
    template int GuiModule::AddGuiEvent<BrnGui::GuiRaceCarInfoEvent>(const BrnGui::GuiRaceCarInfoEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823DA738
    template int GuiModule::AddGuiEvent<BrnGui::GuiRaceCheckpointReached>(const BrnGui::GuiRaceCheckpointReached*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4B08
    template int GuiModule::AddGuiEvent<BrnGui::GuiReplayStatusEvent>(const BrnGui::GuiReplayStatusEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823DAE68
    template int GuiModule::AddGuiEvent<BrnGui::GuiRivalIsFleeing>(const BrnGui::GuiRivalIsFleeing*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6658
    template int GuiModule::AddGuiEvent<BrnGui::GuiRivalryStatusChange>(const BrnGui::GuiRivalryStatusChange*, CgsGuiModuleIO::InputBuffer*);  // 0x823D65A0
    template int GuiModule::AddGuiEvent<BrnGui::GuiRoadRageScoreUpdate>(const BrnGui::GuiRoadRageScoreUpdate*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0F60
    template int GuiModule::AddGuiEvent<BrnGui::GuiSetEasyDriveNotAllowedEvent>(const BrnGui::GuiSetEasyDriveNotAllowedEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2158
    template int GuiModule::AddGuiEvent<BrnGui::GuiShowtimeJustBounced>(const BrnGui::GuiShowtimeJustBounced*, CgsGuiModuleIO::InputBuffer*);  // 0x823D98A8
    template int GuiModule::AddGuiEvent<BrnGui::GuiShowtimeModeSwitch>(const BrnGui::GuiShowtimeModeSwitch*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9510
    template int GuiModule::AddGuiEvent<BrnGui::GuiShowtimeScoreUpdate>(const BrnGui::GuiShowtimeScoreUpdate*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9458
    template int GuiModule::AddGuiEvent<BrnGui::GuiShowtimeTriggered>(const BrnGui::GuiShowtimeTriggered*, CgsGuiModuleIO::InputBuffer*);  // 0x823D83D0
    template int GuiModule::AddGuiEvent<BrnGui::GuiShutdownEvent>(const BrnGui::GuiShutdownEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8990
    template int GuiModule::AddGuiEvent<BrnGui::GuiShutdownFinishedEvent>(const BrnGui::GuiShutdownFinishedEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8A48
    template int GuiModule::AddGuiEvent<BrnGui::GuiSoftTakedownEvent>(const BrnGui::GuiSoftTakedownEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9AD0
    template int GuiModule::AddGuiEvent<BrnGui::GuiSpinningEvent>(const BrnGui::GuiSpinningEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D41B0
    template int GuiModule::AddGuiEvent<BrnGui::GuiTailgatingEvent>(const BrnGui::GuiTailgatingEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D43D8
    template int GuiModule::AddGuiEvent<BrnGui::GuiTakedownEvent>(const BrnGui::GuiTakedownEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9A18
    template int GuiModule::AddGuiEvent<BrnGui::GuiTookLastEvent>(const BrnGui::GuiTookLastEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D9F20
    template int GuiModule::AddGuiEvent<BrnGui::GuiTookLeadEvent>(const BrnGui::GuiTookLeadEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8C70
    template int GuiModule::AddGuiEvent<BrnGui::GuiTrafficCheckEvent>(const BrnGui::GuiTrafficCheckEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D3F88
    template int GuiModule::AddGuiEvent<BrnGui::GuiTraitorousTakedownEvent>(const BrnGui::GuiTraitorousTakedownEvent*, CgsGuiModuleIO::InputBuffer*);  // 0x823D50C8
    template int GuiModule::AddGuiEvent<CgsGui::GuiControllerDisconnected>(const CgsGui::GuiControllerDisconnected*, CgsGuiModuleIO::InputBuffer*);  // 0x823DACF8
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<103>>(const CgsGui::GuiEvent<103>*, CgsGuiModuleIO::InputBuffer*);  // 0x823CF860
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<104>>(const CgsGui::GuiEvent<104>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D5FE0
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<109>>(const CgsGui::GuiEvent<109>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0B10
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<127>>(const CgsGui::GuiEvent<127>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0550
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<129>>(const CgsGui::GuiEvent<129>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D52F0
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<131>>(const CgsGui::GuiEvent<131>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D53A8
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<134>>(const CgsGui::GuiEvent<134>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4600
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<136>>(const CgsGui::GuiEvent<136>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D46B8
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<137>>(const CgsGui::GuiEvent<137>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D4770
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<151>>(const CgsGui::GuiEvent<151>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D67C8
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<152>>(const CgsGui::GuiEvent<152>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6880
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<153>>(const CgsGui::GuiEvent<153>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6378
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<164>>(const CgsGui::GuiEvent<164>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2B68
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<178>>(const CgsGui::GuiEvent<178>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7570
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<180>>(const CgsGui::GuiEvent<180>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7628
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<183>>(const CgsGui::GuiEvent<183>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2FB8
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<271>>(const CgsGui::GuiEvent<271>*, CgsGuiModuleIO::InputBuffer*);  // 0x823CFE20
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<280>>(const CgsGui::GuiEvent<280>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0270
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<285>>(const CgsGui::GuiEvent<285>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0608
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<291>>(const CgsGui::GuiEvent<291>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1FE8
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<296>>(const CgsGui::GuiEvent<296>*, CgsGuiModuleIO::InputBuffer*);  // 0x823CECE0
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<297>>(const CgsGui::GuiEvent<297>*, CgsGuiModuleIO::InputBuffer*);  // 0x823CED98
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<312>>(const CgsGui::GuiEvent<312>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D31E0
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<315>>(const CgsGui::GuiEvent<315>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D34C0
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<500>>(const CgsGui::GuiEvent<500>*, CgsGuiModuleIO::InputBuffer*);  // 0x823CEF08
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<523>>(const CgsGui::GuiEvent<523>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D8540
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<571>>(const CgsGui::GuiEvent<571>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0BC8
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<579>>(const CgsGui::GuiEvent<579>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6098
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<584>>(const CgsGui::GuiEvent<584>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D6150
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<74>>(const CgsGui::GuiEvent<74>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D7908
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<83>>(const CgsGui::GuiEvent<83>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D18B8
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<85>>(const CgsGui::GuiEvent<85>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1748
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<91>>(const CgsGui::GuiEvent<91>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D2660
    template int GuiModule::AddGuiEvent<CgsGui::GuiEvent<95>>(const CgsGui::GuiEvent<95>*, CgsGuiModuleIO::InputBuffer*);  // 0x823D0498
    template int GuiModule::AddGuiEvent<CgsGui::GuiEventActiveUserIndex>(const CgsGui::GuiEventActiveUserIndex*, CgsGuiModuleIO::InputBuffer*);  // 0x823DA960
    template int GuiModule::AddGuiEvent<CgsGui::GuiEventControllerAxis>(const CgsGui::GuiEventControllerAxis*, CgsGuiModuleIO::InputBuffer*);  // 0x823DAAD0
    template int GuiModule::AddGuiEvent<CgsGui::GuiEventControllerInputDown>(const CgsGui::GuiEventControllerInputDown*, CgsGuiModuleIO::InputBuffer*);  // 0x823DAB88
    template int GuiModule::AddGuiEvent<CgsGui::GuiEventControllerInputPressed>(const CgsGui::GuiEventControllerInputPressed*, CgsGuiModuleIO::InputBuffer*);  // 0x823DA8A8
    template int GuiModule::AddGuiEvent<CgsGui::GuiEventControllerInputReleased>(const CgsGui::GuiEventControllerInputReleased*, CgsGuiModuleIO::InputBuffer*);  // 0x823DAC40
    template int GuiModule::AddGuiEvent<CgsGui::GuiEventNetworkLaunching>(const CgsGui::GuiEventNetworkLaunching*, CgsGuiModuleIO::InputBuffer*);  // 0x823CFCB0
    template int GuiModule::AddGuiEvent<CgsGui::GuiEventSetLanguage>(const CgsGui::GuiEventSetLanguage*, CgsGuiModuleIO::InputBuffer*);  // 0x823DADB0
    template int GuiModule::AddGuiEvent<CgsGui::GuiEventTimeInfo>(const CgsGui::GuiEventTimeInfo*, CgsGuiModuleIO::InputBuffer*);  // 0x823D1468
}
