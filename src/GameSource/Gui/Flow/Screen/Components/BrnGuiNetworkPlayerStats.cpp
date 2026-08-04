#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkPlayerStats.h"

#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"                   // BrnGui::GuiCache (AppendExpectedAptComponent)

// ===================================================================================
// BrnGui::GuiNetworkPlayerStats -- the online per-player stats panel.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                 @0x82416CA8
//   Destruct                  @0x82416DE0
//   SetState                  @0x82416E00
//   Update                    @0x82416FA8
//   AppendExpectedAptComponent@0x82417040
//
// FormatNetworkStats (@0x82416E88) IS reconstructed -- it lives in the sibling file
// BrnGuiNetworkPlayerStats_wL_01.cpp, which also carries the definition of the static
// KAE_FORMAT_LOOKUP table. Do NOT add a second definition of either here.
//
// (An earlier revision of this banner said FormatNetworkStats could not be reconstructed
// because KAE_FORMAT_LOOKUP @0x8204C6B4 was "unrecoverable without the binary". That is
// false and must not be restored: the table was dumped from BURNOUT_X360_ARTIST.XEX --
// five dwords 11/0/14/3/13, the only two xrefs to it being 0x82416EE0 and 0x82416EE8,
// both inside FormatNetworkStats, and its 5-element bound is pinned both by the DWARF
// (ParameterFormatType[5], cpp:62) and by the next named head KAE_STATS_VALUE_LOOKUP
// @0x8204C6C8 starting exactly five dwords later.)
// ===================================================================================

namespace BrnGui
{
    // ---- static data (X360 rodata this TU owns; values from the Construct/Update asm) ----
    const char GuiNetworkPlayerStats::KAC_DESCRIPTION_COMPONENT[14]            = "Description%d";
    const char GuiNetworkPlayerStats::KAC_VALUE_COMPONENT[8]                   = "Value%d";
    const char GuiNetworkPlayerStats::KAC_USERNAME_COMPONENT[9]                = "UserName";
    const char GuiNetworkPlayerStats::KAC_WORLD_RANK_DESCRIPTION_COMPONENT[21] = "WorldRankDescription";
    const char GuiNetworkPlayerStats::KAC_WORLD_RANK_VALUE_COMPONENT[15]       = "WorldRankValue";
    const char GuiNetworkPlayerStats::KAC_APT_STATE[10]                        = "apt_state";
    const char GuiNetworkPlayerStats::KAC_PLAYER_IMAGE_INDEX[17]               = "PlayerImageIndex";

    const char* const GuiNetworkPlayerStats::KAPC_STATE_ID[E_STATE_COUNT] =
    {
        "visible",     // E_STATE_VISIBLE   (off_82F25028[0] -> 0x82049C28, X360-attested)
        "invisible",   // E_STATE_INVISIBLE (off_82F2502C  -> 0x8204B4F8, X360-attested)
    };

    // @0x82416CA8
    void GuiNetworkPlayerStats::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                          const char* lpacParentName)
    {
        GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        // Each stat row's description + value field is named "Description<i>" / "Value<i>" and
        // parented to this panel's own name (the X360 passes lpacName, not lpacParentName, here).
        for (s32 liRow = 0; liRow < KI_MAX_STAT_ROWS; ++liRow)
        {
            char acComponentName[32];

            CgsCore::SPrintf(acComponentName, 32, KAC_DESCRIPTION_COMPONENT, liRow);
            maStatRows[liRow].mDescription.Construct(acComponentName, lpStateInterface, lpacName);

            CgsCore::SPrintf(acComponentName, 32, KAC_VALUE_COMPONENT, liRow);
            maStatRows[liRow].mValue.Construct(acComponentName, lpStateInterface, lpacName);
        }

        mUserName.Construct(KAC_USERNAME_COMPONENT, lpStateInterface, lpacName);
        mWorldRank.mDescription.Construct(KAC_WORLD_RANK_DESCRIPTION_COMPONENT, lpStateInterface, lpacName);
        mWorldRank.mValue.Construct(KAC_WORLD_RANK_VALUE_COMPONENT, lpStateInterface, lpacName);

        mbIsLoaded         = false;
        mbIsDirty          = false;
        miPlayerImageIndex = 0;
        meState            = E_STATE_INVISIBLE;
    }

    // @0x82416DE0 -- reset the state block to the post-Construct defaults.
    void GuiNetworkPlayerStats::Destruct()
    {
        mbIsLoaded         = false;
        meState            = E_STATE_INVISIBLE;
        mbIsDirty          = false;
        miPlayerImageIndex = 0;
    }

    // @0x82416E00 -- set the visibility state and mark the panel dirty for the next Update.
    void GuiNetworkPlayerStats::SetState(EState leState)
    {
        if (leState == E_STATE_VISIBLE)
        {
            meState   = E_STATE_VISIBLE;
            mbIsDirty = true;
        }
        else if (leState == E_STATE_INVISIBLE)
        {
            meState   = E_STATE_INVISIBLE;
            mbIsDirty = true;
        }
        else
        {
            CGS_ASSERT(false, "Invalid state");
        }
    }

    // @0x82416FA8 -- flush the dirty state to the bound apt clip: the player image index and
    // the visibility view-state, then clear the dirty flag.
    void GuiNetworkPlayerStats::Update()
    {
        if (mbIsDirty)
        {
            char acPlayerImageIndex[32];

            mbIsDirty = false;
            CgsCore::SPrintf(acPlayerImageIndex, 32, "%d", miPlayerImageIndex);

            AddOutputAptViewState(KAC_PLAYER_IMAGE_INDEX, acPlayerImageIndex, false);
            AddOutputAptViewState(KAC_APT_STATE, KAPC_STATE_ID[meState], false);
        }
    }

    // @0x82417040 -- register every owned text field, by name, as an expected apt component on
    // the given flow layer (so the cache waits for each to initialise). Order mirrors the X360:
    // self, user-name, world-rank value, world-rank description, then each row's description+value.
    void GuiNetworkPlayerStats::AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        lpGuiCache->AppendExpectedAptComponent(leFlow, GetName());
        lpGuiCache->AppendExpectedAptComponent(leFlow, mUserName.GetName());
        lpGuiCache->AppendExpectedAptComponent(leFlow, mWorldRank.mValue.GetName());
        lpGuiCache->AppendExpectedAptComponent(leFlow, mWorldRank.mDescription.GetName());

        for (s32 liRow = 0; liRow < KI_MAX_STAT_ROWS; ++liRow)
        {
            lpGuiCache->AppendExpectedAptComponent(leFlow, maStatRows[liRow].mDescription.GetName());
            lpGuiCache->AppendExpectedAptComponent(leFlow, maStatRows[liRow].mValue.GetName());
        }
    }
}
