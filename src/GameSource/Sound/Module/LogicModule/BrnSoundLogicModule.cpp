#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (attached-buffer guard)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log / Message filter

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
// MINIMAL-THEN-GROW: clear the per-frame trigger table + Construct the embedded registrar so
// the broker is live, then mark constructed. The 3 Voices (X360 +20976/+20988/+21000), the
// CgsSound::Logic::Module engine base, the state managers and the ~79KB IO/state tail are
// grown on top.
void SoundLogicModule::Construct()
{
    mpBrnLogicInputBuffer  = 0;
    mpBrnLogicOutputBuffer = 0;

    // The per-frame trigger-action table starts empty (so GetSoundTriggerAction's
    // "used before Construct/Clear" assert is satisfied).
    maTriggerActions.Clear();

    // The streaming-resource broker: bring up its request queues + requested/queued pools.
    mResourceRegistrar.Construct();

    // The 9 state-manager slots start empty; CreateStateManagers (stage 4) fills them via
    // StateManager::CreateStateMan (null where no leaf is registered). Nulling here keeps
    // PrepareStateManagersOnBoot's `*v5 != 0` guard honest even if a stage runs early.
    for (s32 liIndex = 0; liIndex < KI_NUM_STATE_MANAGERS; ++liIndex)
        mapStateManagers[liIndex] = 0;

    // [grow-in] X360 ctor also constructs the 3 Voices (Submix/Master/GlobalReverb) and the
    //   CgsSound::Logic::Module base; not modelled by this minimal slice.

    mePrepareStage = E_PREPSTAGE_PERFMON;
    mbConstructed  = true;

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "[Sound] SoundLogicModule::Construct (registrar live)\n";
}

// X360 0x82703C18 (vtable+0x58). MINIMAL-THEN-GROW resumable stage machine. The real body
// runs the base Module::Prepare, constructs+connects the 3 Voices, LoadAsset's
// BurnoutGlobalData, bridges resources, then creates + boot-prepares the state managers,
// reporting prepared only when PrepareStateManagersOnBoot completes. Stages 0-3 (PerfMon /
// base Module::Prepare / Voices / partial ResourceBridging) are still guarded grow-in stubs;
// stages 4 (CreateStateManagers) and 5 (PrepareStateManagersOnBoot(4)) are now REAL -- stage 5
// retries (returns false WITHOUT advancing) until the boot managers report ready, exactly like
// the X360. With the 8 manager TUs out of the build the state-manager registry is empty, so
// stage 4 creates nothing and stage 5 returns true immediately (safe no-op) -- the machine
// still completes and reports prepared so the RootSoundModule LOGIC stage drives it.
bool SoundLogicModule::Prepare(void* lpParentModule, void* lpInputBuffer, void* lpOutputBuffer)
{
    // X360 line 1 (0x82703C18): store the parent/allocator handle (a1[19777] = a2). The X360
    // stage-4 caller passes the logic RW allocator here.
    mpParentModule = lpParentModule;

    // X360 asserts both IO buffers are attached (BrnSoundLogicModule.cpp:200-201).
    CGS_ASSERT(lpInputBuffer, "lpInputBuffer");
    CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");

    // X360 line 450: (*(*a1 + 80))(a1, a3, a4) -- the vtable+0x50 call that ATTACHES the per-frame
    // input/output IO buffers into the module so GetBrnInputStructure() and the logic stages can
    // reach them. The exact vtable+0x50 method is not separately modelled by this minimal slice; its
    // effect -- pinning mpBrnLogicInputBuffer/mpBrnLogicOutputBuffer to the passed buffers -- is
    // reproduced here (Construct() zeroes them; Prepare attaches them, matching the X360 ctor->Prepare
    // ordering). FLAG: attach-by-store inferred from the member usage (GetBrnInputStructure returns
    // mpBrnLogicInputBuffer @ X360 this+0x4C94); the call's other side effects are grow-in.
    mpBrnLogicInputBuffer  = (Io::LogicInputBuffer*)lpInputBuffer;
    mpBrnLogicOutputBuffer = (Io::LogicOutputBuffer*)lpOutputBuffer;

    switch (mePrepareStage)
    {
    case E_PREPSTAGE_PERFMON:
        // [grow-in] X360 case 0: CgsDev::PerfMonCpu::AddMonitor("Resource Registrar"). Advance.
        mePrepareStage = E_PREPSTAGE_BASE;
        // fall through
    case E_PREPSTAGE_BASE:
        // [grow-in] X360 case 1: CgsSound::Logic::Module::Prepare (the engine base, resumable).
        //   The CgsSound::Logic::Module base is not modelled by this slice. Advance.
        mePrepareStage = E_PREPSTAGE_VOICES;
        // fall through
    case E_PREPSTAGE_VOICES:
        // [grow-in] X360 case 2: Voice::Construct(Submix/Master/GlobalReverb) + Voice::Connect
        //   ("Send01") + IResourceRequester::LoadAsset("Sound\\BurnoutGlobalData.bin"). The Voice
        //   subsystem + the asset-load path are deferred. Advance.
        mePrepareStage = E_PREPSTAGE_BRIDGE;
        // fall through
    case E_PREPSTAGE_BRIDGE:
        // X360 case 3: SoundLogicModule::ResourceBridging (under the output-buffer lock). The
        //   registrar Update now runs FOR REAL here (the X360 wraps it in IOBuffer LockForWrite/
        //   UnlockForWrite on the output buffer; the lock is skipped in this minimal slice since the
        //   queue-bridge Appends it guards are deferred -- see ResourceBridging). Advance.
        ResourceBridging();
        mePrepareStage = E_PREPSTAGE_STATEMANAGERS;
        // fall through
    case E_PREPSTAGE_STATEMANAGERS:
        // X360 case 4: SoundLogicModule::CreateStateManagers -- create the 9 managers from
        //   the RTTI registry + register them in lEnvironment, then advance. Safe no-op when
        //   the manager TUs are out of the build (the registry is empty -> all slots null).
        CreateStateManagers();
        mePrepareStage = E_PREPSTAGE_BOOTPREPARE;
        // fall through
    case E_PREPSTAGE_BOOTPREPARE:
        // X360 case 5: PrepareStateManagersOnBoot(4). If a manager is not ready it returns
        //   false; the X360 STAYS on this stage and retries next Prepare() tick, so return
        //   false WITHOUT advancing. Otherwise advance. With no managers (empty slots) it
        //   returns true immediately -> advances. (boot mask = 4.)
        if (!PrepareStateManagersOnBoot(4))
        {
            return false;   // stay on E_PREPSTAGE_BOOTPREPARE; retried next tick
        }
        mePrepareStage = E_PREPSTAGE_DONE;
        // fall through
    case E_PREPSTAGE_DONE:
    default:
        break;
    }

    // Machine complete: reset (X360 leaves the stage word at done) and report prepared.
    mePrepareStage = E_PREPSTAGE_PERFMON;
    mbPrepared     = true;
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "[Sound] SoundLogicModule::Prepare: stage machine complete "
                                      "(voices/state-managers/base grow-in) -> prepared\n";
    return true;
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
// registered for that id) is skipped. So with the 8 manager TUs OUT of the build the
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
            bool lbRegistered = lEnvironment.AddStateManager(mapStateManagers[liIndex]);
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
// (no registered leaf) are skipped -> with the managers out of the build this returns
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

} // namespace Module
} // namespace BrnSound
