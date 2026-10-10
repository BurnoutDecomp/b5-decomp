// ===================================================================================
// BrnGui::OnlineMarkMan  -- the online "mark man" screen state (expected-component slice)
//   class:BrnGui::OnlineMarkMan
//
//   SetExpectedComponent   @ 0x82483AC0
//   ClearExpectedComponent @ 0x82483BA8
// Reconstructed store-for-store from the X360 asm; byte-identical twin of
// BrnGui::RaceMainHudState::SetExpectedComponent.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineMarkMan.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"    // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                         // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                        // CgsGui::GuiAccessPointers::GetGuiCache
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"    // StateInterface
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTriggerPayload
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"            // VariableEventQueue<18432,16> (the in-queue view)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                             // GuiOverlayRequest
#include "GameSource/Gui/BrnGuiShared.h"                                    // gGuiResourceIdentifier
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"                            // GuiEventCachePointer (event 64)

#include <cstring>   // strcmp

namespace BrnGui
{
namespace
{
    // The state IN-queue is an 18KB variable event queue (VariableEventQueue<18432,16>).
    typedef CgsModule::VariableEventQueue<18432, 16> MarkManInQueue;

    const s32 KI_CHANNEL_GUI_OUT = 40;

    // The input events this state handles (the screen listens on these five).
    const s32 KI_EVENT_CONTROLLER_ACTION    = 6;
    const s32 KI_EVENT_APT_TRIGGER          = 21;
    const s32 KI_EVENT_CONNECTION_LOST      = 44;
    const s32 KI_EVENT_GUI_CACHE            = 64;
    const s32 KI_EVENT_ONLINE_SCREEN_ADVANCE = 269;

    // The screen's apt package (its resource id; the movie name is that id's identifier).
    const u32 KU_MARK_MAN_RESOURCE_ID = 184;
    const s32 KI_MOVIE_LEVEL          = 3;
    const char* const KPC_NO_MOVIE_NAME = "";

    // The console's game-mode count (the waiting-text table has one row per mode); the
    // tree's GameStateModuleIO::E_MODE_COUNT is one short of it.
    const s32 KI_NUM_GAME_MODES = 18;
    const s32 KI_GAME_MODE_NONE = -1;

    // The waiting text per game mode: the stunt-run modes (12, 14, 17) have their own line.
    const char* const KAPC_MARK_MAN_WAITING_TEXT[KI_NUM_GAME_MODES] =
    {
        "$MARK_MAN_WAITING", "$MARK_MAN_WAITING", "$MARK_MAN_WAITING", "$MARK_MAN_WAITING",
        "$MARK_MAN_WAITING", "$MARK_MAN_WAITING", "$MARK_MAN_WAITING", "$MARK_MAN_WAITING",
        "$MARK_MAN_WAITING", "$MARK_MAN_WAITING", "$MARK_MAN_WAITING", "$MARK_MAN_WAITING",
        "$MARK_MAN_WAITING_STUNT_RUN", "$MARK_MAN_WAITING", "$MARK_MAN_WAITING_STUNT_RUN",
        "$MARK_MAN_WAITING", "$MARK_MAN_WAITING", "$MARK_MAN_WAITING_STUNT_RUN",
    };

    // { 4, 269, 12, 2 }: the advance record this screen posts on its way out.
    struct GuiEventOnlineScreenAdvance : public CgsGui::GuiEvent<KI_EVENT_ONLINE_SCREEN_ADVANCE>
    {
        s32 miValue;   // +0x0C

        GuiEventOnlineScreenAdvance()
            : CgsGui::GuiEvent<KI_EVENT_ONLINE_SCREEN_ADVANCE>(4, 12), miValue(2) {}
    };

    // { 1, 282, 12 }: posted once the screen's movie is up. The console leaves the payload
    // word unwritten.
    struct GuiEventMarkManLoaded : public CgsGui::GuiEvent<282>
    {
        u32 muReserved;   // +0x0C

        GuiEventMarkManLoaded() : CgsGui::GuiEvent<282>(1, 12), muReserved(0) {}
    };

    // The overlay request as the out-queue carries it: the GuiEvent<184> header sized to the
    // request, a pad word, then the request itself.
    struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
    {
        u32               muPad0C;    // +0x0C
        GuiOverlayRequest mRequest;   // +0x10
        GuiOverlayRequestWire()
            : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
            , muPad0C(0) {}
    };

    void PostOnlineScreenAdvance(CgsGui::StateInterface* lpStateInterface)
    {
        GuiEventOnlineScreenAdvance lAdvance;
        lpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAdvance), KI_CHANNEL_GUI_OUT, 16);
    }
}

const CgsGui::sResourceTuple OnlineMarkMan::maResourcesToLoad[1] =
    { { KU_MARK_MAN_RESOURCE_ID, CgsGui::E_GUI_RESOURCETYPE_APT } };
