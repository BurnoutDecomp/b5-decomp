#ifndef CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_VOICE_CONFIG_H
#define CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_VOICE_CONFIG_H

#include "types.hpp"

#include "rw/rwcore_structs.h"  // rw::IResourceAllocator
#include "rw/audio/core/Voice.h"
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"  // the REAL Environment (GetAllocator)
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"

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

    struct GenericRwacVoiceScratchpad
    {
        GenericRwacVoiceScratchpad() : muIndex(0) {}

        void* Allocate(size_t auSize)
        {
            const size_t luIndex = muIndex;
            CGS_ASSERT(luIndex + auSize <= sizeof(mau8Buff), "Allocate Failed");
            if (luIndex + auSize > sizeof(mau8Buff))
                return 0;
            muIndex += auSize;
            return &mau8Buff[luIndex];
        }

        size_t muIndex;
        u8 mau8Buff[64];
    };

    struct GenericRwacVoiceConfig
    {
        // CgsGenericRwacVoice.h:156. Allocates the config block through the sound
        // Environment's RenderWare IResourceAllocator; returns the base pointer.
        void* operator new(size_t auSize, Environment& arEnvironment);
        void operator delete(void* apMemory, Environment& arEnvironment);

        explicit GenericRwacVoiceConfig(Environment& arEnvironment);

        rw::audio::core::VoiceStageConfig& GetConfig(u32 au32Index)
        {
            CGS_ASSERT(au32Index < 32u, "lu32I < SKU32_CONFIG_COUNT");
            return maConfig[au32Index];
        }
        void SetConfig(u32 au32Index, void* apHandle, u8 au8Channels,
                       void* apContext)
        {
            rw::audio::core::VoiceStageConfig& lrConfig = GetConfig(au32Index);
            lrConfig.mpContext = apContext;
            lrConfig.mpDesc = static_cast<rw::audio::core::PlugInDescRunTime*>(apHandle);
            lrConfig.mFlagAndField8 = au8Channels;
        }

        GenericRwacVoiceScratchpad& GetScratchpad() { return mScratchPad; }
        u32 GetPluginCount() const { return mu32PluginCount; }
        void SetPluginCount(u32 auCount) { mu32PluginCount = auCount; }
        u32 GetFirstSendPlugin() const { return mu32FirstSendPlugin; }
        void SetFirstSendPlugin(u32 auIndex) { mu32FirstSendPlugin = auIndex; }
        int GetProcessingStage() const { return miProcessingStage; }
        void SetProcessingStage(int aiStage) { miProcessingStage = aiStage; }
        EVoiceType GetVoiceType() const { return meVoiceType; }
        void SetVoiceType(EVoiceType aeType) { meVoiceType = aeType; }
        Environment& GetEnvironment() const { return mEnvironment; }
        void Release();

    private:
        rw::audio::core::VoiceStageConfig maConfig[32];
        GenericRwacVoiceScratchpad mScratchPad;
        u32 mu32PluginCount;
        u32 mu32FirstSendPlugin;
        int miProcessingStage;
        EVoiceType meVoiceType;
        Environment& mEnvironment;
    };

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_VOICE_CONFIG_H
