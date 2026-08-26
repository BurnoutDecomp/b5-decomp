#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (attached-buffer guard)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log / Message filter
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // the "Resource Registrar" monitor (Prepare case 0)
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo.h"  // Io::LogicPreUpdateOutputBuffer (PreUpdate; phase C1)

// BrnSound::Module::SoundLogicModule -- accessor bodies recovered from
// BURNOUT_X360_ARTIST.XEX. See BrnSoundLogicModule.h for the layout/slice notes.

namespace BrnSound
{
namespace Module
{

// X360 0x826AFF88. Search the per-frame trigger-action table for the entry whose
// EntityId and result-type both match, returning it (or null when absent).
//
// X360 STRUCTURE (0x826AFF88):
//   result = 0; index = 0;
//   base   = &maTriggerActions;           // r30 = this + 0x4CA0
//   do {
//       if (base->miCount == -1) <fire "Array used before Construct/Clear was called">
//       if (index >= base->miCount) break; // unsigned compare against the live count
//       if (base->Ge(index).mEntityId == leEntityId &&
//           base->Ge(index).meResultType == leType)
//           result = &base->Ge(index);
//       ++index;
//   } while (!result);
//   return result;
//
// maTriggerActions.Ge(index) is the committed Array<T,N>::Ge, which carries the
// per-access "Array used before Construct/Clear was called" + bounds asserts the
// X360 body inlines each iteration. EntityId has no operator==, so the identity
// word is compared by its packed value (muValue), matching the X360 raw-word load.
const BrnGameState::GameStateModuleIO::SoundTriggerAction*
SoundLogicModule::GetSoundTriggerAction(
    EntityId leEntityId,
    BrnGameState::GameStateModuleIO::SoundTriggerAction::eType leType)
{
    const BrnGameState::GameStateModuleIO::SoundTriggerAction* lpResult = 0;
    u32 luIndex = 0;
    do
    {
        if (luIndex >= maTriggerActions.GetLength())
        {
            break;
        }
        if (maTriggerActions.Ge(luIndex).mEntityId.muValue == leEntityId.muValue &&
            maTriggerActions.Ge(luIndex).meResultType == leType)
        {
            lpResult = &maTriggerActions.Ge(luIndex);
        }
        ++luIndex;
    }
    while (!lpResult);
    return lpResult;
}

// X360 0x826838C0: `addi r3, r3, 0x588; blr`. Hand out the embedded resource
// registrar by reference (the IResourceRequester override).
BrnSound::Logic::ResourceRegistrar& SoundLogicModule::GetResourceRegistrar()
{
    return mResourceRegistrar;
}

// Bring-up. The X360 ctor (0x827E3DA8) default-constructs the embedded ResourceRegistrar
// (a1+21016); its queues/pools are then initialised by the bring-up Construct (0x826B0470).
// (phase B5): the ENGINE base Construct runs FIRST -- ModuleSingleBuffered + the instance
// counter + the message queue seeds -- then the Brn half: clear the per-frame trigger
// table + Construct the embedded registrar so the broker is live. The 3 Voices (X360
// +20976/+20988/+21000) are still grown on top.
void SoundLogicModule::Construct()
{
    CgsSound::Logic::Module::Construct();

    mpBrnLogicInputBuffer  = 0;
    mpBrnLogicOutputBuffer = 0;

    // The per-frame trigger-action table starts empty (so GetSoundTriggerAction's
    // "used before Construct/Clear" assert is satisfied).
    maTriggerActions.Clear();

    // The streaming-resource broker: bring up its request queues + requested/queued pools.
    mResourceRegistrar.Construct();

    // The pre-update output block's three queues (phase C1; the same trio the
    // RootPreUpdateOutputBuffer carve constructs).
    reinterpret_cast<CgsModule::VariableEventQueue<256, 16>*>(
        mPreUpdateOutput.maGuiOutEventQueueStorage)->Construct();
    mPreUpdateOutput.mAudioCarDataLoadedQueue.Construct();
    mPreUpdateOutput.mAudioEffectsMessageQueue.Construct();

    // The 9 state-manager slots start empty; CreateStateManagers (stage 4) fills them via
    // StateManager::CreateStateMan (null where no leaf is registered). Nulling here keeps
    // PrepareStateManagersOnBoot's `*v5 != 0` guard honest even if a stage runs early.
    for (s32 liIndex = 0; liIndex < KI_NUM_STATE_MANAGERS; ++liIndex)
        mapStateManagers[liIndex] = 0;

    // [grow-in] X360 ctor also constructs the 3 Voices (Submix/Master/GlobalReverb); their
    //   Logic::Voice slice reconciliation is deferred (see Prepare stage 2).

    meBrnPrepareStage = E_PREPSTAGE_PERFMON;
    mbConstructed     = true;

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "[Sound] SoundLogicModule::Construct (registrar live)\n";
}

// X360 0x82703C18 (vtable+0x58). The REAL resumable stage machine (phase B5), console
// switch-for-switch:
//   pre-switch: capture the logic allocator (a1[19777]); assert both buffers (cpp:200/:201);
//     AttachBuffers (the ENGINE virtual, vtbl+0x50).
//   case 0: miResourceRegistrarMonitor = AddMonitor("Resource Registrar", page 14, 2.0) ->
//   case 1: the ENGINE base CgsSound::Logic::Module::Prepare(alloc, in, out, &the 0x820AA480
//     rodata ModuleParams {16,16,16} == the DWARF-declared ModuleParams::DEFAULT); on true
//     advance to 2 (+ the console's progress word = 1); either way DetachBuffers + return
//     false (one chunk per call).
//   case 2: first entry constructs the 3 Voices (Submix ident -16 / Master 1 / GlobalReverb
//     2 against the GenericRwacFactory name + their VoiceSpec names) and returns false; the
//     re-entry Connects Submix+Reverb to "Send01" and LoadAssets BurnoutGlobalData, then
//     falls into
//   case 3: ResourceBridging under the output buffer's write lock -> DetachBuffers + false.
//   case 4: CreateStateManagers ->
//   case 5: LockForWrite(out); PrepareStateManagersOnBoot(4) -- not ready -> ResourceBridging
//     + unlock + detach + false (retry); ready -> unlock ->
//   case 6: DetachBuffers + return true.
//
// FLAG [grow-in, stage 2]: the committed Logic::Voice slice's Construct/Connect signatures
// diverge from this call shape (the console passes (this, ident, dword_83008650 == the
// GenericRwacFactory name hash, MakeHash("<spec>VoiceSpec")); reconciling the Voice slice +
// the LoadAsset data dependency is its own batch), so stage 2 still ADVANCES without
// constructing the voices; everything else in the machine is real.
bool SoundLogicModule::Prepare(rw::IResourceAllocator* apAllocator,
                               CgsModule::IOBuffer* apInputBuffer,
                               CgsModule::IOBuffer* apOutputBuffer)
{
    mpBrnAllocator = apAllocator;   // X360 a1[19777] = a2

    CGS_ASSERT(apInputBuffer, "lpInputBuffer");
    CGS_ASSERT(apOutputBuffer, "lpOutputBuffer");

    // The ENGINE AttachBuffers (X360 vtbl+0x50): pin the engine-side buffer pointers.
    // FLAG (host note): the Brn-side mpBrnLogic* members (+0x4C94/+0x4C98) are pinned
    // beside them -- their console writer is the un-dumped Brn override side; the store
    // keeps GetBrnInputStructure() live exactly as before.
    AttachBuffers(apInputBuffer, apOutputBuffer);
    mpBrnLogicInputBuffer  = reinterpret_cast<Io::LogicInputBuffer*>(apInputBuffer);
    mpBrnLogicOutputBuffer = reinterpret_cast<Io::LogicOutputBuffer*>(apOutputBuffer);

    bool lbPrepared = false;
    switch (meBrnPrepareStage)
    {
    case E_PREPSTAGE_PERFMON:
        // X360 case 0 (a1[19826]): 5-arg AddMonitor, page 14, budget 2.0, libperf-tagged.
        miResourceRegistrarMonitor = CgsDev::PerfMonCpu::AddMonitor(
            "Resource Registrar", static_cast<CgsDev::PerfMonCpuPage>(14), false, 2.0f, true);
        // fall through
    case E_PREPSTAGE_BASE:
        meBrnPrepareStage = E_PREPSTAGE_BASE;
        // The ENGINE base's resumable Prepare (base + environment + proxies stages).
        if (CgsSound::Logic::Module::Prepare(apAllocator, apInputBuffer, apOutputBuffer,
                                             CgsSound::Logic::ModuleParams::DEFAULT))
        {
            meBrnPrepareStage = E_PREPSTAGE_VOICES;
        }
        break;   // one chunk per call (console LABEL_20: detach + return 0)
    case E_PREPSTAGE_VOICES:
        meBrnPrepareStage = E_PREPSTAGE_VOICES;
        // FLAG [grow-in]: the 3 Voice constructs + the "Send01" connects + the
        // BurnoutGlobalData LoadAsset (see the banner). Advance into the bridge.
        // fall through
    case E_PREPSTAGE_BRIDGE:
        meBrnPrepareStage = E_PREPSTAGE_BRIDGE;
        // X360 case 3 / LABEL_12: ResourceBridging under the output write lock.
        apOutputBuffer->LockForWrite();
        ResourceBridging();
        apOutputBuffer->UnlockForWrite();
        meBrnPrepareStage = E_PREPSTAGE_STATEMANAGERS;
        break;   // console: detach + return 0 after the bridge chunk
    case E_PREPSTAGE_STATEMANAGERS:
        meBrnPrepareStage = E_PREPSTAGE_STATEMANAGERS;
        // X360 case 4: create the 9 managers + register them into the ENGINE base's
        // Environment.
        CreateStateManagers();
        // fall through
    case E_PREPSTAGE_BOOTPREPARE:
        meBrnPrepareStage = E_PREPSTAGE_BOOTPREPARE;
        // X360 case 5, under the output write lock; a not-ready manager bridges +
        // retries next tick (stage stays here).
        apOutputBuffer->LockForWrite();
        if (!PrepareStateManagersOnBoot(4))
        {
            ResourceBridging();              // console LABEL_13 on the retry path
            apOutputBuffer->UnlockForWrite();
            break;
        }
        apOutputBuffer->UnlockForWrite();
        meBrnPrepareStage = E_PREPSTAGE_DONE;
        // fall through
    case E_PREPSTAGE_DONE:
        meBrnPrepareStage = E_PREPSTAGE_DONE;
        lbPrepared = true;
        break;
    default:
        CGS_ASSERT(false, "Invalid Stage\n");
        break;
    }

    // Console: every exit detaches the per-call buffers (vtbl+0x54).
    DetachBuffers();

    if (lbPrepared && !mbPrepared)
    {
        mbPrepared = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "[Sound] SoundLogicModule::Prepare: stage machine "
                                          "complete (engine base + environment live; voices "
                                          "grow-in) -> prepared\n";
    }
    return lbPrepared;
}

// X360 0x82702E80. Bridge the broker's per-frame resource traffic:
//   ResourceRegistrar::Update(this+21016);                                    // <- real, runs now
//   VariableEventQueue<4096,16>::Append(*(this+19608)+2068, this+74952);      // <- grow-in
//   VariableEventQueue<2048,16>::Append(*(this+19608)+4,    this+72888);      // <- grow-in
// MINIMAL-THEN-GROW: the registrar Update is REAL (drains the request queues, resolves requested
// resources to handles, promotes queued->requested, GCs unreferenced files). On the freshly-
// Construct'd empty registrar at boot it is a safe no-op (empty queues/pools), but this is what
// actually exercises the reconstructed broker Update path at runtime.
void SoundLogicModule::ResourceBridging()
{
    mResourceRegistrar.Update();

    // [grow-in] FLAG: the two VariableEventQueue Appends that bridge the registrar's request
    //   interfaces (X360 this+74952 = mResourceRequestInterface, this+72888 = mAttribSysRequest-
    //   Interface) into the logic output buffer (*(this+19608)+2068 / +4) are deferred -- they need
    //   the output-buffer VEQ layout + access to the registrar's private request interfaces. The
    //   X360 also brackets this in the output buffer's LockForWrite/UnlockForWrite.
}

// X360 0x826AFEF8. Create the 9 sound-logic state managers and register them in the
// embedded Environment. X360 store-for-store (a1 == this):
//   v2 = a1 + 10576;            ; &lEnvironment
//   v3 = 0;  v4 = a1 + 79064;   ; slot index + &mapStateManagers[0]
//   do {
//       result = StateManager::CreateStateMan(v3, a1);   ; factory(i, this)
//       *v4 = result;                                    ; mapStateManagers[i] = result
//       if ( result ) {
//           result = Environment::AddStateManager(v2);   ; lEnvironment.AddStateManager(result)
//           if ( !result ) <assert "lEnvironment.AddStateManager( mapStateManagers[ i ] )"  // :753>
//       }
//       ++v3; ++v4;
//   } while ( v3 < 9 );
//
// Reproduced BY NAME: the loop fills mapStateManagers[i] from the factory (which scans
// the RTTI registry by id), and every non-null manager is registered in lEnvironment.
// The X360 guards the AddStateManager call with `if (result)` -- a null slot (no leaf
// registered for that id) is skipped. NOTE (2026-08-25): the 8 manager TUs ARE in the build, so the
// registry is empty, every CreateStateMan returns null, every slot is set null, and
// AddStateManager is never called -> a safe no-op exactly as the X360 degrades.
//
// FLAG (faithful guard): the X360 asserts the AddStateManager *return* (it returns 1 on
// every path -- see CgsEnvironment.cpp -- so the assert is a vacuous tripwire). The
// embedded Environment's AddStateManager itself asserts the manager is non-null, its
// state-type is in range, and the slot is free; those are the real registration guards.
void SoundLogicModule::CreateStateManagers()
{
    for (s32 liIndex = 0; liIndex < KI_NUM_STATE_MANAGERS; ++liIndex)
    {
        mapStateManagers[liIndex] =
            CgsSound::Logic::StateManager::CreateStateMan(static_cast<u32>(liIndex), this);

        if (mapStateManagers[liIndex] != 0)
        {
            // The X360 asserts this returns true (it always does); kept as the faithful
            // registration call. AddStateManager itself fires the real registration asserts.
            // (phase B5: the environment is the ENGINE base's, via GetEnvironment().)
            bool lbRegistered = GetEnvironment().AddStateManager(mapStateManagers[liIndex]);
            CGS_ASSERT(lbRegistered,
                       "lEnvironment.AddStateManager( mapStateManagers[ i ] )");
            (void)lbRegistered;
        }
    }
}

// X360 0x826837F8. Boot-prepare the created state managers (boot caller passes mask 4).
// X360 store-for-store (a1 == this, a2 == luSkipMask):
//   v3 = a1 + 79064; v4 = 0; v5 = a1 + 79064;          ; &mapStateManagers[0]
//   do {
//       if ( ((1 << v4) & a2) == 0 && *v5 && !(*(**v5 + 12))(*v5) )   ; skip-bit / null-guard / Prepare()
//           return 0;                                                  ; a manager not ready -> retry boot
//       ++v4; ++v5;
//   } while ( v4 < 9 );
//   if ( *v3 ) {                                          ; mapStateManagers[0]
//       v6 = (*(**v3 + 20))(*v3, 0);                      ; GetChildStateManager(0)  (vtable +0x14)
//       if ( v6 ) (*(*v6 + 12))(v6, 0);                   ; child->Prepare()         (vtable +0x0C)
//   }
//   return 1;
//
// Reproduced BY NAME: for each slot not masked out and non-null, call its Prepare()
// (the per-manager bring-up state machine, overridden by each leaf); a false return
// aborts (returns false so the boot stage stays and retries). Then the
// mapStateManagers[0] child special-case: fetch its child via GetChildStateManager(0)
// and, if present, Prepare() the child. The null-guard `*v5 != 0` means empty slots
// (no registered leaf) are skipped -> (2026-08-25: managers ARE in the build now; only truly-empty ids return)
// true immediately (safe no-op), matching the X360's degenerate behaviour.
//
// NOTE (skip-mask sense): the X360 SKIPS Prepare when `((1<<i) & mask) != 0`; mask 4 ==
// bit 2, so on boot slot 2's Prepare is skipped here (faithful to the boot call).
bool SoundLogicModule::PrepareStateManagersOnBoot(s32 luSkipMask)
{
    for (s32 liIndex = 0; liIndex < KI_NUM_STATE_MANAGERS; ++liIndex)
    {
        const bool lbSkip = (((1 << liIndex) & luSkipMask) != 0);
        if (!lbSkip && mapStateManagers[liIndex] != 0)
        {
            if (!mapStateManagers[liIndex]->Prepare())   // vtable +0x0C
            {
                return false;   // not ready yet -> the boot stage retries
            }
        }
    }

    // The mapStateManagers[0] child-prepare special-case.
    if (mapStateManagers[0] != 0)
    {
        CgsSound::Logic::StateManager* lpChild =
            mapStateManagers[0]->GetChildStateManager(0);   // vtable +0x14
        if (lpChild != 0)
        {
            lpChild->Prepare();                             // vtable +0x0C
        }
    }

    return true;
}

// X360 0x82682518. Return the attached sound logic input buffer, asserting it is
// non-null first (the X360 fires CgsDev::Assert with the stringized member name
// "mpBrnLogicInputBuffer" at BrnSoundLogicModule.h:432, then still returns the
// pointer -- a non-gating tripwire).
Io::LogicInputBuffer* SoundLogicModule::GetBrnInputStructure()
{
    CGS_ASSERT(mpBrnLogicInputBuffer, "mpBrnLogicInputBuffer");
    return mpBrnLogicInputBuffer;
}

// X360 0x826E1F10 (DWARF :152; bodied 2026-08-25, faithful-audio-engine phase C1).
// Publish the module's accumulated pre-update output block into the caller's
// scratch buffer: assert (cpp:495), write-lock, the SetPreUpdateOutput copy
// (@0x826E0C10 -- the two memcpy spans + the car-data Clear+Append), unlock.
void SoundLogicModule::PreUpdate(Io::LogicPreUpdateOutputBuffer* apLogicPreUpdateOutput)
{
    CGS_ASSERT(apLogicPreUpdateOutput != 0, "lpLogicPreUpdateOutput");
    apLogicPreUpdateOutput->LockForWrite();
    apLogicPreUpdateOutput->SetPreUpdateOutput(mPreUpdateOutput);
    apLogicPreUpdateOutput->UnlockForWrite();
}

} // namespace Module
} // namespace BrnSound
