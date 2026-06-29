#ifndef GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DEBUG_LOG_STRING_H
#define GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DEBUG_LOG_STRING_H

// ============================================================================
// GameSource/Director/DirectorModule/BrnDebugLogString.h
//
// BrnDirector::DebugLog::LogString -- one entry in the director debug log's fixed-capacity
// string pool (ObjectPool<LogString,20,int>). The full DebugLog class (and its nested LogString)
// is now defined in BrnDirectorModuleDebugPrinter.h, the owning module header; this file is a thin
// shim kept so the ObjectPool<LogString,20,int> explicit-instantiation TU (BrnDebugLogStringPool.cpp)
// and any other LogString-only consumer keep their existing include working.
//
// LogString layout (verbatim from the DecFIGS DWARF, BrnDirectorModuleDebugPrinter.h:176):
//   +0x00 mfTimeLeft (f32)   +0x04 mRGBA (RGBA == packed u32)   +0x08 macString[64]
// sizeof == 72 (0x48), matching the X360 ObjectPool<LogString,20,int>'s 72-byte slot stride.
// ============================================================================

#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"  // BrnDirector::DebugLog (+ nested LogString)

#endif // GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DEBUG_LOG_STRING_H
