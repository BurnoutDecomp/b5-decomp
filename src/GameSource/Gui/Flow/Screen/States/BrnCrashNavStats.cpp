// ===================================================================================
// BrnGui::CrashNavStats -- out-of-line bodies for the crash-nav "stats" screen state.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   HandleStatData @0x824B5D18, HandleTriggers @0x824B6370, OnEnter @0x824B5BE0,
//   OnLeave @0x824CA8E0, SetExpectedAptComponents @0x824B6408, UpdateInitSetup @0x824CA970,
//   UpdateInitialising @0x824B5C60, UpdateLoading @0x824CAAA0, UpdatePermanent @0x824C1690,
//   Update @0x824D8318.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavStats.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsID.h"            // CgsID / CgsIDConvertToString
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"   // CgsLanguage::LanguageManager::ParameterFormatType
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"       // CgsGui::GuiEvent<N> / CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"   // StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"           // VariableEventQueue<18432,16> (in-queue view)
#include "GameSource/Gui/BrnGuiCache.h"                   // BrnGui::GuiCache
#include "GameSource/Gui/Events/BrnGuiEventStatsResponse.h"  // GuiEventStatsResponse (id 436)

#include <cstring>   // std::strcpy

namespace BrnGui
{
    // ---- static out-of-line definitions -------------------------------------------
    // .rdata @0x820660CC, read out of the image: { 6, 0x15, 0x40, 0x1B4 }. These are the four
    // ids UpdateInitSetup / UpdatePermanent actually switch on -- controller input (6), apt
    // trigger (21), GuiCache handover (64) and the stats response (436). 436 is corroborated
    // independently by its producer: AddGuiEvent<GuiEventStatsResponse> @0x823D71D8 posts
    // `AddEvent(&event, 436, 432)`.
    // A state that registers for the WRONG ids receives nothing, so the placeholder this
    // replaces would have left the tab blank and unnavigable even once it compiled.
    const s32 CrashNavStats::maiEventToObserve[4] = { 6, 21, 64, 436 };   // @ 0x820660CC
    const s32 CrashNavStats::miNumEventsObserved  = 4;

    // Apt-clip names of the 36 stat text fields, read out of the pointer table at .rdata
    // 0x82F26DB0 (36 entries, the exact span OnEnter's `addi r28, r10, 0x6db0` .. `addi r11,
    // r28, 0x90` loop walks). ⭐ AN EMPTY NAME IS A TOTAL NO-OP IN THE APT PATH: the component
    // hash of "" matches no clip, so every field would bind to nothing and the tab would draw
    // with all 36 numbers missing. That is exactly what the placeholder this replaces did.
    //
    // ⭐⭐ THESE NAMES CROSS-VALIDATE THE RECORD LAYOUT. Every one of the 36 matches, by
    // meaning, the GuiEventStatsResponse member HandleStatData reads into it below -- e.g.
    // index 24 is "nem_cpt" (nemesis) against mGreatestRivalId, 34/35 are "timeRR_cpt"/
    // "crashRR_cpt" against mRoadsRuledTime/mRoadsRuledCrash. Two independently recovered
    // tables agreeing 36 times is what pins both.
    const char* const CrashNavStats::KAPC_STAT_TEXTFIELD_NAMES[CrashNavStats::KU_NUM_STAT_TEXTFIELDS] =
    {
        "distOff_cpt",    "distOn_cpt",       "totTime_cpt",    "carsWon_cpt",
        "carsTotal_cpt",  "allMedal_cpt",     "allMedalTot_cpt", "eventMedal_cpt",
        "eventMedalTot_cpt", "RR_cpt",        "totalRR_cpt",    "drivers_cpt",
        "driversTot_cpt", "golds_cpt",        "silvers_cpt",    "bronzes_cpt",
        "jumps_cpt",      "jumpsTot_cpt",     "smash_cpt",      "smashTot_cpt",
        "stunt_cpt",      "stuntTot_cpt",     "faveCar_cpt",    "forCar_cpt",
        "nem_cpt",        "totTd_cpt",        "tdStd_cpt",      "tdVert_cpt",
        "tdTBone_cpt",    "tdAfter_cpt",      "tdCar_cpt",      "tdVan_cpt",
        "tdBus_cpt",      "tdBig_cpt",        "timeRR_cpt",     "crashRR_cpt",
    };

