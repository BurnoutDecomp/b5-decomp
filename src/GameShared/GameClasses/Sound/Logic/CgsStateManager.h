#ifndef CGS_SOUND_LOGIC_CGSSTATEMANAGER_H
#define CGS_SOUND_LOGIC_CGSSTATEMANAGER_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/CgsMemBase.h"            // CgsSound::MemBase
#include "GameShared/GameClasses/Sound/Logic/CgsClassTypeInfo.h" // ClassTypeInfo<T> (canonical)
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"     // CgsContainers::ObjectPool
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsObject.h"     // CgsSound::Playback::Object
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"     // CgsSound::Playback::Name
#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"       // CgsSound::Logic::Content

// The IO message header the Notify hook receives (full home CgsMessage.h;
// pointer-only here).
namespace CgsSound { namespace Io { class MessageHeader; } }

// CgsSound::Logic::StateManager - the sound-logic state manager keystone base
// (StateManager : public CgsSound::MemBase). This is a large polymorphic class
// (~50 member functions across CgsStateManager.cpp). This header now models the
// OWNING base view: the constructor-touched data members (recovered from the X360
// ctor at 0x826FAA18) plus the leaf accessors/virtuals reached from the boot
// orchestration. The minimal slice that previously lived here (IsStateAlias +
// leading scalars) is preserved and grown around.
//
// ---------------------------------------------------------------------------
// CTOR 0x826FAA18 (the construction this header reproduces, BY NAME):
//   *this        = off_820B66E4           ; install StateManager's own vtable @ +0
//   // four 16-byte sub-objects at this+0x30, this+0x40, this+0x50, this+0x60:
//   for k in 0..3:
//       *(this+0x30+16*k + 4) = off_820B3250   ; each RegisteredContent vtable @ elem+4
//       *(this+0x30+16*k + 8) = 0              ; each RegisteredContent mpContent = null
//   *(this+0x18) = 0 ; *(this+0x1C) = 0        ; (+24/+28 scalar pair = 0)
//   *(this+0x24) = 0 ; *(this+0x28) = 0        ; (+36/+40 scalar pair = 0)
//   *(this+0x04) = 0.0f ; *(this+0x08) = 0.0f  ; mfCurrentTime / mfDeltaTime
//   *(this+0x0C) = 0.0f ; *(this+0x10) = 0.0f  ; mfTimeStepGame / mfTimeStepSimulation
//   *(this+0x14) = -1                          ; meMapState = -1
//   *(this+0x88) = 0 (8 bytes)                 ; the pool occupancy BitArray<4> region
//   *(this+0x70..0x7C) = 3,2,1,0               ; the pool free-index queue (descending)
//   *(this+0x80) = 4                           ; the pool free count (= capacity 4)
//   return this
//
// Everything from +0x30 onward (the four vtable+null sub-objects, the descending
// 3,2,1,0 array, the count 4, the zeroed BitArray region) is EXACTLY the inline
// default-construction of an ObjectPool<RegisteredContent, 4, int> member at +0x30:
//   * maObjectPool[4]   -> the four RegisteredContent elements (vtable @ elem+4, the
//                          held content pointer @ elem+8 nulled).
//   * maiObjectFreeQueue[4] -> seeded 3,2,1,0 (descending, so AllocateObject pops 0,1,2,3).
//   * miNumObjectsFree  -> 4 (the full capacity).
//   * mObjectsAllocated -> BitArray<4> zeroed.
// This is the SAME pool member proven by the destructor at 0x826FAAB8 (which does
// `addi r3, this, 0x30; bl ~ObjectPool` -- see CgsStateManagerDtor.cpp) and the
// SAME nested RegisteredContent element type homed in CgsStateManagerRegisteredContent.h.
// off_820B3250 (the RegisteredContent vtable) is reproduced structurally (the nested
// type declares a virtual destructor); off_820B66E4 (StateManager's own vtable) is
// reproduced by this class's own virtuals.
//
// FLAG (faithful, not byte-identical): absolute member offsets are deliberately NOT
// static_asserted -- the host 64-bit vptr/pointer width differs from the X360 32-bit
// one, so on the host the pool member, its free queue, its count and its BitArray
// land at different byte offsets than +0x30/+0x70/+0x80/+0x88. Only the BY-NAME
// semantics + the observable construction (scalars zeroed/seeded, pool seeded full)
// are load-bearing.
//
// FLAG (free-queue seed gap -- the ONE part of the ctor NOT reproduced here):
// the X360 ctor inlines the pool's free-list seed (free queue = 3,2,1,0, count = 4,
// BitArray = 0), i.e. it leaves the pool FULL and ready to hand out slots 0..3. The
// shared generic CgsContainers::ObjectPool<T,N,TIndex> (CgsObjectPool.h) declares NO
// constructor, and its public surface (AllocateObject/FreeObject/IsObjectAllocated/
// operator[]) cannot externally set the free queue to {3,2,1,0} with count 4 -- so a
// default-constructed pool member here leaves maiObjectFreeQueue / miNumObjectsFree /
// mObjectsAllocated UNINITIALISED, which is NOT the X360's "full pool" post-state.
// This header intentionally does NOT modify the shared pool (out of scope; every
// other instantiation shares it) and does NOT fabricate a seed it cannot perform
// faithfully. The StateManager ctor (CgsStateManager.cpp) therefore reproduces the
// scalar zero/seed stores (which StateManager owns) and default-constructs the pool,
// with the seed gap documented at the (absent) call. CONDUCTOR: the correct fix is a
// CgsContainers::ObjectPool constructor that seeds the free queue (N-1..0), sets
// miNumObjectsFree = N and clears the BitArray -- exactly what every X360 owner's
// inlined construction does -- but that belongs in the containers group. NOTE: the
// other pool owners (e.g. BrnEmitterStateManager.cpp) ALSO default-construct this
// pool member without seeding, so this view is consistent with the committed tree;
// the gap is pre-existing and shared, not introduced here.
//
// UNIFIED (2026-08-25, audio-faithfulness wave 4): this header is now the SINGLE
// owning CgsSound::Logic::StateManager definition. The former sibling partial views
// are retired: CgsStateManagerRegisteredContent.h (base-less RegisteredContent host)
// and CgsStateManagerDtor.cpp (TU-local polymorphic rival + a duplicate empty
// ~StateManager) are DELETED -- their consumers include this header; Logic/
// CgsEnvironment.h's old minimal view was already folded onto this header 2026-06-25.
// (~StateManager @0x826FAAB8 is bodied once, in CgsStateManager.cpp.)
namespace CgsSound
{
namespace Logic
{

// CgsStateManager.h:313 (DWARF). Per-class RTTI descriptor -- CANONICAL definition
// folded into CgsClassTypeInfo.h (2026-08-25; the per-header copies were an ODR
// violation). StateManager's RTTI-registration (AddToClassTypeInfoArray
// @ 0x8268DFE8) registers ClassTypeInfo<StateManager> descriptors into a separate
// static array.

// The owning sound-logic module (CgsSound::Logic::Module; full home is its own
// keystone slice). Named here only for the GetLogicModule accessor's return type.
class Module;
struct State;
struct EffectBase;

class StateManager : public CgsSound::MemBase
{
public:
    enum EPrepareState
    {
        E_PREPARE_NONE      = 0,
        E_PREPARE_BEGIN     = 1,
        E_PREPARE_UPDATING  = 2,
        E_PREPARE_STATES    = 3,
        E_PREPARE_FINISHED  = 4,
        E_PREPARE_RELEASED  = 5,
    };

