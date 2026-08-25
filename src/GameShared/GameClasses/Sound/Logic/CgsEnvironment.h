#ifndef CGS_SOUND_LOGIC_CGSENVIRONMENT_H
#define CGS_SOUND_LOGIC_CGSENVIRONMENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"  // CgsSound::Logic::StateManager (CANONICAL)
#include "GameShared/GameClasses/Sound/Logic/CgsMicrophone.h"    // MicrophoneSystem (embedded @ +0x50)
#include "SDKs/EATech/include/Nicotine/IDynamicMixer.hpp"        // Nicotine::IDynamicMixer (DynamicMixer base)

namespace rw { struct IResourceAllocator; }
namespace CgsSound { namespace Io { struct MessageHeader; } }

// =============================================================================
// CgsSound::Logic::Environment (+ EnvironmentSpec / ModuleParams / DynamicMixer)
//   GameShared/GameClasses/Sound/Logic/CgsEnvironment.h (DWARF home) +
//   GameShared/GameClasses/Sound/Logic/CgsEnvironment.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX + the DecFIGS DWARF. The sound-logic
// Environment is the per-module world the SoundLogicModule drives: the allocator,
// the StateManager map, the microphone system, the Nicotine dynamic mixer, the
// per-stage CPU monitors and the accumulated game time.
//
// (2026-08-25, faithful-audio-engine phase B2 RESHAPE): this header used to model
// ONLY a 17-slot manager map at +0 with "slot 0 reserved" and the [type+1] index
// convention. The Construct asm @0x8268D050 + the DWARF (CgsEnvironment.h:397-410)
// prove the truth: the word at +0 is mpAllocator, the 16-slot map starts at +4 --
// the console's (type+1)*4 displacement lands in mapStateManagers[type] -- and the
// class continues through the FULL member list below. The X360 offset map
// (Construct @0x8268D050 / Update @0x826C3F78, byte offsets):
//   +0x000 mpAllocator            (spec.mpAllocator; asserted cpp:66)
//   +0x004 mapStateManagers[16]   (zeroed by Construct)
//   +0x044 mModuleParams          (u16 x3; untouched by Construct)
//   +0x04C mu32StateManagerCount  (spec.mu32StateManagerCount; Update's loop bound)
//   +0x050 mMicrophoneSystem      (0x290; UpdateMicrophones(dt) each Update)
//   +0x2E0 mDynamicMixer          (IDynamicMixer base 0x20 + mpEnvironment @+0x20;
//                                  Construct: mpEnvironment = this, then
//                                  Nicotine::IDynamicMixer::CreateInstance(&mixer,
//                                  {0, managerCount, allocator}))
//   +0x304 mCpuMonitors           ("Dynamic Mixer"/"Process Update"/"Update Params"
//                                  /"Logic Environment", page 14, budget 1.0)
//   +0x314 mfGameTime             (Construct 0.0; Update += game dt)
// Update's per-manager drive (managers reached BY NAME): mfTimeStepGame = gameDt,
// mfTimeStepSimulation = simDt, mfCurrentTime = mfGameTime, then the virtual
// UpdateParams(gameDt); after the mixer's ProcessMixMap(gameDt), the virtual
// ProcessUpdate() pass.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// the console offsets above are comments, not asserted.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

const s32 KI_MAX_NUMBER_OF_STATES = 16; // CgsEnvironment.h (DWARF): asserted bound

// DWARF CgsEnvironment.h:127. The Environment creation spec.
struct EnvironmentSpec
{
    rw::IResourceAllocator* mpAllocator;            // :129
    u32                     mu32StateManagerCount;  // :131
};

// DWARF CgsEnvironment.h:147. Module sizing parameters (untouched by the
// reconstructed Construct; carried for the DWARF shape).
struct ModuleParams
{
    u16 mu16MaxVoiceProxies;    // :149
    u16 mu16MaxContentProxies;  // :150
    u16 mu16MaxStateManagers;   // :151

    static const ModuleParams DEFAULT;  // :153 (definition DEFERRED to its own slice)
};

struct Environment;

// DWARF CgsEnvironment.h:169. The Environment-owned Nicotine mixer: the
// IDynamicMixer base plus the owner back-pointer at +0x20 (the word the Logic
// Module ctor's flattened stores zeroed at module +0x2C50).
struct DynamicMixer : public Nicotine::IDynamicMixer
{
    DynamicMixer() : mpEnvironment(0) {}

