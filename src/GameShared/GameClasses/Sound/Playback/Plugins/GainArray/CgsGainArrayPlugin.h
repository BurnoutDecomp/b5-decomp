#pragma once

// =====================================================================================
// CgsSound::Playback::Plugins::GainArray -- the game-side per-channel gain-array plug-in
// ("JGA0"), one of the three custom descriptors Burnout registers into the RenderWare
// audio plug-in registry alongside the stock rwaudio set.
//
// It holds six TARGET gains as graph attributes and six CURRENT gains, and every frame
// ramps each channel from its current gain toward its target through the shared
// CopyWithGainRamp kernel -- a de-clicked per-channel volume stage.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; decode report
// progress/scratch_dossiers/streammod_gainarray_decode_codex.md (TARGET 2).
//   GetSize        @0x82689E08 -> console 0x70; host sizeof
//   CreateInstance @0x826C3A10
//   Process        @0x8268CDB0
//   `scalar deleting destructor' @0x826AA9E0
//   descriptor off_82F2E664 'JGA0' (type 4, 0 ctor params, 6 attributes, 0 events)
//
// This is a GAME-side plug-in, so there is no Feb-2007 vendor header for it; the member
// names below come from the decode's offset/access analysis. It IS a real
// rw::audio::core::PlugIn subclass: the console's +0x0C store points the base attribute
// table at the six target gains, which only makes sense through the real base.
// =====================================================================================

#include "types.hpp"                    // f32, u8, u32
#include "rw/audio/core/PlugIn.h"       // rw::audio::core::PlugIn + Attribute_t

namespace rw { namespace audio { namespace core { class Mixer; } } }

namespace CgsSound
{
namespace Playback
{
namespace Plugins
{

// -------------------------------------------------------------------------------------
// GainArray -- console sizeof 0x70. Layout grounded in CreateInstance @0x826C3A10 and
// Process @0x8268CDB0:
//   +0x00..+0x27  the rw::audio::core::PlugIn base (mOutputChannels @+0x21 is the loop bound)
//   +0x28  maTargetGain[6]   -- the six graph attributes (8-byte Attribute_t stride), the
//                               level each channel is ramping TOWARD
//   +0x58  maCurrentGain[6]  -- the level each channel is ramping FROM (4-byte stride)
// -------------------------------------------------------------------------------------
class GainArray : public rw::audio::core::PlugIn
{
public:
    enum { KU_GUID = 0x4A474130u };   // 'JGA0'
    enum { KI_CHANNELS = 6 };         // the six gain slots the console initialises

    static char **GetPlugInDescRunTime();
    static int    GetSize();                                  // @0x82689E08
    static int    CreateInstance(GainArray *self);            // @0x826C3A10
    static int    Process(GainArray *self, rw::audio::core::Mixer *ctx,
                          bool firstPass);                    // @0x8268CDB0

    rw::audio::core::PlugIn::Attribute_t maTargetGain[KI_CHANNELS];  // +0x28 .. +0x57
    f32 maCurrentGain[KI_CHANNELS];                                   // +0x58 .. +0x6F
};

} // namespace Plugins
} // namespace Playback
} // namespace CgsSound
