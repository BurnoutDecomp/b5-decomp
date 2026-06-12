#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DD168
//   (BrnGraphics::DebugComponent::GetPath)
//
// Behaviour-faithful to the X360 pseudocode:
//     return "Renderer";
//
// Returns the debug-menu path label for the renderer module's debug component.

namespace BrnGraphics
{
    struct DebugComponent
    {
        const char* GetPath();
    };

    const char* DebugComponent::GetPath()
    {
        return "Renderer";
    }
}
