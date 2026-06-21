#ifndef CGS_DEBUG_COMPONENT_MEMORY_H
#define CGS_DEBUG_COMPONENT_MEMORY_H

#include "types.hpp"

// CgsMemory::DebugComponentMemory -- the memory module's debug-overlay component (the on-screen memory
// report: scroll position, font scale/colour state). Embedded by value in CgsMemory::MemoryModule.
//
// [MINIMAL PLACEHOLDER] The DecFIGS DWARF (GameShared/GameClasses/Memory/DebugComponent/
// CgsDebugComponentMemory.h) exposes only this type's static constants (KI_TAB_SIZE, KF_FONT_SCALE_*,
// KU_COLOUR_*, KF_MAX_SCROLL/STEP) + the default ctor -- no instance members were in the dump. Its real
// instance layout (scroll/render state) is recovered if/when the memory debug overlay is reconstructed;
// until then this is an empty placeholder so MemoryModule can embed it by value. Marked.
namespace CgsMemory
{
    class DebugComponentMemory
    {
    public:
        DebugComponentMemory() {}
    };
}

#endif
