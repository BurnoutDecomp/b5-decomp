#ifndef BRN_SOUND_PASSBY_PROP_BY_EVENT_H
#define BRN_SOUND_PASSBY_PROP_BY_EVENT_H

#include "types.hpp"

// =============================================================================
// BrnSound::Logic::Passby::PassbyPropByEvent -- the 64-byte element of the
// dynamic prop-by event queue PassbyStateManager::UpdateDynamicPropBys walks
// (BaseEventQueue<PassbyPropByEvent>::GetEvent @0x8268EA38: `slwi r11,r29,6` ==
// the 64-byte stride).
//
// FLAG (PLACEHOLDER -- element TYPE NOT ATTESTED): only the 64-byte stride is
// X360-attested; the field layout is not (the caller's DWARF opens
// `using namespace BrnPhysics::Props;` but names no queue; the manager's own
// DynamicPropByCache::Item is a different 12-byte plain-array record). Kept as
// an opaque payload; replace with the real DWARF-named type (same qualified
// name) once UpdateDynamicPropBys is disassembled.
//
// (2026-08-25, audio-faithfulness wave 5: HOMED here -- the definition used to
// live inside BaseEventQueue_PassbyPropByEvent_GetEvent.cpp, a namespace-scope
// class defined in a .cpp and a standing ODR hazard for the moment a second TU
// needed it.)
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Passby
{
    struct alignas(16) PassbyPropByEvent
    {
        u8 macOpaquePayload[64];
    };
}
}
}

#endif // BRN_SOUND_PASSBY_PROP_BY_EVENT_H
