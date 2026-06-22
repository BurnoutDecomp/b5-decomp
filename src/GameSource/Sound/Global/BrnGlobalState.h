#ifndef BRN_SOUND_LOGIC_GLOBAL_STATE_H
#define BRN_SOUND_LOGIC_GLOBAL_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"

// =============================================================================
// BrnSound::Logic::GlobalState
//   GameSource/Sound/Global/BrnGlobalState.h (DWARF home) +
//   GameSource/Sound/Global/BrnGlobalState.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. GlobalState is the "global" node of
// the sound-logic state machine: DWARF shows
//   BrnSound::Logic::GlobalState : public BrnSound::Logic::BrnState
// and it overrides the per-class RTTI hooks. This TU bodies only GetTypeName().
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): GetTypeName() loads a static string
// pointer and ignores `this`, so no member offsets are touched and none are
// asserted here. The committed BrnState base (BrnState.h) is reused BY NAME.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// BrnGlobalState.h:31 (DWARF). Derives the sound-logic state base and overrides
// the RTTI virtuals. GetTypeName() is bodied in this TU's .cpp; the remaining
// RTTI hooks (GetTypeInfo/CreateObject/GetStaticTypeInfo) are declared for the
// vtable shape but DEFERRED (outside this TU's recon'd function set: the boot
// trace executed only GetTypeName).
struct GlobalState : public BrnSound::Logic::BrnState
{
    GlobalState() {}
    virtual ~GlobalState() {}

    // — per-class RTTI. DEFERRED bodies (declared for the
    // state vtable shape; sTypeInfo static-init lives in its own recon slice).
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;

    // @ 0x82686808 — per-class RTTI type name. Bodied here.
    virtual const char* GetTypeName() const;
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_GLOBAL_STATE_H
