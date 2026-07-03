#ifndef CGS_SOUND_LOGIC_CGSSTATE_H
#define CGS_SOUND_LOGIC_CGSSTATE_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/CgsMemBase.h" // CgsSound::MemBase (the base)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // BrnPhysics::Vehicle::RaceCarState (committed)

// =============================================================================
// GameShared/GameClasses/Sound/Logic/CgsState.h  (DWARF home)
//
// CgsSound::Logic::State: the base of the sound-logic state hierarchy
// (State : CgsSound::MemBase). Reconstructed from BURNOUT_X360_ARTIST.XEX with the
// member layout from the CgsState.h DWARF hints. This home also models the three
// concrete state classes whose constructors this TU bodies:
//   BrnSound::Logic::Passby::PassbyState::PassbyState       @ 0x826BF5E0
//   BrnSound::Logic::Streaming::StreamingState::StreamingState @ 0x826B0CB0
//   BrnSound::Vehicles::VehicleState::VehicleState          @ 0x826C9E70
// plus the base accessor
//   CgsSound::Logic::State::IsAttachedToThis                @ 0x826916D8
// (all defined in the sibling CgsState.cpp).
//
// FLAG (derived members are partly opaque): the three derived classes append
// engine state whose field meanings are not recoverable from the constructors
// alone (the ctors only zero/seed them). Those regions are modelled as named byte
// spans / typed-by-name fields at the observed X360 offsets; the few seeded fields
// (floats, flags, the embedded RaceCarState) are named. Absolute X360 offsets are
// documented in comments; because the engine bases (and the 32/64 pointer-width
// boundary) are not fully modelled, the absolute offsets are NOT static_asserted --
// only the member names + the ctor's observable zero/seed sequence are load-bearing.
//
// VehicleState embeds the committed BrnPhysics::Vehicle::RaceCarState (sizeof 1120,
// BrnVehicleEvents.h) at +96 and clears it via RaceCarState::Clear @ 0x8229FFC8.
// The committed type is reused BY NAME and cleared via mRaceCarState.Clear().
// =============================================================================

namespace CgsSound
{
namespace Logic
{

// Forward-declared collaborators (full homes elsewhere; the State ctor/accessor
// touch only pointers to them, so incomplete types suffice).
struct EffectBase;
struct StateManager;
struct Module;

// CgsState.h:57 (DWARF).
const s32 MAX_NUM_SFXOBJS_PER_STATE = 32;

// CgsState.h:313 (DWARF). Per-class RTTI descriptor, templated on the leaf class;
// holds the object id, type name, base descriptor and a factory hook. Same shape as
// the EffectBase-family descriptor (CgsEffectBase.h:313); declared here too because
// CgsState.h is never co-included with CgsEffectBase.h in the same TU (each home is
// included only by its own .cpp), so there is no ODR clash. State's RTTI-registration
// (AddToClassTypeInfoArray @ 0x8268DF08) registers ClassTypeInfo<State> descriptors.
template <typename T>
struct ClassTypeInfo
{
    ClassTypeInfo(s32 aiObjectID,
                  const char* apcTypeName,
                  ClassTypeInfo<T>* apBaseTypeInfo,
                  T* (*apfnCreateObject)(u32))
        : ObjectID(aiObjectID)
        , typeName(apcTypeName)
        , baseTypeInfo(apBaseTypeInfo)
        , createObject(apfnCreateObject)
    {
    }

    s32               ObjectID;     // CgsState.h:316
    const char*       typeName;     // CgsState.h:317
    ClassTypeInfo<T>* baseTypeInfo; // CgsState.h:318
    T* (*createObject)(u32);        // CgsState.h:319
};

// CgsState.h:132 (DWARF). The sound-logic state base.
struct State : public CgsSound::MemBase
{
    // CgsState.h:135 (DWARF).
    static const u16 KU_SIZEOF_CLASS_ARRAY = 16;

