#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82815EF8
//   (CgsDev::DebugManager::SetGamePad)
//
// Behaviour-faithful to the X360 pseudocode:
//     *&this->mpUI->gapA5[19] = lpDebugManagerPad;
//
// `gapA5[19]` is a Hex-Rays gap field: the gap begins at byte offset 0xA5 of the
// UI object and is indexed 19 bytes in, i.e. the pad pointer is stored at byte
// offset 0xA5 + 19 = 0xB8 inside the DebugUI instance pointed to by mpUI.
// We model only what this setter touches.

namespace CgsDev
{
    struct DebugManagerPad;

    // Minimal view of the UI object: only the gamepad slot at byte offset 0xB8 is
    // recovered here; everything before it is opaque padding.
    struct DebugManagerUI
    {
        u8                mPad[0xB8];          // [0x00] opaque
        DebugManagerPad*  mpGamePad;           // [0xB8] set by SetGamePad
    };

    class DebugManager
    {
    public:
        void SetGamePad(DebugManagerPad* lpDebugManagerPad);

    private:
        DebugManagerUI* mpUI;                  // [0x00]
    };

    void DebugManager::SetGamePad(DebugManagerPad* lpDebugManagerPad)
    {
        mpUI->mpGamePad = lpDebugManagerPad;
    }
}
