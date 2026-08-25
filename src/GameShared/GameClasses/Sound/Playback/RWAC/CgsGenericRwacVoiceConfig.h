#ifndef CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_VOICE_CONFIG_H
#define CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_VOICE_CONFIG_H

#include "types.hpp"

#include "rw/rwcore_structs.h"  // rw::IResourceAllocator
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"  // the REAL Environment (GetAllocator)

// ============================================================================
// GameShared/GameClasses/Sound/Playback/Rwac/CgsGenericRwacVoiceConfig.h
//
// MINIMAL home for the one bodied func of the GenericRwacVoiceConfig TU:
//   CgsSound::Playback::GenericRwacVoiceConfig::operator new(size_t, Environment&)
//     @ 0x826AEEE8   (placement new, DWARF CgsGenericRwacVoice.h:156)
//
// The full GenericRwacVoiceConfig (PlugInConfig[32] + scratchpad + counters, DWARF
// CgsGenericRwacVoice.h:96) is its own keystone TU; only the allocation entry point
// is reconstructed in this group. (2026-08-25 wave 4: the former TU-local minimal
// `struct Environment { GetAllocator(); }` rival is RETIRED -- the real
// CgsEnvironment.h home is includable since the wave-3 Object fold, and its
// GetAllocator is header-inline.)
// ============================================================================

namespace CgsSound
{
namespace Playback
{

    // MINIMAL home (full layout in the GenericRwacVoiceConfig keystone TU). Only the
    // placement operator new is reconstructed in this group.
    struct GenericRwacVoiceConfig
    {
        // CgsGenericRwacVoice.h:156. Allocates the config block through the sound
        // Environment's RenderWare IResourceAllocator; returns the base pointer.
        void* operator new(size_t auSize, Environment& arEnvironment);
    };

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_VOICE_CONFIG_H
