#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82815F08
//   (CgsDev::Internal::DebugInternal::GetUI)
//
// Behaviour-faithful to the X360 pseudocode:
//     return *(mpInstance + 80);
//
// `mpInstance` is the class-owned static singleton pointer (no `this` is taken).
// The UI pointer lives at byte offset 80 (0x50) inside the singleton instance.
// Hex-Rays renders the return via `__return_ptr`; the value returned is simply a
// DebugUI* (pointer-sized), so we return it directly.

namespace CgsDev
{
    namespace DebugUI
    {
        struct DebugUI;
    }

    namespace Internal
    {
        struct DebugInternal
        {
            u8                  mPad[80];     // [0x00] opaque
            DebugUI::DebugUI*   mpUI;         // [0x50] the debug UI

            static DebugInternal* mpInstance; // class-owned singleton

            static DebugUI::DebugUI* GetUI();
        };

        DebugUI::DebugUI* DebugInternal::GetUI()
        {
            return mpInstance->mpUI;
        }
    }
}
