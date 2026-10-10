#include "GameSource/Gui/Flow/HUD/BrnHudFlow.h"

#include <new>   // placement new (carve states out of the linear allocator)

#include "GameShared/GameClasses/Core/CgsID.h"                         // CgsIDCompress
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"            // CgsMemory::LinearMalloc
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateMachine.h"// CgsGui::StateMachine
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"       // CgsGui::State
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // gpDebugPrint / gxMessageFilterFlags (PrintStateSizes)

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
// PRE_FLY_BY is DWARF-homed under Flow/PreEvent, not Flow/HUD: the HUD-side
// GameSource/Gui/Flow/HUD/States/BrnPreRaceFlyBy.{h,cpp} pre-wave fork (an empty-shell
// class + a ctor that memset spans the shell never declared) is RETIRED -- its three
// bodies moved to BrnPreRaceFlyBy_wJ_01.cpp. This header is the real 4160-byte console
// object (BrnHudFlow::Prepare's size-4160 slot), so NewPoolState<PreRaceFlyByState> now
// carves and constructs the full class rather than a State-sized shell.
#include "GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h"

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

// Dev dump: a rule, the flow name, a rule, one PrintSingleSize line per pool state in build order,
// a rule, the "TOTAL : " line, a rule. Each rule/title/total line is gated on message filter bit 0
// on its own; the per-state lines are gated inside PrintSingleSize. The sizes are the states'
// sizeof, as the console's are.
void BrnHudFlow::PrintStateSizes()
{
    const char* const KAC_RULE = "-------------------------------\n";
    s32 liTotal = 0;

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << KAC_RULE;
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "BrnHudFlow\n";
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << KAC_RULE;

    PrintSingleSize("BootPreload",          sizeof(BootPreload),          &liTotal);
    PrintSingleSize("BootVideos",           sizeof(BootVideos),           &liTotal);
    PrintSingleSize("BootLegal",            sizeof(BootLegal),            &liTotal);
    PrintSingleSize("BootAttract",          sizeof(BootAttract),          &liTotal);
    PrintSingleSize("PostTitleScreenLoad",  sizeof(PostTitleScreenLoad),  &liTotal);
    PrintSingleSize("BootProfile",          sizeof(BootProfile),          &liTotal);
    PrintSingleSize("BootLoading",          sizeof(BootLoading),          &liTotal);
    PrintSingleSize("RaceMainHudState",     sizeof(RaceMainHudState),     &liTotal);
    PrintSingleSize("FBurnMainHudState",    sizeof(FBurnMainHudState),    &liTotal);
    PrintSingleSize("PausedHudState",       sizeof(PausedHudState),       &liTotal);
    PrintSingleSize("CrashedHudState",      sizeof(CrashedHudState),      &liTotal);
    PrintSingleSize("CrashedStuntHudState", sizeof(CrashedStuntHudState), &liTotal);
    PrintSingleSize("IdleHudState",         sizeof(IdleHudState),         &liTotal);
    PrintSingleSize("PreRaceFlyByState",    sizeof(PreRaceFlyByState),    &liTotal);

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << KAC_RULE;
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "TOTAL : " << liTotal << "\n";
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << KAC_RULE;
}

// @ 0x8251A620 -- base prepare, then build + install the 14-state HUD pool.
bool BrnHudFlow::Prepare(CgsGui::GuiAccessPointers* lpAccessPointers,
                         rw::IResourceAllocator* lpAllocator,
                         CgsMemory::LinearMalloc* lpLinearMalloc,
                         ProfileManager* lpProfileManager)
{
    // Base flow prepare: stash access pointers/allocator + wire the state machine's StateInterface.
    BrnBaseFlow::Prepare(lpAccessPointers, lpAllocator);

    // The console's virtual call right after SetStateInterface is the PrintStateSizes dump (the
    // same slot BrnScreenFlow::Prepare and BrnOverlayFlow::Prepare dispatch at that point).
    PrintStateSizes();

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
