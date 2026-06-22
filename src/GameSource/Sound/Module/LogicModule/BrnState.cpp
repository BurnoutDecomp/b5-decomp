#include "GameSource/Sound/Module/LogicModule/BrnState.h"

// =============================================================================
// BrnSound::Logic::BrnState — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnState.h for the inheritance
// rationale.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// GetTypeName  @ 0x82682A98
//
//   lis   r11, off_82F2E7E0@ha
//   addi  r11, r11, off_82F2E7E0@l
//   lwz   r3,  (off_82F2E7E0)(r11)   ; r3 = "BrnState"
//   blr
//
// Returns the per-class RTTI type name. The X360 loads a pointer to the static
// string literal "BrnState" (the rodata at off_82F2E7E0 holds the address of
// that C string).
// ---------------------------------------------------------------------------
const char* BrnState::GetTypeName() const
{
    return "BrnState";
}

} // namespace Logic
} // namespace BrnSound
