#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E21E0
//   (BrnGui::ProfileHost::HandleProfileTaskResult)
//
// Behaviour-faithful to the X360 pseudocode:
//     if ( (CgsDev::Message::gxMessageFilterFlags & 1) != 0 )
//       return ((*CgsDev::Log::gpDebugPrint)[1])(gpDebugPrint, "SaveLoad: Finished\n");
//     return result;   // (uninitialised)
//
// When the global message filter has bit 0 set, log "SaveLoad: Finished" through
// vtable slot 1 of the global debug-print object and forward its result. Otherwise
// the original returns an uninitialised register (a Hex-Rays __fastcall artifact;
// the function is effectively void on that path) — reconstructed as `return 0`.
//
// NOTE: this isolated decomp keeps its own minimal CgsDev::Log shim (vtable-slot call form)
// rather than the full CgsLog.h; it lives in its own TU so that shim does not clash with the
// real CgsLog.h pulled into BrnGuiModule.cpp.

namespace CgsDev
{
    namespace Message
    {
        extern u32 gxMessageFilterFlags;
    }

    namespace Log
    {
        struct DebugPrint;
        // vtable slot signature: printf-style line writer (object, format string).
        typedef int (*DebugPrintFn)(DebugPrint*, const char*);
        struct DebugPrint
        {
            DebugPrintFn* mpVTable;
        };
        extern DebugPrint* gpDebugPrint;
    }
}

namespace BrnGui
{
    struct ProfileHost
    {
        int HandleProfileTaskResult();
    };

    int ProfileHost::HandleProfileTaskResult()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            return CgsDev::Log::gpDebugPrint->mpVTable[1](
                CgsDev::Log::gpDebugPrint, "SaveLoad: Finished\n");
        return 0;
    }
}
