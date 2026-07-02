#pragma once

#include "types.hpp"
#include "GameSource/Gui/Flow/BrnBaseFlow.h"   // BrnGui::BrnBaseFlow (base)

namespace CgsGui    { struct GuiAccessPointers; struct State; }
namespace CgsMemory { class  LinearMalloc; }
namespace rw        { struct IResourceAllocator; }

// BrnGui::BrnOverlayFlow - the popup-overlay GUI flow (E_GUIFLOW_OVERLAY). A BrnBaseFlow
// that owns the pool of 15 popup states -- the preload/invisible pair plus one state per
// CgsGui::PopupStyle family (CrashNav / CrashNavOnline / InGame / InGameOnline x
// Wait / Ok / OkCancel, and the online enter-freeburn wait) -- and installs them into its
// embedded CgsGui::StateMachine under their script ids. The overlays director + the FSM
// Lua scripts then sequence which popup state runs. DWARF home BrnOverlayFlow.h;
// reconstructed from BURNOUT_X360_ARTIST.XEX (Prepare @0x82515430; the state pointers are
// the X360 fields flow+0x1024C..+0x10284 in the Prepare build order -- on x64 the offsets
// differ, the member order is faithful).
namespace BrnGui
{
    struct PreloadOverlayState;
    struct InvisibleOverlayState;
    struct CrashNavWaitOverlayState;
    struct CrashNavOkOverlayState;
    struct CrashNavOkCancelOverlayState;
    struct CrashNavOnlineWaitOverlayState;
    struct CrashNavOnlineOkOverlayState;
    struct CrashNavOnlineOkCancelOverlayState;
    struct InGameWaitOverlayState;
    struct InGameOkOverlayState;
    struct InGameOkCancelOverlayState;
    struct InGameOnlineWaitOverlayState;
    struct InGameOnlineOkOverlayState;
    struct InGameOnlineOkCancelOverlayState;
    struct InGameOnlineEnterFreeBurnOverlayState;

    struct BrnOverlayFlow : public BrnBaseFlow
    {
        static const s32 KI_NUM_OVERLAY_STATES = 15;   // Prepare's SetStates(..., 15)

        // @0x82515430 (this TU, DWARF h) -- base-prepare, then build + install the
        // 15-popup-state pool. The wider overload (adds the linear allocator the states
        // are carved from); distinct vtable slot from BrnBaseFlow::Prepare(access,
        // allocator).
        bool Prepare(CgsGui::GuiAccessPointers* lpAccessPointers,
                     rw::IResourceAllocator* lpAllocator,
                     CgsMemory::LinearMalloc* lpLinearMalloc);

    private:
        // DWARF (declared with an inline body that is only stripped debug prints in the
        // retail X360 build); Prepare virtually dispatches it right after the base
        // prepare (@0x82515464, vtbl +0x18).
        virtual void PrintStateSizes() {}

        // The 15-state pool, in the Prepare build order (X360 flow+0x1024C..+0x10284).
        PreloadOverlayState*                    mpPreloadOverlayState;                    // PRELOAD
        InvisibleOverlayState*                  mpInvisibleOverlayState;                  // INVISIBLE
        CrashNavWaitOverlayState*               mpCrashNavWaitOverlayState;               // CN_WAIT
        CrashNavOkOverlayState*                 mpCrashNavOkOverlayState;                 // CN_OK
        CrashNavOkCancelOverlayState*           mpCrashNavOkCancelOverlayState;           // CN_OKCANCEL
        CrashNavOnlineWaitOverlayState*         mpCrashNavOnlineWaitOverlayState;         // CNO_WAIT
        CrashNavOnlineOkOverlayState*           mpCrashNavOnlineOkOverlayState;           // CNO_OK
        CrashNavOnlineOkCancelOverlayState*     mpCrashNavOnlineOkCancelOverlayState;     // CNO_OKCANCEL
        InGameWaitOverlayState*                 mpInGameWaitOverlayState;                 // IG_WAIT
        InGameOkOverlayState*                   mpInGameOkOverlayState;                   // IG_OK
        InGameOkCancelOverlayState*             mpInGameOkCancelOverlayState;             // IG_OKCANCEL
        InGameOnlineWaitOverlayState*           mpInGameOnlineWaitOverlayState;           // IGO_WAIT
        InGameOnlineOkOverlayState*             mpInGameOnlineOkOverlayState;             // IGO_OK
        InGameOnlineOkCancelOverlayState*       mpInGameOnlineOkCancelOverlayState;       // IGO_OKCANCEL
        InGameOnlineEnterFreeBurnOverlayState*  mpInGameOnlineEnterFreeBurnOverlayState;  // IGO_ENTER_ON
    };
}
