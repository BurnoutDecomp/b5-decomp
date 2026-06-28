#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"      // CgsDev::DebugComponent (real base)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"            // CgsDev::DebugUI::StringList
#include "GameSource/Network/Managers/BrnNetworkScoreboard.h"                           // BrnNetwork::Scoreboard (by-value member)
#include "GameSource/Network/SharedIO/BrnNetworkOutScoreboardHeadingList.h"            // NetworkOutScoreboardHeadingList (heading param)
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"                               // NetworkInSelectScoreboardEvent (dispatched event)

// BrnNetwork::ScoreboardDebugComponent -- the in-game debug-menu component that drives the online
// scoreboard browser (Category -> Index -> Variation -> Scoreboard drill-down). Derives from the real
// CgsDev::DebugComponent. It owns a working Scoreboard, a back-pointer to its ScoreboardManager, three
// debug-menu enum variables (the current Category / Index / Variation selection) and the option-string
// tables + name buffers that back those three menu drop-downs.
//
// LAYOUT (X360-AUTHORITATIVE; offsets from `this`; the base CgsDev::DebugComponent sub-object occupies
// +0x00..+0x0B). Pinned from the Construct stores, the HandleScoreboardEvent memcpy and the per-branch
// addi constants in HandleScoreboardHeadingEvent @0x82585A88:
//   mScoreboard          +0x000C  Scoreboard (ctor/dtor are BrnNetwork::Scoreboard::Construct/Destruct @
//                                  this+0xC; size 0xB6C). HandleScoreboardEvent memcpy's 0xB6C bytes in.
//   mpScoreboardManager  +0x0B78  ScoreboardManager* back-pointer (a1[734]; this+2936).
//   maCategoryList       +0x0B7C  DebugUI::StringList[15]  (SetOptions arg this+0xB7C; value writes start
//                                  this+0xB80 = &[0].mpcName slot pair). NOTE: 15, not the DWARF's 10 --
//                                  see "DWARF DIVERGENCE" below.
//   maIndexList          +0x0BF4  DebugUI::StringList[10]  (SetOptions arg this+0xBF4).
//   maVariationList      +0x0C44  DebugUI::StringList[66]  (SetOptions arg this+0xC44; ends exactly at
//                                  the char buffers @+0xE54, confirming count 66).
//   maacCategories       +0x0E54  char[15][31]  (strncpy dest base this+0xE54, stride 31).
//   maacIndexes          +0x1025  char[10][31]  (strncpy dest base this+0x1025).
//   maacVariations       +0x115B  char[66][31]  (strncpy dest base this+0x115B; ends +0x1959).
//   miCategory           +0x195C  s32  (a1[1623]; -1 == "none selected"; debug menu variable).
//   miIndex              +0x1960  s32  (a1[1624]).
//   miVariation          +0x1964  s32  (a1[1625]).
//   sizeof == 0x1968.
//
// DWARF DIVERGENCE (rung-2 vs rung-1): the DecFIGS DWARF (PS3) declares maCategoryList/maacCategories as
// [10] and names the +0xC member "NetworkOutScoreboardEvent mScoreboardEvent". The X360 ARTIST binary is
// authoritative: (a) the category StringList stride to maIndexList (+0xB7C..+0xBF4 == 120 bytes == 15
// StringLists) and the category char stride to maacIndexes (+0xE54..+0x1025 == 465 == 15*31) both pin the
// category arrays at 15 entries; (b) the +0xC member is constructed/destructed via Scoreboard::Construct/
// Destruct, so it is reconstructed as a BrnNetwork::Scoreboard.
//
// X360 LEDGER: this TU's recovered function set is exactly the 10 methods declared below. The DWARF also
// lists GetCategories / GetPath / Destruct / PrintScoreboard for this class, but none have an X360 body in
// this TU (GetPath/Destruct fold onto the shared base/empty thunks; GetCategories/PrintScoreboard are not
// attested here), so they are intentionally omitted -- the base CgsDev::DebugComponent defaults apply.

namespace BrnNetwork
{
    struct ScoreboardManager;   // back-pointer member + dispatch helper only (pointer-only use; its full
                                // header embeds this component by value, so including it would cycle).

    // The scoreboard manager owns the select-event queue. The debug component dispatches a freshly built
    // NetworkInSelectScoreboardEvent through it on every menu change. Declared as a free helper here (its
    // body lands with the BrnNetworkScoreboardManager reconstruction) because ScoreboardManager's full
    // header is not yet gate-compilable from this TU -- the same documented cascade-avoidance forward-decl
    // exception used by ServerInterfaceDebugComponent::GetConnectionComponent.
    void DispatchScoreboardEvent(ScoreboardManager* lpScoreboardManager,
                                 const BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent* lpEvent);

