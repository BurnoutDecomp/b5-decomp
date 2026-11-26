#ifndef CGS_SOUND_LOGIC_CGSSTATE_H
#define CGS_SOUND_LOGIC_CGSSTATE_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsClassTypeInfo.h"  // ClassTypeInfo<T> (canonical)

#include "GameShared/GameClasses/Sound/CgsMemBase.h" // CgsSound::MemBase (the base)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

namespace CgsSound { namespace Io { class MessageHeader; } }

// =============================================================================
// GameShared/GameClasses/Sound/Logic/CgsState.h  (DWARF home)
//
// CgsSound::Logic::State: the base of the sound-logic state hierarchy
// (State : CgsSound::MemBase). Reconstructed from BURNOUT_X360_ARTIST.XEX with the
// member layout from the CgsState.h DWARF hints. Bodies in the sibling
// CgsState.cpp:
//   CgsSound::Logic::State::IsAttachedToThis                @ 0x826916D8
//   State::State / ~State (@ 0x826ABCD8) / G / AddToClassTypeInfoArray
//
// (2026-08-25, audio-faithfulness wave 5 RECONCILIATION): this header used to ALSO
// embed the three concrete Brn state leaves (Passby::PassbyState,
// Streaming::StreamingState, Vehicles::VehicleState) as ctor-derived numeric-name
// models -- rival definitions of classes whose DWARF homes are
// GameSource/Sound/{Passby/BrnPassbyState.h, Streaming/BrnStreamingState.h,
// Vehicles/BrnVehicleState.h} (each `: public BrnSound::Logic::BrnState`, DWARF-
// proven). The leaves now live ONLY in those homes (ctors moved with them), and
// BrnState.h derives its BrnState from THIS canonical State -- the ODR knot the
// "RE-HOME ATTEMPTED AND REVERTED" note there recorded is retired.
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

// CgsState.h:313 (DWARF). Per-class RTTI descriptor -- CANONICAL definition folded
// into CgsClassTypeInfo.h (2026-08-25; the per-header copies were an ODR violation).
// State's RTTI-registration (AddToClassTypeInfoArray @ 0x8268DF08) registers
// ClassTypeInfo<State> descriptors.

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

    // DWARF CgsState.h:370: defined in the class (ARTIST has no out-of-line copy; every derived
    // CreateObject inlines it, e.g. BrnState::CreateObject 0x826C83C4: zero +4..+0x3C, then
    // the time pair, then +0x40/+0x44 and the attached flag).
    State()
        : miInstNum(0)
        , meMapState(0)
        , miStateInstType(0)
        , mpvAttachment(0)
        , mpPrevState(0)
        , mpNextState(0)
        , mpHeadEffectControl(0)
        , mpHeadEffectObject(0)
        , mpStateManager(0)
        , mpLogicModule(0)
        , miSFXFlags(0)
        , miNumLoadedEffectObjects(0)
        , miNumLoadedEffectControls(0)
        , meUpdateState(E_UPDATE_UNATTACHED)
        , mePrepareState(E_PREPARE_STATE_CREATE_OBJECTS)
        , mpCurrentEffect(0)
        , mbIsAttached(false)
        , mfCurTime(0.0f)
        , mfDeltaTime(0.0f)
    {
    }

    // @ 0x826ABCD8 (scalar deleting destructor). Declaration-only so CgsState.cpp
    // emits the out-of-line State::~State symbol the deleting-destructor thunk is
    // synthesised from (body = DestroyEffects()). Was an inline no-op.
    virtual ~State();

    virtual ClassTypeInfo<State>* GetTypeInfo() const;
    virtual const char*           GetTypeName() const;
    static ClassTypeInfo<State>* GetStaticTypeInfo();

    // CgsSound::Logic::State::DestroyEffects -- tears down the state's attached
    // effect objects. Called by the State-derived scalar deleting destructors
    // (X360 `bl ...DestroyEffects`, e.g. StreamingState @ 0x826C9B50,
    // GlobalState @ 0x826D2278). Body is un-homed (a separate sound-logic recon
    // slice); declaration-only here so the destructors can call it BY NAME. Do NOT
    // body here.
    void DestroyEffects();

    // CgsSound::Logic::State::Attach(void*) -- records the attachment on the state
    // and wires it into the sound-logic state machine. Called BY NAME from the
    // derived TrafficState::Attach (X360 `bl ...State::Attach` @0x826CB270) and
    // EmitterState::Attach; the body is a separate un-homed sound-logic recon slice.
    // FLAG: declaration-only un-homed base member (do not body here).
    virtual void Attach(void* apvAttachment);
    bool Prepare(s32 liSfxFlags, StateManager* apStateManager);
    virtual void UpdateParams(f32 af32DeltaTime);
    virtual void ProcessUpdate();
    virtual bool Detach();
    virtual void Notify(const CgsSound::Io::MessageHeader* /*apkMessage*/) {}

    // CgsState.h:245 @ 0x826916D8. True when apv is this state's attachment.
    virtual bool IsAttachedToThis(void* apvAttachment);

    // True once the state is bound to an attachment -- the mbIsAttached flag at
    // +72 (0x48), i.e. the `lbz 0x48(state)` byte the derived accessors test
    // (StreamingState::GetRequest @0x82683A00, EmitterState::IsAttachedToThis
    // @0x826BADA0 via its inlined GetSoundEntity()/IsAttached() assert).
    bool IsAttached() const { return mbIsAttached; }

    // DWARF CgsState.h:234 (header-inline; e.g. WheelControl::UpdateDriftingRPM 0x826B7428
    // reads mpState->mfDeltaTime, +0x50).
    f32 GetDeltaTime() { return mfDeltaTime; }

    static ClassTypeInfo<State>* GetRegisteredTypeInfo(u32 auIndex);

    s32 GetInstanceID() const { return miInstNum; }
    s32 GetStateType() const { return miStateInstType; }
    Utils::DataPoint<EUpdateState> GetUpdateState() const { return meUpdateState; }
    State* GetNextState() const { return mpNextState; }
    State* GetPreviousState() const { return mpPrevState; }
    EffectBase* GetHeadEffectObject() const { return mpHeadEffectObject; }
    EffectBase* GetHeadEffectControl() const { return mpHeadEffectControl; }
    StateManager* GetStateManager() const { return mpStateManager; }
    Module* GetLogicModule() const { return mpLogicModule; }
    void* GetAttachment() const { return mpvAttachment; }

protected:
    static ClassTypeInfo<State> sTypeInfo;

    void CreateSFXObjs();
    void CreateSFXCtrls();
    void ForceCreateEffectControls(s32 liMask);
    bool AttachEffect();
    void NewSFXObj(s32 liEffectId);
    EffectBase* NewSFXCtrl(s32 liEffectId);
    EffectBase* HasCtrlBeenAdded(s32 liEffectId);
    void SortSFXCtl();

    friend class StateManager;

public:

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
private:
    Utils::DataPoint<EUpdateState> meUpdateState; // +0x38 current, +0x3C previous

public:
    EPrepareState mePrepareState;           // +64  CgsState.h:345
    EffectBase*   mpCurrentEffect;          // +68  CgsState.h:346
    bool          mbIsAttached;             // +72  CgsState.h:347
    f32           mfCurTime;                // +76  CgsState.h:348
    f32           mfDeltaTime;              // +80  CgsState.h:349
};

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSSTATE_H