    // DWARF :562. FLAG (DEFER): declared-only -- bodied with the mixer slices
    // (the Environment's Construct wires mpEnvironment + CreateInstance directly,
    // matching the @0x8268D050 asm).
    void Construct(Environment* apEnvironment);

    Environment* mpEnvironment;   // DWARF :212 (console mixer +0x20)
};

// DWARF CgsEnvironment.h (the Logic-side per-stage CPU perf-monitor ids, in the
// Construct's store order at console +0x304..+0x310).
struct EnvironmentCpuMonitors
{
    s32 miDynamicMixer;        // +0x304  "Dynamic Mixer"
    s32 miProcessUpdate;       // +0x308  "Process Update"
    s32 miUpdateParams;        // +0x30C  "Update Params"
    s32 miEnvironmentUpdate;   // +0x310  "Logic Environment"
};

struct Environment
{
    // Host-safety default state (the X360 has no distinct Environment ctor -- the
    // owning Logic::Module's ctor stores + Construct() establish everything; this
    // zero-init only guards the pre-Construct window on the host).
    Environment()
        : mpAllocator(0)
        , mu32StateManagerCount(0)
        , mfGameTime(0.0f)
    {
        for (s32 liSlot = 0; liSlot < KI_MAX_NUMBER_OF_STATES; ++liSlot)
            mapStateManagers[liSlot] = 0;
        mModuleParams.mu16MaxVoiceProxies   = 0;
        mModuleParams.mu16MaxContentProxies = 0;
        mModuleParams.mu16MaxStateManagers  = 0;
        mCpuMonitors.miDynamicMixer      = -1;
        mCpuMonitors.miProcessUpdate     = -1;
        mCpuMonitors.miUpdateParams      = -1;
        mCpuMonitors.miEnvironmentUpdate = -1;
    }

    // @ 0x8268D050 (DWARF h:245). Adopt the spec (allocator asserted, cpp:66),
    // zero the manager map, wire + create the dynamic mixer, register the four
    // CPU monitors. Bodied in CgsEnvironment.cpp (phase B2).
    void Construct(const EnvironmentSpec& akrSpec);

    // @ 0x826C3F78 (DWARF h:275). The per-frame drive: accumulate mfGameTime,
    // update the microphones, seed every registered manager's time fields + run
    // its virtual UpdateParams(gameDt), ProcessMixMap the mixer, then the
    // managers' virtual ProcessUpdate() pass. Bodied in CgsEnvironment.cpp.
    void Update(f32 af32GameDt, f32 af32SimDt);

    // @ 0x82680D60. Register a StateManager into the map keyed by its state type.
    // Returns true (the X360 returns li r3,1 on every path).
    bool AddStateManager(StateManager* apStateManager);

    // @ 0x8268D1C0. Fetch the StateManager registered for state id liStateManId.
    // Asserts liStateManId < KI_MAX_NUMBER_OF_STATES (CgsEnvironment.cpp:481).
    // Returns the slot (may be null if nothing registered). Callers:
    // DynamicMixer::GetStateCount/ConnectDMixIO, Environment::Notify.
    StateManager* GetStateManager(s32 liStateManId) const;

    // DWARF h:114 (const). Dispatch one message to the state managers -- the
    // Logic::Module::ProcessMessageQueue @0x826C45A8 target. FLAG (DEFER):
    // declared-only -- the routing body (which managers see which messages) is
    // its own slice; ProcessMessageQueue calls it BY NAME.
    void Notify(const CgsSound::Io::MessageHeader* apkMessage) const;

    // DWARF h:129 / h:132. Accessors for the embedded systems.
    MicrophoneSystem&        GetMicrophoneSystem() { return mMicrophoneSystem; }
    Nicotine::IDynamicMixer& GetDynamicMixer()     { return mDynamicMixer; }

    // ---- members (DWARF order/names; console offsets in the banner) ----
    rw::IResourceAllocator* mpAllocator;                            // +0x000 (:397)
    StateManager*           mapStateManagers[KI_MAX_NUMBER_OF_STATES]; // +0x004 (:401)
    ModuleParams            mModuleParams;                          // +0x044 (:403)
    u32                     mu32StateManagerCount;                  // +0x04C (:404)
    MicrophoneSystem        mMicrophoneSystem;                      // +0x050 (:406)
    DynamicMixer            mDynamicMixer;                          // +0x2E0 (:407)
    EnvironmentCpuMonitors  mCpuMonitors;                           // +0x304 (:408)
    f32                     mfGameTime;                             // +0x314 (:410)
};

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSENVIRONMENT_H
