#pragma once

// FLAG PC-platform leaf: keyboard ownership shared by the debug UI and game input.
// Kept independent of the UI's Windows/render headers for the host input adapter.
namespace CgsDev
{
    bool IsDebugKeyboardCapturedPC();
}
