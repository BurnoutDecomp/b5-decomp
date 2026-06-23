#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimChannel.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// CgsGui::AnimChannel is one live APT animation channel. This TU homes only the
// channel constructor; the behavioural methods (Construct/SetAnimation/Update/
// Stop/IsActive/GetInterpolator) are reconstructed by the CgsAptAnimChannel.cpp
// method TU and are declared in the header.

namespace CgsGui
{
    // @ 0x824E9198 - the X360 body is a single store of the embedded interpolator's
    // vtable into +0x0C:
    //     lis  r11, off_8206C7CC@ha ; addi r11, r11, off_8206C7CC@l
    //     stw  r11, 0xC(r3)         ; blr
    // That store is the construction of the mLinearInterpolator subobject (a
    // CgsGui::Interpolator, which carries a vtable). Default-constructing that member
    // installs the same vtable pointer; the three scalar members are left
    // uninitialised here exactly as in the X360 (they are set up by Construct()/
    // SetAnimation()).
    AnimChannel::AnimChannel()
    {
    }
}