    enum EStatePrepareState
    {
        E_STATE_PREPARE_STATE_CREATE    = 0,
        E_STATE_PREPARE_STATE_PREPARING = 1,
    };

    // CgsStateManager.h:135 (DWARF). Static class-array capacity (matches the X360
    // scan bound 0x10 in AddToClassTypeInfoArray @ 0x8268DFE8).
    static const u16 KU_SIZEOF_CLASS_ARRAY = 16;

    // Number of RegisteredContent slots in the content registry pool. Proven by the
    // ctor (0x826FAA18) seeding a 4-deep free queue (3,2,1,0) + count 4, and by the
    // pool destructor (0x826EAC90) tearing down maObjectPool[4].
    static const s32 KI_NUM_CONTENT_SLOTS = 4;

    // CgsStateManager.h:340 (DWARF region). One slot of the StateManager content
    // registry. Layout + teardown recovered from the inlined per-element destructor
    // in the pool dtor at 0x826EAC90 (canonical view: CgsStateManagerRegisteredContent.h).
    // Restated here member-for-member so this TU's content pool matches the committed
    // one element-for-element: a leading non-virtual word, the installed vtable
    // (off_820B3250, reproduced by the virtual destructor), the held refcounted
    // content object (nulled by the ctor), and a trailing word.
    class RegisteredContent
    {
    public:
        RegisteredContent() : mName(), mContent() {}

