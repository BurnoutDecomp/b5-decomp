#include "GameSource/Gui/Flow/HUD/BrnHudFlow.h"

#include <new>   // placement new (carve states out of the linear allocator)

#include "GameShared/GameClasses/Core/CgsID.h"                         // CgsIDCompress
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"            // CgsMemory::LinearMalloc
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateMachine.h"// CgsGui::StateMachine
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"       // CgsGui::State

// The 14 HUD-flow states (the pool BrnHudFlow::Prepare builds).
#include "GameSource/Gui/Flow/HUD/States/BrnBootPreload.h"
#include "GameSource/Gui/Flow/HUD/States/BrnBootVideos.h"
#include "GameSource/Gui/Flow/HUD/States/BrnBootLegal.h"
#include "GameSource/Gui/Flow/HUD/States/BrnBootAttract.h"
#include "GameSource/Gui/Flow/HUD/States/BrnPostTitleScreenLoad.h"
#include "GameSource/Gui/Flow/HUD/States/BrnBootProfile.h"
#include "GameSource/Gui/Flow/HUD/States/BrnBootLoading.h"
#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"
#include "GameSource/Gui/Flow/HUD/States/BrnFBurnMainHudState.h"
#include "GameSource/Gui/Flow/HUD/States/BrnPausedHudState.h"
#include "GameSource/Gui/Flow/HUD/States/BrnCrashedHudState.h"
#include "GameSource/Gui/Flow/HUD/States/BrnCrashedStuntHudState.h"
#include "GameSource/Gui/Flow/HUD/States/BrnIdleHudState.h"
#include "GameSource/Gui/Flow/HUD/States/BrnPreRaceFlyBy.h"

// ===========================================================================
//  BrnGui::BrnHudFlow -- reconstructed from BURNOUT_X360_ARTIST.XEX. The HUD flow owns the
//  14-state pool and installs it into the embedded CgsGui::StateMachine; the FSM Lua scripts the
//  GuiFsmController loads (PrepareLua) then SetState() the matching state id (BF_PRELOAD, ...).
// ===========================================================================

