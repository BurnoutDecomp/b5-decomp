// Bodies for the online scoreboard browser debug component, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                     @ 0x82585858
//   Prepare                       @ 0x825858D8
//   Release                       @ 0x82585918
//   GetName                       @ 0x82585958 -> "Scoreboards"
//   GetIndexes (static cb)        @ 0x82585968   (Category changed -> select indexes)
//   GetVariations (static cb)     @ 0x825859C0   (Index changed    -> select variations)
//   GetScoreboard (static cb)     @ 0x82585A28   (Variation changed-> show scoreboard)
//   HandleScoreboardHeadingEvent  @ 0x82585A88
//   OnActivate                    @ 0x8258AE08
//   HandleScoreboardEvent         @ 0x8258AE38
//
// The component implements a Category -> Index -> Variation -> Scoreboard drill-down inside the debug
// menu. When a downloaded heading list arrives (HandleScoreboardHeadingEvent), it (re)builds the matching
// drop-down: copies each heading name into its char buffer, fills the parallel DebugUI::StringList option
// table, resets the selection int to 0, unregisters the now-stale downstream variables, then registers the
// variable with its change/select callbacks, range and option list. Changing a selection fires the matching
// static callback, which dispatches a freshly built NetworkInSelectScoreboardEvent through the manager to
// pull the next level down.
//
// The X360 inlines a "copy string with length check" CgsStringUtils helper (the per-name strlen + "String
// too long: <name>" StrStream assert at 0x82585C1C / 0x82585DC0 / 0x82585F70) and the three streamed
// assert messages; all are reduced to CGS_ASSERT per project convention. The decompiler's sub_8282D720 is
// DebugComponent::RegisterVariable(s32*, name); sub_8282F598 is DebugComponent::SetRange(s32*, min, max).

#include "GameSource/Network/Debug Components/BrnNetworkScoreboardDebugComponent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstring>                                    // std::strlen / std::strncpy

namespace BrnNetwork
{
    using CgsDev::DebugUI::StringList;
    namespace IO = BrnNetwork::BrnNetworkModuleIO;

    // ----------------------------------------------------------------------------------------------
    // Construct / lifecycle
    // ----------------------------------------------------------------------------------------------

    // @ 0x82585858. Cache the manager back-pointer, build the working scoreboard, reset the three
    // selections to "none" and register the component with the debug menu.
    void ScoreboardDebugComponent::Construct(ScoreboardManager* lpScoreboardManager)
    {
        CGS_ASSERT(lpScoreboardManager != nullptr, "lpScoreboardManager");
        mpScoreboardManager = lpScoreboardManager;

        mScoreboard.Construct();

        miCategory  = KI_NONE_SELECTED;
        miIndex     = KI_NONE_SELECTED;
        miVariation = KI_NONE_SELECTED;

        Register();
    }

    // @ 0x825858D8. Re-prepare: reset selections to "none" and rebuild the scoreboard. Note the X360
    // body calls Scoreboard::Construct (not Prepare) here, matching Construct.
    bool ScoreboardDebugComponent::Prepare()
    {
        miCategory  = KI_NONE_SELECTED;
        miIndex     = KI_NONE_SELECTED;
        miVariation = KI_NONE_SELECTED;

        mScoreboard.Construct();
        return true;
    }

    // @ 0x82585918. Reset selections to "none" and tear down the scoreboard.
    bool ScoreboardDebugComponent::Release()
    {
        miCategory  = KI_NONE_SELECTED;
        miIndex     = KI_NONE_SELECTED;
        miVariation = KI_NONE_SELECTED;

        mScoreboard.Destruct();
        return true;
    }

    // @ 0x82585958.
    const char* ScoreboardDebugComponent::GetName() const
    {
        return "Scoreboards";
    }

    // @ 0x8258AE08. When the debug menu opens this component, kick off the browse at the top level:
    // dispatch a "get headings" select event (meSelectType == 1). The X360 builds the 16-byte event on
    // the stack and writes ONLY the +0xC discriminator; the three value fields are left uninitialised
    // (they are not consumed by the top-level request).
    void ScoreboardDebugComponent::OnActivate()
    {
        IO::NetworkInSelectScoreboardEvent lEvent;
        lEvent.meSelectType = 1;   // top-level: select category list / download headings (EState GETTING_HEADINGS)
        DispatchScoreboardEvent(mpScoreboardManager, &lEvent);
    }

    // ----------------------------------------------------------------------------------------------
    // Menu-change callbacks (static; lpUserData is the component). Each commits the current selection
    // for its level by dispatching the matching select event, but only once every higher level has
    // been chosen (the -1 guards). The leading lpValue (the changed variable) is unused.
    // ----------------------------------------------------------------------------------------------

    // @ 0x82585968. Category changed -> request the index list for that category.
    void ScoreboardDebugComponent::GetIndexes(void* /*lpValue*/, void* lpUserData)
    {
        ScoreboardDebugComponent* lpThis = static_cast<ScoreboardDebugComponent*>(lpUserData);
        if (lpThis->miCategory != KI_NONE_SELECTED)
        {
            lpThis->UnregisterVariable(&lpThis->miIndex);
            IO::NetworkInSelectScoreboardEvent lEvent =
                IO::NetworkInSelectScoreboardEvent::GetIndexes(lpThis->miCategory);
            DispatchScoreboardEvent(lpThis->mpScoreboardManager, &lEvent);
        }
    }

