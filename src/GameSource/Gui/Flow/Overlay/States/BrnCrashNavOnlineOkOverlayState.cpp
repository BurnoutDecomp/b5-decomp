#include "GameSource/Gui/Flow/Overlay/States/BrnCrashNavOnlineOkOverlayState.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824B1DD0
// (BrnGui::CrashNavOnlineOkOverlayState::FillInPopupType): a single store of the
// class's flash-file-id literal into mpcFlashFileId (X360 this+0x10C).
// Upgraded from the earlier opaque offset-cast shim (this+268) once the real
// BaseOverlayState hierarchy landed -- the member is now written BY NAME.

namespace BrnGui
{
    const char CrashNavOnlineOkOverlayState::KPC_CRASH_NAV_FRAME_LABEL[15] = "CrashNavOnline";

    // @ 0x824B1DD0
    void CrashNavOnlineOkOverlayState::FillInPopupType()
    {
        mpcFlashFileId = KPC_CRASH_NAV_FRAME_LABEL;
    }
}
