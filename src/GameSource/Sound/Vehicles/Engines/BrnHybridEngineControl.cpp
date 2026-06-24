#include "GameSource/Sound/Vehicles/Engines/BrnHybridEngineControl.h"

// =============================================================================
// BrnSound::Vehicles::Engines::HybridEngineControl::UpdateGinsuRPM  @ 0x82699B98
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnHybridEngineControl.h for the
// MINIMAL home note and the full asm trace. Single-step shift of the Ginsu RPM
// DataPoint: read the engine control's current Ginsu RPM, move this control's
// current value into its previous slot, then store the freshly sampled value.
//   lwz  r11, 0x130(this)   ; mpEngineControl
//   lfs  f13, 0x8C(this)    ; old current
//   lfs  f0,  0x8C(r11)     ; new sample from mpEngineControl
//   stfs f13, 0x90(this)    ; previous = old current
//   stfs f0,  0x8C(this)    ; current  = new sample
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

void HybridEngineControl::UpdateGinsuRPM()
{
    const f32 lfNewSample = mpEngineControl->mGinsuRpm.mfCurrent; // lfs f0, 0x8C(r11)
    mGinsuRpm.mfPrevious  = mGinsuRpm.mfCurrent;                  // stfs f13, 0x90(this)
    mGinsuRpm.mfCurrent   = lfNewSample;                          // stfs f0,  0x8C(this)
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