    // Single-APT resource list, read out of the image at .rdata 0x82F26D88: { 0x89, 4 } ==
    // { apt id 137, E_GUI_RESOURCETYPE_APT }, with the count word at 0x82F26D90 == 1 (the two
    // addresses UpdateLoading @0x824CAAA0 loads: `addi r4, r11, 0x6d88` / `lwz r5, 0x6d90`).
    // Resource id 0 -- the placeholder this replaces -- is a different resource, so the screen
    // would have waited on, and then drawn, the wrong movie.
    const CgsGui::sResourceTuple CrashNavStats::maResourcesToLoad[] =
    {
        { 137u, CgsGui::E_GUI_RESOURCETYPE_APT },
    };
    const u32 CrashNavStats::muNumResourcesToLoad = 1;

    // ---- file-local boundaries / helpers ------------------------------------------
    // The apt-component watcher half of the cache the X360 reaches is not on the committed
    // GuiCache public API; faithful default is a no-op (mirrors the committed
    // OnlineMarkManCacheBoundary::ClearExpectedAptComponentList). GROW when it lands.
    namespace CrashNavStatsCacheBoundary
    {
        void ClearExpectedAptComponentList(GuiCache* /*lpCache*/, GuiFlow /*leFlow*/)
        {
        }
    }

    // The state in-queue is an 18KB variable event queue (X360 VariableEventQueue<18432,16>),
    // reached through the base State's mpInGuiEventQueue (the committed BrnCredits idiom).
    typedef CgsModule::VariableEventQueue<18432, 16> CrashNavStatsInQueue;

    // The {1, 435, 12} "setup done" command posted on channel 40 (16 bytes). Opaque-event
    // boundary: modelled as a GuiEvent<435> so the AddEvent size/type match the asm.
    struct GuiEventSetupDone : public CgsGui::GuiEvent<435>
    {
        GuiEventSetupDone() : CgsGui::GuiEvent<435>(1, 12) {}
    };

    // Stats apt-movie binding (UpdateLoading PlayAptMovie record).
    static const char* const KPC_STATS_MOVIE_NAME = "";   // empty-string movie name sentinel
    static const s32         KI_STATS_MOVIE_LEVEL = 3;    // channel-41 level word
    static const s32         KI_ACTION_GO_BACK    = 50;   // UpdatePermanent id-6 sub-action

    // ---- OnEnter @ 0x824B5BE0 -----------------------------------------------------
    void CrashNavStats::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        meCurrentState = E_INTERNALSCREENSTATE_SETUP;   // this+0x38 = 0
        mpGuiCache     = 0;                             // this+0x3C = 0
        mbDataReceived = false;                         // this+0x29E0 = 0

