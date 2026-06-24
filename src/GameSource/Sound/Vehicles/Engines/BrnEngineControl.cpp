#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"

// =============================================================================
// BrnSound::Vehicles::Engines::EngineControl::GetStartRPM  @ 0x82698FC8
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnEngineControl.h for the
// MINIMAL home note. The X360 codegen is a single-field f32 accessor:
//   0x82698FC8  lfs  f1, 0x18(r3)   ; load mfStartRPM
//   0x82698FCC  blr                 ; return it
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

f32 EngineControl::GetStartRPM() const
{
    return mfStartRPM; // lfs f1, 0x18(this)
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
