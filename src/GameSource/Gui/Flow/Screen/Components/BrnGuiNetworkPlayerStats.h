#pragma once

// ===================================================================================
// BrnGui::GuiNetworkPlayerStats  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkPlayerStats.h
//   class:BrnGui::GuiNetworkPlayerStats
//
// The online "network player stats" GUI component: the per-player stats panel shown on
// the online lobby / player-info screens. It owns six description/value text-field rows
// (maStatRows), a user-name field, a world-rank description/value row, plus a small
// visibility/dirty/player-image state block. It is embedded BY VALUE inside the online
// screen states (OnlinePlay / OnlineGameRoomPlayerInfo), which drive its Construct /
// SetState / Update / SetInfo.
//
// CLASS SHAPE + MEMBER ORDER: DecFIGS DWARF
//   references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkPlayerStats.h
//   (X360-attested). GuiNetworkPlayerStats : public CgsGui::GuiComponent, so it carries the
//   base vptr @+0x00, macName[128] @+0x04, muHashedName @+0x84 and mpStateInterface @+0x88
//   (base size 0x8C on the 32-bit X360) before its own members.
//
// MEMBER LAYOUT proven store-for-store from BURNOUT_X360_ARTIST.XEX Construct @0x82416CA8
// (32-bit-pointer guest byte offsets; the PC gate compiles 64-bit, so members are accessed
// BY NAME -- byte-exactness is not the goal, member ORDER is):
//   +0x008C  maStatRows[6]  (StatRow = { TextField mDescription; TextField mValue; },
//                            TextField stride 0x128, StatRow stride 0x250)
//              row i: mDescription @ 0x008C + i*0x250, mValue @ 0x01B4 + i*0x250
//   +0x0E6C  mUserName            (TextField)
//   +0x0F94  mWorldRank           (StatRow: mDescription @0x0F94, mValue @0x10BC)
//   +0x11E4  meState              (EState)
//   +0x11E8  miPlayerImageIndex   (s32)
//   +0x11EC  mbIsLoaded           (bool)
//   +0x11ED  mbIsDirty            (bool)
//
// BrnGui::TextField is a fully-modelled named type (BrnGuiTextField.h) that IS a
// GuiComponent, so every sub-field is reached through its named members / GetName() /
// virtual Construct rather than a raw byte offset.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/BrnGuiTextField.h"                           // BrnGui::TextField (by-value members)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                       // BrnGui::GuiFlow (AppendExpectedAptComponent)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base), CgsGui::StateInterface
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"        // BrnNetwork::NetworkPlayerStats (+ EStatsValue)

namespace BrnGui
{
    class GuiCache;   // AppendExpectedAptComponent / FormatNetworkStats / SetInfo arg (pointer only)

    // GuiNetworkPlayerStats : public CgsGui::GuiComponent (DWARF BrnGuiNetworkPlayerStats.h:54).
    class GuiNetworkPlayerStats : public CgsGui::GuiComponent
    {
    public:
        // BrnGuiNetworkPlayerStats.h:57
        enum EState
        {
            E_STATE_VISIBLE   = 0,
            E_STATE_INVISIBLE = 1,
            E_STATE_COUNT     = 2,
        };

        // BrnGuiNetworkPlayerStats.h:152 -- one description/value text-field row.
        struct StatRow
        {
            TextField mDescription;   // h:154
            TextField mValue;         // h:155
        };

        // ---- ledger functions reconstructed in this TU --------------------------------
        // @0x82416CA8 -- run the base component Construct, then bring up the six stat rows'
        // description/value fields, the user-name field and the world-rank row, and prime the
        // state block (invisible, not loaded, not dirty). Virtual (overrides the base slot 0).
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x82416DE0 -- reset the state block back to its post-Construct defaults.
        void Destruct();

        // @0x82416E00 -- set the panel's visibility state and mark it dirty (asserts on an
        // out-of-range state).
        void SetState(EState leState);

        // @0x82416FA8 -- when dirty, push the player-image index and the visibility apt-state
        // to the bound apt clip, then clear the dirty flag.
        void Update();

        // @0x82417040 -- register every owned text field (by name) as an expected apt
        // component on the given GUI flow layer through the cache.
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache);

        // ---- declared-only (X360-attested; bodies are separate TUs / not yet reconstructed) --
        // @0x82416E88 -- format one network stat value into lpacBuffer under the stat's display
        // type. NOT reconstructed in this TU: its stat-type -> LanguageManager::ParameterFormatType
        // table (KAE_FORMAT_LOOKUP, X360 rodata @0x8204C6B4) is unrecoverable without the binary
        // (no IDA/raw-XEX access here), and guessing it would be an unfaithful invention.
        void FormatNetworkStats(char* lpacBuffer, int liBufferSize,
                                const BrnNetwork::NetworkPlayerStats* lpStats,
                                BrnNetwork::NetworkPlayerStats::EStatsValue leValue,
                                GuiCache* lpGuiCache);

        void SetInfo(const char* lpacName, s32 liValue, const BrnNetwork::NetworkPlayerStats* lpStats,
                     s32 liImageIndex, bool lbShowWorldRank, GuiCache* lpGuiCache);   // cpp:251
        void SetPlayerImageIndex(s32 liImageIndex);                                   // cpp:355
        void DisplayMarkedMan(bool lbMarkedMan);                                      // h:112
        bool IsVisible() const;                                                       // h:173
        bool IsComponentLoaded() const;                                              // h:185
        void SetComponentLoaded();                                                    // h:179

    private:
        // BrnGuiNetworkPlayerStats.h:133 -- number of stat rows.
        static const s32 KI_MAX_STAT_ROWS = 6;

        // Component-name format/literals this TU's data owns (values from the X360 rodata the
        // Construct / Update asm loads verbatim). Defined in the .cpp.
        static const char KAC_DESCRIPTION_COMPONENT[14];             // cpp:26 "Description%d"
        static const char KAC_VALUE_COMPONENT[8];                    // cpp:27 "Value%d"
        static const char KAC_USERNAME_COMPONENT[9];                 // cpp:28 "UserName"
        static const char KAC_WORLD_RANK_DESCRIPTION_COMPONENT[21];  // cpp:29 "WorldRankDescription"
        static const char KAC_WORLD_RANK_VALUE_COMPONENT[15];        // cpp:30 "WorldRankValue"
        static const char KAC_APT_STATE[10];                         // cpp:31 "apt_state"
        static const char KAC_PLAYER_IMAGE_INDEX[17];                // cpp:32 "PlayerImageIndex"

        // cpp:40 -- apt view-state label per EState (off_82F25028; "visible" X360-attested,
        // "invisible" is the codebase-wide apt-state pair for E_STATE_INVISIBLE).
        static const char* const KAPC_STATE_ID[E_STATE_COUNT];

        // ---- data members (DWARF order; see the layout map above) --------------------
        StatRow   maStatRows[KI_MAX_STAT_ROWS];   // h:158  +0x008C
        TextField mUserName;                      // h:159  +0x0E6C
        StatRow   mWorldRank;                     // h:160  +0x0F94
        EState    meState;                        // h:162  +0x11E4
        s32       miPlayerImageIndex;             // h:164  +0x11E8
        bool      mbIsLoaded;                     // h:166  +0x11EC
        bool      mbIsDirty;                      // h:167  +0x11ED
    };
}
