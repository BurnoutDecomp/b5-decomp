#include "GameSource/Gui/Flow/Overlay/States/BrnInGameOnlineEnterFreeBurnOverlayState.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824B1E50
// (BrnGui::InGameOnlineEnterFreeBurnOverlayState::FillInPopupType): a single store of the
// class's flash-file-id literal into mpcFlashFileId (X360 this+0x10C).

namespace BrnGui
{
    const char InGameOnlineEnterFreeBurnOverlayState::KPC_IN_GAME_FRAME_LABEL[14] = "EnterFreeBurn";

    // @ 0x824B1E50
    void InGameOnlineEnterFreeBurnOverlayState::FillInPopupType()
    {
        mpcFlashFileId = KPC_IN_GAME_FRAME_LABEL;
    }
}
