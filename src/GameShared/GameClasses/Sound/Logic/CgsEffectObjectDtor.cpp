// ============================================================================
// CgsEffectObjectDtor.cpp -- CgsSound::Logic::EffectObject destructor home.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   CgsSound::Logic::EffectObject::`vector deleting destructor'  @ 0x826965D0
//
// The X360 codegen at 0x826965D0 is MSVC's vector-deleting-destructor for
// EffectObject. Its store sequence is byte-for-byte identical to the EffectControl
// thunk at 0x826963D8 (same base, same member resets, same allocator free):
//   stw  off_820AA820, 0(this)   ; re-install the MemBase base-subobject vtable
//   stb  0, 0x2D(this)           ; mbHasLoadedData = false
//   stw  3, 0x24(this)           ; meDetachState  = E_DETACH_STATE_FINISHED
//   stw  0, 0x20(this)           ; meAttachState  = E_ATTACH_STATE_NONE
//   if (flags & 1)               ; deleting flavour
//       <sound allocator>.Free(this)   ; via off_82FFB954, vtable slot +0x14
//
// off_820AA820 is the shared CgsSound::MemBase vtable (see CgsMemBase.h). The three
// explicit member stores are reproduced by name through EffectBase::ResetOnDestroy();
// the vtable re-install + conditional allocator free are the compiler-synthesised
// parts of the deleting-destructor thunk (MSVC re-emits them from this virtual
// destructor + the class's operator delete). off_82FFB954 (the sound allocator) is
// not homed in this group, so the host `delete` stands in for the custom dispatch.
//
// The rest of EffectObject's surface (GetTypeName / GetStaticTypeInfo) lives in the
// sibling CgsEffectBase.cpp; this TU owns only the destructor.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsEffectBase.h"

namespace CgsSound
{
namespace Logic
{

EffectObject::~EffectObject()
{
    ResetOnDestroy();
}

} // namespace Logic
} // namespace CgsSound
