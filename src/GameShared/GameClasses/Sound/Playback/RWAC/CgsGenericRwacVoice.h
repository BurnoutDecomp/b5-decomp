#ifndef CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACVOICE_H
#define CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACVOICE_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace rw
{
namespace audio
{
namespace core
{
    class Voice;
    class PlugIn;
}
}
}

// ============================================================================
// CgsGenericRwacVoice.h  (MINIMAL home for the GenericRwacVoice plug-in accessors).
//
// GenericRwacVoice wraps a RenderWare audio Voice + its plug-in chain. The three
// reconstructed accessors read the plug-in table:
//   GenericRwacVoice::GetPlugin(u32)      @ 0x82682700
//   GenericRwacVoice::GetRwacFactory()    @ 0x82682840
//   GenericRwacVoice::GetSendPlugin(u32)  @ 0x82682788
//
// Layout (X360; host-width FLAG -- pointers widen, pinned BY NAME):
//   +0x00 mpFactory   +0x04 mpVoice   +0x08 mppPlugin
//   +0x0C mu16PluginCount   +0x0E mu16FirstSendPlugin
//
// FLAG: MINIMAL home -- GenericRwacVoice's full base hierarchy (Voice + Object) and
// the rest of its surface are DEFERRED to the keystone TU; only the plug-in-table
// members the three accessors read are modelled.
// ============================================================================

namespace CgsSound
{
namespace Playback
{

class GenericRwacFactory;

struct GenericRwacVoice
{
    // @ 0x82682700. Bounds-checked plug-in accessor (assert index < count, non-null).
    rw::audio::core::PlugIn* GetPlugin(u32 au32I);

    // @ 0x82682788. Send-plug-in accessor (index offset by mu16FirstSendPlugin).
    rw::audio::core::PlugIn* GetSendPlugin(u32 au32I);

    // @ 0x82682840. Return the owning RWAC factory (assert non-null).
    GenericRwacFactory& GetRwacFactory();

    GenericRwacFactory*       mpFactory;            // +0x00
    rw::audio::core::Voice*   mpVoice;              // +0x04
    rw::audio::core::PlugIn** mppPlugin;            // +0x08
    u16                       mu16PluginCount;      // +0x0C
    u16                       mu16FirstSendPlugin;  // +0x0E
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACVOICE_H
