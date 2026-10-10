#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/Events/BrnGuiEventOnlinePostEvent.h"   // GuiEventOnlinePostEvent (by value)

// BrnGui::OnlineInstantResultsState - the online "instant results" post-event GUI
// state. Layout/virtuals from the reference declaration (BrnOnlineInstantResults.h); offsets
// from the console asm. The inline resource accessor hands out the static table and its count.
// Only the members the reconstructed bodies read are named; the console span between them is
// kept as padding.
namespace BrnGui
{
    class GuiCache;

    struct OnlineInstantResultsState : public CgsGui::State
    {
        // BrnOnlineInstantResults.h (the screen's internal sub-states).
        enum EInternalState
        {
            E_ONLINE_INSTANT_RESULTS_LOADING_SCREEN          = 0,
            E_ONLINE_INSTANT_RESULTS_LOADING_COMPONENTS      = 1,
            E_ONLINE_INSTANT_RESULTS_UPDATING                = 2,
            E_ONLINE_INSTANT_RESULTS_SHOWING                 = 3,
            E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE         = 4,
            E_ONLINE_INSTANT_RESULTS_CURRENT_STANDINGSUPDATE = 5,
            E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGSUPDATE   = 6,
            E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGS         = 7,
            E_ONLINE_INSTANT_RESULTS_LOADING_STANDINGS       = 8,
            E_ONLINE_INSTANT_RESULTS_DONE                    = 9,
        };

        // BrnOnlineInstantResults.h -- the ticker text of one online award: up to six
        // interchangeable string ids for a plural award value and six for a value of exactly
        // one, and how many message parameters (the player's name, then the value) each takes.
        struct AwardData
        {
            static const s32 KI_MAX_AWARD_STRINGS = 6;

            s8          miMultiParamCount;                          // +0x00
            s8          miSingleParamCount;                         // +0x01
            const char* mapcMultiString[KI_MAX_AWARD_STRINGS];      // +0x04
            const char* mapcSingleString[KI_MAX_AWARD_STRINGS];     // +0x1C

            //  /, header inline: the number of string ids before the first null.
            s32 CountStringsSingle() const { return CountStrings(mapcSingleString); }
            s32 CountStringsPlural() const { return CountStrings(mapcMultiString); }

        private:
            static s32 CountStrings(const char* const* lapcStrings)
            {
                s32 liNumStrings = 0;
                while (liNumStrings < KI_MAX_AWARD_STRINGS && lapcStrings[liNumStrings] != 0)
                {
                    ++liNumStrings;
                }
                return liNumStrings;
            }
        };

        // BrnOnlineInstantResults.cpp -- one entry per BrnGameState::EOnlineAwardID
        // (read from the image).
        static const s32       KI_NUM_ONLINE_AWARD_TYPES = 11;
        static const AwardData KA_AWARD_OVERALL[KI_NUM_ONLINE_AWARD_TYPES];

        // Hands the instant-results screen's resource list out.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        // BrnOnlineInstantResults.cpp. Clear the ticker, then (in the
        // standings-update sub-state) post one custom ticker message per award given.
        void FillOutTicker();

        static const CgsGui::sResourceTuple maResourceTuplesToLoad[];  // .rdata, 1 entry
        static const s32                    miNumResourcesToLoad;      // .rdata, == 1

        EInternalState          meCurrentState;                      // console +0x38
        // console +0x3C..+0xB153: mTable, maTextFields[40], maIcons[8], the two animation
        // components, maapTableCellComponentPtrs, mTableData, maTableRowDataSets[16],
        // miItemsLoaded, muNumTextFilesToLoad, mDisconnectedEvent.
        u8                      maPadToOnlinePostEvent[0xB154 - 0x3C];
        GuiEventOnlinePostEvent mOnlinePostEvent;                    // console +0xB154
        GuiCache*               mpGuiCache;                          // console +0xB38C
    };
}
