#include "GameSource/Gui/Flow/Overlay/BrnOverlayFlow.h"

#include <new>   // placement new (the pool states are carved from the linear allocator)

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                     // CgsIDCompress
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"         // CgsMemory::LinearMalloc
#include "GameSource/Gui/Flow/Overlay/States/BrnPreloadOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnInvisibleOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnCrashNavWaitOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnCrashNavOkOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnCrashNavOkCancelOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnCrashNavOnlineWaitOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnCrashNavOnlineOkOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnCrashNavOnlineOkCancelOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnInGameWaitOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnInGameOkOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnInGameOkCancelOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnInGameOnlineWaitOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnInGameOnlineOkOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnInGameOnlineOkCancelOverlayState.h"
#include "GameSource/Gui/Flow/Overlay/States/BrnInGameOnlineEnterFreeBurnOverlayState.h"

// BrnGui::BrnOverlayFlow::Prepare -- reconstructed from BURNOUT_X360_ARTIST.XEX
// @0x82515430 (this TU's single ledger function; the BrnHudFlow::Prepare idiom).
//
// The X360 body: EventObserver::Prepare + StateMachine::SetStateInterface (the inlined
// BrnBaseFlow::Prepare, de-inlined back to the base call here), the PrintStateSizes
// virtual dispatch (retail-stripped prints), then fifteen LinearMalloc carve-outs --
// 64B Preload, 80B Invisible, and thirteen 328-byte popup states (0x148 == exactly the
// X360 sizeof(BaseOverlayState), independently confirming the reconstructed layout) --
// each placement-constructed (the X360 inlined ctors are pure vtable stores: the state's
// own vptr plus the embedded FlaptIconComponent's @+0x50, which the C++ default ctors
// reproduce) and asserted, then Construct(CgsIDCompress(script id), &state machine) on
// each and one SetStates(..., 15).

