#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E0988
//   (CgsResource::DebugComponent::GetPath)  ->  return "Core";

namespace CgsResource
{
    struct DebugComponent
    {
        const char* GetPath();
    };

    const char* DebugComponent::GetPath()
    {
        return "Core";
    }
}
