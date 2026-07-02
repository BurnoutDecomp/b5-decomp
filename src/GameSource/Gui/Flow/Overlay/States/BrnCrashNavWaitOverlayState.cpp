#include "GameSource/Gui/Flow/Overlay/States/BrnCrashNavWaitOverlayState.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824B1D90
// (BrnGui::CrashNavWaitOverlayState::FillInPopupType): a single store of the
// class's flash-file-id literal into mpcFlashFileId (X360 this+0x10C).
// Upgraded from the earlier opaque offset-cast shim (this+268) once the real
// BaseOverlayState hierarchy landed -- the member is now written BY NAME.

namespace BrnGui
{
    const char CrashNavWaitOverlayState::KPC_CRASH_NAV_FRAME_LABEL[9] = "CrashNav";

    // @ 0x824B1D90
    void CrashNavWaitOverlayState::FillInPopupType()
    {
        mpcFlashFileId = KPC_CRASH_NAV_FRAME_LABEL;
    }
}
