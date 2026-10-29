#ifndef BRN_GUI_MODULE_H
#define BRN_GUI_MODULE_H

#include "GameShared/GameClasses/Gui/CgsGuiModule.h"
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"
#include "GameShared/GameClasses/Gui/CgsImageFileInfo.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"
#include "GameSource/Replays/Serialisers/BrnReplayGuiModuleSerialiser.h"
#include "GameSource/Gui/BrnGuiMovieManager.h"
#include "GameSource/Gui/BrnGuiColourCalibrationScreen.h"
#include "GameSource/Gui/BrnGuiWorldDataController.h"
#include "GameSource/Gui/BrnGuiBurnoutSkillsManager.h"
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"
#include "GameSource/Gui/BrnCustomRendererManager.h"
#include "GameSource/Gui/BrnGuiViewModule.h"
#include "GameSource/Gui/Flow/HUD/BrnHudFlow.h"
#include "GameSource/Gui/Flow/Screen/BrnScreenFlow.h"
#include "GameSource/Gui/Flow/Overlay/BrnOverlayFlow.h"
#include "GameSource/Gui/BrnGuiFsmController.h"
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"
#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"
#include "GameSource/Gui/BrnGuiOverlaysDirector.h"
#include "GameSource/Gui/BrnGuiKeyboardHost.h"
#include "GameSource/Gui/BrnGuiProfileHost.h"
#include "GameShared/GameClasses/Gui/CgsGuideIntegration.h"
#include "GameSource/Gui/BrnGuiDebugComponent.h"
#include "GameSource/Gui/BrnGuiHudMessagesDebugComponent.h"
#include "GameSource/Gui/BrnGuiSatNavDebugComponent.h"
#include "GameSource/Gui/BrnGuiGuiCacheDebugComponent.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"
#include "GameSource/Gui/BrnGuiKeyboard.h"
#include "GameSource/Gui/PFX/BrnGuiEffectsArbitrator.h"
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h"
#include "SharedClasses/BrnSharedConstants.h"

namespace EA { namespace GameTalk { class GameTalkMessage; } }

namespace BrnGui
{
    // ARTIST82518028/82518D68/82527A58. The scheduler owns frame IO;
    // this class owns the original contained GUI objects and reply receivers.
    class GuiModule : public CgsGui::GuiModule
    {
    public:
        enum GuiPrepareStage
        {
            E_PREPARE_START, E_PREPARE_MEMORY, E_PREPARE_PARENT,
            E_PREPARE_HUD, E_PREPARE_SCREEN, E_PREPARE_OVERLAY,
            E_PREPARE_FSM_CONTROLLER, E_PREPARE_CUSTOMRENDERER_PREPARE,
            E_PREPARE_MOVIE_MANAGER, E_PREPARE_COLOUR_CALIBRATION_SCREEN,
            E_PREPARE_GUI_DEBUG_COMPONENT, E_PREPARE_PROFILE,
            E_PREPARE_SYSTEM_NOTIFICATIONS, E_PREPARE_WF_RESOURCES,
            E_PREPARE_WORLD_DATA_CONTROLLER, E_PREPARE_DONE
        };
        enum GuiReleaseStage
        {
            E_RELEASE_START, E_RELEASE_MEMORY, E_RELEASE_PARENT,
            E_RELEASE_HUD, E_RELEASE_SCREEN, E_RELEASE_OVERLAY,
            E_RELEASE_FSM_CONTROLLER, E_RELEASE_CUSTOMRENDERER_DESTRUCT,
            E_RELEASE_CUSTOMRENDERER_RELEASE, E_RELEASE_MOVIE_MANAGER,
            E_RELEASE_COLOUR_CALIBRATION_SCREEN, E_RELEASE_WORLD_DATA_CONTROLLER,
            E_RELEASE_GUI_DEBUG_COMPONENT, E_RELEASE_PROFILE,
            E_RELEASE_SYSTEM_NOTIFICATIONS, E_RELEASE_DONE
        };