    // @ 0x825859C0. Index changed -> request the variation list (needs both category and index set).
    void ScoreboardDebugComponent::GetVariations(void* /*lpValue*/, void* lpUserData)
    {
        ScoreboardDebugComponent* lpThis = static_cast<ScoreboardDebugComponent*>(lpUserData);
        if (lpThis->miCategory != KI_NONE_SELECTED && lpThis->miIndex != KI_NONE_SELECTED)
        {
            lpThis->UnregisterVariable(&lpThis->miVariation);
            IO::NetworkInSelectScoreboardEvent lEvent =
                IO::NetworkInSelectScoreboardEvent::GetVariations(lpThis->miIndex);
            DispatchScoreboardEvent(lpThis->mpScoreboardManager, &lEvent);
        }
    }

    // @ 0x82585A28. Variation changed -> show the scoreboard (needs all three levels set).
    void ScoreboardDebugComponent::GetScoreboard(void* /*lpValue*/, void* lpUserData)
    {
        ScoreboardDebugComponent* lpThis = static_cast<ScoreboardDebugComponent*>(lpUserData);
        if (lpThis->miCategory != KI_NONE_SELECTED && lpThis->miIndex != KI_NONE_SELECTED &&
            lpThis->miVariation != KI_NONE_SELECTED)
        {
            IO::NetworkInSelectScoreboardEvent lEvent =
                IO::NetworkInSelectScoreboardEvent::GetScoreboard(lpThis->miVariation);
            DispatchScoreboardEvent(lpThis->mpScoreboardManager, &lEvent);
        }
    }

    // ----------------------------------------------------------------------------------------------
    // Heading-list ingest
    // ----------------------------------------------------------------------------------------------

    // @ 0x82585A88. A heading list arrived from the server; rebuild the matching debug-menu drop-down.
    void ScoreboardDebugComponent::HandleScoreboardHeadingEvent(
        IO::NetworkOutScoreboardHeadingList* lpHeadingEvent)
    {
        CGS_ASSERT(lpHeadingEvent != nullptr, "lpHeadingEvent");
        CGS_ASSERT(lpHeadingEvent->meHeadingType != IO::E_HEADING_COUNT,
                   "meHeadingType != E_HEADING_COUNT");

        const s32 liCount = lpHeadingEvent->miLength;

        switch (lpHeadingEvent->meHeadingType)
        {
        case IO::E_HEADING_CATEGORY:
        {
            for (s32 li = 0; li < liCount; ++li)
            {
                const char* lpcName = lpHeadingEvent->maHeadings[li];
                CGS_ASSERT(std::strlen(lpcName) < static_cast<u32>(KI_NAME_LENGTH), "String too long");
                std::strncpy(maacCategories[li], lpcName, KI_NAME_LENGTH);
                maCategoryList[li].miValue = li;
                maCategoryList[li].mpcName = maacCategories[li];
            }

            miCategory = 0;
            UnregisterVariable(&miIndex);
            UnregisterVariable(&miVariation);
            RegisterVariable(&miCategory, "Category");
            SetChangeCallback(&miCategory, &GetIndexes, this);
            SetSelectCallback(&miCategory, &GetIndexes, this);
            SetRange(&miCategory, 0, liCount - 1);
            SetOptions(&miCategory, maCategoryList);
            break;
        }

        case IO::E_HEADING_INDEX:
        {
            for (s32 li = 0; li < liCount; ++li)
            {
                const char* lpcName = lpHeadingEvent->maHeadings[li];
                CGS_ASSERT(std::strlen(lpcName) < static_cast<u32>(KI_NAME_LENGTH), "String too long");
                std::strncpy(maacIndexes[li], lpcName, KI_NAME_LENGTH);
                maIndexList[li].miValue = li;
                maIndexList[li].mpcName = maacIndexes[li];
            }

            miIndex = 0;
            UnregisterVariable(&miVariation);
            RegisterVariable(&miIndex, "Index");
            SetChangeCallback(&miIndex, &GetVariations, this);
            SetSelectCallback(&miIndex, &GetVariations, this);
            SetRange(&miIndex, 0, liCount - 1);
            SetOptions(&miIndex, maIndexList);
            break;
        }

        case IO::E_HEADING_VARIATION:
        {
            for (s32 li = 0; li < liCount; ++li)
            {
                const char* lpcName = lpHeadingEvent->maHeadings[li];
                CGS_ASSERT(std::strlen(lpcName) < static_cast<u32>(KI_NAME_LENGTH), "String too long");
                std::strncpy(maacVariations[li], lpcName, KI_NAME_LENGTH);
                maVariationList[li].miValue = li;
                maVariationList[li].mpcName = maacVariations[li];
            }

            miVariation = 0;
            RegisterVariable(&miVariation, "Variation");
            SetChangeCallback(&miVariation, &GetScoreboard, this);
            SetSelectCallback(&miVariation, &GetScoreboard, this);
            SetRange(&miVariation, 0, liCount - 1);
            SetOptions(&miVariation, maVariationList);
            break;
        }

        default:
            // meHeadingType outside [0,2] (and != COUNT): a programming error. The X360 streams the
            // numeric type into the assert message ("Unhandled scoreboard event type: <n>\n").
            CGS_ASSERT(false, "Unhandled scoreboard event type");
            break;
        }
    }

    // @ 0x8258AE38. A downloaded scoreboard arrived; copy it into the working scoreboard and (re)render
    // it. The X360 memcpy's the whole 0xB6C-byte Scoreboard payload, then calls PrintScoreboard(this).
    void ScoreboardDebugComponent::HandleScoreboardEvent(const Scoreboard* lpScoreboard)
    {
        CGS_ASSERT(lpScoreboard != nullptr, "lpScoreboardEvent");
        mScoreboard = *lpScoreboard;   // X360: memcpy(this+0xC, src, 0xB6C) -- whole Scoreboard payload
        PrintScoreboard();             // X360 tail call: render the freshly copied scoreboard (declared-only)
    }
}
