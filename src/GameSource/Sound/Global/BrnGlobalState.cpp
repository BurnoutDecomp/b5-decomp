#include "GameSource/Sound/Global/BrnGlobalState.h"

// =============================================================================
// BrnSound::Logic::GlobalState — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnGlobalState.h for the
// inheritance rationale.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// GetTypeName  @ 0x82686808
//
//   lis   r11, off_82F2F870@ha
//   addi  r11, r11, off_82F2F870@l
//   lwz   r3,  (off_82F2F870)(r11)   ; r3 = "GlobalState"
//   blr
//
// Returns the per-class RTTI type name. The X360 loads a pointer to the static
// string literal (the rodata at off_82F2F870 holds the address of that C
// string), mirroring BrnState::GetTypeName (@ 0x82682A98).
// ---------------------------------------------------------------------------
const char* GlobalState::GetTypeName() const
{
    return "GlobalState";
}

} // namespace Logic
} // namespace BrnSound