    class ScoreboardDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // X360 compile-time limits (the per-list element counts the X360 layout attests).
        static const s32 KI_MAX_CATEGORIES = 15;
        static const s32 KI_MAX_INDEXES    = 10;
        static const s32 KI_MAX_VARIATIONS = 66;
        static const s32 KI_NAME_LENGTH    = 31;
        static const s32 KI_NONE_SELECTED  = -1;   // the -1 the three selection ints reset to.

        void Construct(ScoreboardManager* lpScoreboardManager);   // @ 0x82585858
        bool Prepare();                                            // @ 0x825858D8
        bool Release();                                            // @ 0x82585918

        // Rebuild the menu drop-downs from a downloaded heading list (one of category / index /
        // variation, per its EHeadingType), then wire the per-list change/select callbacks.
        void HandleScoreboardHeadingEvent(
            BrnNetwork::BrnNetworkModuleIO::NetworkOutScoreboardHeadingList* lpHeadingEvent);   // @ 0x82585A88

        // Copy a freshly downloaded scoreboard into mScoreboard and (re)render it.
        void HandleScoreboardEvent(const BrnNetwork::Scoreboard* lpScoreboard);                 // @ 0x8258AE38

    protected:
        const char* GetName() const override;   // @ 0x82585958 -> "Scoreboards"
        void        OnActivate() override;       // @ 0x8258AE08

    private:
        // Debug-menu change/select callbacks (registered against the three selection variables). They are
        // void(*)(void*, void*) menu callbacks; lpUserData is this component. Each commits its level's
        // current selection by dispatching the matching NetworkInSelectScoreboardEvent.
        static void GetIndexes(void* lpValue, void* lpUserData);      // @ 0x82585968 (Category changed)
        static void GetVariations(void* lpValue, void* lpUserData);   // @ 0x825859C0 (Index changed)
        static void GetScoreboard(void* lpValue, void* lpUserData);   // @ 0x82585A28 (Variation changed)

        // Render the current scoreboard to debug text. Called at the tail of HandleScoreboardEvent.
        // Not attested as a separately-recovered body in this TU (its StrStream body folds away in the
        // assert-only build); declared-only so the call site is preserved, the body lands with the full
        // scoreboard-render reconstruction. (Same declared-only idiom as ServerInterfaceDebugComponent.)
        void PrintScoreboard();   // @ ~0x82585xxx (folded; declared-only)

        // ---- member layout (see header comment) ----
        BrnNetwork::Scoreboard      mScoreboard;                                       // +0x000C
        ScoreboardManager*          mpScoreboardManager;                               // +0x0B78
        CgsDev::DebugUI::StringList maCategoryList[KI_MAX_CATEGORIES];                 // +0x0B7C
        CgsDev::DebugUI::StringList maIndexList[KI_MAX_INDEXES];                       // +0x0BF4
        CgsDev::DebugUI::StringList maVariationList[KI_MAX_VARIATIONS];                // +0x0C44
        char                        maacCategories[KI_MAX_CATEGORIES][KI_NAME_LENGTH]; // +0x0E54
        char                        maacIndexes[KI_MAX_INDEXES][KI_NAME_LENGTH];       // +0x1025
        char                        maacVariations[KI_MAX_VARIATIONS][KI_NAME_LENGTH]; // +0x115B
        s32                         miCategory;                                        // +0x195C
        s32                         miIndex;                                           // +0x1960
        s32                         miVariation;                                       // +0x1964
    };

    // NOTE on size: on the original 32-bit PPC ARTIST build (4-byte pointers) this class is 0x1968
    // bytes -- base DebugComponent 0xC + Scoreboard 0xB6C + maCategoryList[15]/maIndexList[10]/
    // maVariationList[66] (8B StringList each) + the char buffers + three trailing s32 selections,
    // matching the +0xB78 manager / +0x195C..+0x1964 selection offsets every member access reads. A
    // fixed-byte sizeof static_assert is intentionally NOT used here: the PC compile gate targets x64
    // (8-byte pointers), so StringList and the two pointer members widen and the absolute byte offsets
    // shift. The X360 offsets above are the layout proof; member order is preserved so the relative
    // ordering is faithful on both targets.
}
