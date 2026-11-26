#include "GameSource/Sound/Vehicles/Engines/BrnLoopModelEffect.h"

// =============================================================================
// BrnSound::Vehicles::Engines::LoopModelEffect — out-of-line deleting-destructor
// body.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnLoopModelEffect.h for the
// inheritance rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// ~LoopModelEffect  @ 0x826AFBA0  (the X360 `scalar deleting destructor')
//   stw  &off_820AE9BC, 0(this)        ; primary vptr settle
//   stw  &off_820AE988, 4(this)        ; IResourceRequester sub-object vptr (intermediate)
//   stw  3,            0x28(this)      ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  &off_820AA820, 4(this)        ; IResourceRequester sub-object vptr (final settle)
//   stb  0,            0x31(this)      ; mbResourcesReady = false
//   stw  0,            0x24(this)      ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// The dual-vptr settle and the attach/detach/resources-ready member clears are the
// inherited BrnEffectObject teardown the compiler emits; this leaf destructor body
// adds nothing of its own.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 deleting destructor) rather
// than reproducing the raw allocator vtable call.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ARTIST 0x82F2F618: {0x10020, "LoopModelEffect", BrnEffectObject, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectObject>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> LoopModelEffect::sTypeInfo(
    0x10020, "LoopModelEffect", &BrnSound::Logic::BrnEffectObject::sTypeInfo, &LoopModelEffect::CreateObject);

CgsSound::Logic::EffectObject* LoopModelEffect::CreateObject(u32 luAllocator)
{
    if (luAllocator == 0)
        return new (sTypeInfo.typeName, false) LoopModelEffect();
    else
        return new (sTypeInfo.typeName, true) LoopModelEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* LoopModelEffect::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* LoopModelEffect::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* LoopModelEffect::GetTypeName() const
{
    return sTypeInfo.typeName;
}

LoopModelEffect::~LoopModelEffect()
{
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
