#ifndef GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DEBUG_LOG_STRING_H
#define GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DEBUG_LOG_STRING_H

#include "types.hpp"

// ============================================================================
// GameSource/Director/DirectorModule/BrnDebugLogString.h
//
// BrnDirector::DebugLog::LogString -- one entry in the director debug log's fixed-capacity
// string pool (ObjectPool<LogString,20,int>). The director's on-screen debug log keeps a pool
// of 20 timed colour strings; ActualAppend allocates a slot, Update/ActualAppend free expired
// ones. Layout/order/types verbatim from the DecFIGS DWARF (BrnDirectorModuleDebugPrinter.h:176):
//   +0x00 mfTimeLeft (f32)   +0x04 mRGBA (RGBA == packed u32)   +0x08 macString[64]
// sizeof == 72 (0x48), matching the X360 ObjectPool<LogString,20,int>'s 72-byte slot stride
// (the free-queue word offset 0x168 == 360 == 20*72/4 places maiObjectFreeQueue right after the
// 20-slot pool; see EventQueue/ObjectPool instance note in BrnDebugLogStringPool.cpp).
//
// The ObjectPool's AllocateObject/FreeObject (this TU's two functions) never touch the element
// contents -- they only manage the slot index, the LIFO free queue and the occupancy bit array --
// so a complete, trivially-copyable element of the right size is all the instantiation needs.
// ============================================================================

namespace BrnDirector
{
    struct DebugLog
    {
        // BrnDirectorModuleDebugPrinter.h:176
        struct LogString
        {
            f32  mfTimeLeft;       // +0x00  remaining on-screen time (seconds)
            u32  mRGBA;            // +0x04  packed RGBA8 text colour (DWARF type: RGBA)
            char macString[64];    // +0x08  the log text
        };
    };
}

#endif // GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DEBUG_LOG_STRING_H
