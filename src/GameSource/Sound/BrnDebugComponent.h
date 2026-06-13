#pragma once

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSound::Debug::DebugComponent::GetName @ 0x827E0798  -> "Sound"
//   BrnSound::Debug::DebugComponent::GetPath @ 0x827E07A8  -> "Sound Module"
//
// Debug-UI identity hooks for the sound module's debug component. Header-keyed TU;
// members inline.

namespace BrnSound
{
    namespace Debug
    {
        class DebugComponent
        {
        public:
            const char* GetName() const { return "Sound"; }
            const char* GetPath() const { return "Sound Module"; }
        };
    }
}
