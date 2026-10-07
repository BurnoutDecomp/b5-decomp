// BrnGuiOptionsDataProfile_wS34_01.cpp -- OptionsDataProfile setter the online news screen
// reaches (BrnGuiOptionsDataProfile.cpp family). The console inlines it at the call site.

#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"

namespace BrnGui
{
    // Written inline by OnlineNews::ShowText (the first look at downloaded news).
    void OptionsDataProfile::SetUnreadNews(bool lbUnread)
    {
        mbIsNewsUnread = lbUnread;
    }
}
