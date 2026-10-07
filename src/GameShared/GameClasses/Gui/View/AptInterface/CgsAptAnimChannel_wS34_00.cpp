// CgsAptAnimChannel_wS34_00.cpp -- AnimChannel::Stop (CgsAptAnimChannel.cpp family). The console
// inlines it at its call sites (Animator::SetAnimation: two stores).

#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimChannel.h"

namespace CgsGui
{
    void AnimChannel::Stop()
    {
        mbActive              = false;
        mpCurrentInterpolator = 0;
    }
}