namespace BrnGui
{
namespace
{
    // Carve one state out of the flow's linear allocator and default-construct it in
    // place. Each X360 carve is followed by the same "Linear allocator ran out of room"
    // assert (cpp:67..96; folded static per convention) -- non-gating, the null is
    // still stored and used.
    template <typename T>
    T* NewPoolState(CgsMemory::LinearMalloc* lpLinearMalloc)
    {
        void* lpMem = lpLinearMalloc->Malloc(sizeof(T));
        T* lpState = lpMem ? new (lpMem) T() : 0;
        CGS_ASSERT(lpState != 0, "Linear allocator ran out of room");
        return lpState;
    }
}

// @ 0x82515430
bool BrnOverlayFlow::Prepare(CgsGui::GuiAccessPointers* lpAccessPointers,
                             rw::IResourceAllocator* lpAllocator,
                             CgsMemory::LinearMalloc* lpLinearMalloc)
{
    // Base flow prepare (the X360 inlines it: EventObserver::Prepare + the state
    // machine's SetStateInterface), then the state-size debug dump (empty in retail).
    BrnBaseFlow::Prepare(lpAccessPointers, lpAllocator);
    PrintStateSizes();

    CgsGui::StateMachine& lStateMachine = GetStateMachine();

    // Allocate the 15 popup states (X360 build order = the SetStates table order).
    mpPreloadOverlayState                   = NewPoolState<PreloadOverlayState>(lpLinearMalloc);
    mpInvisibleOverlayState                 = NewPoolState<InvisibleOverlayState>(lpLinearMalloc);
    mpCrashNavWaitOverlayState              = NewPoolState<CrashNavWaitOverlayState>(lpLinearMalloc);
    mpCrashNavOkOverlayState                = NewPoolState<CrashNavOkOverlayState>(lpLinearMalloc);
    mpCrashNavOkCancelOverlayState          = NewPoolState<CrashNavOkCancelOverlayState>(lpLinearMalloc);
    mpCrashNavOnlineWaitOverlayState        = NewPoolState<CrashNavOnlineWaitOverlayState>(lpLinearMalloc);
    mpCrashNavOnlineOkOverlayState          = NewPoolState<CrashNavOnlineOkOverlayState>(lpLinearMalloc);
    mpCrashNavOnlineOkCancelOverlayState    = NewPoolState<CrashNavOnlineOkCancelOverlayState>(lpLinearMalloc);
    mpInGameWaitOverlayState                = NewPoolState<InGameWaitOverlayState>(lpLinearMalloc);
    mpInGameOkOverlayState                  = NewPoolState<InGameOkOverlayState>(lpLinearMalloc);
    mpInGameOkCancelOverlayState            = NewPoolState<InGameOkCancelOverlayState>(lpLinearMalloc);
    mpInGameOnlineWaitOverlayState          = NewPoolState<InGameOnlineWaitOverlayState>(lpLinearMalloc);
    mpInGameOnlineOkOverlayState            = NewPoolState<InGameOnlineOkOverlayState>(lpLinearMalloc);
    mpInGameOnlineOkCancelOverlayState      = NewPoolState<InGameOnlineOkCancelOverlayState>(lpLinearMalloc);
    mpInGameOnlineEnterFreeBurnOverlayState = NewPoolState<InGameOnlineEnterFreeBurnOverlayState>(lpLinearMalloc);

    // Construct each state with its script-id name + the owning state machine
    // (the X360 dispatches ScriptedState::Construct virtually, vtbl +0x18).
    mpPreloadOverlayState->Construct(CgsIDCompress("PRELOAD"), &lStateMachine);
    mpInvisibleOverlayState->Construct(CgsIDCompress("INVISIBLE"), &lStateMachine);
    mpCrashNavWaitOverlayState->Construct(CgsIDCompress("CN_WAIT"), &lStateMachine);
    mpCrashNavOkOverlayState->Construct(CgsIDCompress("CN_OK"), &lStateMachine);
    mpCrashNavOkCancelOverlayState->Construct(CgsIDCompress("CN_OKCANCEL"), &lStateMachine);
    mpCrashNavOnlineWaitOverlayState->Construct(CgsIDCompress("CNO_WAIT"), &lStateMachine);
    mpCrashNavOnlineOkOverlayState->Construct(CgsIDCompress("CNO_OK"), &lStateMachine);
    mpCrashNavOnlineOkCancelOverlayState->Construct(CgsIDCompress("CNO_OKCANCEL"), &lStateMachine);
    mpInGameWaitOverlayState->Construct(CgsIDCompress("IG_WAIT"), &lStateMachine);
    mpInGameOkOverlayState->Construct(CgsIDCompress("IG_OK"), &lStateMachine);
    mpInGameOkCancelOverlayState->Construct(CgsIDCompress("IG_OKCANCEL"), &lStateMachine);
    mpInGameOnlineWaitOverlayState->Construct(CgsIDCompress("IGO_WAIT"), &lStateMachine);
    mpInGameOnlineOkOverlayState->Construct(CgsIDCompress("IGO_OK"), &lStateMachine);
    mpInGameOnlineOkCancelOverlayState->Construct(CgsIDCompress("IGO_OKCANCEL"), &lStateMachine);
    mpInGameOnlineEnterFreeBurnOverlayState->Construct(CgsIDCompress("IGO_ENTER_ON"), &lStateMachine);

    // Gather into the table the state machine installs.
    CgsGui::State* lapStates[KI_NUM_OVERLAY_STATES];
    lapStates[0]  = mpPreloadOverlayState;
    lapStates[1]  = mpInvisibleOverlayState;
    lapStates[2]  = mpCrashNavWaitOverlayState;
    lapStates[3]  = mpCrashNavOkOverlayState;
    lapStates[4]  = mpCrashNavOkCancelOverlayState;
    lapStates[5]  = mpCrashNavOnlineWaitOverlayState;
    lapStates[6]  = mpCrashNavOnlineOkOverlayState;
    lapStates[7]  = mpCrashNavOnlineOkCancelOverlayState;
    lapStates[8]  = mpInGameWaitOverlayState;
    lapStates[9]  = mpInGameOkOverlayState;
    lapStates[10] = mpInGameOkCancelOverlayState;
    lapStates[11] = mpInGameOnlineWaitOverlayState;
    lapStates[12] = mpInGameOnlineOkOverlayState;
    lapStates[13] = mpInGameOnlineOkCancelOverlayState;
    lapStates[14] = mpInGameOnlineEnterFreeBurnOverlayState;

    lStateMachine.SetStates(lapStates, KI_NUM_OVERLAY_STATES);
    return true;
}
}
