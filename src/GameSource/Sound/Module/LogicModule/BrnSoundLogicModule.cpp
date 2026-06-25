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
// reporting prepared only when PrepareStateManagersOnBoot completes. Each stage here is a
// guarded grow-in stub that advances until its subsystem (Voice / state managers / the engine
// base) is reconstructed; the machine completes and reports prepared so a future
// RootSoundModule LOGIC stage can drive it.
bool SoundLogicModule::Prepare(void* /*lpParentModule*/, void* lpInputBuffer, void* lpOutputBuffer)
{
    // X360 asserts both IO buffers are attached (BrnSoundLogicModule.cpp:200-201).
    CGS_ASSERT(lpInputBuffer, "lpInputBuffer");
    CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");

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
        // [grow-in] X360 case 3: SoundLogicModule::ResourceBridging under the output-buffer lock.
        //   Deferred. Advance.
        mePrepareStage = E_PREPSTAGE_STATEMANAGERS;
        // fall through
    case E_PREPSTAGE_STATEMANAGERS:
        // [grow-in] X360 case 4: SoundLogicModule::CreateStateManagers. Deferred. Advance.
        mePrepareStage = E_PREPSTAGE_BOOTPREPARE;
        // fall through
    case E_PREPSTAGE_BOOTPREPARE:
        // [grow-in] X360 case 5: PrepareStateManagersOnBoot(4) (loops until the boot state
        //   managers report ready). Deferred. Advance.
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