        virtual void Construct(const BrnResource::HudMessageController* lpHudMessageController,
            const BrnResource::PopupController* lpPopupController,
            f32 lfAspectRatio, bool lbHighDef);
        virtual bool Prepare(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput,
            BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
            const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput);
        virtual bool Prepare2(BrnResource::GameDataIO::InputBuffer* lpGameDataInput);
        bool Release();
        void Destruct();
        void PreWorldUpdate(CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput,
            CgsGui::ModelIO::OutputBuffer* lpModelOutputBuffer);
        // ARTIST: r4 is a halfword update set; r9/r10 are model output/view
        // input. GameData input/output and the replay flag occupy stack args.
        virtual void Update(BrnUpdateSet luUpdateSet,
            CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            CgsGui::CgsGuiModuleIO::InputBuffer* lpInput,
            CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput,
            CgsGui::ModelIO::OutputBuffer* lpModelOutputBuffer,
            CgsGui::ViewIO::InputBuffer* lpViewInputBuffer,
            BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
            const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput,
            bool lbSerialise);
        virtual void Render(CgsGui::ViewIO::InputBuffer* lpViewInput,
            CgsGui::CgsGuiModuleIO::InputBuffer* lpInput,
            RendererIO::OutputBuffer* lpRenderOutput);
        void EndOfFrame();
        void UpdateAndRenderMovieManager(CgsGui::ViewIO::InputBuffer* lpViewInput);
        static void FlaptSoundTriggerCallback(void* lpUserData,
            const char* lpcComponentName, const char* lpcSwfName,
            const char* lpcActionName, const char* lpcLabel);

        void SetMovieBootMode(bool lbBootMovieMode) { mbBootMovieMode = lbBootMovieMode; }
        bool IsPrepared() const { return mePrepareStage == E_PREPARE_DONE; }
        CgsGui::GuiAccessPointers* GetGuiAccessPointers() { return &mGuiAccessPointers; }
        MovieManager* GetMovieManager() { return &mMovieManager; }
        ViewModule* GetViewModule() { return &mViewModule; }
        BrnScreenFlow* GetScreenFlow() { return &mScreenFlow; }
        AlwaysAvailableComponentsManager* GetAlwaysAvailableComponentsManager()
        { return &mAlwaysAvailableComponentsManager; }
        ProfileManager& GetProfileManager() { return mProfileHost.GetProfileManager(); }
        const ProfileManager& GetProfileManager() const { return mProfileHost.mProfileManager; }
        const WorldDataController& GetWorldDataController() const { return mWorldDataController; }
        GuiTracker* GetGuiTracker() { return &mGuiTracker; }
        bool IsPerformingInvite() const { return mbPerformingInvite; }
        bool IsLoginNeededForInvite() const { return mbInviteRequiresLogin; }
        void GenerateEffectFrameEvents(RendererIO::OutputBuffer* lpOutput)
        { mEffectsArbitrator.GenerateEffectFrameEvents(lpOutput); }

    private:
        friend struct SatNavDebugComponent;
        friend struct GuiDebugComponent;
        friend struct GuiCacheDebugComponent;
        static void GameTalkMsgHandler(EA::GameTalk::GameTalkMessage* lpMessage);
        void PopulateNativeGuiConfig(const BrnResource::GameDataIO::OutputBuffer* lpOutput);
        bool EnsureNativeGuiResources();
        void PrepareNativeDebugComponents();
        void PrepareNativeSystemNotifications();
        void UpdateNativeSystemNotifications(CgsGui::ModelIO::InputBuffer* lpInput);
        void ReleaseNativeSystemNotifications();
        void PublishNativeMovieResourceRequest(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput);
        void UpdateCollisionWorldValidation(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput);
        const CgsModule::Event* ReadCollisionWorldResponse(s32 liExpectedType);
        void UpdateCarPoolValidation(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput);
        bool PendingCollisionWorldInvalidateForNetworking()
        {
            const bool lbPending = mbCollisionWorldInvalidateForNetworking;
            mbCollisionWorldInvalidateForNetworking = false;
            return lbPending;
        }
        bool PendingCollisionWorldValidateForNetworking()
        {
            const bool lbPending = mbCollisionWorldValidateForNetworking;
            mbCollisionWorldValidateForNetworking = false;
            return lbPending;
        }
        void AutosaveProfile(const CgsGui::ImageFileInfo* lpImageFileInfo);
        void UpdatePlayerName(CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpViewStateQueue);
        void HandleControllerPressed(const CgsModule::Event* lpEvent);
        void PerformInvite(CgsGui::CgsGuiModuleIO::InputBuffer* lpInput,
            CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput);
        void HandleEventsPostBaseModuleUpdate(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput);

