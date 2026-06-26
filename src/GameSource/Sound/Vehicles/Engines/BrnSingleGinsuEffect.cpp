#include "GameSource/Sound/Vehicles/Engines/BrnSingleGinsuEffect.h"

// =============================================================================
// BrnSound::Vehicles::Engines::SingleGinsuEffect — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnSingleGinsuEffect.h for the
// inheritance rationale and the controller-attach modelling note.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// ~SingleGinsuEffect  @ 0x826AFAF0  (the X360 `scalar deleting destructor')
//
//   stw  &off_820AE9BC, 0(this)   ; primary vptr settle (SingleGinsuEffect vtable)
//   stw  &off_820AE988, 4(this)   ; IResourceRequester sub-object vptr (intermediate)
//   stw  3,            0x28(this)  ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  &off_820AA820, 4(this)   ; IResourceRequester sub-object vptr (final settle,
//                                    the shared BrnEffectObject base vtable)
//   stb  0,            0x31(this)  ; mbResourcesReady = false
//   stw  0,            0x24(this)  ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// The dual-vptr settle (+0/+4) and the attach/detach/resources-ready member clears
// (+0x24/+0x28/+0x31) are the inherited BrnEffectObject teardown the compiler emits
// (identical store-for-store to the sibling LoopModelEffect dtor @ 0x826AFBA0 and the
// committed BrnEffectObject dtor @ 0x826AF4C8); this leaf destructor body adds nothing
// of its own. The X360 asm writes NO +0x34 store, so mpHybridControl is left
// untouched by the destructor.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left to
// the host toolchain (the `delete` half of the X360 deleting destructor) rather than
// reproducing the raw allocator vtable call.
// ---------------------------------------------------------------------------
SingleGinsuEffect::~SingleGinsuEffect()
{
}

// The controller-class test in AttachController. The X360 masks the controller's
// object-id (`*(controller+0x14) & 0x7F0`) and compares it to the HybridExhaust
// controller-class tag (0x50). PS3 DecFIGS confirms this: AttachController (0x8297FC)
// tests `((controller->+0x14 >> 4) & 0x7F) == 5`, i.e. the same class-tag 5 (0x50 in
// the unshifted band). FLAG: the mask/tag are the raw X360 class-id magic; the
// controller-class enum that produces them is not yet homed.
static const s32 KI_CONTROLLER_CLASS_MASK         = 0x7F0;
static const s32 KI_HYBRID_EXHAUST_CONTROLLER_TAG = 0x50;

// ---------------------------------------------------------------------------
// GetTypeName  @ 0x82685098
//
//   lis   r11, off_82F2F60C@ha
//   addi  r11, r11, off_82F2F60C@l
//   lwz   r3,  (off_82F2F60C)(r11)   ; r3 = "SingleGinsuEffect"
//   blr
//
// Returns the per-class RTTI type name. The X360 loads a pointer to the static
// string literal at off_82F2F60C.
// ---------------------------------------------------------------------------
const char* SingleGinsuEffect::GetTypeName() const
{
    return "SingleGinsuEffect";
}

// ---------------------------------------------------------------------------
// GetController  @ 0x826850A8
//
//   cmpwi r4, 0
//   ... r3 = (r4 == 0) ? 6 : -1
//   blr
//
// Maps a controller-slot index to the controller id this effect exposes. Slot 0
// is the single (Ginsu) controller -> id 6; any other slot is unsupported -> -1.
// ---------------------------------------------------------------------------
s32 SingleGinsuEffect::GetController(s32 aiSlot)
{
    return aiSlot == 0 ? 6 : -1;
}

// ---------------------------------------------------------------------------
// AttachController  @ 0x826850C0
//
//   lwz   r11, 0x14(r4)            ; r11 = controller->miObjectId
//   andi. r11, r11, 0x7F0
//   cmpwi r11, 0x50                ; is it a HybridExhaustControl?
//   bne   skip
//   subi  r11, r4, 4               ; recover the controller's primary object
//   stw   r11, 0x34(r3)            ; this->mpHybridControl = (HybridExhaustControl*)
//  skip:
//   blr
//
// Latch the supplied controller as this effect's hybrid-exhaust controller IFF its
// object-id identifies it as a HybridExhaustControl. The X360 is handed the
// controller via its EffectBase sub-object pointer and adjusts by -4 to recover
// the primary object; modelled here as a by-name downcast (HybridExhaustControl
// derives from CgsSound::Logic::EffectBase, see header), which performs the
// equivalent sub-object adjustment without raw pointer arithmetic. Non-matching
// controllers leave mpHybridControl unchanged.
// ---------------------------------------------------------------------------
void SingleGinsuEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    if ((apController->GetObjectId() & KI_CONTROLLER_CLASS_MASK) == KI_HYBRID_EXHAUST_CONTROLLER_TAG)
    {
        mpHybridControl = static_cast<HybridExhaustControl*>(apController);
    }
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
