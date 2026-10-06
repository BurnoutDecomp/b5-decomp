#pragma once

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

// FLAG PC diagnostic: exact legend strings only; no layout, ownership or render-state changes.
namespace AptTextLayoutDiagnosticsPC
{
    inline bool Enabled()
    {
        static const bool enabled = std::getenv("BRN_APT_TEXT_LAYOUT_DIAG") != nullptr;
        return enabled;
    }
    inline const char* Caption(unsigned kind)
    {
        static const char* const captions[] = {nullptr, "NAVIGATE ICONS", "NAVIGATE MAP",
            "RETURN TO GAME", "VIEW RACE", "ZOOM OUT", "ZOOM IN"};
        return kind <= 6 ? captions[kind] : nullptr;
    }
    inline unsigned* SlotKinds()
    {
        static unsigned kinds[257] = {};
        return kinds;
    }
    inline unsigned Kind(intptr_t id)
    {
        return Enabled() && id > 0 && id <= 256 ? SlotKinds()[id] : 0;
    }
    inline bool Register(intptr_t id, const char* resolved)
    {
        if (!Enabled() || id <= 0 || id > 256) return false;
        unsigned kind = 0;
        if (resolved)
            for (unsigned i = 1; i <= 6; ++i)
                if (std::strcmp(resolved, Caption(i)) == 0) { kind = i; break; }
        const bool changed = SlotKinds()[id] != kind;
        SlotKinds()[id] = kind;
        return changed && kind != 0;
    }
    inline bool SampleDraw(intptr_t id)
    {
        const unsigned kind = Kind(id);
        if (!kind) return false;
        static unsigned calls[7] = {};
        return (calls[kind]++ % 512u) == 0;
    }
    inline bool SampleFold(intptr_t id)
    {
        const unsigned kind = Kind(id);
        if (!kind) return false;
        static unsigned calls[7] = {};
        return (calls[kind]++ % 512u) == 0;
    }
    inline bool SampleGetter(intptr_t id)
    {
        const unsigned kind = Kind(id);
        if (!kind) return false;
        static unsigned calls[7] = {};
        return (calls[kind]++ % 512u) == 0;
    }
    inline void Emit(const char* line)
    {
        static unsigned rows = 0;
        if (Enabled() && rows < 128 && CgsDev::Log::gpDebugPrint)
        {
            ++rows;
            *CgsDev::Log::gpDebugPrint << line << "\n";
        }
    }
}
