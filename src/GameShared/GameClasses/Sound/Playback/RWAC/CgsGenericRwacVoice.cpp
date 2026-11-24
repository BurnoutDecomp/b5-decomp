#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoice.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoiceConfig.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Playback/CgsSubmixVoice.h"
#include "GameShared/GameClasses/Sound/Playback/CgsMasterVoice.h"
#include "rw/audio/core/PlugIn.h"
#include "rw/audio/core/Voice.h"
#include "rw/audio/core/Send.h"

#include <algorithm>
#include <cstdlib>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace CgsSound
{
namespace Playback
{

GenericRwacVoice::GenericRwacVoice()
    : mpFactory(0), mpVoice(0), mppPlugin(0), mu16PluginCount(0),
      mu16FirstSendPlugin(0), mu32ParameterMapCount(0), mu8Flags(0)
{
}

// ARTIST's file string for this file's asserts (0x820B2E60).
#define CGS_GENERIC_RWAC_VOICE_CPP_ASSERT_FILE \
    "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/RWAC/CgsGenericRwacVoice.cpp"

// @0x826C1DC0 (DWARF cpp:53, local lCommand cpp:59). Release of the vendor voice is
// marshalled through the owning GenericRwacFactory's command queue, unconditionally:
// ARTIST asserts the master voice still exists (cpp:57), builds the release record
// (whose constructor asserts the voice) and posts it.
GenericRwacVoice::~GenericRwacVoice()
{
    CGS_ASSERT(MasterVoice::IsAvailable(), CGS_ASSERT_MESSAGE_AT(
        "MasterVoice::IsAvailable()", CGS_GENERIC_RWAC_VOICE_CPP_ASSERT_FILE, 57));
    RwacCommandVoiceRelease lCommand(mpVoice);
    GetRwacFactory().GetCommandQueue() << lCommand;

    mpVoice = 0;
    mppPlugin = 0;
    mpFactory = 0;
    mu16PluginCount = 0;
}

// @0x826D9B40 (DWARF cpp:74, locals lpVoiceConfig cpp:80 and lCommand cpp:94). ARTIST
// uses the config unchecked, sets the flags to 1 before the player-slot lookup and
// clears them when the voice has no player slot (0x826D9BBC..0x826D9BE8).
bool GenericRwacVoice::CreateVoiceInstance(
    const VoiceSpec& lVoiceSpec, Voice& lVoiceBase,
    GenericRwacFactory& lFactory, rw::audio::core::PlugIn** mppSubmix)
{
    mpFactory = &lFactory;
    GenericRwacVoiceConfig* lpVoiceConfig = lFactory.SetupConfig(lVoiceSpec, lVoiceBase, *this);

    mu16PluginCount = static_cast<uint16_t>(lpVoiceConfig->GetPluginCount());
    mu16FirstSendPlugin = static_cast<uint16_t>(lpVoiceConfig->GetFirstSendPlugin());
    CGS_ASSERT((E_MASTER_VOICE == lVoiceSpec.mu8VoiceType) || (mu16FirstSendPlugin > 0),
        CGS_ASSERT_MESSAGE_AT(
            "(E_MASTER_VOICE == lVoiceSpec.GetVoiceType()) || (mu16FirstSendPlugin > 0)",
            CGS_GENERIC_RWAC_VOICE_CPP_ASSERT_FILE, 84));

    mu8Flags = 1;
    if (!lVoiceBase.FindNamedSlot(PlayerVoice::SK_PLAYER_SLOT_NAME))
    {
        mu8Flags = 0;
    }

    RwacCommandVoiceCreateInstance lCommand(&lVoiceBase, &mpVoice, &mppPlugin, lpVoiceConfig,
                                            mppSubmix);
    GetRwacFactory().GetCommandQueue() << lCommand;
    return true;
}

f32 GenericRwacVoice::GetCpuTicks() const
{
    return mpVoice ? static_cast<f32>(static_cast<u32>(mpVoice->miLastFrameCpuTicks))
                   : 0.0f;
}

void GenericRwacVoice::AddParameterMap(u8 au8ParameterIndex, u8 au8PluginOffset,
                                       u8 au8Attribute, u8 au8Direction)
{
    CGS_ASSERT(mu32ParameterMapCount < 16u,
               "mu32ParameterMapCount < SKU32_PARAMETER_MAP_COUNT");
    if (mu32ParameterMapCount >= 16u)
        return;
    ParameterMap& lrMap = maParameterMap[mu32ParameterMapCount++];
    lrMap.mu8ParameterIndex = au8ParameterIndex;
    lrMap.mu8PluginOffset = au8PluginOffset;
    lrMap.mu8Attribute = au8Attribute;
    lrMap.mu8Direction = au8Direction;
}

// Instantiations of the original Update<T> template have identical bodies. The
// wrapper type is used only to reach the ordinary Playback::Voice tables, so the
// host keeps one by-name implementation.
void GenericRwacVoice::Update(rw::audio::core::System* apSystem, Voice& arVoice)
{
    CGS_ASSERT(apSystem != 0, "lpRwacSystem");
    if (mpVoice == 0)
        return;

    const bool lbForce = (mu8Flags & 2u) != 0;
    mu8Flags &= static_cast<u8>(~2u);

    for (u32 luSend = 0; luSend < arVoice.GetSendCount(); ++luSend)
    {
        Send& lrSend = arVoice.GetSend(luSend);
        if (lrSend.HasChanged() || lbForce)
        {
            rw::audio::core::PlugIn::SetAttribute(GetSendPlugin(luSend), 0,
                                                   lrSend.Get());
            lrSend.AcknowledgeChange();
        }
    }

    for (u32 luMap = 0; luMap < mu32ParameterMapCount; ++luMap)
    {
        const ParameterMap& lrMap = maParameterMap[luMap];
        rw::audio::core::PlugIn* lpPlugin = GetPlugin(lrMap.mu8PluginOffset);
        if (lrMap.mu8Direction == E_PARAMETER_OUTPUT)
        {
            OutputParameter& lrOutput =
                arVoice.GetOutputParameter(lrMap.mu8ParameterIndex);
            RwacCommandPluginGetAttribute lCommand(lpPlugin, lrMap.mu8Attribute,
                                                   lrOutput.GetAddress());
            GetRwacFactory().GetCommandQueue() << lCommand;
        }
        else
        {
            InputParameter& lrInput =
                arVoice.GetInputParameter(lrMap.mu8ParameterIndex);
            if (lrInput.HasChanged() || lbForce)
            {
                const f32 lfValue = std::max(lrInput.GetMin(),
                    std::min(lrInput.GetMax(), lrInput.GetValueRaw()));
                // [DIAG] NOT IN THE X360 BINARY -- BRN_RWAC_PARAM_DIAG=1. The clamp above is
                // silent: a schema whose maximum is below the value the game computed turns a
                // live parameter into a constant, with no symptom anywhere else. Print what
                // was asked for and what was actually pushed.
                {
                    static int siDiag = -1;
                    if (siDiag < 0)
                    {
                    }
                    if (siDiag == 1 && CgsDev::Log::gpDebugPrint != 0)
                    {
                        static u32 suCount = 0u;
                        if ((suCount++ % 64u) == 0u)
                        {
                            *CgsDev::Log::gpDebugPrint
                                << "[rwac-param] plug " << static_cast<s32>(
                                       reinterpret_cast<intptr_t>(lpPlugin) & 0xFFFFFF)
                                << " name=" << static_cast<s32>(
                                       static_cast<u32>(lrInput.GetName().GetValue()))
                                << " attr=" << static_cast<s32>(lrMap.mu8Attribute)
                                << " raw=" << lrInput.GetValueRaw()
                                << " min=" << lrInput.GetMin()
                                << " max=" << lrInput.GetMax()
                                << " pushed=" << lfValue
                                << (lfValue != lrInput.GetValueRaw() ? "  <-- CLAMPED" : "")
                                << "\n";
                        }
                    }
                }
                rw::audio::core::PlugIn::SetAttribute(lpPlugin,
                                                       lrMap.mu8Attribute,
                                                       lfValue);
                lrInput.SetChanged(false);
            }
        }
    }
}

bool GenericRwacVoice::ConnectSend(u32 au32Index, SubmixVoice* apSubmix)
{
    CGS_ASSERT(apSubmix != 0, "lpSubmixVoice");
    if (apSubmix == 0 || mpVoice == 0)
        return false;

    void* lpTarget = apSubmix->GetSubmix();
    void* lapPayload[1] = { lpTarget };
    rw::audio::core::Send::EventEvent(
        reinterpret_cast<rw::audio::core::Send*>(GetSendPlugin(au32Index)),
        0, lapPayload);
    return true;
}

} // namespace Playback
} // namespace CgsSound
