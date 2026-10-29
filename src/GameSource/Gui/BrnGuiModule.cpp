#include "GameSource/Gui/BrnGuiPerfmons.h"
#include "GameSource/Gui/BrnGuiModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // original movie acquire request
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h" // collision-world event 493
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h" // collision-world swap requests/replies
#include "GameSource/Resource/BrnResourceAllocator.h" // console-memory markers
#include "SDKs/EA/GameTalk/GameTalk.h"
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"   // [H3b] MapTransform (the sat-nav view-rect install)

#include <cstdio>                                                         // std::snprintf (log formatting)
#include <xtl.h>                                                      // [gate] GetEnvironmentVariableA (BRN_FLAPT_AFTER_DISPATCH)
#include <cstddef>  // offsetof for the native event-22 payload
#include <cstring>  // std::strcmp (ARTIST GUI-audio action-name table)

#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h" // CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"// CgsResource::ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"    // E_RESOURCETYPE_LUACODE / E_MEMTYPE_*
#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"      // CgsResource::LuaCodeResource
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> (channel command records)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::GuiEventPlayAptMovie (channel-41 payload)
#include "GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.h"   // priority removal/blocking event ids
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::GuiEventLoadNotification / GuiEventLoadRequest
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // BrnGui::GuiAudioTriggerEvent
#include "GameSource/GameState/Progression/BrnProfile.h"                   // BrnProgression::Profile::Construct (the PC progression block seed)
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h"        // AlwaysAvailableComponentsManager + free accessor (bodied below)
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers (flow interface wiring)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"       // CgsGui::AptAuxPointer (the AptAux singleton)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // CgsGui::AptCommunicator (the per-frame trigger publish)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"              // CgsSound::Playback::Name::MakeHash (event-155 keys)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                // FLApt live-instance allocator
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "rw/rwcore_general_alloc.h"                                   // actual public allocator derivation
#include "GameSource/Resource/BrnGameDataModuleIO.h"                      // BrnResource::GameDataIO::Input/OutputBuffer (the colour-calibration screen's IO)
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"        // AllocatorList::GetRWLinearResourceAllocator (mGuiConfig.mpTextureAllocator)
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"      // BrnNetwork::LiveRevengeProfile (the PC live-revenge block)

extern "C" void* XNotifyCreateListener(unsigned long long luAreas);
extern "C" int XNotifyGetNext(void* lhListener, unsigned long luFilter,
    unsigned long* lpuId, unsigned long* lpuParam);

struct RGBA
{
    u32 mPacked;
};

extern bool gbLogGuiAudioTriggers; // X360 byte_82FB5098; defined by CgsAptCommunicator.cpp.

namespace
{
    struct GuiEventSoundTriggerPayload22
    {
        char macTypeName[32];
        char macActionName[32];
        char macLabel[32];
        s32 miLayer;
    };
    CGS_STATIC_ASSERT((sizeof(GuiEventSoundTriggerPayload22) == 100 &&
                  offsetof(GuiEventSoundTriggerPayload22, miLayer) == 96),
                  "ARTIST event-22 payload reads its layer at byte 96");

    char gacGuiToolHookName[32] = {};
    bool gbGuiToolStartHookRequested = false;
    bool gbGuiToolStopHookRequested = false;
    bool gbGuiForceSystemUIOpen = false;
    bool gbGuiForceSystemUIClosed = false;

    void CopyGuiToolHookName(const char* lpcContent, s32 liOriginalLine)
    {
        if (!lpcContent)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpcContent != NULL",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", liOriginalLine);
            CgsDev::Assert::EndAssert();
        }
        if (std::strlen(lpcContent) >= 32)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStream << "String too long: " << (lpcContent ? lpcContent : "<NULLSTRING>");
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 55);
            CgsDev::Assert::EndAssert();
        }
        std::strncpy(gacGuiToolHookName, lpcContent, 32);
    }

}

#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "rw/rwcore_general_alloc.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"

namespace BrnGui
{

    const RGBA KA_ALTERNATE_TEXT_COLOURS[8] =
    {
        { 0xFF000000u },
        { 0xFF00CCFFu },
        { 0xFFFFFFFFu },
        { 0xFF2864B7u },
        { 0xFFA68C4Au },
        { 0xFF0F0F9Cu },
        { 0xFF6B8A57u },
        { 0xFF33B6E6u },
    };

    void GuiModule::Construct(const BrnResource::HudMessageController* lpHudMessageController,
                              const BrnResource::PopupController* lpPopupController,
                              f32 lfAspectRatio, bool lbHighDef)
    {
        GuiPerfmons().Initialise();

        CGS_ASSERT(lpHudMessageController != 0, "lpHudMessageController");   // BrnGuiModule.cpp:229
        CGS_ASSERT(lpPopupController != 0, "lpPopupController");

        mViewModule.Construct(this, NULL, 7, lfAspectRatio,
                              KA_ALTERNATE_TEXT_COLOURS, 8);
        CgsGui::GuiModule::Construct(&mViewModule, lbHighDef);
        mViewModule.GetFlaptManager()->SetSoundTriggerHandler(
            &GuiModule::FlaptSoundTriggerCallback, this);
        mfAspectRatio = lfAspectRatio;
        mEffectsArbitrator.Construct();
        mMovieManager.Construct();
        mbBootMovieMode = true;
        mePrepareStage = E_PREPARE_START;
        muPrepare2State = 0;
        meReleaseStage = E_RELEASE_DONE;
        mColourCalibrationScreen.Construct();
        meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
        mbCollisionWorldInvalid = false;
        meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_IDLE;
        mbCollisionWorldInvalidateForNetworking = false;
        mbCollisionWorldValidateForNetworking = false;

        mfLastProfileAutosaveTime = 0.0f;
        mbProfileAutosavePending  = false;
        mbPerformingInvite = false;
        mbInviteRequiresLogin = false;
        mbStartLobbyDueToPlayerJoin = false;
        mbTrafficPoolEmptied = false;
        mbHasSeenNeedUpdatePopup = false;

        mAlwaysAvailableComponentsManager.Construct();

        mEventReceiverQueue.Construct();
        mCollisonPoolValidationReceiverQueue.Construct();
        mGDMReceiverQueue.Construct();
        gbGuiToolStartHookRequested = false; // ARTIST82518C38..82518C44
        mhSysListenerHandle = 0; // ARTIST82518C48, before serialiser Construct
        mGuiModuleSerialiser.Construct(); // ARTIST82518C4C, before manager Construct
        mCustomRendererManager.Construct();

        mCustomRendererManager.SetFlaptRenderer(&mViewModule.GetFlaptManager()->mRenderer);

        mpOutputBuffer = 0;

        mGuiTracker.Construct();

        mGuiCache.Construct(&mGuiTracker, &mSystemUserProfile);

        mWorldDataController.Construct();
        mGuiCache.SetWorldDataController(&mWorldDataController);

        mMapIconManager.Construct(&mGuiCache);
        mGuiCache.SetMapIconManager(&mMapIconManager);
        mGuiCache.SetIsHighDefinition(lbHighDef);

        MapTransform::SetSatNavRect(lbHighDef ? MapTransform::GetSatNavViewRectHD()
                                              : MapTransform::GetSatNavViewRectSD());
        SetMaskAspectCorrectionMatrix(&mGuiCache);

        mBurnoutSkillsManager.Construct(&mGuiCache);
        mGuiCache.SetSkillsManager(&mBurnoutSkillsManager);
        mFreeburnChallengeManager.Construct(&mGuiCache);
        mGuiCache.SetChallengeManager(&mFreeburnChallengeManager);

        mGuiAccessPointers.Construct();
        mGuiAccessPointers.mpLanguageManager = mViewModule.GetLanguageManager();
        mGuiAccessPointers.SetFlaptManager(mViewModule.GetFlaptManager());
        mGuiAccessPointers.SetGuiCache(&mGuiCache);
        mGuiAccessPointers.SetGDMReceiverQueue(&mGDMReceiverQueue);
        mGuiAccessPointers.SetSerialiser(&mGuiModuleSerialiser);

        mSystemUserProfile.Construct();
        mProfileHost.GetProfileManager().Construct(mGuiCache, mSystemUserProfile,
                                  mViewModule.GetLanguageManager());
        mGuiKeyboard.Construct();
        mKeyboardHost.Construct();
        mScreenFlow.Construct(&mGuiCache);
        mHudFlow.Construct(&mGuiCache);
        mOverlayFlow.Construct(&mGuiCache);
        mGuiModel.AddGuiEventObserver(&mHudFlow, 0);
        mGuiModel.AddGuiEventObserver(&mScreenFlow, 0);
        mGuiModel.AddGuiEventObserver(&mOverlayFlow, 0);
        mGuiModel.AddGuiEventObserver(&mAlwaysAvailableComponentsManager, 0);
        mFsmController.Construct();

        mHudMessageDirector.Construct(&mGuiModel, &mGuiCache);
        mHudMessageAnalyzer.Construct(&mHudMessageDirector);
        mOverlaysDirector.Construct(&mGuiModel);

        mDebugComponent.Construct(this);
        mHudMessageDebugComponent.Construct(&mHudMessageDirector);
        mSatNavDebugComponent.Construct(this);
        mGuiCacheDebugComponent.Construct(this);

        CGS_ASSERT(lpHudMessageController != 0,
                   "lpHudMessageController (GuiCache::mpHudMessageController would stay null "
                   "and every HUD message would die at FilterAndSendOffMessage's mpController "
                   "assert)");
        mGuiCache.SetHudMessageController(lpHudMessageController);

        mGuiCache.SetHudMessageDirector(&mHudMessageDirector);
        mGuiCache.SetPopupController(lpPopupController);

        {
            GuiEventCachePointer lCachePointerEvent;
            lCachePointerEvent.mpCachePointer = &mGuiCache;
            mGuiTracker.RecEvent(
                reinterpret_cast<const CgsModule::Event*>(&lCachePointerEvent), 64,
                static_cast<s32>(sizeof(lCachePointerEvent)));
        }

    }

