#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoice.h"

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoiceConfig.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Playback/CgsSubmixVoice.h"
#include "rw/audio/core/PlugIn.h"
#include "rw/audio/core/Voice.h"
#include "rw/audio/core/Send.h"

#include <algorithm>

namespace CgsSound
{
namespace Playback
{

GenericRwacVoice::GenericRwacVoice()
    : mpFactory(0), mpVoice(0), mppPlugin(0), mu16PluginCount(0),
      mu16FirstSendPlugin(0), mu32ParameterMapCount(0), mu8Flags(0)
{
}

// @0x826C1DC0. Release of the vendor voice is marshalled through the owning
// GenericRwacFactory so it runs under the same lock as voice creation.
GenericRwacVoice::~GenericRwacVoice()
{
    if (mpVoice != 0 && mpFactory != 0)
    {
        RwacCommandQueue& lrQueue = mpFactory->GetCommandQueue();
        lrQueue.PostCommand(2u);
        lrQueue.PostCommand(E_RWAC_COMMAND_VOICE_RELEASE);
        lrQueue.PostCommand(reinterpret_cast<uintptr_t>(mpVoice));
    }

    mpVoice = 0;
    mppPlugin = 0;
    mpFactory = 0;
    mu16PluginCount = 0;
}

bool GenericRwacVoice::CreateVoiceInstance(
    const VoiceSpec& akrSpec, Voice& arBaseVoice,
    GenericRwacFactory& arFactory, rw::audio::core::PlugIn** appSubmix)
{
    mpFactory = &arFactory;
    GenericRwacVoiceConfig* lpConfig =
        arFactory.SetupConfig(akrSpec, arBaseVoice, *this);
    CGS_ASSERT(lpConfig != 0, "lpVoiceConfig");
    if (lpConfig == 0)
        return false;

    mu16PluginCount = static_cast<u16>(lpConfig->GetPluginCount());
    mu16FirstSendPlugin = static_cast<u16>(lpConfig->GetFirstSendPlugin());
    CGS_ASSERT(akrSpec.mu8VoiceType == E_MASTER_VOICE || mu16FirstSendPlugin > 0,
               "(E_MASTER_VOICE == lVoiceSpec.GetVoiceType()) || (mu16FirstSendPlugin > 0)");

    mu8Flags = arBaseVoice.FindNamedSlot(PlayerVoice::SK_PLAYER_SLOT_NAME) != 0 ? 1u : 0u;

    RwacCommandVoiceCreateInstance lCommand(
        reinterpret_cast<uintptr_t>(&arBaseVoice),
        reinterpret_cast<uintptr_t>(&mpVoice),
        reinterpret_cast<uintptr_t>(&mppPlugin),
        reinterpret_cast<uintptr_t>(lpConfig),
        reinterpret_cast<uintptr_t>(appSubmix));
    arFactory.GetCommandQueue().Post(lCommand);
    return true;
}

rw::audio::core::PlugIn* GenericRwacVoice::GetPlugin(u32 au32I)
{
    CGS_ASSERT(au32I < mu16PluginCount, "lu32I < mu16PluginCount");
    CGS_ASSERT(mppPlugin != 0 && mppPlugin[au32I] != 0, "mppPlugin[lu32I]");
    return mppPlugin[au32I];
}

rw::audio::core::PlugIn* GenericRwacVoice::GetSendPlugin(u32 au32I)
{
    CGS_ASSERT(mu16FirstSendPlugin > 0, "mu16FirstSendPlugin > 0");
    const u32 lu32I = static_cast<u32>(mu16FirstSendPlugin) + au32I;
    return GetPlugin(lu32I);
}

GenericRwacFactory& GenericRwacVoice::GetRwacFactory()
{
    CGS_ASSERT(mpFactory != 0, "mpFactory");
    return *mpFactory;
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
            RwacCommandQueue& lrQueue = GetRwacFactory().GetCommandQueue();
            lrQueue.PostCommand(4u);
            lrQueue.PostCommand(E_RWAC_COMMAND_PLUGIN_GET_ATTRIBUTE);
            lrQueue.PostCommand(reinterpret_cast<uintptr_t>(lpPlugin));
            lrQueue.PostCommand(lrMap.mu8Attribute);
            lrQueue.PostCommand(reinterpret_cast<uintptr_t>(lrOutput.GetAddress()));
        }
        else
        {
            InputParameter& lrInput =
                arVoice.GetInputParameter(lrMap.mu8ParameterIndex);
            if (lrInput.HasChanged() || lbForce)
            {
                const f32 lfValue = std::max(lrInput.GetMin(),
                    std::min(lrInput.GetMax(), lrInput.GetValueRaw()));
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
