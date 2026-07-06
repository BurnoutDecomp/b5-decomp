#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"   // BrnGui::TextField (embedded by value, 36-field bank)

// ===================================================================================
// BrnGui::CrashNavStats -- the crash-navigation post-crash "stats" screen state.
//
// Class shape / member order from the DecFIGS DWARF (BrnCrashNavStats.h:46): a CgsGui::State
// leaf that observes four stat GUI events, latches a GuiCache, and drives a bank of 36 embedded
// TextField components (one per stat readout). Byte offsets X360-attested from the XEX:
//   * meCurrentState     @ +0x38  (OnEnter sets 0 / OnLeave sets 4)
//   * mpGuiCache         @ +0x3C  (OnEnter clears; SetExpectedAptComponents reads it)
//   * maStatTextfields   @ +0x40  (36 x sizeof(TextField)==0x128 == 10656 bytes)
//   * mbDataReceived     @ +0x29E0 (== 0x40 + 36*0x128; OnEnter clears)
// Virtuals (OnEnter/OnLeave/Update/GetResourcesToLoad) + private helper set per the DWARF.
// ===================================================================================
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;

    // The stats-response GUI event carried into HandleStatData. Its field layout is not
    // DWARF-attested (BrnRivalMapPanel treats the same response as an opaque byte blob), so it
    // is an incomplete type reached only through the file-local StatsReader boundary in the .cpp
    // against the X360-attested byte offsets.
    struct GuiEventStatsResponse;

    struct CrashNavStats : public CgsGui::State
    {
        // Internal screen state machine (X360 this+0x38). DWARF BrnCrashNavStats.h:137.
        enum EInternalScreenState
        {
            E_INTERNALSCREENSTATE_SETUP        = 0,
            E_INTERNALSCREENSTATE_LOADING      = 1,
            E_INTERNALSCREENSTATE_INITIALISING = 2,
            E_INTERNALSCREENSTATE_RUNNING      = 3,
            E_INTERNALSCREENSTATE_LEAVING      = 4,
            E_RACEINTERNALSTATE_COUNT          = 5,
        };

        // Number of embedded stat text fields (X360 off_82F26DB0 name table span / bank stride).
        static const u32 KU_NUM_STAT_TEXTFIELDS = 36;

        // @ 0x824B5BE0 / 0x824CA8E0 -- the FSM virtuals.
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82500008 - hands the stats screen's static resource list to the loader.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // Per-phase Update helpers.
        bool UpdateInitSetup();
        bool UpdateLoading();
        bool UpdateInitialising();
        bool UpdateRunning();
        void UpdatePermanent();

        // @ 0x824B5D18 -- fill the 36 stat text fields from an incoming stats response.
        void HandleStatData(const GuiEventStatsResponse* lpStatsEvent);
        // @ 0x824B6370 -- trigger dispatch (null-event guard only).
        void HandleTriggers(const CgsModule::Event* lpEvent);
        // @ 0x824B6408 -- register the 36 stat fields as expected apt components.
        void SetExpectedAptComponents();

        // ---- members (over the CgsGui::State base) ----
        EInternalScreenState meCurrentState;                          // @ +0x38
        GuiCache*            mpGuiCache;                               // @ +0x3C
        TextField           maStatTextfields[KU_NUM_STAT_TEXTFIELDS]; // @ +0x40, stride 0x128
        bool                mbDataReceived;                           // @ +0x29E0

        // ---- statics ----
        // The four observed stat GUI event ids (X360 .rdata @&dword_820660CC, count == 4). The id
        // VALUES are not decoded in this packet (only the base address + count are attested) ->
        // placeholders so the state links; adopt the XEX-recovered ids when decoded.
        static const s32 maiEventToObserve[4];
        static const s32 miNumEventsObserved;     // == 4

        // The apt-clip names of the 36 stat text fields (X360 off_82F26DB0[] name table). The
        // concrete names are not decoded -> empty-string placeholders so the bank Constructs.
        static const char* const KAPC_STAT_TEXTFIELD_NAMES[KU_NUM_STAT_TEXTFIELDS];

        // The static resource list handed to the loader (X360 unk_82F26D88, count 1).
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F26D88 (unk_82F26D88, .rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F26D90 == 1
    };
}
