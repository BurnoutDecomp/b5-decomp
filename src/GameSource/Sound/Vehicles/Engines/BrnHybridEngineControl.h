#ifndef BRN_SOUND_VEHICLES_ENGINES_HYBRID_ENGINE_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_HYBRID_ENGINE_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Vehicles/Engines/BrnHybridExhaustControl.h"   // HybridExhaustControl base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Engines::HybridEngineControl
//   GameSource/Sound/Vehicles/Engines/BrnHybridEngineControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity). DWARF
// (BrnHybridEngineControl.h:187): HybridEngineControl : public HybridExhaustControl.
// The dual-source (loop + Ginsu) engine EffectControl leaf. It adds no data members of
// its own (all engine/mix state lives on the HybridExhaustControl base); the two leaf
// vptr installs (primary/EffectControl @+0, IResourceRequester sub-object @+4) are
// produced structurally by the base spine + the virtual dtor.
//
// UPGRADE NOTE: this class was previously a standalone minimal struct (for
// UpdateGinsuRPM only). It is now the real X360-attested leaf deriving from
// HybridExhaustControl, so CreateObject / the deleting-destructor anchor can be homed
// and UpdateGinsuRPM operates on the base's committed DataPoint<f32> mGinsuRpm.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct HybridEngineControl : public HybridExhaustControl
{
    HybridEngineControl() {}
    virtual ~HybridEngineControl();     // anchor for the scalar deleting destructor @ 0x826CC7F0

    // @ 0x826CC738 -- RTTI factory hook.
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );

    // @ 0x82699B98 -- shift the Ginsu RPM DataPoint forward one frame, sampling the
    // freshly-computed Ginsu RPM from the sampled engine control (a HybridExhaustControl-
    // shaped control at +0x130; its mGinsuRpm current lives at +0x8C on the X360).
    void UpdateGinsuRPM();
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_HYBRID_ENGINE_CONTROL_H
