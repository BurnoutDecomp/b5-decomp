#include "GameSource/Gui/Flow/Overlay/States/BrnInGameOnlineOkCancelOverlayState.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824B1E40
// (BrnGui::InGameOnlineOkCancelOverlayState::FillInPopupType): a single store of the
// class's flash-file-id literal into mpcFlashFileId (X360 this+0x10C).

namespace BrnGui
{
    const char InGameOnlineOkCancelOverlayState::KPC_IN_GAME_FRAME_LABEL[13] = "InGameOnline";

    // @ 0x824B1E40
    void InGameOnlineOkCancelOverlayState::FillInPopupType()
    {
        mpcFlashFileId = KPC_IN_GAME_FRAME_LABEL;
    }
}
