// ============================================================================
// CgsAemsFactory.cpp -- CgsSound::Playback::AemsFactory runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   AemsFactory::CsisPrint(const char*)        @ 0x8268A018
//   AemsFactory::FindPatchMonitor(const char*) @ 0x82689E98
//
// CsisPrint @ 0x8268A018:
//   if (CgsDev::Message::gxMessageFilterFlags & 1)
//       (*gpDebugPrint)[vslot 1](gpDebugPrint, str ? str : "<NULLSTRING>");
// The X360 calls the gpDebugPrint stream's second vtable slot, which is the
// committed DebugPrint::operator<<(const char*) (CgsLog.h). So the print routes
// through `*gpDebugPrint << text` exactly as the engine logging convention.
// DWARF (CgsAemsFactory.cpp:397) confirms this is `protected` and returns
// `void`; the tail-called vtable slot's own return value is not propagated.
//
// FindPatchMonitor @ 0x82689E98:
//   if (muPatchMonitorCount == 0) return 0;
//   for each maPatchMonitors[i] (i < count):
//       byte-compare maPatchMonitors[i].mpName against lpcName;  // strcmp
//       if equal -> return &maPatchMonitors[i];
//   return 0;
// The X360 inner do/while is a hand-inlined strcmp over the two C strings; the
// outer loop walks the table and bails out at the registered count. Called by
// AemsFactory::AddPatchMonitor (DEFERRED sibling).
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstring>

namespace CgsSound
{
namespace Playback
{

// ---------------------------------------------------------------------------
// AemsFactory::CsisPrint(const char* lpcText)  @ 0x8268A018
// ---------------------------------------------------------------------------
void AemsFactory::CsisPrint(const char* lpcText)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
    {
        *CgsDev::Log::gpDebugPrint << (lpcText ? lpcText : "<NULLSTRING>");
    }
}

// ---------------------------------------------------------------------------
// AemsFactory::FindPatchMonitor(const char* lpcName)  @ 0x82689E98
// ---------------------------------------------------------------------------
PatchMonitor* AemsFactory::FindPatchMonitor(const char* lpcName)
{
    if (muPatchMonitorCount == 0)
    {
        return nullptr; // cmplwi count,0 ; beq -> li r3,0
    }

    for (u32 luI = 0; luI < muPatchMonitorCount; ++luI)
    {
        // Hand-inlined strcmp over the C strings (the X360 do/while loop).
        if (std::strcmp(maPatchMonitors[luI].mpName, lpcName) == 0)
        {
            return &maPatchMonitors[luI];
        }
    }
    return nullptr;
}

} // namespace Playback
} // namespace CgsSound
