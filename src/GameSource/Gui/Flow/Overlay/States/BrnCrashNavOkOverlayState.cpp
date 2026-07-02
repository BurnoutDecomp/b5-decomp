#include "GameSource/Gui/Flow/Overlay/States/BrnCrashNavOkOverlayState.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824B1DA0
// (BrnGui::CrashNavOkOverlayState::FillInPopupType): a single store of the
// class's flash-file-id literal into mpcFlashFileId (X360 this+0x10C).
// Upgraded from the earlier opaque offset-cast shim (this+268) once the real
// BaseOverlayState hierarchy landed -- the member is now written BY NAME.

namespace BrnGui
{
    const char CrashNavOkOverlayState::KPC_CRASH_NAV_FRAME_LABEL[9] = "CrashNav";

    // @ 0x824B1DA0
    void CrashNavOkOverlayState::FillInPopupType()
    {
        mpcFlashFileId = KPC_CRASH_NAV_FRAME_LABEL;
    }
}