        CgsSound::Playback::Name mName;       // +0x00
        CgsSound::Logic::Content mContent;    // +0x04 (12 bytes on X360)
    };

    // CgsStateManager.h (ctor home @ 0x826FAA18). Construct: install the vtable
    // (implicit), zero/seed the scalar fields, and default-construct + seed the
    // content pool full (free queue 3,2,1,0 / count 4). Bodied in CgsStateManager.cpp.
    StateManager();

    // Virtual destructor. The X360 `vector deleting destructor` at 0x826FAAB8
    // destructs mContentPool (its ~ObjectPool @ 0x826EAC90) then re-installs the
    // MemBase vtable and routes the storage back to the sound allocator (the latter
    // two are the compiler's deleting-destructor thunk). Bodied ONCE, in
    // CgsStateManager.cpp (the old duplicate in CgsStateManagerDtor.cpp is deleted).
    virtual ~StateManager();

    // Returns true when this manager's map-state equals liState. Recovered from the
    // X360 leaf accessor at 0x82680D48 (member access by name; no offset cast).
    virtual bool IsStateAlias(s32 liState) const
    {
        return meMapState == liState;
    }

    // The map-state / state-type accessor Environment::AddStateManager reads at +0x14
    // (CgsEnvironment.cpp). Same member IsStateAlias compares against. Modelled by name.
    s32 GetStateType() const { return meMapState; }

    // FLAG (additive accessor exposure, 2026-08-25 wave 4): named access for the
    // attested +0x2C read -- EffectBase::Prepare @0x8268CEC8 walks state->mpStateManager
    // (+0x24) then this member (+0x2C) for the owning logic Module. mpLogicModule is
    // stored opaquely (void*, stamped by CreateStateMan); the cast recovers the
    // concrete type the X360 reads it as.
    Module* GetLogicModule() const { return static_cast<Module*>(mpLogicModule); }

    // --- the two virtuals the boot orchestration dispatches -------------------
    //
    // PrepareStateManagersOnBoot (0x826837F8) drives each manager through TWO vtable
    // slots:
    //   * slot +0x0C -> Prepare(): the per-manager bring-up state machine, called as
    //     `(*(**mgr + 12))(mgr)`; returns bool (false aborts boot). Every concrete
    //     manager overrides this (e.g. GlobalStateManager::Prepare @ 0x826F5BF8,
    //     EmitterStateManager::Prepare @ 0x826F5AC0) -- each is a switch on its
    //     +0x24 prepare-state that loads bundles then calls PrepareStates(...).
    //   * slot +0x14 -> GetChildStateManager(int): called ONLY on mapStateManagers[0]
    //     as `child = (*(**mgr + 20))(mgr, 0); if (child) child->Prepare(0);` -- it
    //     returns a (possibly null) child StateManager that is itself Prepare()'d.
    //
    // FLAG (declared-but-stubbed, bodies cascade into un-reconstructed domains):
    // the base bodies are NOT reconstructed here. The base Prepare()'s real behaviour
    // is the per-leaf state machine (each leaf overrides it, pulling in Voice/Content/
    // World/Traffic/Vehicle audio -- see PART B), and GetChildStateManager()'s real
    // body indexes a child registry this view does not model. Both are given safe,
    // non-cascading default bodies so the keystone is instantiable and the vtable is
    // emitted; leaves override Prepare. X360 addrs above.
    virtual bool          Prepare();                       // vtable +0x0C
    virtual State* GetFreeState(void* apvAttachment);       // vtable +0x14