const u32 OnlineMarkMan::muNumResourcesToLoad = 1;
const s32 OnlineMarkMan::maiEventToObserve[5] =
    { KI_EVENT_CONTROLLER_ACTION, KI_EVENT_CONNECTION_LOST, KI_EVENT_APT_TRIGGER,
      KI_EVENT_GUI_CACHE, KI_EVENT_ONLINE_SCREEN_ADVANCE };
const s32 OnlineMarkMan::miNumEventsObserved = 5;
    // FLAG boundary no-op: the apt-component watcher half of the cache the X360 reaches is
    // not on the committed GuiCache public API. Faithful default is a no-op; mirrors the
    // committed BrnGui::BootLegalCacheBoundary::ClearExpectedAptComponentList body. GROW when
    // the GuiCache apt-component watcher lands.
    namespace OnlineMarkManCacheBoundary
    {
        void ClearExpectedAptComponentList(GuiCache* /*lpCache*/, s32 /*liFlow*/)
        {
        }
    }

    // ---- SetExpectedComponent @ 0x82483AC0 -------------------------------------
    // Append the hash of an expected APT-component name to the table. Asserts there
    // is room (count < 9), inline-strlens the name (length EXCLUDES the NUL terminator,
    // the byte count CgsHash::CalculateHash expects), stores the hash at
    // mauExpectedComponentIds[count] (X360 +0x40 + count*4), and increments the count.
    // The X360 returns the computed hash in r3 (twin RaceMainHudState::SetExpectedComponent
    // is homed u32; DWARF declares void, the sole caller ignores the return).
    u32 OnlineMarkMan::SetExpectedComponent(const char* lpcName)
    {
        CGS_ASSERT(muNumExpectedComponents < KU_MAX_INIT_COMPONENTS_NUM,
                   "No space for new expected component");

        const char* lpc = lpcName;
        while (*lpc)
        {
            ++lpc;
        }
        u32 luHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpcName), static_cast<int>(lpc - lpcName));   // length excludes the NUL

        mauExpectedComponentIds[muNumExpectedComponents] = luHash;
        ++muNumExpectedComponents;
        return luHash;
    }

    // ---- ClearExpectedComponent @ 0x82483BA8 -----------------------------------
    // Reset the expected-APT-component table: zero all nine hash slots
    // (mauExpectedComponentIds, X360 +0x40) and the live count
    // (muNumExpectedComponents, +0x64), then hand the (now empty) list to the cache's
    // apt-component watcher (flow 0, E_GUIFLOW_SCREEN). The X360 clears the array +
    // count BEFORE asserting the cache handle, so that order is preserved.
    void OnlineMarkMan::ClearExpectedComponent()
    {
        for (u32 luSlot = 0; luSlot < KU_MAX_INIT_COMPONENTS_NUM; ++luSlot)
        {
            mauExpectedComponentIds[luSlot] = 0;
        }
        muNumExpectedComponents = 0;

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        OnlineMarkManCacheBoundary::ClearExpectedAptComponentList(mpGuiCache, 0);
    }

    // ---- OnEnter ---------------------------------------------------------------
    // Listen on the five input events, forget the cache (UpdateGetCache latches it from event
    // 64), build the countdown icon and its text field, and show the game mode's waiting text
    // (read through the access pointers' cache). The sub-state machine starts at GETCACHE.
    void OnlineMarkMan::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mpGuiCache = 0;
        mTimeFieldIcon.Construct("TimeCounterIcon_mc", mpStateInterface, 0, 0);
        mTimeField.Construct("TimeCounter_mc", mpStateInterface, mTimeFieldIcon.GetName());

        GuiCache* lpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();
        CGS_ASSERT(lpGuiCache, "lpGuiCache");
        CGS_ASSERT(lpGuiCache->GetGameMode() != KI_GAME_MODE_NONE,
                   "lpGuiCache->GetGameMode() != GsmIO::E_MODE_NONE");
        CGS_ASSERT(lpGuiCache->GetGameMode() != KI_NUM_GAME_MODES,
                   "lpGuiCache->GetGameMode() != GsmIO::E_MODE_COUNT");
        mTimeField.SetText(KAPC_MARK_MAN_WAITING_TEXT[lpGuiCache->GetGameMode()]);

        meInternalState = E_INTERNALSTATE_GETCACHE;
    }

    // ---- OnLeave ---------------------------------------------------------------
    // Stop listening, unbind the screen's movie (the empty name at level 3) and go inert.
    void OnlineMarkMan::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mpStateInterface->PlayAptMovie(KPC_NO_MOVIE_NAME, KI_MOVIE_LEVEL);
        meInternalState = E_INTERNALSTATE_LEFT;
    }

    // ---- Update ----------------------------------------------------------------
    // The family's fall-through sub-state ladder: each rung re-stamps meInternalState and,
    // when it completes, falls straight into the next in the same frame. GETCACHE always falls
    // through. The permanent handler runs every frame (LEFT included) and the in-queue is
    // cleared at the tail.
    void OnlineMarkMan::Update()
    {
        switch (meInternalState)
        {
            case E_INTERNALSTATE_GETCACHE:
                UpdateGetCache();
                // fall through

            case E_INTERNALSTATE_LOADRESOURCES:
                meInternalState = E_INTERNALSTATE_LOADRESOURCES;
                if (!UpdateLoadResources())
                {
                    break;
                }
                // fall through

            case E_INTERNALSTATE_WFINIT:
                meInternalState = E_INTERNALSTATE_WFINIT;
                if (!UpdateWFInit())
                {
                    break;
                }
                // fall through

            case E_INTERNALSTATE_SETUP:
                meInternalState = E_INTERNALSTATE_SETUP;
                UpdateSetup();
                // fall through

            case E_INTERNALSTATE_SYNCING:
                meInternalState = E_INTERNALSTATE_SYNCING;
                UpdateSyncing();
                break;

            case E_INTERNALSTATE_LEFT:
                break;

            default:
                // The console streams "Invalid internal state : " << state << "\n".
                CGS_ASSERT(false, "Invalid internal state : ");
                break;
        }

        UpdatePermanent();
        reinterpret_cast<MarkManInQueue*>(mpInGuiEventQueue)->Clear();
    }

    // ---- UpdateGetCache --------------------------------------------------------
    // Latch the cache from the first cache-pointer event in the in-queue.
    void OnlineMarkMan::UpdateGetCache()
    {
        CGS_ASSERT(mpGuiCache == 0, "NULL == mpGuiCache");

        MarkManInQueue* lpInQueue = reinterpret_cast<MarkManInQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            if (liEventId == KI_EVENT_GUI_CACHE)
            {
                const GuiEventCachePointer* lpCacheEvent = reinterpret_cast<const GuiEventCachePointer*>(lpEvent);
                CGS_ASSERT(lpCacheEvent->mpCachePointer != 0, "NULL != lpCacheEvent->mpCachePointer");
                mpGuiCache = lpCacheEvent->mpCachePointer;
                break;
            }
        }

        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");
    }

    // ---- UpdateLoadResources ---------------------------------------------------
    // Once the screen's package is loaded: register the icon as the expected component, bind
    // the screen's movie and post the loaded record.
    bool OnlineMarkMan::UpdateLoadResources()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            return false;
        }

        SetExpectedAptComponentList();
        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_MARK_MAN_RESOURCE_ID], KI_MOVIE_LEVEL);

        GuiEventMarkManLoaded lLoaded;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lLoaded), KI_CHANNEL_GUI_OUT, 16);
        return true;
    }

    // ---- UpdateWFInit ----------------------------------------------------------
    bool OnlineMarkMan::UpdateWFInit()
    {
        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            return false;
        }
        ClearExpectedComponent();
        return true;
    }

    // ---- UpdateSetup -----------------------------------------------------------
    void OnlineMarkMan::UpdateSetup()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache != NULL");
        mTimeFieldIcon.SetState("idle");
    }

    // ---- UpdateSyncing ---------------------------------------------------------
    // If the local player dropped out of the session, post the advance record and advance.
    bool OnlineMarkMan::UpdateSyncing()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        if (mpGuiCache->GetOnlinePlayerDisconnected(
                static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex())))
        {
            PostOnlineScreenAdvance(mpStateInterface);
            SendStateEvent("ADVANCE");
        }
        return false;
    }

    // ---- UpdatePermanent -------------------------------------------------------
    // Every frame: when the countdown field's apt movie reports loaded, re-push its stored text;
    // a lost connection (while online) pops the "OnLostConn" overlay and advances; the advance
    // event advances.
    void OnlineMarkMan::UpdatePermanent()
    {
        MarkManInQueue* lpInQueue = reinterpret_cast<MarkManInQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            if (liEventId == KI_EVENT_APT_TRIGGER)
            {
                const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
                    reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
                if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD &&
                    std::strcmp(lpTrigger->mpacComponentName, mTimeField.GetName()) == 0)
                {
                    mTimeField.SetText(mTimeField.GetText());
                }
            }
            else if (liEventId == KI_EVENT_CONNECTION_LOST)
            {
                if (mpGuiCache != 0 && mpGuiCache->IsOnline())
                {
                    GuiOverlayRequestWire lWire;
                    lWire.mRequest.Construct("OnLostConn");
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(GuiOverlayRequestWire)));

                    PostOnlineScreenAdvance(mpStateInterface);
                    SendStateEvent("ADVANCE");
                }
            }
            else if (liEventId == KI_EVENT_ONLINE_SCREEN_ADVANCE)
            {
                PostOnlineScreenAdvance(mpStateInterface);
                SendStateEvent("ADVANCE");
            }
        }
    }

    // ---- SetExpectedAptComponentList -------------------------------------------
    // The countdown icon is the one component this screen waits for.
    void OnlineMarkMan::SetExpectedAptComponentList()
    {
        ClearExpectedComponent();
        SetExpectedComponent(mTimeFieldIcon.GetName());
        CGS_ASSERT(muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM,
                   "muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM");
        mpGuiCache->SetExpectedAptComponentList(E_GUIFLOW_SCREEN, mauExpectedComponentIds,
                                                muNumExpectedComponents);
    }
}