    // CgsState.h:137 (DWARF).
    enum EUpdateState
    {
        E_UPDATE_UNATTACHED          = 0,
        E_INITIALIZE_CONTROLS        = 1,
        E_INITIALIZE_CONTROLS_UPDATE = 2,
        E_INITIALIZE_EFFECTS         = 3,
        E_INITIALIZE_EFFECTS_UPDATE  = 4,
        E_UPDATE_ATTACHED            = 5,
        E_UPDATE_DETATCHING          = 6,
    };

    // CgsState.h:149 (DWARF).
    enum EPrepareState
    {
        E_PREPARE_STATE_CREATE_OBJECTS = 0,
        E_PREPARE_STATE_OBJECTS        = 1,
        E_PREPARE_STATE_CONTROLS       = 2,
        E_PREPARE_STATE_DONE           = 3,
    };

    // CgsState.h:370. Default ctor: zero/seed the base members (matches the inlined
    // base-init the derived ctors emit @ +4..+80). Defined in CgsState.cpp.
    State();

    // @ 0x826ABCD8 (scalar deleting destructor). Declaration-only so CgsState.cpp
    // emits the out-of-line State::~State symbol the deleting-destructor thunk is
    // synthesised from (body = DestroyEffects()). Was an inline no-op.
    virtual ~State();

    // CgsState.h:245 @ 0x826916D8. True when apv is this state's attachment.
    virtual bool IsAttachedToThis(void* apvAttachment);

    // CgsSound::Logic::State::DestroyEffects -- tears down the state's attached
    // effect objects. Called by the State-derived scalar deleting destructors
    // (X360 `bl ...DestroyEffects`, e.g. StreamingState @ 0x826C9B50,
    // GlobalState @ 0x826D2278). Body is un-homed (a separate sound-logic recon
    // slice); declaration-only here so the destructors can call it BY NAME. Do NOT
    // body here.
    void DestroyEffects();

    // @ 0x8268D410. Returns the class rodata sentinel unk_82F2FA90 (reconstructed as
    // the empty string literal per the &unk_XXXX convention). Semantics (likely a
    // type-name/empty-name accessor) not recoverable from the one-instruction body --
    // confidence low.
    void* G();

    // CgsState.h:363 (DWARF) @ 0x8268DF08. Register a per-class RTTI descriptor into
    // State's static ClassTypeInfo<State> array. Scans for the first empty slot
    // (cap KU_SIZEOF_CLASS_ARRAY == 16); only asserts "Too Many Class registations"
    // once the 16-bit slot counter reaches 4*KU. Returns the registered descriptor.
    static ClassTypeInfo<State>* AddToClassTypeInfoArray(ClassTypeInfo<State>* apTypeInfo);

    // --- members (DWARF order; X360 offsets in comments) ---
    s32           miInstNum;                // +4   CgsState.h:284
    s32           meMapState;               // +8   CgsState.h:285
    s32           miStateInstType;          // +12  CgsState.h:286
    void*         mpvAttachment;            // +16  CgsState.h:287 (IsAttachedToThis target)
    State*        mpPrevState;              // +20  CgsState.h:328
    State*        mpNextState;              // +24  CgsState.h:329
    EffectBase*   mpHeadEffectControl;      // +28  CgsState.h:330
    EffectBase*   mpHeadEffectObject;       // +32  CgsState.h:331
    StateManager* mpStateManager;           // +36  CgsState.h:332
    Module*       mpLogicModule;            // +40  CgsState.h:333
    s32           miSFXFlags;               // +44  CgsState.h:335
    s32           miNumLoadedEffectObjects; // +48  CgsState.h:336
    s32           miNumLoadedEffectControls;// +52  CgsState.h:337
    // CgsState.h:344 DataPoint<EUpdateState> meUpdateState -- 8 bytes (value + dirty
    // bookkeeping); modelled by its two zeroed words to keep the layout/offsets.
    u32           mauUpdateState[2];        // +56  CgsState.h:344
    EPrepareState mePrepareState;           // +64  CgsState.h:345
    EffectBase*   mpCurrentEffect;          // +68  CgsState.h:346
    bool          mbIsAttached;             // +72  CgsState.h:347
    f32           mfCurTime;                // +76  CgsState.h:348
    f32           mfDeltaTime;              // +80  CgsState.h:349
};

} // namespace Logic
} // namespace CgsSound