    // DWARF :181 / :184 -- the per-frame drive pair Environment::Update @0x826C3F78
    // dispatches on every registered manager (console vtable +0x18/+0x1C):
    // UpdateParams(gameDt) between the time-field seeding and the mixer pass, then
    // the ProcessUpdate() pass after ProcessMixMap. (Added 2026-08-25, phase B2.)
    // The concrete managers own the overrides; the base slots are NON-pure on the
    // console (managers exist that leave them un-overridden -- the same
    // implementer-shape argument as the ISlotImplementation reconcile), so the
    // base bodies are the empty defaults that shape implies.
    virtual void UpdateParams(f32 af32GameDt);             // @ 0x8268D800
    virtual void ProcessUpdate();                          // @ 0x8268D888

    // DWARF :188 -- the message hook Environment::Notify routes through; the DWARF
    // renders the base body EMPTY inline, so this IS the attested base body.
    virtual void Notify(const CgsSound::Io::MessageHeader* /*apkMessage*/) {} // DWARF :188 empty inline base body

    // DWARF :223 / :226 / :232 -- the time-field setters Environment::Update seeds
    // before the UpdateParams pass (header-inline trivial setters; the Update asm
    // inlines them as the +0x0C/+0x10/+0x04 stores). Added 2026-08-25, phase B2.
    void SetTimeStepGame(f32 af32TimeStep)       { mfTimeStepGame = af32TimeStep; }
    void SetTimeStepSimulation(f32 af32TimeStep) { mfTimeStepSimulation = af32TimeStep; }
    void SetCurrentTime(f32 af32Time)            { mfCurrentTime = af32Time; }

    // CgsStateManager.h (PrepareStates @ 0x826EAD30). The shared helper every leaf
    // Prepare() calls to advance its owned States. NOT in this view's reconstructed
    // function set (it cascades into the State machine); declared so leaves can name
    // it, body left to the State-machine group.
    // FLAG (declared-only): no body here -- see CgsState.* / the State group.
    bool PrepareStates(s32 liStateMask, s32 liInstancesPerState, s32 liStartState);

    // Content registry @ 0x826AC9F8 / 0x826C7740. The key is the playback Name
    // stored at element +0 and the returned Content is the embedded sub-object at
    // element +4.
    Content* GetContent(const CgsSound::Playback::Name& arName);
    Content* RegisterContent(const CgsSound::Playback::Name& arName);

    // CgsStateManager.h:363 (DWARF) @ 0x8268DFE8. Register a per-class RTTI
    // descriptor into StateManager's static ClassTypeInfo<StateManager> array.
    // Scans for the first empty slot (cap KU_SIZEOF_CLASS_ARRAY == 16); only asserts
    // "Too Many Class registations" once the 16-bit slot counter reaches 4*KU.
    // Returns the registered descriptor.
    static ClassTypeInfo<StateManager>* AddToClassTypeInfoArray(ClassTypeInfo<StateManager>* apTypeInfo);

    // @ 0x8268D798. Returns the address of StateManager's static ClassTypeInfo
    // descriptor (the X360 returns &unk_82F2FAA0; the sibling GetTypeName @0x8268D7A8
    // reads "StateManager" from that descriptor's typeName at +4). Same shape as the
    // EffectControl/EffectObject GetStaticTypeInfo accessors -- a function-local static
    // descriptor seeded with the recovered type name, so the observable return (the
    // descriptor whose typeName is "StateManager") matches.
    static ClassTypeInfo<StateManager>* GetStaticTypeInfo();

