#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsSound::Logic::StateManager -- owning keystone base bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This TU homes:
//   StateManager::StateManager()             ctor                 @ 0x826FAA18  (real)
//   StateManager::~StateManager()            (vector deleting dtor) @ 0x826FAAB8 (structural)
//   StateManager::IsStateAlias(s32)          accessor             @ 0x82680D48  (inline in .h)
//   StateManager::AddToClassTypeInfoArray()  RTTI register        @ 0x8268DFE8  (real)
//   StateManager::GetStaticTypeInfo()        RTTI descriptor      @ 0x8268D798  (real)
//   StateManager::Prepare()                  vtable +0x0C         (stub -- see FLAG)
//   StateManager::GetChildStateManager(s32)  vtable +0x14         (stub -- see FLAG)
//   StateManager::PrepareStates(...)         shared helper        @ 0x826EAD30  (stub -- see FLAG)
//   StateManager::CreateStateMan(u32, void*) factory              @ 0x826A5B60  (stub -- see FLAG)
//
// The minimal slice this file used to be (just IsStateAlias against an opaque
// mPad[20]) is superseded by the DWARF-named owning layout in the header; the
// IsStateAlias accessor is now inline in CgsStateManager.h (member access by name --
// meMapState).

namespace CgsSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// StateManager::StateManager()  @ 0x826FAA18
//
// X360 store-for-store (this == r3):
//   *this        = off_820B66E4           ; vtable @ +0          (implicit here)
//   four 16-byte RegisteredContent sub-objects at this+0x30..+0x6F:
//     elem+4 = off_820B3250 ; elem+8 = 0   (vtable + null content, per element)
//   *(this+0x18)=0 ; *(this+0x1C)=0
//   *(this+0x24)=0 ; *(this+0x28)=0
//   *(this+0x04..0x10) = 0.0f (four floats)
//   *(this+0x14) = -1
//   *(this+0x88) = 0 (8 bytes)            ; pool BitArray<4> region
//   *(this+0x70..0x7C) = 3,2,1,0          ; pool free-index queue (descending)
//   *(this+0x80) = 4                      ; pool free count
//
// Reproduced BY NAME: the four floats + meMapState + the four zeroed scalar pairs are
// member-initialised; the four RegisteredContent sub-objects, their vtables, and their
// nulled content pointers are produced by the default construction of the
// mContentPool member (each RegisteredContent's default ctor nulls mpContent and the
// compiler installs its vtable).
//
// FLAG (free-queue seed gap): the X360 ctor ALSO leaves the pool FULL (free queue
// 3,2,1,0 / count 4 / BitArray 0). The shared generic ObjectPool has no constructor
// and its public API cannot externally set the free queue to {3,2,1,0} with count 4,
// so mContentPool is left default-constructed (uninitialised free-queue/count/bitarray)
// -- identical to how the sibling views (CgsStateManagerDtor.cpp,
// BrnEmitterStateManager.cpp) construct this same member. The correct fix is an
// ObjectPool constructor in the containers group (see header FLAG); NOT done here to
// avoid touching shared infra. Until then, a freshly-constructed StateManager's
// content pool must be treated as "not yet seeded" rather than "full".
// ---------------------------------------------------------------------------
StateManager::StateManager()
    : CgsSound::MemBase()
    , mfCurrentTime(0.0f)        // +0x04
    , mfDeltaTime(0.0f)          // +0x08
    , mfTimeStepGame(0.0f)       // +0x0C
    , mfTimeStepSimulation(0.0f) // +0x10
    , meMapState(-1)             // +0x14  (the X360 li r10,-1 ; stw r10,0x14(r3))
    , mu24(0)                    // +0x18
    , mu28(0)                    // +0x1C
    , mu36(0)                    // +0x24
    , mu40(0)                    // +0x28
    , mpLogicModule(0)           // +0x2C  (stamped later by CreateStateMan)
    // mContentPool: default-constructed (the four RegisteredContent vtable+null
    // sub-objects). Free-queue/count/bitarray seed gap -- see FLAG above.
{
}

