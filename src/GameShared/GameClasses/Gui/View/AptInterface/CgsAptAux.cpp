#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCallbackRender.h"  // the render + render-flag callback family

// =============================================================================
// CgsGui::AptAux / CgsGui::AptAuxPointer.
//
// AptAuxPointer::mpAptAuxInst is the singleton handle EVERY render callback resolves through.
// AptAux::ConstructApt @0x5BA0F8 installs the host callback table (gAptFuncs): it points each
// memory / debug / file / variable / render / deprecated slot at the matching CgsGui::AptCallback*
// free function. This TU reconstructs the RENDER + RENDER-FLAG slot installs (the family this goal
// reconstructs); the remaining families (Memory / Debug / File / Variable / Custom / Deprecated)
// are installed by the same ConstructApt body but are FLAG'd below -- their callback functions are
// not reconstructed yet, and the concrete gAptFuncs layout they index into is owned by the EATech
// Apt user-function-table (a follow-on once those families land).
// =============================================================================

namespace CgsGui
{
    // The singleton AptAux* the render callbacks load (guest _ZN6CgsGui13AptAuxPointer12mpAptAuxInstE).
    // AptAux::Construct @0x5C4B6C publishes the constructed instance here. Starts null.
    AptAux* AptAuxPointer::mpAptAuxInst = nullptr;

    // The render-family slot of the Apt host user-function table. The full gAptFuncs table (40
    // slots across six callback families) is the EATech Apt user-function-table; only the render +
    // render-flag function pointers ConstructApt installs are in scope here, modelled as a named
    // sub-table so the installs are real, typed assignments (not raw dword stores into an
    // unmodelled layout). The non-render families' slots are FLAG'd in ConstructApt below.
    struct AptRenderFuncSlots
    {
        // matches the ConstructApt store order for the render slots (dword_1059C658..0x700).
        void (*pfnSetBackgroundColour)(u32);                                          // dword_1059C658
        AptAssetString (*pfnAllocateString)(AptAllocateStringParameters*);            // dword_1059C690
        void (*pfnDeallocateString)(AptAssetString, u32);                             // dword_1059C694
        void (*pfnDrawString)(AptAssetString, AptMaskRenderOperation, s32);           // dword_1059C698
        AptAssetTexture (*pfnLoadTexture)(AptAnimationUserData, s32);                 // dword_1059C69C
        void (*pfnFreeTexture)(AptAssetTexture);                                      // dword_1059C6A0
        void (*pfnBindTexture)(AptAnimationUserData, s32, AptAssetTexture);           // dword_1059C6A4
        AptAssetRenderingUnit (*pfnLoadRenderingUnit)(AptAnimationUserData, s32);     // dword_1059C6A8
        void (*pfnFreeRenderingUnit)(AptAssetRenderingUnit);                          // dword_1059C6AC
        void (*pfnSetVertexMatrix)(AptMatrix*);                                       // dword_1059C6B0
        void (*pfnSetColourTransform)(AptCXForm*);                                    // dword_1059C6B4
        void (*pfnDrawRenderingUnit)(AptAssetRenderingUnit, AptMaskRenderOperation, s32); // dword_1059C6B8
        f32  (*pfnGetStageHeight)();                                                  // dword_1059C6E8
        f32  (*pfnGetStageWidth)();                                                   // dword_1059C6EC
        void (*pfnRenderFlagsPush)(const char*);                                      // dword_1059C6FC
        void (*pfnRenderFlagsPop)(const char*);                                       // dword_1059C700
    };

    // The installed render slots (the part of gAptFuncs ConstructApt fills for this family).
    static AptRenderFuncSlots gAptRenderFuncs;

    // -------------------------------------------------------------------------
    // AptAux::ConstructApt - PS3/X360 0x5BA0F8. Install the host callback table.
    // -------------------------------------------------------------------------
    void AptAux::ConstructApt()
    {
        // ---- the render + render-flag family (the slots this goal reconstructs) -------------
        gAptRenderFuncs.pfnSetBackgroundColour = &AptCallbackRender::SetBackgroundColour;
        gAptRenderFuncs.pfnAllocateString      = &AptCallbackRender::AllocateString;
        gAptRenderFuncs.pfnDeallocateString    = &AptCallbackRender::DeallocateString;
        gAptRenderFuncs.pfnDrawString          = &AptCallbackRender::DrawString;
        gAptRenderFuncs.pfnLoadTexture         = &AptCallbackRender::LoadTexture;
        gAptRenderFuncs.pfnFreeTexture         = &AptCallbackRender::FreeTexture;
        gAptRenderFuncs.pfnBindTexture         = &AptCallbackRender::BindTexture;
        gAptRenderFuncs.pfnLoadRenderingUnit   = &AptCallbackRender::LoadRenderingUnit;
        gAptRenderFuncs.pfnFreeRenderingUnit   = &AptCallbackRender::FreeRenderingUnit;
        gAptRenderFuncs.pfnSetVertexMatrix     = &AptCallbackRender::SetVertexMatrix;
        gAptRenderFuncs.pfnSetColourTransform  = &AptCallbackRender::SetColourTransform;
        gAptRenderFuncs.pfnDrawRenderingUnit   = &AptCallbackRender::DrawRenderingUnit;
        gAptRenderFuncs.pfnGetStageHeight      = &AptCallbackRender::GetStageHeight;
        gAptRenderFuncs.pfnGetStageWidth       = &AptCallbackRender::GetStageWidth;
        gAptRenderFuncs.pfnRenderFlagsPush      = &AptCallbackRenderFlags::Push;
        gAptRenderFuncs.pfnRenderFlagsPop       = &AptCallbackRenderFlags::Pop;

        // FLAG: the remaining gAptFuncs slots ConstructApt @0x5BA0F8 also installs --
        //   Memory      (Alloc / Free / Free(sized))                       dword_1059C648..0x650
        //   Debug       (AssertFail / Print / AddSavedInput / SetScreenGrabPending)  0x654..0x664
        //   File        (LoadAnimation / FreeConstantTable / LoadAnimationCompleted /
        //                FreeAnimation / OnUnload / GetBytesTotal / GetBytesLoaded)  0x668..0x6E0
        //   Variable    (Set/GetExternVariable)                            0x684 / 0x688
        //   Custom      (ControlRender / ControlUpdate)                    0x6BC / 0x6C0
        //   Deprecated  (UninitializedVarAccess / FsCommand / LoadVariables /
        //                LoadVariablesNULL / SendVariables / PointHitTest / GetRealTimeClock)
        // are NOT installed here: those CgsGui::AptCallback* families are not reconstructed yet,
        // and the concrete EATech gAptFuncs table layout the slots index is owned by the Apt
        // user-function table. Wiring them up is a follow-on once those families land; the render
        // family above is the complete, installable bridge to AptRenderHandler::Render.
    }
}