        for (u32 luField = 0; luField < KU_NUM_STAT_TEXTFIELDS; ++luField)
            maStatTextfields[luField].Construct(KAPC_STAT_TEXTFIELD_NAMES[luField], mpStateInterface, 0);
    }

    // ---- OnLeave @ 0x824CA8E0 -----------------------------------------------------
    void CrashNavStats::OnLeave()
    {
        meCurrentState = E_INTERNALSCREENSTATE_LEAVING;   // this+0x38 = 4 (set before UnRegister in asm)
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mpStateInterface->PlayAptMovie("", 3);
    }

    // ---- HandleStatData @ 0x824B5D18 ----------------------------------------------
    void CrashNavStats::HandleStatData(const GuiEventStatsResponse* lpStatsEvent)
    {
        CGS_ASSERT(lpStatsEvent != 0, "lpStatsEvent");   // cpp:447

        // READ BY NAME, NOT BY OFFSET. GuiEventStatsResponse (BrnGuiEventStatsResponse.h)
        // names every field of this 432-byte record, and its own banner calls out BOTH of its
        // consumers for having read it through file-local byte cursors -- this TU was the
        // second one. The offsets that cursor used are preserved in that header's comments.
        // The 39 SetLocalisedText calls below are the console's 39, in the console's order
        // (which writes golds/silvers/bronzes TWICE -- that repeat is the X360's, not a slip
        // here; the body has exactly 39 `bl ...SetLocalisedText` at 0x824B5D18).

        char lacBuffer[128];   // X360 sp scratch (formatter capped at 63)
        char lacId[16];        // X360 CgsID expansion buffer
        lacBuffer[63] = 0;

        typedef CgsLanguage::LanguageManager LM;

        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miDistanceOffline);      maStatTextfields[0].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miDistanceOnline);       maStatTextfields[1].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miTimePlayed);           maStatTextfields[2].SetLocalisedText(lacBuffer, LM::E_FORMAT_MINUTES_SECONDS);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miCarsCollected);        maStatTextfields[3].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miCarsTotal);            maStatTextfields[4].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miGolds);                maStatTextfields[13].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miSilvers);              maStatTextfields[14].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miBronzes);              maStatTextfields[15].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miAllMedalsEarned);      maStatTextfields[5].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miAllMedalsTotal);       maStatTextfields[6].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miEventMedalsEarned);    maStatTextfields[7].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miEventMedalsTotal);     maStatTextfields[8].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miRoadRules);            maStatTextfields[9].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->mRoadsRuledTotal);       maStatTextfields[10].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miDrivers);              maStatTextfields[11].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miDriversTot);           maStatTextfields[12].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miGolds);                maStatTextfields[13].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miSilvers);              maStatTextfields[14].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miBronzes);              maStatTextfields[15].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miJumps);                maStatTextfields[16].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miJumpTot);              maStatTextfields[17].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miSmashes);              maStatTextfields[18].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miSmashTot);             maStatTextfields[19].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miStunts);               maStatTextfields[20].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miStuntTot);             maStatTextfields[21].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);

        // ---- car ids (CAR_<id>) ----
        CgsIDConvertToString(lpStatsEvent->mFaveCarId, lacId);
        CgsCore::SPrintf(lacBuffer, 63, "CAR_%s", lacId);
        maStatTextfields[22].SetLocalisedText(lacBuffer, LM::E_FORMAT_ID_LOOKUP);
        CgsIDConvertToString(lpStatsEvent->mForgottenCarId, lacId);
        CgsCore::SPrintf(lacBuffer, 63, "CAR_%s", lacId);
        maStatTextfields[23].SetLocalisedText(lacBuffer, LM::E_FORMAT_ID_LOOKUP);

        // ---- "nem_cpt": the nemesis id (RVL_<u64>), or a dash when there is no rival yet ----
        const CgsID lRivalId = lpStatsEvent->mGreatestRivalId;
        LM::ParameterFormatType leRivalFormat;
        if (lRivalId != 0)
        {
            CgsCore::SPrintf(lacBuffer, 63, "RVL_%llu", lRivalId);
            leRivalFormat = LM::E_FORMAT_ID_LOOKUP;
        }
        else
        {
            leRivalFormat = LM::E_FORMAT_TEXT;
            std::strcpy(lacBuffer, "-");
        }
        maStatTextfields[24].SetLocalisedText(lacBuffer, leRivalFormat);

        // ---- "totTd_cpt": the running total of the eight per-type takedown counts ----
        const s32 liTotalTakedowns =
            lpStatsEvent->miStandardTakedowns + lpStatsEvent->miVerticalTakedowns +
            lpStatsEvent->miTBoneTakedowns    + lpStatsEvent->miAftertouchTakedowns +
            lpStatsEvent->miCarTakedowns      + lpStatsEvent->miVanTakedowns +
            lpStatsEvent->miBusTakedowns      + lpStatsEvent->miBigRigTakedowns;
        CgsCore::SPrintf(lacBuffer, 63, "%d", liTotalTakedowns);
        maStatTextfields[25].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);

        // ---- the eight counts themselves, then the two road-rule columns ----
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miStandardTakedowns);    maStatTextfields[26].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miVerticalTakedowns);    maStatTextfields[27].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miTBoneTakedowns);       maStatTextfields[28].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miAftertouchTakedowns);  maStatTextfields[29].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miCarTakedowns);         maStatTextfields[30].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miVanTakedowns);         maStatTextfields[31].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miBusTakedowns);         maStatTextfields[32].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->miBigRigTakedowns);      maStatTextfields[33].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->mRoadsRuledTime);        maStatTextfields[34].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lpStatsEvent->mRoadsRuledCrash);       maStatTextfields[35].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
    }

    // ---- HandleTriggers @ 0x824B6370 ----------------------------------------------
    void CrashNavStats::HandleTriggers(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event in CrashNavStats::HandleTriggers");   // cpp:615
    }

    // ---- SetExpectedAptComponents @ 0x824B6408 ------------------------------------
    void CrashNavStats::SetExpectedAptComponents()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        CrashNavStatsCacheBoundary::ClearExpectedAptComponentList(mpGuiCache, E_GUIFLOW_SCREEN);

        for (u32 luField = 0; luField < KU_NUM_STAT_TEXTFIELDS; ++luField)
        {
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   maStatTextfields[luField].GetName());
        }
    }

    // ---- UpdateInitSetup @ 0x824CA970 ---------------------------------------------
    bool CrashNavStats::UpdateInitSetup()
    {
        CrashNavStatsInQueue* lpInQueue =
            reinterpret_cast<CrashNavStatsInQueue*>(mpInGuiEventQueue);

        bool lbSetupDone = false;

        const CgsModule::Event* lpEvent = NULL;
        s32 liSize = 0;
        s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != NULL)
        {
            if (liEventId == 64)
            {
                GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
                CGS_ASSERT(lpCache, "Invalid cache in RaceMainHudState::OnEnter");

                mpGuiCache = lpCache;   // X360 stw r10,0x3C(r28)

                GuiEventSetupDone lSetupEvent;
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lSetupEvent), 40, 16);

                lbSetupDone = true;
            }

            liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }

        return lbSetupDone;
    }

    // ---- UpdateLoading @ 0x824CAAA0 -----------------------------------------------
    bool CrashNavStats::UpdateLoading()
    {
        CGS_ASSERT(mpGuiCache, "NULL != mpGuiCache");

        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
            return false;

        mpStateInterface->PlayAptMovie(KPC_STATS_MOVIE_NAME, KI_STATS_MOVIE_LEVEL);
        SetExpectedAptComponents();

        return true;
    }

    // ---- UpdateInitialising @ 0x824B5C60 ------------------------------------------
    bool CrashNavStats::UpdateInitialising()
    {
        CGS_ASSERT(mpGuiCache, "NULL != mpGuiCache");

        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            return false;

        if (mbDataReceived)
        {
            for (u32 luField = 0; luField < KU_NUM_STAT_TEXTFIELDS; ++luField)
            {
                maStatTextfields[luField].SetText(maStatTextfields[luField].GetText());
            }
        }

        return true;
    }

    // ---- Update @ 0x824D8318 ------------------------------------------------------
    // The family's fall-through sub-state ladder: each rung re-stamps meCurrentState and,
    // when its Update* returns true, falls straight into the next rung in the SAME frame, so
    // a fast load settles in one Update. The X360 dispatches 0..4 through the jump table at
    // 0x824D8350 and asserts above it.
    //
    // THIS LADDER IS NOT CrashNavColourCalibrate'S. There, every early rung clears the
    // "run permanent" flag. Here `li r27, 1` runs BEFORE the switch and ONLY the LEAVING arm
    // clears it (`li r27, 0` @0x824D83C4), so UpdatePermanent runs on frames where the screen
    // is still loading -- which is how the stats response (event 436) gets latched into
    // mbDataReceived before the apt components exist, ready for UpdateInitialising to push
    // into the fields. Copying the sibling's shape here would drop that event.
    void CrashNavStats::Update()
    {
        bool lbRunPermanent = true;   // `li r27, 1` before the switch

        switch (meCurrentState)
        {
        case E_INTERNALSCREENSTATE_SETUP:
            meCurrentState = E_INTERNALSCREENSTATE_SETUP;
            if (!UpdateInitSetup())
            {
                break;
            }
            // fall through

        case E_INTERNALSCREENSTATE_LOADING:
            meCurrentState = E_INTERNALSCREENSTATE_LOADING;
            if (!UpdateLoading())
            {
                break;
            }
            // fall through

        case E_INTERNALSCREENSTATE_INITIALISING:
            meCurrentState = E_INTERNALSCREENSTATE_INITIALISING;
            if (!UpdateInitialising())
            {
                break;
            }
            // fall through

        case E_INTERNALSCREENSTATE_RUNNING:
            // The DWARF's UpdateRunning (cpp:374) is folded to exactly this by the X360
            // compiler -- stay in RUNNING, no call.
            meCurrentState = E_INTERNALSCREENSTATE_RUNNING;
            break;

        case E_INTERNALSCREENSTATE_LEAVING:
            meCurrentState = E_INTERNALSCREENSTATE_LEAVING;
            lbRunPermanent = false;
            break;

        default:
            // X360 cpp:207 -- the streamed form, "Invalid internal state (" << state << ").
            CGS_ASSERT(false, "Invalid internal state");
            break;
        }

        if (lbRunPermanent)
        {
            UpdatePermanent();
        }

        // The in-queue is cleared unconditionally, so an event no rung consumed is dropped.
        reinterpret_cast<CrashNavStatsInQueue*>(mpInGuiEventQueue)->Clear();
    }

    // ---- UpdatePermanent @ 0x824C1690 ---------------------------------------------
    void CrashNavStats::UpdatePermanent()
    {
        CrashNavStatsInQueue* lpInQueue = reinterpret_cast<CrashNavStatsInQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = NULL;
        s32 liSize = 0;
        s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != NULL)
        {
            switch (liEventId)
            {
                case 6:
                {
                    const s32 liAction =
                        *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);
                    if (liAction == KI_ACTION_GO_BACK)
                        SendStateEvent("GO_BACK");
                    break;
                }
                case 21:
                    HandleTriggers(lpEvent);
                    break;
                case 436:
                    mbDataReceived = true;
                    HandleStatData(reinterpret_cast<const GuiEventStatsResponse*>(lpEvent));
                    break;
            }

            liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }
}