// --- derived state classes (each its own namespace) -------------------------

namespace BrnSound
{
namespace Logic
{
namespace Passby
{
    // BrnPassbyState. State subclass for drive-by sound logic. The ctor zeros a
    // 16-byte region at +96, seeds three scalar fields and two floats.
    struct PassbyState : public CgsSound::Logic::State
    {
        PassbyState();
        virtual ~PassbyState() {}

        // Derived members at the observed X360 offsets (+96..). Meanings beyond the
        // seeded values are not recoverable from the ctor; modelled by name.
        u8  mau8Region[16]; // +96   zeroed as a unit (X360 stvx128)
        s32 mi112;          // +112  seeded 0
        f32 mf116;          // +116  seeded 0.0
        s32 mi120;          // +120  seeded 3
        f32 mf124;          // +124  seeded 1.0
        u8  mu128;          // +128  seeded 0 (byte)
    };
} // namespace Passby

namespace Streaming
{
    // BrnStreamingState. State subclass for streamed sound logic. The ctor seeds a
    // small block of scalars/floats past the State base.
    struct StreamingState : public CgsSound::Logic::State
    {
        StreamingState();

        // @ 0x826C9B28 -- scalar deleting destructor. Installs StreamingState's own
        // vtable (off_820AE1F4), calls State::DestroyEffects() to tear down attached
        // effects, re-installs the MemBase base vtable (off_820AA820), and (deleting
        // flavour) routes the storage back through the sound allocator. Observable
        // body = the DestroyEffects() call; the vtable installs + conditional
        // allocator free are the compiler-synthesised deleting-destructor parts.
        // Bodied out-of-line in CgsState.cpp (was an inline no-op).
        virtual ~StreamingState();

        // Derived members at the observed X360 offsets (+84..). Modelled by name.
        s32 mi84;   // +84   seeded 0
        s32 mi88;   // +88   seeded 0
        f32 mf92;   // +92   seeded 0.0
        f32 mf96;   // +96   seeded 0.0
        s32 mi100;  // +100  seeded 0
        u8  mu104;  // +104  seeded 0 (byte)
    };
} // namespace Streaming
} // namespace Logic

namespace Vehicles
{
    // BrnVehicleState. State subclass for per-vehicle engine-sound logic. Embeds a
    // RaceCarState at +96 (cleared via RaceCarState::Clear) and seeds a block of
    // tail fields. Its full member meanings are not recoverable from the ctor.
    struct VehicleState : public CgsSound::Logic::State
    {
        VehicleState();
        virtual ~VehicleState() {}

        // The committed RaceCarState (sizeof 1120) embedded at +96, cleared in the
        // ctor via mRaceCarState.Clear() (X360 RaceCarState::Clear @ 0x8229FFC8).
        BrnPhysics::Vehicle::RaceCarState mRaceCarState; // +96
        s32 mi1216;                 // +1216  seeded 0
        s32 mi1220;                 // +1220  seeded 0
        u64 mu1224;                 // +1224  seeded 0 (8 bytes)
        u8  mu1268;                 // +1268  seeded 0 (byte)
        u8  mu1281;                 // +1281  seeded 0 (byte)
        u64 mu1296;                 // +1296  seeded 0 (8 bytes)
        u64 mu1304;                 // +1304  seeded 0 (8 bytes)
        f32 mf1312;                 // +1312  seeded 0.0
        u8  mu1317;                 // +1317  seeded 0 (byte)
        u8  mu1318;                 // +1318  seeded 0 (byte)
    };
} // namespace Vehicles
} // namespace BrnSound

#endif // CGS_SOUND_LOGIC_CGSSTATE_H