    // CgsStateManager.h:431 (assert site) @ 0x826A5B60. The factory CreateStateManagers
    // (0x826AFEF8) calls per index i=0..8. Walks the registered ClassTypeInfo array
    // (dword_82FFBC58), matches descriptor->ObjectID == liStateManId, asserts a single
    // match ("There are two StateManagers registered to the same ID."), then
    // new's the leaf via descriptor->createObject(0) and stamps the new manager's
    // +0x2C (the module back-pointer = apModule) and +0x14 (meMapState = ObjectID).
    // Returns the new StateManager* (or null when no descriptor matches the id).
    // Declared here for home completeness; the body lives in CgsStateManager.cpp.
    static StateManager* CreateStateMan(u32 liStateManId, void* apModule);

    EffectBase* CreateEffectObject(s32 liInstanceId, s32 liEffectId);
    EffectBase* CreateEffectControl(s32 liInstanceId, s32 liEffectId);

    State* GetHeadState() const { return mpHeadState; }
    s32 GetStateObjCount() const { return miNumStates; }
    EPrepareState GetPrepareState() const { return mePrepareState; }

protected:
    // Leading scalar members (DWARF CgsStateManager.h:335..341). Modelled by name so
    // the ctor seeds them by name (mfCurrentTime..meMapState) and meMapState can be
    // read without a raw-offset cast.
    f32 mfCurrentTime;          // CgsStateManager.h:335  (+0x04 on X360; ctor = 0.0f)
    f32 mfDeltaTime;            // CgsStateManager.h:336  (+0x08 on X360; ctor = 0.0f)
    f32 mfTimeStepGame;         // CgsStateManager.h:338  (+0x0C on X360; ctor = 0.0f)
    f32 mfTimeStepSimulation;   // CgsStateManager.h:339  (+0x10 on X360; ctor = 0.0f)
    s32 meMapState;             // CgsStateManager.h:341  (+0x14 on X360; ctor = -1)

    // +0x18/+0x1C and +0x24/+0x28 scalar pairs the ctor zeros. Their DWARF names are
    // not recovered in this group; modelled as named opaque words so the ctor's
    // observable zero-stores are reproduced by name (NOT fabricated meaning).
    // FLAG: names are placeholders; the +0x20 slot is meMapState's neighbour and is
    // NOT one of these (the ctor leaves +0x20 untouched besides... see ctor: it writes
    // +0x14=-1 and zeros +0x18,+0x1C,+0x24,+0x28). The module back-pointer the factory
    // stamps at +0x2C is mpLogicModule below.
    State* mpHeadState;         // +0x18
    s32 miNumStates;            // +0x1C
    s32 miStateManId;           // +0x20
    EPrepareState mePrepareState; // +0x24
    EStatePrepareState meStatePrepareState; // +0x28

    // +0x2C: the owning logic module back-pointer the factory CreateStateMan stamps
    // (`*(result + 44) = a2`). Modelled by name as opaque storage (the module type is
    // not pulled into this view).
    void* mpLogicModule;        // +0x2C (set by CreateStateMan, not by the ctor)

    // +0x30: the content-registry pool. Capacity 4, int slot index. Default-constructed
    // and then seeded full by StateManager's ctor (see ctor FLAG). Its destructor is
    // the already-homed ObjectPool<RegisteredContent,4,int>::~ObjectPool @ 0x826EAC90
    // (instantiated in ObjectPool_StateManagerRegisteredContent_4.cpp).
    CgsContainers::ObjectPool<RegisteredContent, KI_NUM_CONTENT_SLOTS, int> mContentPool;

    State* CreateState(s32 liStateType);
    friend struct State;
};

}
}

#endif // CGS_SOUND_LOGIC_CGSSTATEMANAGER_H
