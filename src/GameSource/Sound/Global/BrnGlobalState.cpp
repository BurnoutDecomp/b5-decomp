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

// ---------------------------------------------------------------------------
// ~GlobalState  @ 0x826D2250  (scalar deleting destructor)
//
//   stw  off_820AE1F4, 0(this)               ; install GlobalState's own vtable
//   bl   CgsSound::Logic::State::DestroyEffects   ; (this in r3) tear down effects
//   stw  off_820AA820, 0(this)               ; re-install the MemBase base vtable
//   if (flags & 1)                           ; deleting flavour
//       <sound allocator>.Free(this)         ; via off_82FFB954, vtable slot +0x14
//
// The single observable source-level side effect is the DestroyEffects() call on
// the State base (reused BY NAME). The two vtable installs and the conditional
// allocator-routed free are the compiler-synthesised parts of MSVC's deleting-
// destructor thunk, re-emitted here from this virtual destructor + the class's
// operator delete; off_82FFB954 (the sound allocator) is not homed in this group,
// so the host toolchain's `delete` stands in for the custom-allocator dispatch.
//
// FLAG: State::DestroyEffects() is declaration-only in BrnState.h (its own body is
// a separate un-homed sound-logic recon slice). It is called BY NAME here to match
// the X360 `bl` exactly; no body is fabricated for it.
// ---------------------------------------------------------------------------
GlobalState::~GlobalState()
{
    DestroyEffects();
}

// ---------------------------------------------------------------------------
// GetStaticTypeInfo  @ 0x826867F8
//
//   lis   r11, unk_82F2F86C@ha
//   addi  r3,  r11, unk_82F2F86C@l   ; r3 = &sTypeInfo (the rodata descriptor)
//   blr
//
// Returns GlobalState's per-class static RTTI descriptor. typeName is PROVEN:
// descriptor unk_82F2F86C + 4 = off_82F2F870, which the committed GetTypeName above
// attests as "GlobalState". Mirrors the committed PassbyState::GetStaticTypeInfo form
// (function-local static, aggregate-initialised ClassTypeInfo<State>).
//
// FLAG (high on class+typeName; low on ObjectID/base): the State-side ObjectID is NOT in
// the committed PS3 manager-id list and is unrecovered -> seeded 0 and FLAGGED; base/
// createObject deferred (BrnState descriptor chain un-homed), mirroring PassbyState.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GlobalState::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> sTypeInfo(
        0, "GlobalState", 0, &GlobalState::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::State* GlobalState::CreateObject(u32 /*auType*/)
{
    return new GlobalState();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GlobalState::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* const gpGlobalStateReg =
    CgsSound::Logic::State::AddToClassTypeInfoArray(GlobalState::GetStaticTypeInfo());

} // namespace Logic
} // namespace BrnSound
