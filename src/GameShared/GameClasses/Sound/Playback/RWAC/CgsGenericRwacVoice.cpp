// ============================================================================
// CgsGenericRwacVoice.cpp -- CgsSound::Playback::GenericRwacVoice plug-in accessors.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GenericRwacVoice::GetPlugin(u32)      @ 0x82682700
//   GenericRwacVoice::GetSendPlugin(u32)  @ 0x82682788
//   GenericRwacVoice::GetRwacFactory()    @ 0x82682840
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoice.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

// @ 0x82682700.
rw::audio::core::PlugIn* GenericRwacVoice::GetPlugin(u32 au32I)
{
    CGS_ASSERT(au32I < mu16PluginCount, "lu32I < mu16PluginCount");
    CGS_ASSERT(mppPlugin[au32I] != 0, "mppPlugin[lu32I]");
    return mppPlugin[au32I];
}

// @ 0x82682788.
rw::audio::core::PlugIn* GenericRwacVoice::GetSendPlugin(u32 au32I)
{
    CGS_ASSERT(mu16FirstSendPlugin > 0, "mu16FirstSendPlugin > 0");
    u32 lu32I = static_cast<u32>(mu16FirstSendPlugin) + au32I;
    CGS_ASSERT(lu32I < mu16PluginCount, "lu32I < mu16PluginCount");
    CGS_ASSERT(mppPlugin[lu32I] != 0, "mppPlugin[lu32I]");
    return mppPlugin[lu32I];
}

// @ 0x82682840.
GenericRwacFactory& GenericRwacVoice::GetRwacFactory()
{
    CGS_ASSERT(mpFactory != 0, "mpFactory");
    return *mpFactory;
}

} // namespace Playback
} // namespace CgsSound
