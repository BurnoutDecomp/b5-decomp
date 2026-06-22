#ifndef CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_VOICE_CONFIG_H
#define CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_VOICE_CONFIG_H

#include "types.hpp"

#include "rw/rwcore_structs.h"  // rw::IResourceAllocator

// ============================================================================
// GameShared/GameClasses/Sound/Playback/Rwac/CgsGenericRwacVoiceConfig.h
//
// MINIMAL home for the one bodied func of the GenericRwacVoiceConfig TU:
//   CgsSound::Playback::GenericRwacVoiceConfig::operator new(size_t, Environment&)
//     @ 0x826AEEE8   (placement new, DWARF CgsGenericRwacVoice.h:156)
//
// The full GenericRwacVoiceConfig (PlugInConfig[32] + scratchpad + counters, DWARF
// CgsGenericRwacVoice.h:96) is its own keystone TU; only the allocation entry point
// is reconstructed in this group. Environment is likewise its own keystone home
// (CgsEnvironment.h); modelled here BY NAME via GetAllocator() (CgsEnvironment.h:675)
// -- the only Environment surface operator new touches.
// ============================================================================

namespace CgsSound
{
namespace Playback
{
    // MINIMAL home (full layout in the Environment keystone TU). operator new only
    // reads the RenderWare allocator the Environment holds.
    struct Environment
    {
        rw::IResourceAllocator* GetAllocator() const; // CgsEnvironment.h:675 (own TU)
    };

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
