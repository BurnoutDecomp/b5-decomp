#include "GameSource/Gui/Flow/Overlay/States/BrnInGameOkOverlayState.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824B1E00
// (BrnGui::InGameOkOverlayState::FillInPopupType): a single store of the
// class's flash-file-id literal into mpcFlashFileId (X360 this+0x10C).
// Upgraded from the earlier opaque offset-cast shim (this+268) once the real
// BaseOverlayState hierarchy landed -- the member is now written BY NAME.

namespace BrnGui
{
    const char InGameOkOverlayState::KPC_IN_GAME_FRAME_LABEL[7] = "InGame";

    // @ 0x824B1E00
    void InGameOkOverlayState::FillInPopupType()
    {
        mpcFlashFileId = KPC_IN_GAME_FRAME_LABEL;
    }
}
