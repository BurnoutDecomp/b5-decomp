#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824B1DB0
//   (BrnGui::CrashNavOkCancelOverlayState::FillInPopupType)
//
// Behaviour-faithful to the X360 pseudocode:
//     *(result + 268) = "CrashNav";
//     return result;
//
// Stamps the popup-type identifier string into the overlay state at +0x10C and
// returns `this`. The surrounding object is opaque here, so the slot is addressed
// by its original byte offset through a raw view of `this`.

namespace BrnGui
{
    struct CrashNavOkCancelOverlayState
    {
        CrashNavOkCancelOverlayState* FillInPopupType();
    };

    CrashNavOkCancelOverlayState* CrashNavOkCancelOverlayState::FillInPopupType()
    {
        *reinterpret_cast<const char**>(reinterpret_cast<u8*>(this) + 268) = "CrashNav";
        return this;
    }
}
