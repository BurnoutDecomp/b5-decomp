#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"

// =============================================================================
// BrnSound::Logic::BrnEffectControl — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnEffectControl.h for the
// dual-base layout rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly two entries:
//   ~BrnEffectControl  (the X360 `vector deleting destructor`)  @ 0x826AEF68
//   `vector deleting destructor' adjustor{4}                    @ 0x82696868
// Both are reproduced below (the second is a compiler-generated thunk; see note).
//
// dep_flags: none un-homed for THIS TU. The destructor teardown touches only
// meAttachState / meDetachState / mbResourcesReady (all modelled BY NAME). The
// (a2 & 1) deallocation tail dispatches the global sound allocator (off_82FFB954);
// that allocator vtable is not homed here, so the `delete` half of the X360 vector
// deleting destructor is left to the host toolchain rather than reproducing the raw
// allocator vtable call (same treatment as the BrnEffectObject sibling home).
// =============================================================================

namespace BrnSound
{
namespace Logic
{

BrnEffectControl::BrnEffectControl()
    : CgsSound::Logic::EffectControl()
    , IResourceRequester()
    , mbResourceRequestActive(false)
    , mbResourcesReady(false)
{
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
BrnEffectControl::GetTypeInfo() const
{
    return CgsSound::Logic::EffectControl::GetStaticTypeInfo();
}

const char* BrnEffectControl::GetTypeName() const
{
    return "BrnEffectControl";
}

void BrnEffectControl::ResourcesAreReady()
{
    mbResourcesReady = true;
    CGS_ASSERT(GetAttachState() == CgsSound::Logic::EffectBase::E_ATTACH_STATE_WAITING_FOR_DATA,
               "GetAttachState() == E_ATTACH_STATE_WAITING_FOR_DATA");
    meAttachState = CgsSound::Logic::EffectBase::E_ATTACH_STATE_PREPARING;
}

ResourceRegistrar& BrnEffectControl::GetResourceRegistrar()
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    return lpModule->GetResourceRegistrar();
}

// ---------------------------------------------------------------------------
// BrnEffectControl::Detach  @ 0x826EB8B0
//
//   if ( *(a1 + 45) )                       ; the outstanding-request byte
//   {
//     v2 = (*(*(a1 - 4) + 4))(a1 - 4);      ; IResourceRequester::GetResourceRegistrar
//     ResourceRegistrar::RemoveRequests(v2, a1 - 4);
//   }
//   *(a1 + 45) = 0;
//   *(a1 + 36) = 3;                         ; this+0x28  meDetachState = E_DETACH_STATE_FINISHED
//   *(a1 + 32) = 0;                         ; this+0x24  meAttachState = E_ATTACH_STATE_NONE
//   return 1;
//
// (a1 is the IResourceRequester sub-object -- note the `a1 - 4` -- so +36/+32 are this+0x28 /
// this+0x24: the same two members the vector deleting destructor @0x826AEF68 stores 3 and 0
// into. The body is byte-for-byte the sibling BrnEffectObject::Detach @0x826EBF88, which this
// tree already writes as "clear the request byte, then EffectBase::Detach()".)
//
// ⭐⭐⭐ THE ENGINE SOUND OF EVERY CAR BUT THE FIRST ONE (owner, 2026-09-15: "the Revenge Racer
// stays at idle"; "the Cavalry is the ONLY car that has working engine sounds"). This body used
// to end with
//     meAttachState = E_ATTACH_STATE_FINISHED;   // and no meDetachState store
// -- the console's FINISHED written into the ATTACH member instead of the DETACH one. It is a
// DEAD state for re-attachment: State::AttachEffect @CgsState.cpp answers
// E_ATTACH_STATE_FINISHED with a bare `break`, so it never calls Attach() again. Every sound
// EffectControl therefore attached EXACTLY ONCE per session, while the EffectObjects around it
// (whose Detach resets the pair correctly) re-attached on every car change.
// HybridExhaustControl::Attach() is the only thing that re-resolves the player car's
// `vehicleengine` collection -- FindCollectionWithDefault(0x7F161D94482CB3BF,
// PhysicsControl::GetEngineComponentKey(component)) -- so from the second car of a session
// onwards the whole engine graph kept the FIRST car's attributes: its GinsuFileAccel /
// GinsuFileDecel / LoopModel recordings, its IdleRpm / MaxRpm mapping, its EQ and its decel
// crossfade points. Measured with BRN_GINSU_DIAG (scratch/eng_audio/run7_BrnGame.log): after
// swapping to the Revenge Racer, `[engine-attach] car=Revenge Racer engineName=CARRGT_EN` is
// followed by `[ginsu-prep] CREATE_VOICES ... DragMustang2_en_acl.gin` -- the Hunter Cavalry's
// engine, on the Revenge Racer.
// ---------------------------------------------------------------------------
bool BrnEffectControl::Detach()
{
    if (mbResourceRequestActive)
        GetResourceRegistrar().RemoveRequests(static_cast<IResourceRequester*>(this));
    mbResourceRequestActive = false;
    // meDetachState = E_DETACH_STATE_FINISHED; meAttachState = E_ATTACH_STATE_NONE --
    // the console's two stores, which is exactly what the shared base does. Same expression
    // as the sibling BrnEffectObject::Detach, whose X360 body is identical.
    return CgsSound::Logic::EffectBase::Detach();
}

// ---------------------------------------------------------------------------
// ~BrnEffectControl  @ 0x826AEF68  (the X360 `vector deleting destructor`)
//
//   stw  off_820AEA6C, 0(r31)      ; primary vptr (EffectControl path)
//   stw  off_820AEA38, 4(r31)      ; (transient) base-class IResourceRequester vptr
//   li   r7, 3 ; stw r7, 0x28(r31)  ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  off_820AA820, 4(r31)      ; final IResourceRequester sub-object vptr
//   stb  0, 0x31(r31)              ; mbResourcesReady = false
//   stw  0, 0x24(r31)             ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { ... deallocate via off_82FFB954 (the MemBase allocator) }
//   return this
//
// The leading vptr stores are the compiler-emitted devirtualization of the
// destructor base sub-objects; in reconstructed C++ they are produced implicitly
// by the destructor chain, so the BODY here is the observable member teardown.
// The store ORDER below mirrors the asm: meDetachState (+0x28), then mbResourcesReady
// (+0x31), then meAttachState (+0x24).
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 vector deleting destructor).
// ---------------------------------------------------------------------------
BrnEffectControl::~BrnEffectControl()
{
    meDetachState    = CgsSound::Logic::EffectBase::E_DETACH_STATE_FINISHED; // stw 3, 0x28
    mbResourcesReady = false;                                               // stb 0, 0x31
    meAttachState    = CgsSound::Logic::EffectBase::E_ATTACH_STATE_NONE;     // stw 0, 0x24
}

// ---------------------------------------------------------------------------
// `vector deleting destructor' adjustor{4}  @ 0x82696868
//
//   addi r3, r3, -4
//   b    ~BrnEffectControl (vector deleting destructor @ 0x826AEF68)
//
// The IResourceRequester sub-object lives at this+4; a delete through an
// IResourceRequester* enters here, recovers the primary BrnEffectControl `this`
// (this - 4), and forwards to the real destructor. In reconstructed C++ this thunk
// is generated by the compiler from the multiple-inheritance vtable layout (the
// destructor declared in the header is virtual and reached through the
// IResourceRequester base), so no hand-written body is emitted.
// FLAG: thunk reproduced structurally by the dual-base declaration in the header;
// not a hand-bodied function.
// ---------------------------------------------------------------------------

} // namespace Logic
} // namespace BrnSound