    void GuiModule::PopulateNativeGuiConfig(
        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput)
    {
        if (!lpGameDataOutput->GetAllocatorList())
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStream << "Invalid allocator list passed into BrnGui::GuiModule::Prepare";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 367);
            CgsDev::Assert::EndAssert();
        }
        mGuiConfig.miAptPersistentBunldePoolId = 8;
        mGuiConfig.miAptStreamedbundlePoolId = 9;
        mGuiConfig.miFontBundlePoolId = 0;
        mGuiConfig.miGlobalTexturePoolId = 10;
        mGuiConfig.miFSMBunldePoolId = 11;
        mGuiConfig.miLanguageBundlePoolId = 11;
        mGuiConfig.miTexturesBundlePoolId = 12;
        mGuiConfig.mpAptAllocator = lpGameDataOutput->GetAllocatorList()->GetHeapAllocator(32);
        mGuiConfig.mpFSMAllocator = lpGameDataOutput->GetAllocatorList()->GetHeapAllocator(38);
        mGuiConfig.mpRendererAllocator =
            lpGameDataOutput->GetAllocatorList()->GetRWGeneralResourceAllocator(31);
        mGuiConfig.mpLanguageAllocator = lpGameDataOutput->GetAllocatorList()->GetHeapAllocator(38);
        mGuiConfig.mpGuiStateAllocator = lpGameDataOutput->GetAllocatorList()->GetLinearAllocator(44);
        mGuiConfig.mpTextureAllocator =
            lpGameDataOutput->GetAllocatorList()->GetRWLinearResourceAllocator(42);
        mGuiConfig.mpFlaptAllocator = lpGameDataOutput->GetAllocatorList()->GetLinearAllocator(66);
        mGuiConfig.mpGuiStateAllocator->SetAlignment(16);
        mGuiConfig.mpFlaptAllocator->SetAlignment(16);
    }

    bool GuiModule::EnsureNativeGuiResources()
    {
        static const CgsGui::sResourceTuple maEuropeanFontResourcesToLoad[3] =
        {
            { 17u, static_cast<CgsGui::ResourceRequestTypes>(16) },
            { 18u, static_cast<CgsGui::ResourceRequestTypes>(16) },
            { 19u, static_cast<CgsGui::ResourceRequestTypes>(16) },
        };
        static const CgsGui::sResourceTuple maJapaneseFontResourcesToLoad[2] =
        {
            { 20u, static_cast<CgsGui::ResourceRequestTypes>(16) },
            { 21u, static_cast<CgsGui::ResourceRequestTypes>(16) },
        };
        static const CgsGui::sResourceTuple maBasicResourcesToLoad[2] =
        {
            { 125u, static_cast<CgsGui::ResourceRequestTypes>(7) },
            { 196u, static_cast<CgsGui::ResourceRequestTypes>(10) },
        };
        const CgsGui::sResourceTuple* lpFonts;
        u32 luFontCount;
        if (mGuiCache.GetLanguage() == CgsLanguage::E_LANGUAGE_JAPANESE)
        {
            lpFonts = maJapaneseFontResourcesToLoad;
            luFontCount = 2;
        }
        else
        {
            lpFonts = maEuropeanFontResourcesToLoad;
            luFontCount = 3;
        }
        if (!mGuiCache.EnsureResourcesAreLoaded(lpFonts, luFontCount))
            return false;
        return mGuiCache.EnsureResourcesAreLoaded(maBasicResourcesToLoad, 2);
    }

    void GuiModule::GameTalkMsgHandler(EA::GameTalk::GameTalkMessage* lpMessage)
    {
        const char* lpcKey = lpMessage->GetKey(0);
        if (_strnicmp(lpcKey, "StartHook", 9) == 0)
        {
            CopyGuiToolHookName(lpMessage->GetKeyContent("StartHook"), 3554);
            gbGuiToolStartHookRequested = true;
        }
        else if (_strnicmp(lpcKey, "StopHook", 8) == 0)
        {
            CopyGuiToolHookName(lpMessage->GetKeyContent("StopHook"), 3564);
            gbGuiToolStopHookRequested = true;
        }
        else if (_strnicmp(lpcKey, "StopMenuHook", 12) == 0)
        {
            std::strncpy(gacGuiToolHookName, "", 32);
            gbGuiToolStopHookRequested = true;
        }
    }

    bool GuiModule::Prepare(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput,
                            BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
                            const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput)
    {
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
        {
            EA::GameTalk::GameTalkManager::RegisterMessageHandler(
                EA::GameTalk::GameTalkManager::GetInstance(), GameTalkMsgHandler, "Tool.GUI");
            GuiReplayRegisterSerialiser lRegistration;
            lRegistration.mpSerialiser = &mGuiModuleSerialiser;
            lpOutput->LockForWrite();
            lpOutput->AddGuiOutEvent(lRegistration);
            lpOutput->UnlockForWrite();
        }
        case E_PREPARE_MEMORY:
            mePrepareStage = E_PREPARE_MEMORY;
            meReleaseStage = E_RELEASE_MEMORY;
            PopulateNativeGuiConfig(lpGameDataOutput);
        case E_PREPARE_PARENT:
            mePrepareStage = E_PREPARE_PARENT;
            meReleaseStage = E_RELEASE_PARENT;
            if (!CgsGui::GuiModule::Prepare(mGuiConfig))
                return false;
        case E_PREPARE_HUD:
            mePrepareStage = E_PREPARE_HUD;
            meReleaseStage = E_RELEASE_HUD;
            if (!mHudFlow.Prepare(&mGuiAccessPointers, mGuiConfig.mpRendererAllocator,
                mGuiConfig.mpGuiStateAllocator, &mProfileHost.GetProfileManager()))
                return false;
            mHudMessageAnalyzer.SetAccessPointers(&mGuiAccessPointers);
        case E_PREPARE_SCREEN:
            mePrepareStage = E_PREPARE_SCREEN;
            meReleaseStage = E_RELEASE_SCREEN;
            if (!mScreenFlow.Prepare(&mGuiAccessPointers, mGuiConfig.mpRendererAllocator,
                mGuiConfig.mpGuiStateAllocator, mProfileHost.GetProfileManager()))
                return false;
            mScreenFlow.SetStreamingMode(BrnBaseFlow::E_STREAMING_CURRENT);
        case E_PREPARE_OVERLAY:
            mePrepareStage = E_PREPARE_OVERLAY;
            meReleaseStage = E_RELEASE_OVERLAY;
            if (!mOverlayFlow.Prepare(&mGuiAccessPointers, mGuiConfig.mpRendererAllocator,
                mGuiConfig.mpGuiStateAllocator))
                return false;
        case E_PREPARE_FSM_CONTROLLER:
            mePrepareStage = E_PREPARE_FSM_CONTROLLER;
            meReleaseStage = E_RELEASE_FSM_CONTROLLER;
            mFsmController.Prepare(&mGuiModel, mGuiConfig.mpFSMAllocator);
        case E_PREPARE_CUSTOMRENDERER_PREPARE:
        {
            mePrepareStage = E_PREPARE_CUSTOMRENDERER_PREPARE;
            meReleaseStage = E_RELEASE_CUSTOMRENDERER_RELEASE;
            const bool lbPrepared = mCustomRendererManager.Prepare(
                mGuiConfig.mpRendererAllocator, mGuiConfig.mpTextureAllocator);
            mViewModule.SetCustomRendererManager(&mCustomRendererManager, 10, &mGuiModuleSerialiser);
            if (!lbPrepared)
                return false;
        }
        case E_PREPARE_MOVIE_MANAGER:
            mePrepareStage = E_PREPARE_MOVIE_MANAGER;
            meReleaseStage = E_RELEASE_MOVIE_MANAGER;
            if (!mMovieManager.Prepare(lpGameDataInput, lpGameDataOutput, mSku.GetLanguage()))
                return false;
        case E_PREPARE_COLOUR_CALIBRATION_SCREEN:
            mePrepareStage = E_PREPARE_COLOUR_CALIBRATION_SCREEN;
            meReleaseStage = E_RELEASE_COLOUR_CALIBRATION_SCREEN;
        case E_PREPARE_GUI_DEBUG_COMPONENT:
            mePrepareStage = E_PREPARE_GUI_DEBUG_COMPONENT;
            meReleaseStage = E_RELEASE_GUI_DEBUG_COMPONENT;
            PrepareNativeDebugComponents();
            CGS_ASSERT(mSystemUserProfile.Prepare(), "mSystemUserProfile.Prepare()");
            CGS_ASSERT(mGuiKeyboard.Prepare(&mSystemUserProfile), "mGuiKeyboard.Prepare(&mSystemUserProfile )");
            CGS_ASSERT(mKeyboardHost.Prepare(&mGuiKeyboard), "mKeyboardHost.Prepare(&mGuiKeyboard)");
            mbCollisionWorldInvalidateForNetworking = false;
            mbCollisionWorldValidateForNetworking = false;
        case E_PREPARE_PROFILE:
            mePrepareStage = E_PREPARE_PROFILE;
            meReleaseStage = E_RELEASE_PROFILE;
            if (!mProfileHost.GetProfileManager().Prepare(mGuiConfig.mpFSMAllocator,
                lpGameDataOutput->GetAllocatorList()->GetLinearAllocator(65)))
                return false;
        case E_PREPARE_SYSTEM_NOTIFICATIONS:
            mePrepareStage = E_PREPARE_SYSTEM_NOTIFICATIONS;
            meReleaseStage = E_RELEASE_SYSTEM_NOTIFICATIONS;
            PrepareNativeSystemNotifications();
        case E_PREPARE_WF_RESOURCES:
            mePrepareStage = E_PREPARE_WF_RESOURCES;
            if (!EnsureNativeGuiResources())
                return false;
        case E_PREPARE_WORLD_DATA_CONTROLLER:
            mePrepareStage = E_PREPARE_WORLD_DATA_CONTROLLER;
            if (!mWorldDataController.Prepare(lpGameDataInput))
                return false;
        case E_PREPARE_DONE:
            mePrepareStage = E_PREPARE_DONE;
            meReleaseStage = E_RELEASE_START;
            return true;
        default:
            CGS_ASSERT(false, "Invalid prepare state");
            return false;
        }
    }

    bool GuiModule::Prepare2(BrnResource::GameDataIO::InputBuffer* lpGameDataInput)
    {
        if (muPrepare2State < 2)
        {
            muPrepare2State = 1;
            if (!mWorldDataController.Prepare2(lpGameDataInput))
                return false;
        }
        else if (muPrepare2State >= 3)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Invalid prepare state", "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 658);
            CgsDev::Assert::EndAssert();
            return false;
        }
        muPrepare2State = 2;
        return true;
    }

    void GuiModule::PrepareNativeSystemNotifications()
    {
        mhSysListenerHandle = XNotifyCreateListener(1);
        if (!mhSysListenerHandle)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lStream << "Failed to create an X360 system listener";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 557);
            CgsDev::Assert::EndAssert();
        }
    }

    void GuiModule::UpdateNativeSystemNotifications(
        CgsGui::ModelIO::InputBuffer* lpInputBuffer)
    {
        unsigned long luNotificationId;
        unsigned long luNotificationParam;
        if (mhSysListenerHandle &&
            XNotifyGetNext(mhSysListenerHandle, 9, &luNotificationId, &luNotificationParam))
        {
            if (luNotificationId == 0x02000002)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::StrStream lStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
                lStream << "Invite notification processed by GuiModule. This could break cross game invites\n";
                CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 2918);
                CgsDev::Assert::EndAssert();
            }
            const u32 luUiVisible = luNotificationParam != 1;
            if (!lpInputBuffer)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpInputBuffer",
                    "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 238);
                CgsDev::Assert::EndAssert();
            }
            lpInputBuffer->LockForWrite();
            lpInputBuffer->GetEventQueueNonConst()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&luUiVisible), 516, sizeof(luUiVisible));
            if (!lpInputBuffer)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpInputBuffer",
                    "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 248);
                CgsDev::Assert::EndAssert();
            }
            lpInputBuffer->UnlockForWrite();
        }
    }

    void GuiModule::ReleaseNativeSystemNotifications()
    {
        if (mhSysListenerHandle)
        {
            if (!CloseHandle(mhSysListenerHandle))
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CloseHandle( mhSysListenerHandle )",
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 681);
                CgsDev::Assert::EndAssert();
            }
            mhSysListenerHandle = 0;
        }
    }

    bool GuiModule::Release()
    {
        ReleaseNativeSystemNotifications();
        mbCollisionWorldInvalidateForNetworking = false;
        mbCollisionWorldValidateForNetworking = false;
        switch (meReleaseStage)
        {
        case E_RELEASE_START:
            meReleaseStage = E_RELEASE_START;
        case E_RELEASE_SYSTEM_NOTIFICATIONS:
            meReleaseStage = E_RELEASE_SYSTEM_NOTIFICATIONS;
        case E_RELEASE_PROFILE:
            meReleaseStage = E_RELEASE_PROFILE;
            CGS_ASSERT(mProfileHost.Release(), "mProfileHost.Release()");
        case E_RELEASE_GUI_DEBUG_COMPONENT:
            meReleaseStage = E_RELEASE_GUI_DEBUG_COMPONENT;
            mEventReceiverQueue.Clear();
            CGS_ASSERT(mSystemUserProfile.Release(), "mSystemUserProfile.Release()");
            mGuiKeyboard.Release();
            CGS_ASSERT(mKeyboardHost.Release(), "mKeyboardHost.Release()");
        case E_RELEASE_WORLD_DATA_CONTROLLER:
            meReleaseStage = E_RELEASE_WORLD_DATA_CONTROLLER;
            mWorldDataController.Release();
        case E_RELEASE_COLOUR_CALIBRATION_SCREEN:
            meReleaseStage = E_RELEASE_COLOUR_CALIBRATION_SCREEN;
        case E_RELEASE_MOVIE_MANAGER:
            meReleaseStage = E_RELEASE_MOVIE_MANAGER;
        case E_RELEASE_CUSTOMRENDERER_RELEASE:
            meReleaseStage = E_RELEASE_CUSTOMRENDERER_RELEASE;
            CGS_ASSERT(mCustomRendererManager.Release(), "mCustomRenderManager.Release()");
        case E_RELEASE_CUSTOMRENDERER_DESTRUCT:
            meReleaseStage = E_RELEASE_CUSTOMRENDERER_DESTRUCT;
            mCustomRendererManager.Destruct();
        case E_RELEASE_FSM_CONTROLLER:
            meReleaseStage = E_RELEASE_FSM_CONTROLLER;
        case E_RELEASE_OVERLAY:
            meReleaseStage = E_RELEASE_OVERLAY;
            if (!mOverlayFlow.Release())
                return false;
        case E_RELEASE_SCREEN:
            meReleaseStage = E_RELEASE_SCREEN;
            if (!mScreenFlow.Release())
                return false;
        case E_RELEASE_HUD:
            meReleaseStage = E_RELEASE_HUD;
            if (!mHudFlow.Release())
                return false;
        case E_RELEASE_PARENT:
            meReleaseStage = E_RELEASE_PARENT;
            if (!CgsGui::GuiModule::Release())
                return false;
        case E_RELEASE_MEMORY:
            meReleaseStage = E_RELEASE_MEMORY;
        case E_RELEASE_DONE:
            meReleaseStage = E_RELEASE_DONE;
            return true;
        default:
            return false;
        }
    }

    void GuiModule::PublishNativeMovieResourceRequest(CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput)
    {
        const CgsResource::ID lResourceId = mMovieManager.PendingVideoDataResourceRequest();
        if (lResourceId.GetHash() == 0)
            return;

        CgsResource::Events::AcquireResourceRequest lRequest;
        lRequest.mpUser = mMovieManager.GetRecieverQueue();
        lRequest.miEventId = MovieManager::KI_VIDEO_DATA_RESOURCE_EVENT_ID;
        lRequest.miPoolId = 11;
        lRequest.mResourceId = lResourceId;
        lpGuiOutput->LockForWrite();
        lpGuiOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 4);
        lpGuiOutput->UnlockForWrite();
    }

    void GuiModule::PreWorldUpdate(CgsModule::IOBufferStack* lpInputBufferStack,
                                   CgsModule::IOBufferStack* lpOutputBufferStack,
                                   CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
                                   CgsGui::ModelIO::OutputBuffer* lpModelOutputBuffer)
    {
        CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miGuiModulePreWorldUpdate);
        CGS_ASSERT(lpGuiOutput != 0, "lpGuiOutput != NULL");
        CGS_ASSERT(lpModelOutputBuffer != 0, "lpModelOutputBuffer != NULL");
        CGS_ASSERT(lpInputBufferStack != 0, "lpInputBufferStack != NULL");
        CGS_ASSERT(lpOutputBufferStack != 0, "lpOutputBufferStack != NULL");

        lpGuiOutput->LockForWrite();
        UpdateCollisionWorldValidation(lpGuiOutput);
        UpdateCarPoolValidation(lpGuiOutput);
        mProfileHost.GetProfileManager().LoadProfileUpgradeData(lpGuiOutput, &mEventReceiverQueue);
        lpGuiOutput->UnlockForWrite();

        PublishNativeMovieResourceRequest(lpGuiOutput);
        CgsGui::GuiModule::PreWorldUpdate(lpInputBufferStack, lpOutputBufferStack,
                                          lpGuiOutput, lpModelOutputBuffer);
        CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miGuiModulePreWorldUpdate);
    }

    const CgsModule::Event* GuiModule::ReadCollisionWorldResponse(s32 liExpectedType)
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        const s32 liType = mCollisonPoolValidationReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
        CGS_ASSERT(mCollisonPoolValidationReceiverQueue.GetLength() == 1,
            "mCollisonPoolValidationReceiverQueue.GetLength() == 1");
        if (liExpectedType == 67)
            CGS_ASSERT(liType == 67, "liEventId == BrnResource::GameDataIO::EVENT_SWAP_OUT_COLLISION_WORLD");
        else
            CGS_ASSERT(liType == 68, "liEventId == BrnResource::GameDataIO::EVENT_SWAP_IN_COLLISION_WORLD");
        return lpEvent;
    }

    void GuiModule::UpdateCollisionWorldValidation(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        switch (meCollisionWorldStateValidationState)
        {
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE:
        {
            const bool lbMovie = mMovieManager.PendingCollisionWorldInvalidate();
            const bool lbNetworking = PendingCollisionWorldInvalidateForNetworking();
            if (mbBootMovieMode)
            {
                const bool lbProfile = mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate();
                if (lbMovie || lbProfile)
                {
                    BrnResource::PrintConsoleMemory(0);
                    mCollisonPoolValidationReceiverQueue.Clear();
                    {
                    BrnResource::GameDataIO::SwapOutCollisionWorldRequest lRequest;
                    lRequest.miEventId = 0;
                    lRequest.mpReceiverQueue = &mCollisonPoolValidationReceiverQueue;
                    lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 67);
                }
                    meCollisionWorldStateValidationState = lbMovie
                        ? E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_BOOT
                        : E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_SAVELOAD;
                }
            }
            else if (lbMovie || lbNetworking)
            {
                CGS_ASSERT(!mbCollisionWorldInvalid, "!mbCollisionWorldInvalid");
                CGS_ASSERT((lbMovie && !lbNetworking) || (!lbMovie && lbNetworking),
                    "( lbMoviePendingInvalidate && !lbNetworkingPendingInvalidate) || (!lbMoviePendingInvalidate && lbNetworkingPendingInvalidate)");
                {
                    GuiEventRequestCollisionWorldEvent lRequest;
                    lRequest.meEventType = GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_INVALIDATE;
                    lpOutput->AddGuiOutEvent(lRequest);
                }
                meCollisionWorldStateValidationState = lbMovie
                    ? E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE
                    : E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_NETWORKING;
            }
            break;
        }
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_BOOT:
            {
                const bool lbPending = PendingCollisionWorldInvalidateForNetworking();
                CGS_ASSERT(!lbPending, "!PendingCollisionWorldInvalidateForNetworking()");
            }
            if (mCollisonPoolValidationReceiverQueue.GetLength() > 0)
            {
                const BrnResource::GameDataIO::SwapOutCollisionWorldResponse* lpResponse = static_cast<const BrnResource::GameDataIO::SwapOutCollisionWorldResponse*>(ReadCollisionWorldResponse(67));
                mMovieManager.FreeCollisionPoolMemory(lpResponse);
                BrnResource::PrintConsoleMemory(0);
                mbCollisionWorldInvalid = true;
                mMovieManager.SetCollisionWorldValid(false);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_BOOT;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_BOOT:
            {
                const bool lbPending = PendingCollisionWorldInvalidateForNetworking();
                CGS_ASSERT(!lbPending, "!PendingCollisionWorldInvalidateForNetworking()");
            }
            CGS_ASSERT(mbCollisionWorldInvalid, "mbCollisionWorldInvalid");
            if (mMovieManager.PendingCollisionWorldValidate())
            {
                {
                    BrnResource::GameDataIO::SwapInCollisionWorldRequest lRequest;
                    lRequest.miEventId = 0;
                    lRequest.mpReceiverQueue = &mCollisonPoolValidationReceiverQueue;
                    lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 68);
                }
                mCollisonPoolValidationReceiverQueue.Clear();
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_BOOT;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_BOOT:
            {
                const bool lbPending = PendingCollisionWorldInvalidateForNetworking();
                CGS_ASSERT(!lbPending, "!PendingCollisionWorldInvalidateForNetworking()");
            }
            if (mCollisonPoolValidationReceiverQueue.GetLength() > 0)
            {
                ReadCollisionWorldResponse(68);
                BrnResource::PrintConsoleMemory(0);
                BrnResource::PrintConsoleMemory(0);
                mbCollisionWorldInvalid = false;
                mMovieManager.SetCollisionWorldValid(true);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_SAVELOAD:
            {
                const bool lbPending = mMovieManager.PendingCollisionWorldInvalidate();
                CGS_ASSERT(!lbPending, "!mMovieManager.PendingCollisionWorldInvalidate()");
            }
            {
                const bool lbPending = PendingCollisionWorldInvalidateForNetworking();
                CGS_ASSERT(!lbPending, "!PendingCollisionWorldInvalidateForNetworking()");
            }
            if (mCollisonPoolValidationReceiverQueue.GetLength() > 0)
            {
                ReadCollisionWorldResponse(67);
                BrnResource::PrintConsoleMemory(0);
                mbCollisionWorldInvalid = true;
                mProfileHost.GetProfileManager().SetCollisionWorldValid(false);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_SAVELOAD;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_SAVELOAD:
            {
                const bool lbPending = mMovieManager.PendingCollisionWorldInvalidate();
                CGS_ASSERT(!lbPending, "!mMovieManager.PendingCollisionWorldInvalidate()");
            }
            {
                const bool lbPending = PendingCollisionWorldInvalidateForNetworking();
                CGS_ASSERT(!lbPending, "!PendingCollisionWorldInvalidateForNetworking()");
            }
            CGS_ASSERT(mbCollisionWorldInvalid, "mbCollisionWorldInvalid");
            if (mProfileHost.GetProfileManager().PendingCollisionWorldValidate())
            {
                {
                    BrnResource::GameDataIO::SwapInCollisionWorldRequest lRequest;
                    lRequest.miEventId = 0;
                    lRequest.mpReceiverQueue = &mCollisonPoolValidationReceiverQueue;
                    lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 68);
                }
                mCollisonPoolValidationReceiverQueue.Clear();
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_SAVELOAD;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_SAVELOAD:
            {
                const bool lbPending = mMovieManager.PendingCollisionWorldInvalidate();
                CGS_ASSERT(!lbPending, "!mMovieManager.PendingCollisionWorldInvalidate()");
            }
            {
                const bool lbPending = PendingCollisionWorldInvalidateForNetworking();
                CGS_ASSERT(!lbPending, "!PendingCollisionWorldInvalidateForNetworking()");
            }
            if (mCollisonPoolValidationReceiverQueue.GetLength() > 0)
            {
                ReadCollisionWorldResponse(68);
                BrnResource::PrintConsoleMemory(0);
                mbCollisionWorldInvalid = false;
                mProfileHost.GetProfileManager().SetCollisionWorldValid(true);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE:
            {
                const bool lbPending = mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate();
                CGS_ASSERT(!lbPending, "!mProfileManager.PendingCollisionWorldInvalidate()");
            }
            {
                const bool lbPending = PendingCollisionWorldInvalidateForNetworking();
                CGS_ASSERT(!lbPending, "!PendingCollisionWorldInvalidateForNetworking()");
            }
            if (mbCollisionWorldInvalid)
            {
                mMovieManager.SetCollisionWorldValid(false);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE:
            {
                const bool lbPending = mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate();
                CGS_ASSERT(!lbPending, "!mProfileManager.PendingCollisionWorldInvalidate()");
            }
            {
                const bool lbPending = PendingCollisionWorldInvalidateForNetworking();
                CGS_ASSERT(!lbPending, "!PendingCollisionWorldInvalidateForNetworking()");
            }
            CGS_ASSERT(mbCollisionWorldInvalid, "mbCollisionWorldInvalid");
            if (mMovieManager.PendingCollisionWorldValidate())
            {
                {
                    GuiEventRequestCollisionWorldEvent lRequest;
                    lRequest.meEventType = GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_VALIDATE;
                    lpOutput->AddGuiOutEvent(lRequest);
                }
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE:
            {
                const bool lbPending = mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate();
                CGS_ASSERT(!lbPending, "!mProfileManager.PendingCollisionWorldInvalidate()");
            }
            {
                const bool lbPending = PendingCollisionWorldInvalidateForNetworking();
                CGS_ASSERT(!lbPending, "!PendingCollisionWorldInvalidateForNetworking()");
            }
            if (!mbCollisionWorldInvalid)
            {
                mMovieManager.SetCollisionWorldValid(true);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_NETWORKING:
            {
                const bool lbPending = mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate();
                CGS_ASSERT(!lbPending, "!mProfileManager.PendingCollisionWorldInvalidate()");
            }
            {
                const bool lbPending = mMovieManager.PendingCollisionWorldInvalidate();
                CGS_ASSERT(!lbPending, "!mMovieManager.PendingCollisionWorldInvalidate()");
            }
            if (mbCollisionWorldInvalid)
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING;
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING:
            {
                const bool lbPending = mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate();
                CGS_ASSERT(!lbPending, "!mProfileManager.PendingCollisionWorldInvalidate()");
            }
            {
                const bool lbPending = mMovieManager.PendingCollisionWorldInvalidate();
                CGS_ASSERT(!lbPending, "!mMovieManager.PendingCollisionWorldInvalidate()");
            }
            if (PendingCollisionWorldValidateForNetworking())
            {
                {
                    GuiEventRequestCollisionWorldEvent lRequest;
                    lRequest.meEventType = GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_VALIDATE;
                    lpOutput->AddGuiOutEvent(lRequest);
                }
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_NETWORKING;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_NETWORKING:
            {
                const bool lbPending = mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate();
                CGS_ASSERT(!lbPending, "!mProfileManager.PendingCollisionWorldInvalidate()");
            }
            {
                const bool lbPending = mMovieManager.PendingCollisionWorldInvalidate();
                CGS_ASSERT(!lbPending, "!mMovieManager.PendingCollisionWorldInvalidate()");
            }
            if (!mbCollisionWorldInvalid)
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
            break;
        default:
            CGS_ASSERT(false, "Unhandled state");
            break;
        }
    }

    void GuiModule::UpdateCarPoolValidation(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        switch (meCarPoolStateValidationState)
        {
        case E_CARPOOLSTATEVALIDATIONSTATE_IDLE:
            if (mMovieManager.PendingCarPoolInvalidate())
            {
                if (mbBootMovieMode)
                    mbTrafficPoolEmptied = true;
                else
                {
                    const bool lbEmpty = true;
                    lpOutput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lbEmpty), 13, sizeof(lbEmpty));
                    mbTrafficPoolEmptied = false;
                }
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_EMPTY;
            }
            break;
        case E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_EMPTY:
            if (mbTrafficPoolEmptied)
            {
                CgsResource::Events::InvalidatePoolRequest lRequest;
                lRequest.mpUser = &mEventReceiverQueue;
                lRequest.miEventId = BrnResource::E_POOL_TRAFFIC;
                lRequest.miPoolId = BrnResource::E_POOL_TRAFFIC;
                lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 7);
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE;
            }
            break;
        case E_CARPOOLSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE:
            if (mEventReceiverQueue.GetLength() > 0)
            {
                CGS_ASSERT(mEventReceiverQueue.GetLength() == 1, "mEventReceiverQueue.GetLength() == 1");
                const CgsModule::Event* lpEvent = 0;
                s32 liSize = 0;
                const s32 liEventId = mEventReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
                CGS_ASSERT(liEventId == 7, "liEventId == CgsResource::ResourceIO::EVENT_INVALIDATEPOOL");
                const CgsResource::Events::InvalidatePoolResponse* lpResponse = reinterpret_cast<const CgsResource::Events::InvalidatePoolResponse*>(lpEvent);
                CGS_ASSERT(lpResponse->miEventId == BrnResource::E_POOL_TRAFFIC,
                    "lpInvalidatePoolResponse->GetEventId() == BrnResource::E_POOL_TRAFFIC");
                mEventReceiverQueue.Clear(); // Original Clear preserves payload storage.
                if (lpResponse->GetResult() != CgsResource::Events::InvalidatePoolResponse::E_RESULT_SUCCESS)
                    meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_EMPTY;
                else
                {
                    mMovieManager.SetCarPoolValid(false, lpResponse->GetResource(), lpResponse->GetDescriptor());
                    meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE;
                }
            }
            break;
        case E_CARPOOLSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE:
            if (mMovieManager.PendingCarPoolValidate())
            {
                CgsResource::Events::ValidatePoolRequest lRequest;
                lRequest.mpUser = &mEventReceiverQueue;
                lRequest.miEventId = BrnResource::E_POOL_TRAFFIC;
                lRequest.miPoolId = BrnResource::E_POOL_TRAFFIC;
                lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 8);
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE;
            }
            break;
        case E_CARPOOLSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE:
            if (mEventReceiverQueue.GetLength() <= 0)
                break;
            {
                CGS_ASSERT(mEventReceiverQueue.GetLength() == 1, "mEventReceiverQueue.GetLength() == 1");
                const CgsModule::Event* lpEvent = 0;
                s32 liSize = 0;
                const s32 liEventId = mEventReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
                CGS_ASSERT(liEventId == 8, "liEventId == CgsResource::ResourceIO::EVENT_VALIDATEPOOL");
                const CgsResource::Events::ValidatePoolResponse* lpResponse = reinterpret_cast<const CgsResource::Events::ValidatePoolResponse*>(lpEvent);
                CGS_ASSERT(lpResponse->miEventId == BrnResource::E_POOL_TRAFFIC,
                    "lpValidatePoolResponse->GetEventId() == BrnResource::E_POOL_TRAFFIC");
                mEventReceiverQueue.Clear();
                if (mbBootMovieMode)
                    mbTrafficPoolEmptied = false;
                else
                {
                    const bool lbEmpty = false;
                    lpOutput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lbEmpty), 13, sizeof(lbEmpty));
                    CGS_ASSERT(mbTrafficPoolEmptied, "mbTrafficPoolEmptied");
                }
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_FILL;
            }
        case E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_FILL:
            if (!mbTrafficPoolEmptied)
            {
                CgsResource::SmallResource lResource;
                CgsResource::Entry::ResourceDescriptor lDescriptor;
                for (s32 liType = 0; liType < 3; ++liType)
                {
                    lResource.m_baseResources[liType] = 0;
                    lDescriptor.m_baseResourceDescriptors[liType].m_size = 0;
                    lDescriptor.m_baseResourceDescriptors[liType].m_alignment = 1;
                }
                mMovieManager.SetCarPoolValid(true, lResource, lDescriptor);
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_IDLE;
            }
            break;
        default:
            CGS_ASSERT(false, "Unhandled state");
            break;
        }
    }

    void GuiModule::PrepareNativeDebugComponents()
    {
        mDebugComponent.Register();
        mHudMessageDebugComponent.Register();
        mSatNavDebugComponent.Register();
        mGuiCacheDebugComponent.Register();
        mEventReceiverQueue.Clear();
    }

void GuiModule::Destruct()
    {
        mMovieManager.Destruct();
        mDebugComponent.Destruct();
        mHudMessageDebugComponent.Destruct();
        mSatNavDebugComponent.Destruct();
        mGuiCacheDebugComponent.Destruct();
        mKeyboardHost.Destruct();
        mGuiKeyboard.Destruct();
        mColourCalibrationScreen.Destruct();
        CgsGui::GuiModule::Destruct();
    }

    void GuiModule::FlaptSoundTriggerCallback(void* lpUserData,
        const char* lpcComponentName, const char* lpcSwfName,
        const char* lpcActionName, const char* lpcLabel)
    {
        CGS_ASSERT(lpUserData != NULL, "lpUserData");
        CGS_ASSERT(lpcComponentName != NULL, "lpcComponentName");
        CGS_ASSERT(lpcSwfName != NULL, "lpcSwfName");
        CGS_ASSERT(lpcActionName != NULL, "lpcActionName");
        CGS_ASSERT(lpcLabel != NULL, "lpcLabel");
        GuiModule* lpThis = static_cast<GuiModule*>(lpUserData);
        CGS_ASSERT(lpThis->mpOutputBuffer != NULL, "mpOutputBuffer");
        GuiAudioTriggerEvent lAudio;
        lAudio.Construct(lpcComponentName, lpcActionName, lpcLabel, lpcSwfName);
        lpThis->mpOutputBuffer->AddGuiOutEvent(lAudio);
    }

    namespace
    {
        const char* const KAPC_UPN_PLAYER_NAME_ID        = "PLAYER_NAME_STRING_ID";      // @0x8206E7DC
        const char* const KAPC_UPN_PLAYER_NAME_QUOTED_ID = "PLAYER_NAME_STRING_ID_Q";    // @0x8206E7C4
        const char* const KAPC_UPN_DEFAULT_NAME_ID       = "DEFAULTPLAYERNAME";          // off_82F278B4
        const char* const KAPC_UPN_DEFAULT_QUOTED_ID     = "DEFAULTPLAYERNAMEQUOTED";    // off_82F278B8
    }

    void GuiModule::UpdatePlayerName(CgsGui::GuiStackEventQueue::GuiEventQueueLarge*)
    {
        char lacName[28];
        char lacQuoted[32];
        CgsLanguage::LanguageManager* lpLanguageManager = mGuiAccessPointers.mpLanguageManager;
        if (XUserGetName(mSystemUserProfile.GetUserIndex(), lacName, 16) != ERROR_SUCCESS)
        {
            const u8* lpcDefaultName = lpLanguageManager->FindString(KAPC_UPN_DEFAULT_NAME_ID);
            CGS_ASSERT(lpcDefaultName != NULL, "Couldn't find default player name in string database.");
            CGS_ASSERT(std::strlen(reinterpret_cast<const char*>(lpcDefaultName)) < 27, "String too long: ");
            std::strncpy(lacName, reinterpret_cast<const char*>(lpcDefaultName), 27);
            lpLanguageManager->AddString(KAPC_UPN_PLAYER_NAME_ID, reinterpret_cast<const u8*>(lacName));
            const u8* lpcDefaultQuoted = lpLanguageManager->FindString(KAPC_UPN_DEFAULT_QUOTED_ID);
            CGS_ASSERT(lpcDefaultQuoted != NULL, "Couldn't find default player name quoted in string database.");
            CGS_ASSERT(std::strlen(reinterpret_cast<const char*>(lpcDefaultQuoted)) < 31, "String too long: ");
            std::strncpy(lacQuoted, reinterpret_cast<const char*>(lpcDefaultQuoted), 31);
        }
        else
        {
            lpLanguageManager->AddString(KAPC_UPN_PLAYER_NAME_ID, reinterpret_cast<const u8*>(lacName));
            CgsCore::SPrintf(lacQuoted, 32, "''%s''", lacName);
        }
        lacQuoted[31] = 0;
        lpLanguageManager->AddString(KAPC_UPN_PLAYER_NAME_QUOTED_ID, reinterpret_cast<const u8*>(lacQuoted));
    }

    void GuiModule::AutosaveProfile(const CgsGui::ImageFileInfo* lpImageFileInfo)
    {
        if (lpImageFileInfo != 0 && !mProfileHost.GetProfileManager().AutosaveIsEnabled())
        {
            mProfileHost.GetProfileManager().CopyImageToBuffer(lpImageFileInfo);
        }

        if (mProfileHost.GetProfileManager().AutosaveIsEnabled())
        {
            mProfileHost.GetProfileManager().Autosave(&mProfileHost,
                                     lpImageFileInfo != 0 ? 1 : 0, lpImageFileInfo);
            mbProfileAutosavePending  = false;
            mfLastProfileAutosaveTime = mGuiCache.GetTime();
        }
    }

    void GuiModule::HandleControllerPressed(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != NULL, "lpEvent");
        const u32 luButton = reinterpret_cast<const u32*>(lpEvent)[1];
        if ((luButton == 49 || luButton == 50) &&
            mProfileHost.GetProfileManager().GetTaskResultHandler() == &mProfileHost)
            mProfileHost.GetProfileManager().HandleMessageChoice(luButton == 50 ? 1 : 0);
    }

    void GuiModule::PerformInvite(CgsGui::CgsGuiModuleIO::InputBuffer*,
        CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        if (!mbPerformingInvite)
        {
            mbPerformingInvite = true;
            if (mbInviteRequiresLogin)
            {
                CgsGui::GuiEvent<89> lLogin;
                lpOutput->AddGuiOutEvent(lLogin);
            }
            else
            {
                GuiEventRunFsm lFsm;
                lFsm.mFsmId = CgsIDCompress("BrnScreenFsm");
                lFsm.mInitialStateId = CgsIDCompress("LOADING");
                lFsm.meFlowToUse = E_GUIFLOW_SCREEN;
                mStoredGuiEvents.AddEvent(&lFsm, 144, sizeof(lFsm));
                lFsm.mFsmId = CgsIDCompress("BrnFBFsm");
                lFsm.mInitialStateId = 0;
                lFsm.meFlowToUse = E_GUIFLOW_HUD;
                mStoredGuiEvents.AddEvent(&lFsm, 144, sizeof(lFsm));
            }
        }
    }

    void GuiModule::Update(BrnUpdateSet luUpdateSet,
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        CgsGui::CgsGuiModuleIO::InputBuffer* lpInput,
        CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput,
        CgsGui::ModelIO::OutputBuffer* lpModelOutputBuffer,
        CgsGui::ViewIO::InputBuffer* lpViewInputBuffer,
        BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput,
        bool lbSerialise)
    {
        CGS_ASSERT(lpInput != NULL, "lpInput != NULL");
        CGS_ASSERT(lpOutput != NULL, "lpOutput != NULL");
        CGS_ASSERT(lpInputBufferStack != NULL, "lpInputBufferStack != NULL");
        CGS_ASSERT(lpOutputBufferStack != NULL, "lpOutputBufferStack != NULL");
        bool lbForceAutosave = false;
        bool lbHasSaveImage = false;
        CgsGui::ImageFileInfo lSaveImageInfo;
        lSaveImageInfo.Construct();

        lpInput->LockForWrite();
        lpOutput->LockForWrite();
        CGS_ASSERT(mpOutputBuffer == NULL, "mpOutputBuffer == NULL");
        mpOutputBuffer = lpOutput;
        mGuiCache.mbGameplayUpdateActive = (luUpdateSet & 8) != 0;
        mGuiAccessPointers.SetGDMInput(lpGameDataInput);

        if (lbSerialise)
        {
            if (mGuiModuleSerialiser.IsPlaying())
                mGuiModuleSerialiser.Read();
            else if (mGuiModuleSerialiser.IsRecording())
                mGuiModuleSerialiser.GetStaticLayout()->SetupStartOfFrame();
            CGS_ASSERT(!mGuiModuleSerialiser.IsRecording() ||
                mGuiModuleSerialiser.GetBufferUsed() == 0,
                "( mReplayGuiModuleSerialiser.IsRecording() == false ) || ( mReplayGuiModuleSerialiser.GetBufferUsed() == 0 )");
        }

        CgsGui::ViewIO::OutputBuffer* lpViewOutputBuffer = NULL;
        lpOutputBufferStack->CreateIOBuffer(&lpViewOutputBuffer, "GUIView");
        CgsGui::ModelIO::InputBuffer* lpModelInputBuffer = NULL;
        lpInputBufferStack->CreateIOBuffer(&lpModelInputBuffer, "GUIModel");
        if (mSystemUserProfile.miState == 1 && mGuiCache.mbPerformingInvite)
            PerformInvite(lpInput, lpOutput);

        CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miGuiModuleEventPump);
        CgsGui::CgsGuiModuleIO::InputBuffer::GuiEventInputQueue* lpEventQueue = lpInput->GetGuiEvents();
        CGS_ASSERT(lpEventQueue != NULL, "lpEventQueue != NULL");
        lpEventQueue->Append(mStoredGuiEvents);
        mStoredGuiEvents.Clear();
        if (mGDMReceiverQueue.GetLength() > 0)
            mGDMReceiverQueue.Clear();

        if (gbGuiToolStartHookRequested)
        {
            GuiPFXHookEvent lHook;
            lHook.mfMaximumBloomWeight = 1.0f;
            lHook.mfMaximumVignetteWeight = 1.0f;
            lHook.mfMaximumBlurWeight = 1.0f;
            lHook.mfMaximumDepthOfFieldWeight = 1.0f;
            lHook.mfMaximum2DTintWeight = 1.0f;
            lHook.mfMaximum3DTintWeight = 1.0f;
            CGS_ASSERT(std::strlen(gacGuiToolHookName) < 32, "String too long: ");
            std::strncpy(lHook.macName, gacGuiToolHookName, 32);
            lpEventQueue->AddEvent(&lHook, 495, sizeof(lHook));
            gbGuiToolStartHookRequested = false;
        }
        else if (gbGuiToolStopHookRequested)
        {
            if (gacGuiToolHookName[0])
            {
                GuiPFXHookStopEvent lStop;
                CGS_ASSERT(std::strlen(gacGuiToolHookName) < 32, "String too long: ");
                std::strncpy(lStop.macName, gacGuiToolHookName, 32);
                lpEventQueue->AddEvent(&lStop, 496, sizeof(lStop));
            }
            else
            {
                CgsGui::GuiEvent<497> lStop;
                lpEventQueue->AddEvent(&lStop, 497, sizeof(lStop));
            }
            gbGuiToolStopHookRequested = false;
        }
        mDebugComponent.UpdateWorstCaseHUD(lpInput);

        const CgsModule::Event* lpEvent = NULL;
        s32 liSize = 0;
        s32 liId = lpEventQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent)
        {
            const u32* lpWords = reinterpret_cast<const u32*>(lpEvent);
            const u8* lpBytes = reinterpret_cast<const u8*>(lpEvent);
            switch (liId)
            {
            case 1:
            {
                GuiAudioTriggerEvent lAudio;
                lAudio.Construct(reinterpret_cast<const char*>(lpEvent),
                    mViewModule.GetMovieNameByLevel(0));
                lpOutput->AddGuiOutEvent(lAudio);
                break;
            }
            case 6:
                HandleControllerPressed(lpEvent);
                break;
            case 7:
                CGS_ASSERT(lpEvent != NULL, "lpEvent");
                break;
            case 14:
            case 16:
            case 481:
                if (liId == 481)
                    mFsmController.HandleHudStateLoadComplete();
                CgsModule::LockBuffersForIO(lpModelOutputBuffer);
                lpModelOutputBuffer->GetLoadNotificationsNonConst()->AddEvent(lpEvent, liId, liSize);
                CgsModule::UnlockBuffersForIO(lpModelOutputBuffer);
                break;
            case 22:
            {
                const GuiEventSoundTriggerPayload22* lpSound =
                    reinterpret_cast<const GuiEventSoundTriggerPayload22*>(lpEvent);
                GuiAudioTriggerEvent lAudio;
                lAudio.Construct(lpSound->macTypeName, lpSound->macActionName,
                    lpSound->macLabel, mViewModule.GetMovieNameByLevel(lpSound->miLayer));
                lpOutput->AddGuiOutEvent(lAudio);
                break;
            }
            case 28:
                mSku.SetLanguage(static_cast<CgsLanguage::ELanguage>(lpWords[0]));
                mViewModule.GetLanguageManager()->UnloadStringTable();
                break;
            case 30:
            {
                CGS_ASSERT(lpEvent != NULL, "NULL != lpEvent");
                CGS_ASSERT(mGuiCache.mLanguage != CgsLanguage::E_LANGUAGE_INVALID,
                    "Language NOT set yet.Test later in exectution");
                CgsGui::GuiEventGetLanguage lLanguage;
                lLanguage.mLanguage = mGuiCache.mLanguage;
                lpOutput->AddGuiOutEvent(lLanguage);
                break;
            }
            case 43:
                CGS_ASSERT(lpEvent != NULL, "lpConnectedEvent");
                if (lpBytes[1] && !mGuiCache.mbRoadRuleFriendScoresAvailable)
                {
                    mGuiCache.GetOptionsDataProfile()->SetUnreadNews(true);
                    mbProfileAutosavePending = true;
                }
                mGuiCache.mbRoadRuleFriendScoresAvailable = true;
                break;
            case 51:
            {
                CGS_ASSERT(lpEvent != NULL, "lpInGameFailedEvent");
                GuiOverlayWaitFinishRequest lWait;
                lWait.Construct("CNOnlEntGame");
                lpOutput->AddGuiOutEvent(lWait);
                const char* lpcOverlay;
                switch (lpWords[0])
                {
                case 0: lpcOverlay = "CNGameFull"; break;
                case 1: lpcOverlay = "CNGameStrted"; break;
                case 2: lpcOverlay = "OnJoinNoGame"; break;
                case 3: lpcOverlay = "CNInvGmFail"; break;
                default: lpcOverlay = "OnJoinFail"; break;
                }
                GuiOverlayRequest lRequest;
                lRequest.Construct(lpcOverlay);
                lpOutput->AddGuiOutEvent(lRequest);
                break;
            }
            case 93:
                if (lpWords[3] == 15 && mGuiCache.meActiveRoadRule != 3 &&
                    mGuiCache.meActiveRoadRule != 4)
                    mBurnoutSkillsManager.ResetSkillsData();
                mGuiTracker.GenerateRouteData();
                break;
            case 95:
                if (mFsmController.IsTransitionPending(1u))
                    mGuiCache.muFriendsPanelBranchMirror = 0;
                break;
            case 109:
                mbStartLobbyDueToPlayerJoin = true;
                break;
            case 128:
                CGS_ASSERT(lpEvent != NULL, "lpInviteEvent != NULL");
                mbPerformingInvite = false;
                mbInviteRequiresLogin = lpBytes[0] != 0;
                break;
            case 133:
            {
                CGS_ASSERT(lpEvent != NULL, "lpInviteFailedEvent");
                if (lpWords[0] == 3 || lpWords[0] == 4)
                {
                    GuiOverlayRequest lRequest;
                    lRequest.Construct(lpWords[0] == 3 ? "OnJoinNoGame" : "OnReqHDD");
                    lpOutput->AddGuiOutEvent(lRequest);
                }
                break;
            }
            case 141:
            {
                GuiEventKeyboardResponse lResponse;
                lResponse.lpKeyboard = &mGuiKeyboard;
                lpOutput->AddGuiOutEvent(lResponse);
                break;
            }
            case 143:
                mSystemUserProfile.SetUserIndex(lpWords[0]);
                mSystemUserProfile.UpdateUserSigninState();
                break;
            case 144:
                mFsmController.RunFsm(reinterpret_cast<const GuiEventRunFsm*>(lpEvent));
                break;
            case 177:
                mHudMessageDirector.StartPaybackAggressor();
                break;
            case 179:
            case 370:
                if (mHudMessageDirector.meStopFlagPaybackSequence == HudMessageDirector::E_PAYBACK_AGGRESSOR)
                    mHudMessageDirector.meStopFlagPaybackSequence = HudMessageDirector::E_PAYBACK_NONE;
                break;
            case 221:
                mHudMessageDirector.SetBlackBarSize(*reinterpret_cast<const f32*>(lpEvent));
                break;
            case 267:
                mBurnoutSkillsManager.ResetPlayerData(static_cast<EActiveRaceCarIndex>(lpWords[0]));
                break;
            case 275:
            {
                GuiOverlayWaitFinishRequest lWait;
                lWait.Construct("CNOnlLvgGame");
                lpOutput->AddGuiOutEvent(lWait);
                break;
            }
            case 291:
            case 320:
                mGuiCache.mInGameMessagesQueue.muCurrentEventEndTime = 0;
                if (mGuiCache.mInGameMessagesQueue.maeMessageState[0] == E_MESSAGESTATE_WAITING)
                    mGuiCache.mInGameMessagesQueue.maeMessageState[0] = E_MESSAGESTATE_NOMESSAGE;
                if (mGuiCache.mInGameMessagesQueue.maeMessageState[1] == E_MESSAGESTATE_WAITING)
                    mGuiCache.mInGameMessagesQueue.maeMessageState[1] = E_MESSAGESTATE_NOMESSAGE;
                break;
            case 325:
                switch (lpWords[2])
                {
                case 0: case 1: case 2: case 3:
                    mHudMessageDirector.SetCameraSequenceFilter(true);
                    break;
                case 4: case 5:
                    mHudMessageDirector.SetCameraSequenceFilter(false);
                    mHudMessageDirector.meStopFlagPaybackSequence = HudMessageDirector::E_PAYBACK_NONE;
                    break;
                }
                break;
            case 343:
                mBurnoutSkillsManager.SetRoadRuleMode(static_cast<BrnGameState::EActiveRoadRule>(lpWords[0]));
                break;
            case 350:
            {
                const GuiEventProgressionProfileData* lpProfile =
                    reinterpret_cast<const GuiEventProgressionProfileData*>(lpEvent);
                mProfileHost.GetProfileManager().SetProgressionProfile(lpProfile->mpProfile,
                    lpProfile->mpProgressionData);
                break;
            }
            case 351:
                mProfileHost.GetProfileManager().SetLiveRevengeProfile(
                    reinterpret_cast<const GuiEventLiveRevengeProfileData*>(lpEvent)->mpLiveRevengeProfile);
                break;
            case 356:
                lbForceAutosave = lbForceAutosave || lpBytes[0] != 0;
                mbProfileAutosavePending = true;
                break;
            case 358:
                std::memcpy(&lSaveImageInfo, lpEvent, sizeof(lSaveImageInfo));
                lbForceAutosave = true;
                lbHasSaveImage = true;
                mbProfileAutosavePending = true;
                break;
            case 359:
                mProfileHost.GetProfileManager().LoadImageFiles(&mProfileHost,
                    reinterpret_cast<const CgsGui::ImageFileInfo*>(lpEvent));
                break;
            case 363:
            {
                const GuiTakedownEvent* lpTakedown = reinterpret_cast<const GuiTakedownEvent*>(lpEvent);
                const EActiveRaceCarIndex leLocal = static_cast<EActiveRaceCarIndex>(mGuiCache.mePlayerActiveRaceCarIndex);
                if (leLocal == lpTakedown->meAggressorIndex)
                {
                    if (lpTakedown->mbMarkedManTakeDown)
                        mHudMessageDirector.StartPaybackAggressor();
                }
                else if (leLocal == lpTakedown->meVictimIndex)
                {
                    if (lpTakedown->mbMarkedManTakeDown)
                        mHudMessageDirector.StartPaybackVictim();
                    else if (mHudMessageDirector.meStopFlagPaybackSequence == HudMessageDirector::E_PAYBACK_AGGRESSOR)
                        mHudMessageDirector.meStopFlagPaybackSequence = HudMessageDirector::E_PAYBACK_NONE;
                }
                if ((leLocal == lpTakedown->meAggressorIndex || leLocal == lpTakedown->meVictimIndex) &&
                    mGuiCache.mbOnlineStartInProgress)
                {
                    GuiEventRequestTraining lTraining;
                    lTraining.meTrainingType = mGuiCache.GetCamStatus() == 0 ? 69 : 60;
                    lpOutput->AddGuiOutEvent(lTraining);
                }
                break;
            }
            case 377:
                if (lpWords[0] == 0 &&
                    mHudMessageDirector.meStopFlagPaybackSequence == HudMessageDirector::E_PAYBACK_AGGRESSOR)
                    mHudMessageDirector.meStopFlagPaybackSequence = HudMessageDirector::E_PAYBACK_NONE;
                break;
            case 475:
                CGS_ASSERT(lpEvent != NULL, "lpEvent != NULL");
                mGuiCache.GetOptionsDataProfile()->SetDefaultGameCamera(lpWords[0] != 0);
                break;
            case 493:
                if (lpWords[0] == 2) mbCollisionWorldInvalid = true;
                else if (lpWords[0] == 3) mbCollisionWorldInvalid = false;
                break;
            case 504: case 508: case 513:
                mMovieManager.RecvEvent(lpEvent, liId);
                break;
            case 505:
                CGS_ASSERT(lpEvent != NULL, "lpPausedEvent");
                if (lpBytes[4] && mbProfileAutosavePending)
                    lbForceAutosave = true;
                break;
            case 507:
                CgsModule::LockBuffersForIO(lpViewInputBuffer);
                CGS_ASSERT(&lpViewInputBuffer->GetViewStateQueue() != NULL,
                    "lpViewInputBuffer->GetViewStateQueue()");
                UpdatePlayerName(&lpViewInputBuffer->GetViewStateQueue());
                CgsModule::UnlockBuffersForIO(lpViewInputBuffer);
                break;
            case 512:
                if (lpBytes[0])
                {
                    CGS_ASSERT(!mbTrafficPoolEmptied, "!mbTrafficPoolEmptied");
                    mbTrafficPoolEmptied = true;
                }
                else
                {
                    CGS_ASSERT(mbTrafficPoolEmptied, "mbTrafficPoolEmptied");
                    mbTrafficPoolEmptied = false;
                }
                break;
            case 541:
                mBurnoutSkillsManager.SetSkillsData(
                    reinterpret_cast<const GuiNewBurnoutSkillzEvent*>(lpEvent)->meActiveRaceCarIndex,
                    reinterpret_cast<const GuiNewBurnoutSkillzEvent*>(lpEvent), lpOutput);
                break;
            case 543: mBurnoutSkillsManager.SelectNext(); break;
            case 544: mFreeburnChallengeManager.SelectNext(); break;
            case 574:
                mFreeburnChallengeManager.StartChallenge(reinterpret_cast<const GuiChallengeStartEvent*>(lpEvent));
                break;
            case 576:
                mFreeburnChallengeManager.TriggerChallenge(reinterpret_cast<const GuiChallengeTriggerResponse*>(lpEvent));
                break;
            case 577:
                mFreeburnChallengeManager.HandleNewData(reinterpret_cast<const GuiChallengeUpdateEvent*>(lpEvent));
                break;
            case 578: mFreeburnChallengeManager.StartResults(); break;
            case 579: mFreeburnChallengeManager.FinishChallenge(); break;
            case 581:
                mFreeburnChallengeManager.HandleCompletionStatus(
                    reinterpret_cast<const GuiEventFburnChallengeEveryPlayerStatus*>(lpEvent));
                break;
            case 583:
                mFreeburnChallengeManager.StartNotActiveChallenge(
                    reinterpret_cast<const GuiChallengeNotActiveStartEvent*>(lpEvent));
                break;
            case 584: mFreeburnChallengeManager.EndNotActiveChallenge(); break;
            case 593:
            {
                GuiResponseTimeDateString lResponse;
                lResponse.muRequestId = lpWords[0];
                CgsSystem::DateAndTime lDateTime;
                lDateTime.SetLocal(true);
                lDateTime.Update();
                char lacDate[32];
                char lacTime[32];
                mViewModule.GetLanguageManager()->FormatDateString(lacDate,
                    lDateTime.GetDay(), lDateTime.GetMonth(), lDateTime.GetYear(), 32);
                mViewModule.GetLanguageManager()->FormatHoursAndMinutesAndSecondsString(lacTime,
                    lDateTime.GetHour(), lDateTime.GetMinute(), lDateTime.GetSecond(), 32);
                CGS_ASSERT(CgsUnicode::IsSingleByteOnlyUtf8String(reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacDate)),
                    "CgsUnicode::IsSingleByteOnlyUtf8String( lacDate )");
                CGS_ASSERT(CgsUnicode::IsSingleByteOnlyUtf8String(reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacTime)),
                    "CgsUnicode::IsSingleByteOnlyUtf8String( lacTime )");
                CgsCore::SnPrintf(lResponse.macTimeDate, 32, "%s %s", lacDate, lacTime);
                lpOutput->AddGuiOutEvent(lResponse);
                break;
            }
            }
            mGuiCache.RecEvent(lpEvent, liId);
            if (liId == 232 || liId == 165 || liId == 211 || liId == 64 || liId == 233)
                mGuiTracker.RecEvent(lpEvent, liId, liSize);
            liId = lpEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
        CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miGuiModuleEventPump);

        if (mbProfileAutosavePending)
        {
            const bool lbTimedSave = !mGuiCache.mbIsGamePaused &&
                mGuiCache.GetTime() > mfLastProfileAutosaveTime + 60.0f;
            if (lbForceAutosave || lbTimedSave)
            {
                if (lbHasSaveImage)
                    AutosaveProfile(&lSaveImageInfo);
                else if (mSystemUserProfile.miState == 1 && mProfileHost.GetProfileManager().AutosaveIsEnabled())
                {
                    mProfileHost.GetProfileManager().Autosave(&mProfileHost, 0, NULL);
                    mbProfileAutosavePending = false;
                    mfLastProfileAutosaveTime = mGuiCache.GetTime();
                }
            }
        }
        else if (mGuiCache.mpProfile && mGuiCache.mpProfile->AreGoldCarsUnlocked() &&
            mGuiCache.GetTime() > mfLastProfileAutosaveTime + 300.0f)
            mbProfileAutosavePending = true;

        CgsGui::State* lpScreenState = mScreenFlow.GetStateMachine().GetCurrentState();
        if (lpScreenState && !mGuiCache.mbImageExportInProgress &&
            meCollisionWorldStateValidationState == E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE &&
            mbStartLobbyDueToPlayerJoin)
        {
            CGS_ASSERT(lpScreenState != NULL, "mScreenFlow.GetStateMachine().GetCurrentState()");
            if (lpScreenState->GetId() != CgsIDCompress("ON_GAME_ROOM"))
            {
                GuiEventRunFsm lFsm;
                lFsm.mFsmId = CgsIDCompress("BrnScreenFsm");
                lFsm.mInitialStateId = CgsIDCompress("LOADING");
                lFsm.meFlowToUse = E_GUIFLOW_SCREEN;
                mStoredGuiEvents.AddEvent(&lFsm, 144, sizeof(lFsm));
                lFsm.mFsmId = CgsIDCompress("BRNEVENTFSM");
                lFsm.mInitialStateId = 0;
                lFsm.meFlowToUse = E_GUIFLOW_HUD;
                mStoredGuiEvents.AddEvent(&lFsm, 144, sizeof(lFsm));
            }
            mbStartLobbyDueToPlayerJoin = false;
        }
        GuiCache* lpCache = &mGuiCache;
        lpEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lpCache), 64, sizeof(lpCache));
        if (mGuiCache.GetHudMessageController())
        {
            if (!mHudMessageDebugComponent.ControllerIsValid())
                mHudMessageDebugComponent.SetController(mGuiCache.GetHudMessageController());
            mHudMessageDirector.SetController(mGuiCache.GetHudMessageController());
        }

        if (mGuiModuleSerialiser.IsRecording())
        {
            BrnReplays::GuiModuleStaticLayout* lpLayout = mGuiModuleSerialiser.GetStaticLayout();
            for (s32 liCar = 0; liCar < 8; ++liCar)
            {
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayer = NULL;
                for (s32 liPlayer = 0; liPlayer < 8; ++liPlayer)
                {
                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpCandidate =
                        mGuiCache.GetOnlinePlayerInfo(liPlayer);
                    if (lpCandidate->meActiveRaceCarIndex == liCar)
                    {
                        lpPlayer = lpCandidate;
                        break;
                    }
                }
                if (lpPlayer)
                {
                    CgsNetwork::PlayerName* lpName = reinterpret_cast<CgsNetwork::PlayerName*>(
                        lpLayout->mbFrameParity ? lpLayout->maCarFlagsBuffer1[liCar] : lpLayout->maCarFlagsBuffer0[liCar]);
                    lpName->Construct(lpPlayer->mPlayerName.GetPlayerName());
                }
                CGS_ASSERT(liCar + 1 <= 8, "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
            }
        }
        else if (mGuiModuleSerialiser.IsPlaying())
        {
            BrnReplays::GuiModuleStaticLayout* lpLayout = mGuiModuleSerialiser.GetStaticLayout();
            for (s32 liCar = 0; liCar < 8; ++liCar)
            {
                if (mGuiCache.IsReplayARCRendered(static_cast<EActiveRaceCarIndex>(liCar)))
                    mGuiCache.IncrementReplayPlayerActive(reinterpret_cast<const char*>(
                        lpLayout->mbFrameParity ? lpLayout->maCarFlagsBuffer1[liCar] : lpLayout->maCarFlagsBuffer0[liCar]),
                        static_cast<s32>(lpLayout->mauCarRecordsC[liCar]));
                CGS_ASSERT(liCar + 1 <= 8, "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
            }
        }

        if (mGuiCache.GetPopupController())
            mOverlaysDirector.SetController(mGuiCache.GetPopupController());
        mColourCalibrationScreen.Update(lpGameDataInput, lpGameDataOutput, lpOutput,
            mGuiConfig.mpTextureAllocator);
        mGuiTracker.Update(lpOutput);
        CGS_ASSERT(lpViewInputBuffer != NULL, "lpInputBuffer");
        lpViewInputBuffer->LockForWrite();
        CGS_ASSERT(&lpViewInputBuffer->GetViewStateQueue() != NULL,
            "lpViewInputBuffer->GetViewStateQueue()");
        mHudMessageAnalyzer.Update(lpInput->GetGuiEvents(), &lpViewInputBuffer->GetViewStateQueue());
        CGS_ASSERT(lpViewInputBuffer != NULL, "lpInputBuffer");
        lpViewInputBuffer->UnlockForWrite();
        mOverlaysDirector.Update(lpInput);
        lpInput->UnlockForWrite();
        mHudMessageDirector.Update(lpModelInputBuffer);
        mDebugComponent.Update(lpInput);
        mSatNavDebugComponent.Update(lpInput);
        mGuiCacheDebugComponent.Update(lpInput);
        mScreenFlow.SetStreamingMode(mDebugComponent.GetScreenFlowStreamingMode());
        mOverlaysDirector.BridgeOutEvents(lpModelInputBuffer);
        CGS_ASSERT(lpModelInputBuffer != NULL, "lpInputBuffer");
        lpModelInputBuffer->LockForWrite();
        mGuiCache.Update(lpModelInputBuffer);
        CGS_ASSERT(lpModelInputBuffer != NULL, "lpInputBuffer");
        lpModelInputBuffer->UnlockForWrite();
        UpdateNativeSystemNotifications(lpModelInputBuffer);
        if (gbGuiForceSystemUIClosed || gbGuiForceSystemUIOpen)
        {
            const u32 luUiVisible = !gbGuiForceSystemUIClosed;
            CGS_ASSERT(lpModelInputBuffer != NULL, "lpInputBuffer");
            lpModelInputBuffer->LockForWrite();
            lpModelInputBuffer->GetEventQueueNonConst()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&luUiVisible), 516, sizeof(luUiVisible));
            CGS_ASSERT(lpModelInputBuffer != NULL, "lpInputBuffer");
            lpModelInputBuffer->UnlockForWrite();
            gbGuiForceSystemUIClosed = false;
            gbGuiForceSystemUIOpen = false;
        }

        CGS_ASSERT(lpModelInputBuffer != NULL, "lpInputBuffer");
        CGS_ASSERT(lpModelOutputBuffer != NULL, "lpOutputBuffer0");
        CgsModule::LockBuffersForIO(lpModelInputBuffer, lpModelOutputBuffer);
        mFsmController.Update(lpModelInputBuffer, lpModelOutputBuffer);
        CGS_ASSERT(lpModelInputBuffer != NULL, "lpInputBuffer");
        CGS_ASSERT(lpModelOutputBuffer != NULL, "lpOutputBuffer0");
        CgsModule::UnlockBuffersForIO(lpModelInputBuffer, lpModelOutputBuffer);
        CGS_ASSERT(lpModelInputBuffer != NULL, "lpInputBuffer");
        CGS_ASSERT(lpModelOutputBuffer != NULL, "lpOutputBuffer0");
        CgsModule::LockBuffersForIO(lpModelInputBuffer, lpModelOutputBuffer);
        mEffectsArbitrator.ResourceUpdate(lpModelInputBuffer, lpModelOutputBuffer);
        CGS_ASSERT(lpModelInputBuffer != NULL, "lpInputBuffer");
        CGS_ASSERT(lpModelOutputBuffer != NULL, "lpOutputBuffer0");
        CgsModule::UnlockBuffersForIO(lpModelInputBuffer, lpModelOutputBuffer);
        lpInput->LockForRead();
        mEffectsArbitrator.EventUpdate(luUpdateSet, lpInput, lpOutput);
        lpInput->UnlockForRead();
        CGS_ASSERT(lpModelOutputBuffer != NULL, "lpInputBuffer");
        lpModelOutputBuffer->LockForWrite();
        lpModelOutputBuffer->GetLoadNotificationsNonConst()->Clear();
        CGS_ASSERT(lpModelOutputBuffer != NULL, "lpInputBuffer");
        lpModelOutputBuffer->UnlockForWrite();
        mSystemUserProfile.Update();
        mGuiKeyboard.Update();
        mProfileHost.GetProfileManager().Update();
        CgsGui::GuiEventQueueSmall& lrProfileEvents = mProfileHost.GetProfileManager().GetOutEventQueue();
        if (lrProfileEvents.GetLength())
        {
            lpOutput->AddGuiOutEvents(&lrProfileEvents);
            lrProfileEvents.Clear();
        }
        GuiReplayRegisterSerialiser lRegister;
        lRegister.mpSerialiser = &mGuiModuleSerialiser;
        lpOutput->AddGuiOutEvent(lRegister);
        if (lbSerialise)
        {
            if (mGuiModuleSerialiser.GetMode() == BrnReplays::BaseSerialiser::E_MODE_RESTORING)
                mGuiModuleSerialiser.SetDataRestored(true);
            else if (mGuiModuleSerialiser.IsPlaying() ||
                mGuiModuleSerialiser.GetMode() == BrnReplays::BaseSerialiser::E_MODE_RECORDING_PREPARING)
                mGuiModuleSerialiser.SetDataReady(true);
        }
        MovieManager::VideoDefinition lVideo;
        if (mMovieManager.GetState() == MovieManager::E_MOVIEMANAGERSTATE_REPORTING_FINISHED)
        {
            lVideo.Copy(&mMovieManager.GetPlayingMovieDefinition());
            mMovieManager.SetState(MovieManager::E_MOVIEMANAGERSTATE_IDLE);
            MovieManager::VideoDefinition lFinished;
            lFinished.Prepare();
            lFinished.Copy(&lVideo);
            lpModelInputBuffer->LockForRead();
            const CgsGui::ModelIO::InputBuffer::GuiEventInputQueue* lpModelEvents =
                lpModelInputBuffer->GetEventQueue();
            lpModelInputBuffer->UnlockForRead();
            CgsGui::GuiEventPlayMusicOnMenuStream lMusic(0, false, false);
            lpOutput->AddGuiOutEvent(lMusic);
            const_cast<CgsGui::ModelIO::InputBuffer::GuiEventInputQueue*>(lpModelEvents)->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lFinished), 510, sizeof(lFinished));
        }
        if (mMovieManager.GetState() == MovieManager::E_MOVIEMANAGERSTATE_REQUESTING_AUDIO)
        {
            lVideo.Copy(&mMovieManager.GetPlayingMovieDefinition());
            mMovieManager.SetState(MovieManager::E_MOVIEMANAGERSTATE_WAITING_FOR_AUDIO);
            CgsGui::GuiEventPlayMusicOnMenuStream lMusic(lVideo.mSoundStreamName, false,
                lVideo.mbDisableCustomSoundtracks);
            lpOutput->AddGuiOutEvent(lMusic);
        }
        if (mAlwaysAvailableComponentsManager.Prepare(&mGuiAccessPointers))
            mAlwaysAvailableComponentsManager.UpdateTimedComponents();
        mBurnoutSkillsManager.Update();
        mFreeburnChallengeManager.Update();
        CgsGui::GuiModule::Update(lpInputBufferStack, lpOutputBufferStack, lpInput, lpOutput,
            lpModelInputBuffer, lpModelOutputBuffer, lpViewInputBuffer, lpViewOutputBuffer);
        mpOutputBuffer = NULL;
        lpOutput->UnlockForWrite();
        HandleEventsPostBaseModuleUpdate(lpOutput);
        mCustomRendererManager.Update();
        if (lbSerialise && mGuiModuleSerialiser.IsRecording())
            mGuiModuleSerialiser.Write();
        mGuiAccessPointers.SetGDMInput(NULL);
        lpInputBufferStack->DestroyIOBuffer(&lpModelInputBuffer);
        lpOutputBufferStack->DestroyIOBuffer(&lpViewOutputBuffer);
    }

    void GuiModule::HandleEventsPostBaseModuleUpdate(
        CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        lpOutput->LockForRead();

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpOutput->GetOutEventQueue()->GetFirstEvent(&lpEvent, &liEventSize);
        while (lpEvent != 0)
        {
            switch (liEventType)
            {
            case 493:
            {
                const GuiEventRequestCollisionWorldEvent* lpRequest =
                    reinterpret_cast<const GuiEventRequestCollisionWorldEvent*>(lpEvent);
                switch (lpRequest->meEventType)
                {
                case GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_INVALIDATE:
                    if (meCollisionWorldStateValidationState !=
                        E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE)
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(
                            "meCollisionWorldStateValidationState == E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE",
                            "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1729);
                        CgsDev::Assert::EndAssert();
                    }
                    mbCollisionWorldInvalidateForNetworking = true;
                    break;

                case GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_VALIDATE:
                    if (meCollisionWorldStateValidationState !=
                        E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING)
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(
                            "meCollisionWorldStateValidationState == E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING",
                            "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1737);
                        CgsDev::Assert::EndAssert();
                    }
                    mbCollisionWorldValidateForNetworking = true;
                    break;

                default:
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "Invalid collsion world request output event type",
                        "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1745);
                    CgsDev::Assert::EndAssert();
                    break;
                }
                break;
            }

            case 508:
            case 509:
                mMovieManager.RecvEvent(lpEvent, liEventType);
                break;

            case 514:
            case 515:
                mColourCalibrationScreen.RecvEvent(lpEvent, liEventType);
                break;

            default:
                break;
            }

            liEventType = lpOutput->GetOutEventQueue()->GetNextEvent(
                lpEvent, &lpEvent, &liEventSize);
        }

        lpOutput->UnlockForRead();
    }

    void GuiModule::EndOfFrame()
    {
        mCustomRendererManager.EndOfFrame();
    }

    void GuiModule::Render(CgsGui::ViewIO::InputBuffer* lpViewInput,
                           CgsGui::CgsGuiModuleIO::InputBuffer* lpInput,
                           RendererIO::OutputBuffer* lpRenderOutput)
    {
        CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miGuiModuleRender);
        CGS_ASSERT(lpInput != 0, "lpInput");
        CGS_ASSERT(lpRenderOutput != 0, "lpRenderOutput");
        if (IsPrepared() && mDebugComponent.IsGuiRenderingEnabled())
        {
            CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miGuiRender);
            CGS_ASSERT(lpViewInput != 0, "lpInputBuffer");
            CGS_ASSERT(lpInput != 0, "lpOutputBuffer0");
            CgsModule::LockBuffersForIO(lpViewInput, lpInput);
            lpViewInput->SetImRenderers(lpInput->GetImRenderers());
            CGS_ASSERT(lpViewInput != 0, "lpInputBuffer");
            CGS_ASSERT(lpInput != 0, "lpOutputBuffer0");
            CgsModule::UnlockBuffersForIO(lpViewInput, lpInput);
            mViewModule.Render(lpViewInput);
            CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miGuiRender);

            UpdateAndRenderMovieManager(lpViewInput);
            mEffectsArbitrator.GenerateEffectFrameEvents(lpRenderOutput);
        }
        CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miGuiModuleRender);
    }

    void GuiModule::UpdateAndRenderMovieManager(CgsGui::ViewIO::InputBuffer* lpViewInput)
    {
        mMovieManager.Update();
        lpViewInput->LockForRead();
        mMovieManager.Render(lpViewInput->GetImRenderers().mpIm2dRenderBuffer);
        lpViewInput->UnlockForRead();
    }

}

namespace BrnGui
{
    AlwaysAvailableComponentsManager* GetAlwaysAvailableComponentsManager(GuiModule* lpGuiModule)
    {
        return lpGuiModule->GetAlwaysAvailableComponentsManager();
    }

}
