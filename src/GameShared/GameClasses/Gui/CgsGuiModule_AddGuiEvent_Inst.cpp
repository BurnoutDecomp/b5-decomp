// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/CgsGuiModule_AddGuiEvent_Inst.cpp
//
// The 270 X360-attested explicit instantiations of CgsGui::GuiModule::AddGuiEvent<T>
// (one per queued GUI event type). ARTIST emits each instantiation out of line
// (0x823CEB70..0x823DAE68) from the header-inline template (CgsGuiModule.h, DecFIGS
// h:284); every one is the same body, differing only in the two per-T literals of
// AddEvent(&event, id, size) -- id == lEvent.GetEventType() (the GuiEvent<N> id) and
// size == sizeof(T) (e.g. AddGuiEvent<BrnGui::GuiAftertouchEvent> @0x823D4490 ->
// AddEvent(q, ev, 403, 32)). This file only makes the tree emit every instance in one
// object; on the console each instance came from its callers' objects.
//
// sub_8284F238(buffer) is CgsGui::CgsGuiModuleIO::InputBuffer::GetGuiEvents() (PS3
// DecFIGS: the instance calls ._ZN6CgsGui14CgsGuiModuleIO11InputBuffer12GetGuiEventsEv).
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

// ---- the 270 explicit instantiations (X360 addresses in trailing comments) -------
namespace CgsGui
{
    template void GuiModule::AddGuiEvent<BrnGui::GuiAftertouchEvent>(BrnGui::GuiAftertouchEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4490
    template void GuiModule::AddGuiEvent<BrnGui::GuiAttackScoreUpdate>(BrnGui::GuiAttackScoreUpdate&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1188
    template void GuiModule::AddGuiEvent<BrnGui::GuiAutosaveRequestEvent>(BrnGui::GuiAutosaveRequestEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D03E0
    template void GuiModule::AddGuiEvent<BrnGui::GuiBHRCheckpointReachedEvent>(BrnGui::GuiBHRCheckpointReachedEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5968
    template void GuiModule::AddGuiEvent<BrnGui::GuiBlueTeamIsBehindYouEvent>(BrnGui::GuiBlueTeamIsBehindYouEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4C78
    template void GuiModule::AddGuiEvent<BrnGui::GuiBlueTeamIsEscapingEvent>(BrnGui::GuiBlueTeamIsEscapingEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4BC0
    template void GuiModule::AddGuiEvent<BrnGui::GuiCarSelectAbortEvent>(BrnGui::GuiCarSelectAbortEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D88D8
    template void GuiModule::AddGuiEvent<BrnGui::GuiCarSelectOnlineTimeLeftEvent>(BrnGui::GuiCarSelectOnlineTimeLeftEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1800
    template void GuiModule::AddGuiEvent<BrnGui::GuiCarSelectReadyToExitEvent>(BrnGui::GuiCarSelectReadyToExitEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D86B0
    template void GuiModule::AddGuiEvent<BrnGui::GuiCarSelectStartEvent>(BrnGui::GuiCarSelectStartEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1690
    template void GuiModule::AddGuiEvent<BrnGui::GuiCarSelectionChangedDropIn>(BrnGui::GuiCarSelectionChangedDropIn&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8768
    template void GuiModule::AddGuiEvent<BrnGui::GuiCarSelectionChangedOnline>(BrnGui::GuiCarSelectionChangedOnline&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8820
    template void GuiModule::AddGuiEvent<BrnGui::GuiCarSelectionEvent>(BrnGui::GuiCarSelectionEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4998
    template void GuiModule::AddGuiEvent<BrnGui::GuiCarUnlockEvent>(BrnGui::GuiCarUnlockEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7A78
    template void GuiModule::AddGuiEvent<BrnGui::GuiCarUnlockNewCarEvent>(BrnGui::GuiCarUnlockNewCarEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7850
    template void GuiModule::AddGuiEvent<BrnGui::GuiCarUnlockStartEvent>(BrnGui::GuiCarUnlockStartEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D15D8
    template void GuiModule::AddGuiEvent<BrnGui::GuiCarUnlockedLiveryEvent>(BrnGui::GuiCarUnlockedLiveryEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D48E0
    template void GuiModule::AddGuiEvent<BrnGui::GuiChallengeEndEvent>(BrnGui::GuiChallengeEndEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5F28
    template void GuiModule::AddGuiEvent<BrnGui::GuiChallengeNotActiveStartEvent>(BrnGui::GuiChallengeNotActiveStartEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D09A0
    template void GuiModule::AddGuiEvent<BrnGui::GuiChallengeStartEvent>(BrnGui::GuiChallengeStartEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5DB8
    template void GuiModule::AddGuiEvent<BrnGui::GuiChallengeTriggerResponse>(BrnGui::GuiChallengeTriggerResponse&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5E70
    template void GuiModule::AddGuiEvent<BrnGui::GuiChallengeUpdateEvent>(BrnGui::GuiChallengeUpdateEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6208
    template void GuiModule::AddGuiEvent<BrnGui::GuiChangeCarEvent>(BrnGui::GuiChangeCarEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D79C0
    template void GuiModule::AddGuiEvent<BrnGui::GuiCompletedStuntEvent>(BrnGui::GuiCompletedStuntEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2888
    template void GuiModule::AddGuiEvent<BrnGui::GuiCrashComboEvent>(BrnGui::GuiCrashComboEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7798
    template void GuiModule::AddGuiEvent<BrnGui::GuiCrashScoreUpdate>(BrnGui::GuiCrashScoreUpdate&, CgsGuiModuleIO::InputBuffer*);  // 0x823D10D0
    template void GuiModule::AddGuiEvent<BrnGui::GuiDeveloperChallengesCompleted>(BrnGui::GuiDeveloperChallengesCompleted&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9230
    template void GuiModule::AddGuiEvent<BrnGui::GuiDirtyTrickEndedEvent>(BrnGui::GuiDirtyTrickEndedEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9CF8
    template void GuiModule::AddGuiEvent<BrnGui::GuiDirtyTrickNewEvent>(BrnGui::GuiDirtyTrickNewEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9B88
    template void GuiModule::AddGuiEvent<BrnGui::GuiDirtyTrickTriggerEvent>(BrnGui::GuiDirtyTrickTriggerEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9C40
    template void GuiModule::AddGuiEvent<BrnGui::GuiDriftingEvent>(BrnGui::GuiDriftingEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D40F8
    template void GuiModule::AddGuiEvent<BrnGui::GuiDriveThroughEvent>(BrnGui::GuiDriveThroughEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3070
    template void GuiModule::AddGuiEvent<BrnGui::GuiEnteredJunkyard>(BrnGui::GuiEnteredJunkyard&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1520
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventAllJunctionsDiscoveredOfType>(BrnGui::GuiEventAllJunctionsDiscoveredOfType&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3408
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventAllOfRivalsShutdown>(BrnGui::GuiEventAllOfRivalsShutdown&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3298
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventAllOfTypeComplete>(BrnGui::GuiEventAllOfTypeComplete&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3350
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventBoostBarStuntInfo>(BrnGui::GuiEventBoostBarStuntInfo&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3A80
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventBoostInfo>(BrnGui::GuiEventBoostInfo&, CgsGuiModuleIO::InputBuffer*);  // 0x823DA510
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventBuddyNotification>(BrnGui::GuiEventBuddyNotification&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF638
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventCamPicCompressed>(BrnGui::GuiEventCamPicCompressed&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0778
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventCanSkipCrash>(BrnGui::GuiEventCanSkipCrash&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3630
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventCantPaintCar>(BrnGui::GuiEventCantPaintCar&, CgsGuiModuleIO::InputBuffer*);  // 0x823D37A0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventCarJoinedEvent>(BrnGui::GuiEventCarJoinedEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6710
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventChallengedEventDataResponse>(BrnGui::GuiEventChallengedEventDataResponse&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7120
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventChangeDistrict>(BrnGui::GuiEventChangeDistrict&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2CD8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventCurrentStatus>(BrnGui::GuiEventCurrentStatus&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0DF0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventDirectorSettings>(BrnGui::GuiEventDirectorSettings&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF2A0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventDriveThruDiscovered>(BrnGui::GuiEventDriveThruDiscovered&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3128
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventEnterEventStartLocation>(BrnGui::GuiEventEnterEventStartLocation&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1E78
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventEnterLandmarkArea>(BrnGui::GuiEventEnterLandmarkArea&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3ED0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventEventStateResponse>(BrnGui::GuiEventEventStateResponse&, CgsGuiModuleIO::InputBuffer*);  // 0x823D85F8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventFailedToStartEvent>(BrnGui::GuiEventFailedToStartEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6430
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventFburnChallengeEveryPlayerStatus>(BrnGui::GuiEventFburnChallengeEveryPlayerStatus&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8B00
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventFinishedModeResults>(BrnGui::GuiEventFinishedModeResults&, CgsGuiModuleIO::InputBuffer*);  // 0x823D24F0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventGameCompleted>(BrnGui::GuiEventGameCompleted&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3E18
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventGameCompletedOnline>(BrnGui::GuiEventGameCompletedOnline&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3D60
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventHideDriveThru>(BrnGui::GuiEventHideDriveThru&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2D90
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventInviteComplete>(BrnGui::GuiEventInviteComplete&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5460
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventInviteFailed>(BrnGui::GuiEventInviteFailed&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF6F0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventJumpStarted>(BrnGui::GuiEventJumpStarted&, CgsGuiModuleIO::InputBuffer*);  // 0x823D39C8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventJunctionInfo>(BrnGui::GuiEventJunctionInfo&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1AE0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventLiveRevengeProfileData>(BrnGui::GuiEventLiveRevengeProfileData&, CgsGuiModuleIO::InputBuffer*);  // 0x823D01B8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventLoadImageFiles>(BrnGui::GuiEventLoadImageFiles&, CgsGuiModuleIO::InputBuffer*);  // 0x823D81A8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventMedalUpdate>(BrnGui::GuiEventMedalUpdate&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1A28
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventMiniMapSwitch>(BrnGui::GuiEventMiniMapSwitch&, CgsGuiModuleIO::InputBuffer*);  // 0x823D74B8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventMustFixCarFirst>(BrnGui::GuiEventMustFixCarFirst&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3858
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkGameParams>(BrnGui::GuiEventNetworkGameParams&, CgsGuiModuleIO::InputBuffer*);  // 0x823CFA88
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkLeftGame>(BrnGui::GuiEventNetworkLeftGame&, CgsGuiModuleIO::InputBuffer*);  // 0x823CFB40
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkLobbyPlayerList>(BrnGui::GuiEventNetworkLobbyPlayerList&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF4C8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkPlayerImage>(BrnGui::GuiEventNetworkPlayerImage&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7B30
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkPlayerJoinedLobby>(BrnGui::GuiEventNetworkPlayerJoinedLobby&, CgsGuiModuleIO::InputBuffer*);  // 0x823D76E0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkPlayerLeftLobby>(BrnGui::GuiEventNetworkPlayerLeftLobby&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0830
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkPlayerList>(BrnGui::GuiEventNetworkPlayerList&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF358
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkPlayerStatus>(BrnGui::GuiEventNetworkPlayerStatus&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF410
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkPostGameProcessingFinished>(BrnGui::GuiEventNetworkPostGameProcessingFinished&, CgsGuiModuleIO::InputBuffer*);  // 0x823CFBF8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkShowFreeBurnIntro>(BrnGui::GuiEventNetworkShowFreeBurnIntro&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2AB0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventNetworkSplashEvent>(BrnGui::GuiEventNetworkSplashEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2C20
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventOfflinePostEvent>(BrnGui::GuiEventOfflinePostEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D20A0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventOnlineAccountSettings>(BrnGui::GuiEventOnlineAccountSettings&, CgsGuiModuleIO::InputBuffer*);  // 0x823D06C0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventOnlineNumFriendsCount>(BrnGui::GuiEventOnlineNumFriendsCount&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF918
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventOnlinePostEvent>(BrnGui::GuiEventOnlinePostEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1240
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventOnlinePostEventScalps>(BrnGui::GuiEventOnlinePostEventScalps&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF7A8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventOnlineReceiveFriendInfo>(BrnGui::GuiEventOnlineReceiveFriendInfo&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF9D0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventOnlineTimeout>(BrnGui::GuiEventOnlineTimeout&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0328
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventPlayerReachedRoadRageTarget>(BrnGui::GuiEventPlayerReachedRoadRageTarget&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1DC0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventPlayerWrecked>(BrnGui::GuiEventPlayerWrecked&, CgsGuiModuleIO::InputBuffer*);  // 0x823DA680
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventPreRaceMessages>(BrnGui::GuiEventPreRaceMessages&, CgsGuiModuleIO::InputBuffer*);  // 0x823D29F8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventPrepareForInvite>(BrnGui::GuiEventPrepareForInvite&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5238
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventPrepareForModeStart>(BrnGui::GuiEventPrepareForModeStart&, CgsGuiModuleIO::InputBuffer*);  // 0x823D27D0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventPreraceTrigger>(BrnGui::GuiEventPreraceTrigger&, CgsGuiModuleIO::InputBuffer*);  // 0x823CEC28
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventProgressionProfileData>(BrnGui::GuiEventProgressionProfileData&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4548
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRaceDistanceRemaining>(BrnGui::GuiEventRaceDistanceRemaining&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0C80
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRaceDistanceToCheckpoint>(BrnGui::GuiEventRaceDistanceToCheckpoint&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0D38
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRacePositionInfo>(BrnGui::GuiEventRacePositionInfo&, CgsGuiModuleIO::InputBuffer*);  // 0x823D92E8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRankProgressResponse>(BrnGui::GuiEventRankProgressResponse&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7290
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRequestCollisionWorldEvent>(BrnGui::GuiEventRequestCollisionWorldEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823DA230
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventReturnDistrict>(BrnGui::GuiEventReturnDistrict&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5518
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRivalInfoResponse>(BrnGui::GuiEventRivalInfoResponse&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7400
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRivalryFullInfoResponse>(BrnGui::GuiEventRivalryFullInfoResponse&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7348
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRagePlayerDamage>(BrnGui::GuiEventRoadRagePlayerDamage&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2210
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRageTimeExtended>(BrnGui::GuiEventRoadRageTimeExtended&, CgsGuiModuleIO::InputBuffer*);  // 0x823D64E8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleBatchDataResponse>(BrnGui::GuiEventRoadRuleBatchDataResponse&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8BB8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleBegin>(BrnGui::GuiEventRoadRuleBegin&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6C18
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleChangeMode>(BrnGui::GuiEventRoadRuleChangeMode&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7068
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleData>(BrnGui::GuiEventRoadRuleData&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6AA8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleEnd>(BrnGui::GuiEventRoadRuleEnd&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6CD0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleEnter>(BrnGui::GuiEventRoadRuleEnter&, CgsGuiModuleIO::InputBuffer*);  // 0x823D69F0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleLeave>(BrnGui::GuiEventRoadRuleLeave&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6D88
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleNewHighScore>(BrnGui::GuiEventRoadRuleNewHighScore&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6FB0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleNewRulers>(BrnGui::GuiEventRoadRuleNewRulers&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7EC8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleTickerScoreResponse>(BrnGui::GuiEventRoadRuleTickerScoreResponse&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7E10
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleUpcomingRoads>(BrnGui::GuiEventRoadRuleUpcomingRoads&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6E40
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleUpdate>(BrnGui::GuiEventRoadRuleUpdate&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6EF8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRoadRuleUpdateTargetScores>(BrnGui::GuiEventRoadRuleUpdateTargetScores&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6B60
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventRunFsm>(BrnGui::GuiEventRunFsm&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2438
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventSaveImageFileAndAutosave>(BrnGui::GuiEventSaveImageFileAndAutosave&, CgsGuiModuleIO::InputBuffer*);  // 0x823D80F0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventScoreUpdate>(BrnGui::GuiEventScoreUpdate&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0EA8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventScoreboardDownloadedChallengeable>(BrnGui::GuiEventScoreboardDownloadedChallengeable&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8DE0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventScoreboardResponseCategoryEvent>(BrnGui::GuiEventScoreboardResponseCategoryEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823CFED8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventScoreboardResponseEvScoreTarget>(BrnGui::GuiEventScoreboardResponseEvScoreTarget&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8D28
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventScoreboardResponseIndexEvent>(BrnGui::GuiEventScoreboardResponseIndexEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823CFF90
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventScoreboardResponseTableEvent>(BrnGui::GuiEventScoreboardResponseTableEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0100
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventScoreboardResponseVariationEvent>(BrnGui::GuiEventScoreboardResponseVariationEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0048
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventSetAvailablePresetRaces>(BrnGui::GuiEventSetAvailablePresetRaces&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2E48
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventSetBlackBars>(BrnGui::GuiEventSetBlackBars&, CgsGuiModuleIO::InputBuffer*);  // 0x823CEE50
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventSetRoadRuleScoreMode>(BrnGui::GuiEventSetRoadRuleScoreMode&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7F80
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventShowFreeburnChallenge>(BrnGui::GuiEventShowFreeburnChallenge&, CgsGuiModuleIO::InputBuffer*);  // 0x823D62C0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventShowHideHud>(BrnGui::GuiEventShowHideHud&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1C50
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventSpecificPresetRaces>(BrnGui::GuiEventSpecificPresetRaces&, CgsGuiModuleIO::InputBuffer*);  // 0x823D13B0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventStatsResponse>(BrnGui::GuiEventStatsResponse&, CgsGuiModuleIO::InputBuffer*);  // 0x823D71D8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventStopMode>(BrnGui::GuiEventStopMode&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2380
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventStuntAllComplete>(BrnGui::GuiEventStuntAllComplete&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3CA8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventStuntAreaComplete>(BrnGui::GuiEventStuntAreaComplete&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3BF0
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventStuntInfo>(BrnGui::GuiEventStuntInfo&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3B38
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventSuperJumpFailed>(BrnGui::GuiEventSuperJumpFailed&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3910
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventTickerClearMessages>(BrnGui::GuiEventTickerClearMessages&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2718
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventTickerCustomMessage>(BrnGui::GuiEventTickerCustomMessage&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1D08
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventTimeUp>(BrnGui::GuiEventTimeUp&, CgsGuiModuleIO::InputBuffer*);  // 0x823D36E8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventToggleChangeCarMessage>(BrnGui::GuiEventToggleChangeCarMessage&, CgsGuiModuleIO::InputBuffer*);  // 0x823DAA18
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventTogglePictureParadise>(BrnGui::GuiEventTogglePictureParadise&, CgsGuiModuleIO::InputBuffer*);  // 0x823CEB70
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventTriggerOnlinePostEvent>(BrnGui::GuiEventTriggerOnlinePostEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1F30
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventTrophyCarUnlock>(BrnGui::GuiEventTrophyCarUnlock&, CgsGuiModuleIO::InputBuffer*);  // 0x823D22C8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventUpdateEventCountdown>(BrnGui::GuiEventUpdateEventCountdown&, CgsGuiModuleIO::InputBuffer*);  // 0x823D25A8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventUpdateEventStarts>(BrnGui::GuiEventUpdateEventStarts&, CgsGuiModuleIO::InputBuffer*);  // 0x823D12F8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventUpdateHud>(BrnGui::GuiEventUpdateHud&, CgsGuiModuleIO::InputBuffer*);  // 0x823DA5C8
    template void GuiModule::AddGuiEvent<BrnGui::GuiEventUpdateSatNav>(BrnGui::GuiEventUpdateSatNav&, CgsGuiModuleIO::InputBuffer*);  // 0x823D93A0
    template void GuiModule::AddGuiEvent<BrnGui::GuiFinishRaceEvent>(BrnGui::GuiFinishRaceEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9E68
    template void GuiModule::AddGuiEvent<BrnGui::GuiGameModeStarted>(BrnGui::GuiGameModeStarted&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4A50
    template void GuiModule::AddGuiEvent<BrnGui::GuiGamePausedEvent>(BrnGui::GuiGamePausedEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1B98
    template void GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageBHRRunnerCrashed>(BrnGui::GuiHUDMessageBHRRunnerCrashed&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5A20
    template void GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageComboPerformed>(BrnGui::GuiHUDMessageComboPerformed&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5B90
    template void GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageCrushCombo>(BrnGui::GuiHUDMessageCrushCombo&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9738
    template void GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageShowtimeMultiplier>(BrnGui::GuiHUDMessageShowtimeMultiplier&, CgsGuiModuleIO::InputBuffer*);  // 0x823D97F0
    template void GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageSignSmashed>(BrnGui::GuiHUDMessageSignSmashed&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9960
    template void GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageStuntPerformed>(BrnGui::GuiHUDMessageStuntPerformed&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5AD8
    template void GuiModule::AddGuiEvent<BrnGui::GuiHUDMessageStuntTimeUp>(BrnGui::GuiHUDMessageStuntTimeUp&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5C48
    template void GuiModule::AddGuiEvent<BrnGui::GuiHitVehicleEvent>(BrnGui::GuiHitVehicleEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9680
    template void GuiModule::AddGuiEvent<BrnGui::GuiImageGalleryCollectedCountEvent>(BrnGui::GuiImageGalleryCollectedCountEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7CA0
    template void GuiModule::AddGuiEvent<BrnGui::GuiImageGalleryCollectedDataEvent>(BrnGui::GuiImageGalleryCollectedDataEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7D58
    template void GuiModule::AddGuiEvent<BrnGui::GuiImageGalleryImageInfoEvent>(BrnGui::GuiImageGalleryImageInfoEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7BE8
    template void GuiModule::AddGuiEvent<BrnGui::GuiImpactEvent>(BrnGui::GuiImpactEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823DA7F0
    template void GuiModule::AddGuiEvent<BrnGui::GuiInAirEvent>(BrnGui::GuiInAirEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4268
    template void GuiModule::AddGuiEvent<BrnGui::GuiInEventFinisher>(BrnGui::GuiInEventFinisher&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5740
    template void GuiModule::AddGuiEvent<BrnGui::GuiInEventLeaderSplit>(BrnGui::GuiInEventLeaderSplit&, CgsGuiModuleIO::InputBuffer*);  // 0x823D55D0
    template void GuiModule::AddGuiEvent<BrnGui::GuiInEventNeckAndNeck>(BrnGui::GuiInEventNeckAndNeck&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5688
    template void GuiModule::AddGuiEvent<BrnGui::GuiInEventRivalProgress>(BrnGui::GuiInEventRivalProgress&, CgsGuiModuleIO::InputBuffer*);  // 0x823D57F8
    template void GuiModule::AddGuiEvent<BrnGui::GuiInProgressStuntEvent>(BrnGui::GuiInProgressStuntEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2940
    template void GuiModule::AddGuiEvent<BrnGui::GuiLastBlueTeamMemberEvent>(BrnGui::GuiLastBlueTeamMemberEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5010
    template void GuiModule::AddGuiEvent<BrnGui::GuiLeaderPassedKMBoundaryEvent>(BrnGui::GuiLeaderPassedKMBoundaryEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4DE8
    template void GuiModule::AddGuiEvent<BrnGui::GuiLeaderPassedMileBoundaryEvent>(BrnGui::GuiLeaderPassedMileBoundaryEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4D30
    template void GuiModule::AddGuiEvent<BrnGui::GuiLeaptVehicleEvent>(BrnGui::GuiLeaptVehicleEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D95C8
    template void GuiModule::AddGuiEvent<BrnGui::GuiLiveRevengeUpdateEvent>(BrnGui::GuiLiveRevengeUpdateEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF580
    template void GuiModule::AddGuiEvent<BrnGui::GuiLocalPlayerEliminatedEvent>(BrnGui::GuiLocalPlayerEliminatedEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4EA0
    template void GuiModule::AddGuiEvent<BrnGui::GuiMugshotControlEvent>(BrnGui::GuiMugshotControlEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D08E8
    template void GuiModule::AddGuiEvent<BrnGui::GuiNearMissEvent>(BrnGui::GuiNearMissEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4040
    template void GuiModule::AddGuiEvent<BrnGui::GuiNetworkLastStunRunEvent>(BrnGui::GuiNetworkLastStunRunEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D90C0
    template void GuiModule::AddGuiEvent<BrnGui::GuiNetworkPlayerCrashingEvent>(BrnGui::GuiNetworkPlayerCrashingEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D58B0
    template void GuiModule::AddGuiEvent<BrnGui::GuiNetworkPlayerOnTailEvent>(BrnGui::GuiNetworkPlayerOnTailEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9FD8
    template void GuiModule::AddGuiEvent<BrnGui::GuiNetworkRemotePlayerDisconnectEvent>(BrnGui::GuiNetworkRemotePlayerDisconnectEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6938
    template void GuiModule::AddGuiEvent<BrnGui::GuiNetworkStuntRunEliminationEvent>(BrnGui::GuiNetworkStuntRunEliminationEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8F50
    template void GuiModule::AddGuiEvent<BrnGui::GuiNetworkStuntRunLeadingEvent>(BrnGui::GuiNetworkStuntRunLeadingEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9008
    template void GuiModule::AddGuiEvent<BrnGui::GuiNetworkStuntRunVictoryEvent>(BrnGui::GuiNetworkStuntRunVictoryEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8E98
    template void GuiModule::AddGuiEvent<BrnGui::GuiNetworkSuntRunInfoMessageEvent>(BrnGui::GuiNetworkSuntRunInfoMessageEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9178
    template void GuiModule::AddGuiEvent<BrnGui::GuiNewBurnoutHudMessageEvent>(BrnGui::GuiNewBurnoutHudMessageEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8318
    template void GuiModule::AddGuiEvent<BrnGui::GuiNewBurnoutSkillzEvent>(BrnGui::GuiNewBurnoutSkillzEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8260
    template void GuiModule::AddGuiEvent<BrnGui::GuiOncomingEvent>(BrnGui::GuiOncomingEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4320
    template void GuiModule::AddGuiEvent<BrnGui::GuiOnlineCarStatusEvent>(BrnGui::GuiOnlineCarStatusEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0A58
    template void GuiModule::AddGuiEvent<BrnGui::GuiOnlineTeamChangeEvent>(BrnGui::GuiOnlineTeamChangeEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5D00
    template void GuiModule::AddGuiEvent<BrnGui::GuiOverlayRequest>(BrnGui::GuiOverlayRequest&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5180
    template void GuiModule::AddGuiEvent<BrnGui::GuiOverlayWaitFinishRequest>(BrnGui::GuiOverlayWaitFinishRequest&, CgsGuiModuleIO::InputBuffer*);  // 0x823CFD68
    template void GuiModule::AddGuiEvent<BrnGui::GuiOvertakeEvent>(BrnGui::GuiOvertakeEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9DB0
    template void GuiModule::AddGuiEvent<BrnGui::GuiPFXHookEvent>(BrnGui::GuiPFXHookEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF078
    template void GuiModule::AddGuiEvent<BrnGui::GuiPFXHookStopEvent>(BrnGui::GuiPFXHookStopEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823CEFC0
    template void GuiModule::AddGuiEvent<BrnGui::GuiPFXStartBackgroundHookEvent>(BrnGui::GuiPFXStartBackgroundHookEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF130
    template void GuiModule::AddGuiEvent<BrnGui::GuiPFXStopBackgroundHookEvent>(BrnGui::GuiPFXStopBackgroundHookEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF1E8
    template void GuiModule::AddGuiEvent<BrnGui::GuiPaybackReceivedEvent>(BrnGui::GuiPaybackReceivedEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2F00
    template void GuiModule::AddGuiEvent<BrnGui::GuiPlayerCarColourResponse>(BrnGui::GuiPlayerCarColourResponse&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1970
    template void GuiModule::AddGuiEvent<BrnGui::GuiPlayerCrashingStateChangeEvent>(BrnGui::GuiPlayerCrashingStateChangeEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3578
    template void GuiModule::AddGuiEvent<BrnGui::GuiPlayerDrivableFromCrash>(BrnGui::GuiPlayerDrivableFromCrash&, CgsGuiModuleIO::InputBuffer*);  // 0x823DA2E8
    template void GuiModule::AddGuiEvent<BrnGui::GuiPlayerEliminatedEvent>(BrnGui::GuiPlayerEliminatedEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4F58
    template void GuiModule::AddGuiEvent<BrnGui::GuiPlayerEngineEvent>(BrnGui::GuiPlayerEngineEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823DA3A0
    template void GuiModule::AddGuiEvent<BrnGui::GuiPlayerInShortcutEvent>(BrnGui::GuiPlayerInShortcutEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8038
    template void GuiModule::AddGuiEvent<BrnGui::GuiPlayerInfoResponse>(BrnGui::GuiPlayerInfoResponse&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4828
    template void GuiModule::AddGuiEvent<BrnGui::GuiPlayerRaceCarIdEvent>(BrnGui::GuiPlayerRaceCarIdEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823DA458
    template void GuiModule::AddGuiEvent<BrnGui::GuiPowerParkResult>(BrnGui::GuiPowerParkResult&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8488
    template void GuiModule::AddGuiEvent<BrnGui::GuiPursuitScoreUpdate>(BrnGui::GuiPursuitScoreUpdate&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1018
    template void GuiModule::AddGuiEvent<BrnGui::GuiRaceCarInfoEvent>(BrnGui::GuiRaceCarInfoEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823DA738
    template void GuiModule::AddGuiEvent<BrnGui::GuiRaceCheckpointReached>(BrnGui::GuiRaceCheckpointReached&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4B08
    template void GuiModule::AddGuiEvent<BrnGui::GuiReplayStatusEvent>(BrnGui::GuiReplayStatusEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823DAE68
    template void GuiModule::AddGuiEvent<BrnGui::GuiRivalIsFleeing>(BrnGui::GuiRivalIsFleeing&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6658
    template void GuiModule::AddGuiEvent<BrnGui::GuiRivalryStatusChange>(BrnGui::GuiRivalryStatusChange&, CgsGuiModuleIO::InputBuffer*);  // 0x823D65A0
    template void GuiModule::AddGuiEvent<BrnGui::GuiRoadRageScoreUpdate>(BrnGui::GuiRoadRageScoreUpdate&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0F60
    template void GuiModule::AddGuiEvent<BrnGui::GuiSetEasyDriveNotAllowedEvent>(BrnGui::GuiSetEasyDriveNotAllowedEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2158
    template void GuiModule::AddGuiEvent<BrnGui::GuiShowtimeJustBounced>(BrnGui::GuiShowtimeJustBounced&, CgsGuiModuleIO::InputBuffer*);  // 0x823D98A8
    template void GuiModule::AddGuiEvent<BrnGui::GuiShowtimeModeSwitch>(BrnGui::GuiShowtimeModeSwitch&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9510
    template void GuiModule::AddGuiEvent<BrnGui::GuiShowtimeScoreUpdate>(BrnGui::GuiShowtimeScoreUpdate&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9458
    template void GuiModule::AddGuiEvent<BrnGui::GuiShowtimeTriggered>(BrnGui::GuiShowtimeTriggered&, CgsGuiModuleIO::InputBuffer*);  // 0x823D83D0
    template void GuiModule::AddGuiEvent<BrnGui::GuiShutdownEvent>(BrnGui::GuiShutdownEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8990
    template void GuiModule::AddGuiEvent<BrnGui::GuiShutdownFinishedEvent>(BrnGui::GuiShutdownFinishedEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8A48
    template void GuiModule::AddGuiEvent<BrnGui::GuiSoftTakedownEvent>(BrnGui::GuiSoftTakedownEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9AD0
    template void GuiModule::AddGuiEvent<BrnGui::GuiSpinningEvent>(BrnGui::GuiSpinningEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D41B0
    template void GuiModule::AddGuiEvent<BrnGui::GuiTailgatingEvent>(BrnGui::GuiTailgatingEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D43D8
    template void GuiModule::AddGuiEvent<BrnGui::GuiTakedownEvent>(BrnGui::GuiTakedownEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9A18
    template void GuiModule::AddGuiEvent<BrnGui::GuiTookLastEvent>(BrnGui::GuiTookLastEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D9F20
    template void GuiModule::AddGuiEvent<BrnGui::GuiTookLeadEvent>(BrnGui::GuiTookLeadEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8C70
    template void GuiModule::AddGuiEvent<BrnGui::GuiTrafficCheckEvent>(BrnGui::GuiTrafficCheckEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D3F88
    template void GuiModule::AddGuiEvent<BrnGui::GuiTraitorousTakedownEvent>(BrnGui::GuiTraitorousTakedownEvent&, CgsGuiModuleIO::InputBuffer*);  // 0x823D50C8
    template void GuiModule::AddGuiEvent<CgsGui::GuiControllerDisconnected>(CgsGui::GuiControllerDisconnected&, CgsGuiModuleIO::InputBuffer*);  // 0x823DACF8
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<103>>(CgsGui::GuiEvent<103>&, CgsGuiModuleIO::InputBuffer*);  // 0x823CF860
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<104>>(CgsGui::GuiEvent<104>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D5FE0
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<109>>(CgsGui::GuiEvent<109>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0B10
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<127>>(CgsGui::GuiEvent<127>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0550
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<129>>(CgsGui::GuiEvent<129>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D52F0
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<131>>(CgsGui::GuiEvent<131>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D53A8
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<134>>(CgsGui::GuiEvent<134>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4600
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<136>>(CgsGui::GuiEvent<136>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D46B8
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<137>>(CgsGui::GuiEvent<137>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D4770
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<151>>(CgsGui::GuiEvent<151>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D67C8
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<152>>(CgsGui::GuiEvent<152>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6880
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<153>>(CgsGui::GuiEvent<153>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6378
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<164>>(CgsGui::GuiEvent<164>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2B68
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<178>>(CgsGui::GuiEvent<178>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7570
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<180>>(CgsGui::GuiEvent<180>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7628
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<183>>(CgsGui::GuiEvent<183>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2FB8
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<271>>(CgsGui::GuiEvent<271>&, CgsGuiModuleIO::InputBuffer*);  // 0x823CFE20
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<280>>(CgsGui::GuiEvent<280>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0270
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<285>>(CgsGui::GuiEvent<285>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0608
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<291>>(CgsGui::GuiEvent<291>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1FE8
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<296>>(CgsGui::GuiEvent<296>&, CgsGuiModuleIO::InputBuffer*);  // 0x823CECE0
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<297>>(CgsGui::GuiEvent<297>&, CgsGuiModuleIO::InputBuffer*);  // 0x823CED98
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<312>>(CgsGui::GuiEvent<312>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D31E0
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<315>>(CgsGui::GuiEvent<315>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D34C0
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<500>>(CgsGui::GuiEvent<500>&, CgsGuiModuleIO::InputBuffer*);  // 0x823CEF08
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<523>>(CgsGui::GuiEvent<523>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D8540
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<571>>(CgsGui::GuiEvent<571>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0BC8
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<579>>(CgsGui::GuiEvent<579>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6098
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<584>>(CgsGui::GuiEvent<584>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D6150
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<74>>(CgsGui::GuiEvent<74>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D7908
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<83>>(CgsGui::GuiEvent<83>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D18B8
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<85>>(CgsGui::GuiEvent<85>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1748
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<91>>(CgsGui::GuiEvent<91>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D2660
    template void GuiModule::AddGuiEvent<CgsGui::GuiEvent<95>>(CgsGui::GuiEvent<95>&, CgsGuiModuleIO::InputBuffer*);  // 0x823D0498
    template void GuiModule::AddGuiEvent<CgsGui::GuiEventActiveUserIndex>(CgsGui::GuiEventActiveUserIndex&, CgsGuiModuleIO::InputBuffer*);  // 0x823DA960
    template void GuiModule::AddGuiEvent<CgsGui::GuiEventControllerAxis>(CgsGui::GuiEventControllerAxis&, CgsGuiModuleIO::InputBuffer*);  // 0x823DAAD0
    template void GuiModule::AddGuiEvent<CgsGui::GuiEventControllerInputDown>(CgsGui::GuiEventControllerInputDown&, CgsGuiModuleIO::InputBuffer*);  // 0x823DAB88
    template void GuiModule::AddGuiEvent<CgsGui::GuiEventControllerInputPressed>(CgsGui::GuiEventControllerInputPressed&, CgsGuiModuleIO::InputBuffer*);  // 0x823DA8A8
    template void GuiModule::AddGuiEvent<CgsGui::GuiEventControllerInputReleased>(CgsGui::GuiEventControllerInputReleased&, CgsGuiModuleIO::InputBuffer*);  // 0x823DAC40
    template void GuiModule::AddGuiEvent<CgsGui::GuiEventNetworkLaunching>(CgsGui::GuiEventNetworkLaunching&, CgsGuiModuleIO::InputBuffer*);  // 0x823CFCB0
    template void GuiModule::AddGuiEvent<CgsGui::GuiEventSetLanguage>(CgsGui::GuiEventSetLanguage&, CgsGuiModuleIO::InputBuffer*);  // 0x823DADB0
    template void GuiModule::AddGuiEvent<CgsGui::GuiEventTimeInfo>(CgsGui::GuiEventTimeInfo&, CgsGuiModuleIO::InputBuffer*);  // 0x823D1468
}
