#ifndef CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACVOICE_H
#define CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACVOICE_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// ARTIST's file string for this header's inline asserts (0x820AB8D8).
#define CGS_GENERIC_RWAC_VOICE_H_ASSERT_FILE \
    "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/Rwac/CgsGenericRwacVoice.h"

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

    // CgsGenericRwacVoice.h:357/360/374 (DWARF), bodied inline below.
    rw::audio::core::PlugIn* GetPlugin(uint32_t lu32I);
    rw::audio::core::PlugIn* GetSendPlugin(uint32_t lu32Index);
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

// CgsGenericRwacVoice.h:467 (DWARF h:357). ARTIST copy 0x82682700, asserts 469/470.
inline rw::audio::core::PlugIn* GenericRwacVoice::GetPlugin(uint32_t lu32I)
{
    CGS_ASSERT(lu32I < mu16PluginCount, CGS_ASSERT_MESSAGE_AT(
        "lu32I < mu16PluginCount", CGS_GENERIC_RWAC_VOICE_H_ASSERT_FILE, 469));
    CGS_ASSERT(mppPlugin[lu32I], CGS_ASSERT_MESSAGE_AT(
        "mppPlugin[lu32I]", CGS_GENERIC_RWAC_VOICE_H_ASSERT_FILE, 470));
    return mppPlugin[lu32I];
}

// CgsGenericRwacVoice.h:477 (DWARF h:360). ARTIST copy 0x82682788: its own index
// asserts (479, then 483/484 -- not a GetPlugin call; PS3 inlines the same lines).
inline rw::audio::core::PlugIn* GenericRwacVoice::GetSendPlugin(uint32_t lu32Index)
{
    CGS_ASSERT(mu16FirstSendPlugin > 0, CGS_ASSERT_MESSAGE_AT(
        "mu16FirstSendPlugin > 0", CGS_GENERIC_RWAC_VOICE_H_ASSERT_FILE, 479));
    uint32_t lu32I = mu16FirstSendPlugin + lu32Index;
    CGS_ASSERT(lu32I < mu16PluginCount, CGS_ASSERT_MESSAGE_AT(
        "lu32I < mu16PluginCount", CGS_GENERIC_RWAC_VOICE_H_ASSERT_FILE, 483));
    CGS_ASSERT(mppPlugin[lu32I], CGS_ASSERT_MESSAGE_AT(
        "mppPlugin[lu32I]", CGS_GENERIC_RWAC_VOICE_H_ASSERT_FILE, 484));
    return mppPlugin[lu32I];
}

// CgsGenericRwacVoice.h:596 (DWARF h:374). ARTIST copy 0x82682840, assert 598.
inline GenericRwacFactory& GenericRwacVoice::GetRwacFactory()
{
    CGS_ASSERT(mpFactory, CGS_ASSERT_MESSAGE_AT(
        "mpFactory", CGS_GENERIC_RWAC_VOICE_H_ASSERT_FILE, 598));
    return *mpFactory;
}

} // namespace Playback
} // namespace CgsSound

#endif
