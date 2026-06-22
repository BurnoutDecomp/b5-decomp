#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"

// =============================================================================
// BrnSound::Logic::BrnStateManager — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnStateManager.h for the
// dual-base inheritance rationale.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// GetTypeName  @ 0x82682AB8
//
//   lis   r11, off_82F2E7F0@ha
//   addi  r11, r11, off_82F2E7F0@l
//   lwz   r3,  (off_82F2E7F0)(r11)   ; r3 = "BrnStateManager"
//   blr
//
// Returns the per-class RTTI type name. The X360 loads a pointer to the static
// string literal "BrnStateManager" (the rodata at off_82F2E7F0 holds the
// address of that C string).
// ---------------------------------------------------------------------------
const char* BrnStateManager::GetTypeName() const
{
    return "BrnStateManager";
}

} // namespace Logic
} // namespace BrnSound
