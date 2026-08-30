#ifndef CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACVOICE_H
#define CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACVOICE_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace rw { namespace audio { namespace core {
class System;
class Voice;
class PlugIn;
} } }

namespace CgsSound
{
namespace Playback
{

class GenericRwacFactory;
class Voice;
struct VoiceSpec;
struct SubmixVoice;

// The non-polymorphic RWAC half shared by the player, submix and master wrapper
// classes. The X360 wrappers use multiple inheritance: their ordinary Playback
// Voice base is first and this block follows it at +0x2C/+0x30.
struct GenericRwacVoice
{
    struct ParameterMap
    {
        u8 mu8ParameterIndex;
        u8 mu8PluginOffset;
        u8 mu8Attribute;
        u8 mu8Direction;
    };

    GenericRwacVoice();
    ~GenericRwacVoice();

    bool CreateVoiceInstance(const VoiceSpec& akrSpec, Voice& arBaseVoice,
                             GenericRwacFactory& arFactory,
                             rw::audio::core::PlugIn** appSubmix);

    rw::audio::core::PlugIn* GetPlugin(u32 au32I);
    rw::audio::core::PlugIn* GetSendPlugin(u32 au32I);
    GenericRwacFactory& GetRwacFactory();

    f32 GetCpuTicks() const;
    void Update(rw::audio::core::System* apSystem, Voice& arVoice);
    bool ConnectSend(u32 au32Index, SubmixVoice* apSubmix);

    void AddParameterMap(u8 au8ParameterIndex, u8 au8PluginOffset,
                         u8 au8Attribute, u8 au8Direction);

    void ForceParameterUpdate() { mu8Flags |= 2u; }

    rw::audio::core::Voice* GetRwacVoice() const { return mpVoice; }
    u32 GetPluginCount() const { return mu16PluginCount; }

private:
    GenericRwacFactory*       mpFactory;            // X360 +0x00
    rw::audio::core::Voice*   mpVoice;              // +0x04
    rw::audio::core::PlugIn** mppPlugin;            // +0x08
    u16                       mu16PluginCount;       // +0x0C
    u16                       mu16FirstSendPlugin;   // +0x0E
    ParameterMap              maParameterMap[16];   // +0x10
    u32                       mu32ParameterMapCount; // +0x50
    u8                        mu8Flags;              // +0x54
};

} // namespace Playback
} // namespace CgsSound

#endif