        enum ECollisionWorldStateValidationState
        {
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_BOOT,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_BOOT,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_BOOT,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_SAVELOAD,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_SAVELOAD,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_SAVELOAD,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_NETWORKING,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_NETWORKING,
            E_COLLISIONWORLDSTATEVALIDATIONSTATE_COUNT
        };
        enum ECarPoolValidationState
        {
            E_CARPOOLSTATEVALIDATIONSTATE_IDLE,
            E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_EMPTY,
            E_CARPOOLSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE,
            E_CARPOOLSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE,
            E_CARPOOLSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE,
            E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_FILL,
            E_CARPOOLSTATEVALIDATIONSTATE_COUNT
        };

        // DecFIGS declaration order, with ARTIST's second Prepare word,
        // notification handle and replay serialiser additions.
        ECollisionWorldStateValidationState meCollisionWorldStateValidationState;
        bool mbCollisionWorldInvalid;
        bool mbBootMovieMode;
        ECarPoolValidationState meCarPoolStateValidationState;
        CgsGui::GuiAccessPointers::GuiEventReceiverQueue mEventReceiverQueue;
        CgsGui::GuiAccessPointers::GuiEventReceiverQueue mCollisonPoolValidationReceiverQueue;
        ViewModule mViewModule;
        MovieManager mMovieManager;
        ColourCalibrationScreen mColourCalibrationScreen;
        WorldDataController mWorldDataController;
        BurnoutSkillsManager mBurnoutSkillsManager;
        FreeburnChallengeManager mFreeburnChallengeManager;
        GuiPrepareStage mePrepareStage;
        u32 muPrepare2State;
        GuiReleaseStage meReleaseStage;
        CgsGui::GuiModuleConfig mGuiConfig;
        CustomRendererManager mCustomRendererManager;
        f32 mfAspectRatio;
        s32 miNumWaitingFlows;
        s32 miNumHandledFlows;
        BrnHudFlow mHudFlow;
        BrnScreenFlow mScreenFlow;
        BrnOverlayFlow mOverlayFlow;
        GuiFsmController mFsmController;
        HudMessageDirector mHudMessageDirector;
        HudMessageAnalyzer mHudMessageAnalyzer;
        GuiOverlaysDirector mOverlaysDirector;
        KeyboardHost mKeyboardHost;
        ProfileHost mProfileHost;
        CgsGui::SystemUserProfile mSystemUserProfile;
        CgsGui::CgsGuiModuleIO::OutputBuffer* mpOutputBuffer;
        GuiDebugComponent mDebugComponent;
        GuiHudMessagesDebugComponent mHudMessageDebugComponent;
        SatNavDebugComponent mSatNavDebugComponent;
        GuiCacheDebugComponent mGuiCacheDebugComponent;
        CgsGui::GuiAccessPointers mGuiAccessPointers;
        GuiCache mGuiCache;
        MapIconManager mMapIconManager;
        GuiTracker mGuiTracker;
        BrnGuiKeyboard mGuiKeyboard;
        EffectsArbitrator mEffectsArbitrator;
        AlwaysAvailableComponentsManager mAlwaysAvailableComponentsManager;
        bool mbPerformingInvite;
        bool mbInviteRequiresLogin;
        bool mbStartLobbyDueToPlayerJoin;
        bool mbCollisionWorldInvalidateForNetworking;
        bool mbCollisionWorldValidateForNetworking;
        bool mbTrafficPoolEmptied;
        f32 mfLastProfileAutosaveTime;
        bool mbProfileAutosavePending;
        bool mbHasSeenNeedUpdatePopup;
        CgsGui::GuiAccessPointers::GuiEventReceiverQueue mGDMReceiverQueue;
        void* mhSysListenerHandle;
        BrnReplays::GuiModuleSerialiser mGuiModuleSerialiser;
    };
}

#endif
