// BrnGuiOptionsDataProfile_wS34_00.cpp -- OptionsDataProfile accessors the online screens reach
// (BrnGuiOptionsDataProfile.cpp family).

#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // Read inline by OnlinePlay::CheckForCompletedLoads (the "new news" transition state).
    bool OptionsDataProfile::IsThereUnreadNews() const
    {
        return mbIsNewsUnread;
    }

    // Read inline by the create-match screen (OnlineGameOptions SetupHelpBar and the load list).
    s32 OptionsDataProfile::GetNumCreatedOnlineGameOptions() const
    {
        return miNumCreatedOnlineGameOptions;
    }

    s32 OptionsDataProfile::GetNumReceivedOnlineGameOptions() const
    {
        return miNumReceivedOnlineGameOptions;
    }

    // read entry liIndex of the created route table back out into game params.
    // The index-range asserts stream the offending index; lowered to the constant prefix.
    void OptionsDataProfile::GetCreatedOnlineGameOptions(s32 liIndex, GuiCache* lpCache,
                                                         GuiEventNetworkGameParams* lpParams) const
    {
        CGS_ASSERT(lpCache, "lpGuiCache");
        CGS_ASSERT(lpParams, "lpParams");
        CGS_ASSERT(liIndex >= 0, "Invalid Created online game options index of ");
        CGS_ASSERT(liIndex < KI_MAX_CREATED_ONLINE_GAME_OPTIONS, "Invalid Created online game options index of ");

        maCreatedOnlineGameOptions[liIndex].SetToGameParams(lpCache, lpParams);
    }
}