// ---------------------------------------------------------------------------
// StateManager::~StateManager()  @ 0x826FAAB8 (the X360 vector deleting destructor)
//
//   stw  off_820B66E4, 0(this)        ; (transient) StateManager vtable
//   addi r3, this, 0x30               ; &mContentPool
//   bl   ObjectPool<RegisteredContent,4,int>::~ObjectPool  ; destruct the pool
//   stw  off_820AA820, 0(this)        ; re-install MemBase base vtable
//   if (flags & 1) <sound allocator>.Free(this)            ; deleting flavour
//
// Destructing mContentPool runs its ~ObjectPool (and therefore each RegisteredContent
// reference drop) -- exactly the X360 sub-object destructor call. The vtable re-installs
// and the conditional allocator-routed free are the compiler's deleting-destructor
// thunk (MSVC re-synthesises them from this virtual destructor + the class's operator
// delete). The sound allocator (off_82FFB954) is not homed in this group, so the host
// toolchain's `delete` stands in for the custom-allocator dispatch.
// ---------------------------------------------------------------------------
StateManager::~StateManager()
{
}

// ---------------------------------------------------------------------------
// StateManager::Prepare()  -- vtable slot +0x0C
//
// PrepareStateManagersOnBoot (0x826837F8) calls this per manager as
// `(*(**mgr + 12))(mgr)`; a false return aborts boot. The BASE body is not the
// load-bearing one -- every concrete manager overrides Prepare() with its own
// bring-up state machine (GlobalStateManager::Prepare @ 0x826F5BF8,
// EmitterStateManager::Prepare @ 0x826F5AC0, ... -- see PART B), each pulling its
// domain (Voice/Content/World/Traffic/Vehicle audio).
//
// FLAG (stub): the base StateManager has no recovered standalone Prepare body in this
// group (the real per-manager logic lives in the overrides). Returning true here is a
// safe non-cascading default so the keystone is instantiable and overridable; it does
// NOT reproduce any X360 base behaviour (there may be none -- the slot may be pure in
// the source). Leaves MUST override.
// ---------------------------------------------------------------------------
bool StateManager::Prepare()
{
    return true;
}

// ---------------------------------------------------------------------------
// StateManager::GetChildStateManager(s32)  -- vtable slot +0x14
//
// PrepareStateManagersOnBoot calls this ONLY on mapStateManagers[0] as
// `child = (*(**mgr + 20))(mgr, 0); if (child) child->Prepare(0);`. It returns a
// (possibly null) child StateManager that is itself Prepare()'d on boot.
//
// FLAG (stub): the child registry this indexes is not modelled in this view, and the
// argument's exact role (the X360 passes a literal 0) is unverified. Returning null is
// a safe non-cascading default (the boot caller guards `if (child)`), so no child is
// dispatched from the base. The owning manager that actually has children overrides
// this. NOT an X360-faithful body -- shape only.
// ---------------------------------------------------------------------------
StateManager* StateManager::GetChildStateManager(s32 /*liIndex*/)
{
    return 0;
}

// ---------------------------------------------------------------------------
// StateManager::PrepareStates(s32, s32, s32)  @ 0x826EAD30
//
// The shared helper every leaf Prepare() calls to advance its owned States (the X360
// signature is PrepareStates(mask, instancesPerState, startState) -> bool). Its body
// walks the State machine (creating/preparing State instances), which cascades into
// CgsSound::Logic::State and the per-state effect graph -- none of which is in this
// view's reconstructed surface.
//
// FLAG (stub): declared so leaf Prepare() overrides (in their own TUs) can name it;
// the real body belongs to the State-machine group (CgsState.*). Returns true as a
// non-cascading placeholder. NOT reconstructed here.
// ---------------------------------------------------------------------------
bool StateManager::PrepareStates(s32 /*liStateMask*/,
                                 s32 /*liInstancesPerState*/,
                                 s32 /*liStartState*/)
{
    return true;
}

