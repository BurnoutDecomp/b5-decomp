#pragma once

// A Send or Route owns a connector. SubMix keeps the connector's intrusive node
// in its inbound list. LegacySrc submix.h and the Xbox 360 audio PDB agree on
// the member order used by ARTIST Disconnect at 0x82B9C3C0.

#include "types.hpp"             // f32
#include "rw/audio/core/private/linklist.h"

#include <cstddef> // offsetof -- the relative layout pins below

namespace rw
{
namespace audio
{
namespace core
{

class SubMix;          // fwd -- the real declaration is in rw/audio/core/SubMix.h
class Voice;

class SubMixConnector
{
public:
    // Unlink this connector from its SubMix's connector list. When `foldBackGains` is
    // non-null it points at a per-channel gain array (f32[mbNumChannels]) whose values
    // are added into the SubMix's channel gains (marking the SubMix dirty) before the
    // connector's owning-SubMix / cursor / flag fields are cleared.
    //
    void Connect(Voice *voice, SubMix *submix);
    void Disconnect(f32 *foldBackGains = 0);

    static SubMixConnector *GetConnectorFromNode(void *node)
    {
        return reinterpret_cast<SubMixConnector *>(node);
    }

    ListDNode mListNode;        // +0x00
    f32 *mpSubMixBuffer;       // +0x08  (was mField08/int) -- the owning SubMix's buffer
    SubMix *mpSubMix;          // +0x0C
    u8 mNumSubMixChannels;     // +0x10  (was mbField10)
};

CGS_STATIC_ASSERT(offsetof(SubMixConnector, mListNode) == 0, "connector starts at its list node");

} // namespace core
} // namespace audio
} // namespace rw
