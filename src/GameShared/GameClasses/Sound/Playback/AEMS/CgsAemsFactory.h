#ifndef CGS_SOUND_PLAYBACK_AEMS_CGSAEMSFACTORY_H
#define CGS_SOUND_PLAYBACK_AEMS_CGSAEMSFACTORY_H

#include "types.hpp"

// =============================================================================
// CgsSound::Playback::AemsFactory  (+ supporting CSIS command types)
//   GameShared/GameClasses/Sound/Playback/aems/CgsAemsFactory.h (DWARF home) +
//   GameShared/GameClasses/Sound/Playback/aems/CgsAemsFactory.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This home models the slice of
// AemsFactory exercised by the two boot-trace functions in this TU:
//   AemsFactory::CsisPrint(const char*)      @ 0x8268A018
//   AemsFactory::FindPatchMonitor(const char*) @ 0x82689E98
//
// FLAG (MINIMAL home of a deep un-homed base): the real AemsFactory derives from
// AemsRWSampleFactory (-> ... -> Factory), a large engine factory hierarchy that is
// NOT yet reconstructed. The X360 functions reach this->muPatchMonitorCount at
// +0x5C (=92) and this->maPatchMonitors at +0x46C (=1132); between them sit the
// registry pointer, the RWAC factory handle and a 255-deep CsisCommandQueue. None
// of that base hierarchy is homed here. To keep the two leaf functions bodyable BY
// NAME (the project rule forbids raw-offset member access), this home models the
// AemsFactory members the functions touch (muPatchMonitorCount, maPatchMonitors)
// plus typed-by-name PLACEHOLDERS for the intervening members, on a plain
// (non-engine-base) struct. Because the base chain is not modelled, ABSOLUTE X360
// offsets (+92 / +1132) are intentionally NOT static_asserted -- only the member
// names and access logic are load-bearing. The full base hierarchy + the factory's
// virtual surface (DoCreateVoice/DoCreateContent/DoUpdate/ctors/Create) is DEFERRED
// to a dedicated AemsFactory keystone TU.
// =============================================================================

namespace CgsSound
{
namespace Playback
{

// CgsAemsFactory.h:59 (DWARF).
enum ECsisCommandType
{
    E_CSIS_COMMAND_SET_CLASS_HANDLE = 0,
    E_CSIS_COMMAND_CREATE           = 1,
    E_CSIS_COMMAND_RELEASE          = 2,
    E_CSIS_COMMAND_UPDATE           = 3,
};

// CgsAemsFactory.h:68 (DWARF). One registered AEMS patch monitor.
struct PatchMonitor
{
    const char* mpName;       // CgsAemsFactory.h:70
    void*       mpClientFunc; // CgsAemsFactory.h:71
    void*       mpClientData; // CgsAemsFactory.h:72
    s32         miPerfmon;    // CgsAemsFactory.h:73
};

const u32 KU_MAX_PATCH_MONITORS = 16; // CgsAemsFactory.h:373 (DWARF)

// CgsAemsFactory.h:291 (DWARF): AemsFactory : public AemsRWSampleFactory.
// MINIMAL home -- see file banner FLAG. Only the patch-monitor table and the two
// leaf functions are modelled; the engine factory base is a typed-by-name
// placeholder.
class AemsFactory
{
public:
    // CgsAemsFactory.cpp:397 @ 0x8268A018. Debug-prints lpcText through the engine
    // log front-end when the log-category filter is enabled; returns lpcText so it
    // can chain. (X360 returns "<NULLSTRING>" when handed a null pointer.)
    const char* CsisPrint(const char* lpcText);

protected:
    // CgsAemsFactory.cpp:333 @ 0x82689E98. Linear-search the patch-monitor table
    // for the monitor whose mpName matches lpcName; returns it, or null if none.
    PatchMonitor* FindPatchMonitor(const char* lpcName);

private:
    // --- members (DWARF order; X360 offsets in comments, NOT asserted) ---
    // FLAG: placeholder for the un-homed AemsRWSampleFactory base sub-object that
    // precedes muPatchMonitorCount in the X360 layout. Not modelled by field.
    u32         muPatchMonitorCount;       // CgsAemsFactory.h:364  (+0x5C X360)
    void*       mpRegistry;                // CgsAemsFactory.h:366  (Registry*, opaque)
    void*       mhRwacFactory;             // CgsAemsFactory.h:367  (GenericRwacFactoryHandle, opaque)
    // CgsAemsFactory.h:368 mCommandQueue (CsisCommandQueue: 255-deep CommandQueue<uintptr_t>).
    // Modelled only by size so maPatchMonitors keeps its position relative to the
    // count; contents are not used by this TU's functions.
    uintptr_t   maCommandQueuePlaceholder[256]; // count word + 255 slots
    PatchMonitor maPatchMonitors[16];      // CgsAemsFactory.h:375  (+0x46C X360)
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_AEMS_CGSAEMSFACTORY_H
