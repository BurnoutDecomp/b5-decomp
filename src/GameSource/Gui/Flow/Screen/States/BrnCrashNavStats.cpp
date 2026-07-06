// ===================================================================================
// BrnGui::CrashNavStats -- out-of-line bodies for the crash-nav "stats" screen state.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   HandleStatData @0x824B5D18, HandleTriggers @0x824B6370, OnEnter @0x824B5BE0,
//   OnLeave @0x824CA8E0, SetExpectedAptComponents @0x824B6408, UpdateInitSetup @0x824CA970,
//   UpdateInitialising @0x824B5C60, UpdateLoading @0x824CAAA0, UpdatePermanent @0x824C1690.
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

#include <cstring>   // std::strcpy

namespace BrnGui
{
    // ---- static out-of-line definitions -------------------------------------------
    // Four observed stat event ids (values not decoded; placeholders so the state links).
    const s32 CrashNavStats::maiEventToObserve[4] = { 0, 0, 0, 0 };   // @ 0x820660CC (values not recovered)
    const s32 CrashNavStats::miNumEventsObserved  = 4;

    // Apt-clip names of the 36 stat text fields (off_82F26DB0[]; names not recovered).
    const char* const CrashNavStats::KAPC_STAT_TEXTFIELD_NAMES[CrashNavStats::KU_NUM_STAT_TEXTFIELDS] =
    {
        "", "", "", "", "", "", "", "", "", "", "", "",
        "", "", "", "", "", "", "", "", "", "", "", "",
        "", "", "", "", "", "", "", "", "", "", "", "",
    };

    // Single-APT resource list (unk_82F26D88, count 1). The tuple id/type are not attested in
    // scope; every sibling state's list is one {apt-id, E_GUI_RESOURCETYPE_APT} tuple.
    const CgsGui::sResourceTuple CrashNavStats::maResourcesToLoad[] =
    {
        { 0u, CgsGui::E_GUI_RESOURCETYPE_APT },   // FLAG: id unattested in scope
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

        // File-local boundary over the opaque stats-response event (layout not DWARF-attested).
        const u8* lpcEvent = reinterpret_cast<const u8*>(lpStatsEvent);
        struct StatsReader
        {
            const u8* mpBase;
            s32   Word(u32 luOffset) const { return *reinterpret_cast<const s32*>(mpBase + luOffset); }
            CgsID Id(u32 luOffset)   const { return *reinterpret_cast<const CgsID*>(mpBase + luOffset); }
            u64   U64(u32 luOffset)  const { return *reinterpret_cast<const u64*>(mpBase + luOffset); }
        } lReader = { lpcEvent };

        char lacBuffer[128];   // X360 sp scratch (formatter capped at 63)
        char lacId[16];        // X360 CgsID expansion buffer
        lacBuffer[63] = 0;

        typedef CgsLanguage::LanguageManager LM;

        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x1C)); maStatTextfields[0].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x18)); maStatTextfields[1].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x20)); maStatTextfields[2].SetLocalisedText(lacBuffer, LM::E_FORMAT_MINUTES_SECONDS);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x24)); maStatTextfields[3].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x28)); maStatTextfields[4].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x5C)); maStatTextfields[13].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x60)); maStatTextfields[14].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x64)); maStatTextfields[15].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x34)); maStatTextfields[5].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x38)); maStatTextfields[6].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x3C)); maStatTextfields[7].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x40)); maStatTextfields[8].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x4C)); maStatTextfields[9].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x50)); maStatTextfields[10].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x54)); maStatTextfields[11].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x58)); maStatTextfields[12].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x5C)); maStatTextfields[13].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x60)); maStatTextfields[14].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x64)); maStatTextfields[15].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x68)); maStatTextfields[16].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x6C)); maStatTextfields[17].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x70)); maStatTextfields[18].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x74)); maStatTextfields[19].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x78)); maStatTextfields[20].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x7C)); maStatTextfields[21].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);

        // ---- car ids (CAR_<id>) ----
        CgsIDConvertToString(lReader.Id(0x00), lacId);
        CgsCore::SPrintf(lacBuffer, 63, "CAR_%s", lacId);
        maStatTextfields[22].SetLocalisedText(lacBuffer, LM::E_FORMAT_ID_LOOKUP);
        CgsIDConvertToString(lReader.Id(0x08), lacId);
        CgsCore::SPrintf(lacBuffer, 63, "CAR_%s", lacId);
        maStatTextfields[23].SetLocalisedText(lacBuffer, LM::E_FORMAT_ID_LOOKUP);

        // ---- rival id (RVL_<u64>) or a placeholder when absent ----
        const u64 luRivalId = lReader.U64(0x10);
        LM::ParameterFormatType leRivalFormat;
        if (luRivalId != 0)
        {
            CgsCore::SPrintf(lacBuffer, 63, "RVL_%llu", luRivalId);
            leRivalFormat = LM::E_FORMAT_ID_LOOKUP;
        }
        else
        {
            leRivalFormat = LM::E_FORMAT_TEXT;
            std::strcpy(lacBuffer, "-");
        }
        maStatTextfields[24].SetLocalisedText(lacBuffer, leRivalFormat);

        // ---- running total of the eight per-medal counts (event +0x8C .. +0xA8) ----
        const s32 liTotal = lReader.Word(0x8C) + lReader.Word(0x90) + lReader.Word(0x94) +
                            lReader.Word(0x98) + lReader.Word(0x9C) + lReader.Word(0xA0) +
                            lReader.Word(0xA4) + lReader.Word(0xA8);
        CgsCore::SPrintf(lacBuffer, 63, "%d", liTotal);
        maStatTextfields[25].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);

        // ---- the ten per-medal counts themselves (+0x8C .. +0xB0) ----
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x8C)); maStatTextfields[26].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x90)); maStatTextfields[27].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x94)); maStatTextfields[28].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x98)); maStatTextfields[29].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0x9C)); maStatTextfields[30].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0xA0)); maStatTextfields[31].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0xA4)); maStatTextfields[32].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0xA8)); maStatTextfields[33].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0xAC)); maStatTextfields[34].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
        CgsCore::SPrintf(lacBuffer, 63, "%d", lReader.Word(0xB0)); maStatTextfields[35].SetLocalisedText(lacBuffer, LM::E_FORMAT_INTEGER);
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
