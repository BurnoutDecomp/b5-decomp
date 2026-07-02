#include "GameSource/Gui/Flow/Overlay/States/BrnInGameOnlineOkOverlayState.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824B1E30
// (BrnGui::InGameOnlineOkOverlayState::FillInPopupType): a single store of the
// class's flash-file-id literal into mpcFlashFileId (X360 this+0x10C).

namespace BrnGui
{
    const char InGameOnlineOkOverlayState::KPC_IN_GAME_FRAME_LABEL[13] = "InGameOnline";

    // @ 0x824B1E30
    void InGameOnlineOkOverlayState::FillInPopupType()
    {
        mpcFlashFileId = KPC_IN_GAME_FRAME_LABEL;
    }
}
