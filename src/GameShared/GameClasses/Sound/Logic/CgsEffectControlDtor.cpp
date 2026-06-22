// ============================================================================
// CgsEffectControlDtor.cpp -- CgsSound::Logic::EffectControl destructor home.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   CgsSound::Logic::EffectControl::`scalar deleting destructor'  @ 0x826963D8
//
// The X360 codegen at 0x826963D8 is MSVC's scalar-deleting-destructor for
// EffectControl. Decoded from the asm:
//   stw  off_820AA820, 0(this)   ; re-install the MemBase base-subobject vtable
//   stb  0, 0x2D(this)           ; mbHasLoadedData = false
//   stw  3, 0x24(this)           ; meDetachState  = E_DETACH_STATE_FINISHED
//   stw  0, 0x20(this)           ; meAttachState  = E_ATTACH_STATE_NONE
//   if (flags & 1)               ; deleting flavour
//       <sound allocator>.Free(this)   ; via off_82FFB954, vtable slot +0x14 (0x20/4)
//
// off_820AA820 is the shared CgsSound::MemBase vtable (see CgsMemBase.h) -- the
// final base-subobject vptr installed as the destructor chain unwinds. The three
// explicit member stores are the source-level destructor body and are reproduced
// by name through EffectBase::ResetOnDestroy(); the vtable re-install and the
// conditional allocator-routed free are the compiler-synthesised parts of the
// deleting-destructor thunk, which MSVC re-emits from this virtual destructor +
// the class's own operator delete. The sound allocator instance (off_82FFB954) is
// not homed in this group, so the host toolchain's `delete` stands in for the
// custom-allocator dispatch rather than fabricating the raw allocator vtable call.
//
// The rest of EffectControl's surface (GetTypeName / GetStaticTypeInfo) lives in
// the sibling CgsEffectBase.cpp; this TU owns only the destructor so its vtable
// slot is emitted exactly once.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"

namespace CgsSound
{
namespace Logic
{

EffectControl::~EffectControl()
{
    ResetOnDestroy();
}

} // namespace Logic
} // namespace CgsSound
