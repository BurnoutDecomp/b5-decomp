#include "GameSource/Gui/Flow/Overlay/States/BrnCrashNavOnlineWaitOverlayState.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824B1DC0
// (BrnGui::CrashNavOnlineWaitOverlayState::FillInPopupType): a single store of the
// class's flash-file-id literal into mpcFlashFileId (X360 this+0x10C).
// Upgraded from the earlier opaque offset-cast shim (this+268) once the real
// BaseOverlayState hierarchy landed -- the member is now written BY NAME.

namespace BrnGui
{
    const char CrashNavOnlineWaitOverlayState::KPC_CRASH_NAV_FRAME_LABEL[15] = "CrashNavOnline";

    // @ 0x824B1DC0
    void CrashNavOnlineWaitOverlayState::FillInPopupType()
    {
        mpcFlashFileId = KPC_CRASH_NAV_FRAME_LABEL;
    }
}