namespace BrnGui
{
namespace
{
    // Carve one state object out of the flow's linear allocator and default-construct it in place.
    // The X360 placement-new'd a fixed sizeof into each LinearMalloc block (and stored 0 on
    // overflow); the PC-faithful translation uses sizeof(T) (x64 layouts differ) + the C++ ctor.
    template <typename T>
    T* NewPoolState(CgsMemory::LinearMalloc* lpLinearMalloc)
    {
        void* lpMem = lpLinearMalloc->Malloc(sizeof(T));
        return lpMem ? new (lpMem) T() : 0;
    }
}

// @ 0x824F1E78 -- tail-chains BrnBaseFlow::Construct (the X360 passes the GUI cache straight through).
void BrnHudFlow::Construct(GuiCache* lpGuiCache)
{
    BrnBaseFlow::Construct(lpGuiCache);
}

// @ 0x82508620 -- BrnBaseFlow::Update wrapped in the HUD-flow CPU perf monitor.
void BrnHudFlow::Update()
{
    // FLAG: the X360 brackets this with CgsDev::PerfMonCpu::Start/StopMonitor(dword_82F2763C) --
    // a profiling-only span. The behaviour is BrnBaseFlow::Update; the perf-monitor wrap is omitted.
    BrnBaseFlow::Update();
}

// @ 0x8251A620 -- base prepare, then build + install the 14-state HUD pool.
bool BrnHudFlow::Prepare(CgsGui::GuiAccessPointers* lpAccessPointers,
                         rw::IResourceAllocator* lpAllocator,
                         CgsMemory::LinearMalloc* lpLinearMalloc,
                         ProfileManager* lpProfileManager)
{
    // Base flow prepare: stash access pointers/allocator + wire the state machine's StateInterface.
    BrnBaseFlow::Prepare(lpAccessPointers, lpAllocator);

    // FLAG: the X360 makes a self virtual call `this->vtable[6](this)` immediately after
    // SetStateInterface (0x8251A650). In BrnHudFlow's own vtable slot 6 *is* this 4-arg Prepare
    // (the 2-arg base Prepare sits at slot 3), so taken literally it is infinite self-recursion --
    // an IDA artifact of the dual-Prepare-overload vtable, not a real call (EventObserver::Prepare
    // does not re-vtable the object, confirmed @0x8284FBF0). Construct (which brings up the embedded
    // StateMachine/LuaState/cache) is sequenced by the controller before Prepare, so omitting it
    // drops no initialisation. Substance follows: build the 14-state pool and install it.

    CgsGui::StateMachine& lStateMachine = GetStateMachine();

    // Allocate the 14 states (X360 build order = the SetStates table order).
    mpPreload             = NewPoolState<BootPreload>(lpLinearMalloc);
    mpVideos              = NewPoolState<BootVideos>(lpLinearMalloc);
    mpLegal               = NewPoolState<BootLegal>(lpLinearMalloc);
    mpAttract             = NewPoolState<BootAttract>(lpLinearMalloc);
    mpPostTitleScreenLoad = NewPoolState<PostTitleScreenLoad>(lpLinearMalloc);
    mpProfile             = NewPoolState<BootProfile>(lpLinearMalloc);
    mpLoading      = NewPoolState<BootLoading>(lpLinearMalloc);
    mpRaceMain     = NewPoolState<RaceMainHudState>(lpLinearMalloc);
    mpFBurnMain    = NewPoolState<FBurnMainHudState>(lpLinearMalloc);
    mpPaused       = NewPoolState<PausedHudState>(lpLinearMalloc);
    mpCrashed      = NewPoolState<CrashedHudState>(lpLinearMalloc);
    mpCrashedStunt = NewPoolState<CrashedStuntHudState>(lpLinearMalloc);
    mpIdle         = NewPoolState<IdleHudState>(lpLinearMalloc);
    mpPreRaceFlyBy = NewPoolState<PreRaceFlyByState>(lpLinearMalloc);

    // Construct each state with its script-id name + the owning state machine.
    mpPreload->Construct(CgsIDCompress("BF_PRELOAD"), &lStateMachine);
    mpVideos->Construct(CgsIDCompress("BF_VIDEOS"), &lStateMachine);
    mpLegal->Construct(CgsIDCompress("BF_LEGAL"), &lStateMachine);
    mpAttract->Construct(CgsIDCompress("BF_ATTR"), &lStateMachine);
    mpPostTitleScreenLoad->Construct(CgsIDCompress("BF_COMPLOAD"), &lStateMachine);
    // The X360 BF_PROFILE slot alone dispatches the wider Construct(id, fsm, manager)
    // virtual (Prepare @0x8251A620: `(*(v62 + 36))(*v26, v63, v8, a5)` -- vtable slot 9
    // vs the 2-arg slot 6 every other state gets), threading the module's ProfileManager
    // into the state. DWARF signature: Construct(CgsID, CgsFsm::ScriptedFsm*, ProfileManager&).
    if (lpProfileManager != 0)
    {
        mpProfile->Construct(CgsIDCompress("BF_PROFILE"), &lStateMachine, *lpProfileManager);
    }
    else
    {
        // FLAG PC defensive fallback: the X360 Prepare always receives a live manager;
        // a config that passes none falls back to the shared 2-arg Construct and
        // BootProfile's null-manager accept shortcut drives the phase.
        mpProfile->Construct(CgsIDCompress("BF_PROFILE"), &lStateMachine);
    }
    mpLoading->Construct(CgsIDCompress("BF_LOADING"), &lStateMachine);
    mpRaceMain->Construct(CgsIDCompress("RACE_MAIN"), &lStateMachine);
    mpFBurnMain->Construct(CgsIDCompress("FBURN_MAIN"), &lStateMachine);
    mpPaused->Construct(CgsIDCompress("PAUSED"), &lStateMachine);
    mpCrashed->Construct(CgsIDCompress("CRASHED"), &lStateMachine);
    mpCrashedStunt->Construct(CgsIDCompress("CRASHEDSTNT"), &lStateMachine);
    mpIdle->Construct(CgsIDCompress("IDLE"), &lStateMachine);
    mpPreRaceFlyBy->Construct(CgsIDCompress("PRE_FLY_BY"), &lStateMachine);

    // Gather into the table the state machine installs (BrnHudFlow.cpp:125 validates each slot).
    CgsGui::State* lapStates[KI_NUM_HUD_STATES];
    lapStates[0]  = mpPreload;
    lapStates[1]  = mpVideos;
    lapStates[2]  = mpLegal;
    lapStates[3]  = mpAttract;
    lapStates[4]  = mpPostTitleScreenLoad;
    lapStates[5]  = mpProfile;
    lapStates[6]  = mpLoading;
    lapStates[7]  = mpRaceMain;
    lapStates[8]  = mpFBurnMain;
    lapStates[9]  = mpPaused;
    lapStates[10] = mpCrashed;
    lapStates[11] = mpCrashedStunt;
    lapStates[12] = mpIdle;
    lapStates[13] = mpPreRaceFlyBy;

    for (s32 li = 0; li < KI_NUM_HUD_STATES; ++li)
        CGS_ASSERT(lapStates[li] != 0, "Invalid state pointer in state list");

    lStateMachine.SetStates(lapStates, KI_NUM_HUD_STATES);
    return true;
}

} // namespace BrnGui
