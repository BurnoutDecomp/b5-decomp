#pragma once

#include "types.hpp"

// CgsDev::Message - the log-category filter the message/log system gates output on. Every log
// call site tests `(gxMessageFilterFlags & <category bit>)` before emitting, and the in-game
// "Message Filter" debug component (CgsDebugComponentMessageFilter) edits this mask live: each
// of up to KI_MAX_FILTER (64) categories owns one bit. Recovered from the DecFIGS DWARF
// (Development/MessageSystem/CgsMessage.h) cross-checked against the X360 call sites.
//
// FilterFlag is a 64-bit mask (DWARF CgsMessage.h:40 typedef uint64_t; the X360 loads/stores it
// with ld/std and shifts the per-category bit with sld, i.e. doubleword ops), so up to 64
// categories are addressable. The debug menu exposes the two 32-bit halves as "(filter value 1)"
// / "(filter value 2)" sliders.

namespace CgsDev
{
namespace Message
{
    // A 64-bit log-category bitmask (one bit per message category).
    typedef u64 FilterFlag;

    // Maximum number of distinct message categories (one per bit of FilterFlag). DWARF CgsMessage.h:41.
    const s32 KI_MAX_FILTER = 64;

    // The named category bits (DWARF CgsMessage.h:48-70). The X360 "Message Filter" component
    // registers a name for each of these via SetFilterName.
    const FilterFlag KX_FILTER_NONE          = 0;
    const FilterFlag KX_FILTER_GLOBAL        = 1;
    const FilterFlag KX_FILTER_GSCONTAINERS  = 2;
    const FilterFlag KX_FILTER_GSDEVELOPMENT = 4;
    const FilterFlag KX_FILTER_GSGEOMETRIC   = 8;
    const FilterFlag KX_FILTER_GSGRAPHICS    = 16;
    const FilterFlag KX_FILTER_GSLANGUAGE    = 32;
    const FilterFlag KX_FILTER_GSMEMORY      = 64;
    const FilterFlag KX_FILTER_GSMODULE      = 128;
    const FilterFlag KX_FILTER_GSNETWORK     = 256;
    const FilterFlag KX_FILTER_GSPHYSICS     = 512;
    const FilterFlag KX_FILTER_GSSCENEMANAGER = 1024;
    const FilterFlag KX_FILTER_GSSOUND       = 2048;
    const FilterFlag KX_FILTER_GSSYSTEM      = 4096;
    const FilterFlag KX_FILTER_GSFILESYSTEM  = 0x2000;
    const FilterFlag KX_FILTER_GSRESOURCE    = 0x4000;
    const FilterFlag KX_FILTER_GAME          = 0x8000;

    // The live log-category filter mask (DWARF CgsMessage.h:147). Edited by the in-game Message
    // Filter debug component.
    extern FilterFlag gxMessageFilterFlags;
}
}