// ---------------------------------------------------------------------------
// StateManager::CreateStateMan(u32 liStateManId, void* apModule)  @ 0x826A5B60
//
// The factory CreateStateManagers (0x826AFEF8) calls per index i=0..8. The X360 body:
//   walk the registered ClassTypeInfo array dword_82FFBC58 (up to dword_82FFBC98);
//   for each non-null descriptor whose descriptor->ObjectID == liStateManId, remember
//   it (asserting "There are two StateManagers registered to the same ID."
//   CgsStateManager.cpp:436 if a second match is found);
//   if a descriptor matched:
//       result = descriptor->createObject(0);   // new <LeafManagerType>
//       result->mpLogicModule (+0x2C) = apModule;
//       result->meMapState    (+0x14) = descriptor->ObjectID;
//   return result;   // null when no descriptor matched
//
// FLAG (stub): the registered-descriptor array (dword_82FFBC58) is the static RTTI
// registry that each leaf manager's class-registration populates (via
// AddToClassTypeInfoArray with an explicit per-manager ObjectID). That registry +
// the per-leaf createObject factories are NOT modelled in this view (they live with
// the leaf managers and their RegisterClass sites), so this factory cannot be bodied
// faithfully here without pulling all nine leaves + their registration in. Returning
// null is a safe placeholder (the caller guards `if (result)`); it does NOT create
// any manager. CONDUCTOR: body this once the leaf registration + factory set is
// reconstructed (see PART B for the per-leaf createObject addresses). The index->type
// mapping is driven by the per-leaf ObjectID values, NOT a switch.
// ---------------------------------------------------------------------------
StateManager* StateManager::CreateStateMan(u32 /*liStateManId*/, void* /*apModule*/)
{
    return 0;
}

// ---------------------------------------------------------------------------
// StateManager::AddToClassTypeInfoArray  @ 0x8268DFE8  (static array dword_82FFBC58)
//
// Identical RTTI-registration routine to State::AddToClassTypeInfoArray
// (@ 0x8268DF08), here templated on CgsSound::Logic::StateManager and over a
// separate per-class static array. Scans the array for the first NULL slot, capped
// at the class-array size (loop bound 0x10 == KU_SIZEOF_CLASS_ARRAY); stores the
// descriptor on finding a free slot, otherwise falls through without storing. The
// "Too Many Class registations" assert (CgsStateManager.h:363) only fires once the
// (16-bit) slot counter reaches 4*KU (0x40), which the bounded loop never reaches.
// Reproduced generically by array NAME; no raw-offset cast.
// ---------------------------------------------------------------------------
ClassTypeInfo<StateManager>* StateManager::AddToClassTypeInfoArray(ClassTypeInfo<StateManager>* apTypeInfo)
{
    static ClassTypeInfo<StateManager>* saClassTypeInfoArray[KU_SIZEOF_CLASS_ARRAY] = { 0 };

    // Scan for the first empty slot, capped at the class-array size (0x10).
    u32 lu32Index = 0;
    for (lu32Index = 0; lu32Index < KU_SIZEOF_CLASS_ARRAY; ++lu32Index)
    {
        if (saClassTypeInfoArray[lu32Index] == 0)
        {
            saClassTypeInfoArray[lu32Index] = apTypeInfo;
            return apTypeInfo;
        }
    }

    // No empty slot within the cap; the X360 only asserts once the (16-bit) counter
    // reaches 4*KU (0x40). The bounded loop stops at KU, so the predicate holds and
    // no assert fires.
    CGS_ASSERT(lu32Index < (4u * KU_SIZEOF_CLASS_ARRAY),
               "Too Many Class registations. Increase KU_SIZEOF_CLASS_ARRAY");
    return apTypeInfo;
}

// ---------------------------------------------------------------------------
// StateManager::GetStaticTypeInfo()  @ 0x8268D798  -> &unk_82F2FAA0 ("StateManager")
//   The X360 body returns the address of StateManager's static ClassTypeInfo
//   descriptor (lis/addi unk_82F2FAA0; blr). The sibling StateManager::GetTypeName
//   @0x8268D7A8 reads the descriptor's typeName ("StateManager") from +4. Mirrors the
//   EffectControl/EffectObject GetStaticTypeInfo accessors: a function-local static
//   descriptor seeded with the recovered type name, so the observable return (the
//   descriptor carrying typeName "StateManager") matches byte-for-byte.
// ---------------------------------------------------------------------------
ClassTypeInfo<StateManager>* StateManager::GetStaticTypeInfo()
{
    static ClassTypeInfo<StateManager> sTypeInfo(0, "StateManager", nullptr, nullptr);
    return &sTypeInfo;
}

} // namespace Logic
} // namespace CgsSound
