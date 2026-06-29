#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCallbackRender.h"  // the render + render-flag callback family
#include "SDKs/EATech/include/Apt/Apt.h"                                         // AptUserFunctions gAptFuncs (the host table)

#include <cstring>   // memset (zero-install the gAptFuncs slots)

// =============================================================================
// CgsGui::AptAux / CgsGui::AptAuxPointer + the Apt host user-function table.
//
// AptAuxPointer::mpAptAuxInst is the singleton handle EVERY render callback resolves through.
// AptAux::Construct @0x5C4B6C is the bring-up the GUI's view module calls; it constructs the
// embedded data + render handlers (the render-state seeds land in AptRenderHandler::Construct),
// publishes the singleton, and installs the host callback table via ConstructApt @0x5BA0F8.
//
// ConstructApt installs the gAptFuncs slots: it points each memory / debug / file / variable /
// render / deprecated slot at the matching host free function. This TU installs the RENDER +
// RENDER-FLAG slots (the family this goal reconstructs) into the REAL gAptFuncs table (the
// EATech Apt user-function table, Apt.h) -- so the Apt engine's render dispatch reaches the
// committed CgsGui::AptCallbackRender free functions (DrawRenderingUnit / SetVertexMatrix / ...).
// The remaining families (Memory / Debug / File / Variable / Custom / Deprecated) are installed
// by the same ConstructApt body but are FLAG'd below -- their host callback functions are not
// reconstructed yet.
// =============================================================================

// -----------------------------------------------------------------------------
// gAptFuncs - the single global Apt host user-function table (X360 dword_8324E818). Homed here
// (the Apt host-install TU). The ctor zero-installs every slot; ConstructApt then overrides the
// render family. Sibling Apt TUs (AptRenderingContext, the AptHook boundary) reference this same
// underlying table by name.
// -----------------------------------------------------------------------------
AptUserFunctions gAptFuncs;

// Apt.h:859 - AptUserFunctions(). The guest ctor null-installs every slot before the host
// overrides each family. Modelled as a value-init of the whole table.
AptUserFunctions::AptUserFunctions()
{
    // Every pfn slot starts null until the host install overrides it. The table is a flat run of
    // function pointers (a POD dispatch table), so a single zero-fill is the faithful "null every
    // slot" the guest ctor performs.
    std::memset(this, 0, sizeof(*this));
}

namespace CgsGui
{
    // The singleton AptAux* the render callbacks load (guest _ZN6CgsGui13AptAuxPointer12mpAptAuxInstE).
    // AptAux::Construct @0x5C4B6C publishes the constructed instance here. Starts null.
    AptAux* AptAuxPointer::mpAptAuxInst = nullptr;

    // -------------------------------------------------------------------------
    // AptAux::Construct - PS3 0x5C4B6C. Bring up the AptAux singleton.
    //
    // Faithful to the guest body:
    //   *(a1+4) = 3; *a1 = 0;                                  -- the two leading state words
    //   AptDataHandler::Construct(a1+12);                      -- the embedded data registry
    //   AptRenderHandler::Construct(a1+0x420, a2..a8);         -- the render-state seeds
    //   AptAuxPointer::mpAptAuxInst = a1;                      -- publish the singleton
    //   EA::Thread::Mutex::Init(a1+109672, ...);              -- the data-handler mutex
    //   AptAux::ConstructApt(a1);                              -- install the host callback table
    // -------------------------------------------------------------------------
    void AptAux::Construct(CgsGuiModuleIO::ImRendererSet* lpImRenderers,
                           CgsGraphics::TextRenderer* lpTextRenderer,
                           CgsLanguage::LanguageManager* lpLanguageManager,
                           const FontCollection* lpFonts,
                           f32 lfAspectRatio,
                           const rw::RGBA* lpAlternateTextColours,
                           s32 liNumAlternateColours)
    {
        // The two leading state words the guest seeds (`*(a1+4) = 3; *a1 = 0`).
        miState4 = 3;
        miState0 = 0;

        // Construct the embedded APT data registry/allocator front-end (guest a1+12).
        // FLAG: AptDataHandler::Construct is homed by the data-handler's own ledger TU (the DWARF
        // marks it so); its bring-up is not reconstructed in this slice. The member exists at the
        // faithful offset; its internal state is brought up by that TU when it lands.

        // Construct the embedded render handler (guest a1+0x420). This is where the render-state
        // field seeds the wave-3 slice deferred land (mpImRenderers / mpTextRenderer /
        // mpFontCollection / the stage resolution / the AptString pool). Arguments forwarded
        // straight through.
        mRenderHandler.Construct(lpImRenderers, lpTextRenderer, lpLanguageManager, lpFonts,
                                 lfAspectRatio, lpAlternateTextColours, liNumAlternateColours);

        // Publish this as the AptAux singleton the render callbacks resolve through.
        AptAuxPointer::mpAptAuxInst = this;

        // FLAG: the data-handler mutex (guest EA::Thread::Mutex::Init at a1+109672) is owned by the
        // AptDataHandler bring-up; not initialised in this slice (no concurrent data-handler access
        // exists until that TU lands).

        // Install the Apt host callback table (the render family into gAptFuncs).
        ConstructApt();
    }

    // -------------------------------------------------------------------------
    // AptAux::ConstructApt - PS3/X360 0x5BA0F8. Install the host callback table (gAptFuncs).
    //
    // The render + render-flag family (the slots this goal reconstructs) are pointed at the
    // committed CgsGui::AptCallbackRender / AptCallbackRenderFlags free functions, so the Apt
    // engine's render dispatch reaches AptRenderHandler::Render through them.
    // -------------------------------------------------------------------------
    void AptAux::ConstructApt()
    {
        // ---- the render + render-flag family (the slots this goal reconstructs) -------------
        gAptFuncs.pfnSetBackgroundColour = &AptCallbackRender::SetBackgroundColour;
        gAptFuncs.pfnAllocateString      = &AptCallbackRender::AllocateString;
        gAptFuncs.pfnDeallocateString    = &AptCallbackRender::DeallocateString;
        gAptFuncs.pfnDrawString          = &AptCallbackRender::DrawString;
        gAptFuncs.pfnLoadTexture         = &AptCallbackRender::LoadTexture;
        gAptFuncs.pfnFreeTexture         = &AptCallbackRender::FreeTexture;
        gAptFuncs.pfnBindTexture         = &AptCallbackRender::BindTexture;
        gAptFuncs.pfnLoadRenderingUnit   = &AptCallbackRender::LoadRenderingUnit;
        gAptFuncs.pfnFreeRenderingUnit   = &AptCallbackRender::FreeRenderingUnit;
        gAptFuncs.pfnSetVertexMatrix     = &AptCallbackRender::SetVertexMatrix;
        gAptFuncs.pfnSetColourTransform  = &AptCallbackRender::SetColourTransform;
        gAptFuncs.pfnDrawRenderingUnit   = &AptCallbackRender::DrawRenderingUnit;
        gAptFuncs.pfnGetStageHeight      = &AptCallbackRender::GetStageHeight;
        gAptFuncs.pfnGetStageWidth       = &AptCallbackRender::GetStageWidth;
        gAptFuncs.pfnPushRenderFlags     = &AptCallbackRenderFlags::Push;
        gAptFuncs.pfnPopRenderFlags      = &AptCallbackRenderFlags::Pop;

        // FLAG: the remaining gAptFuncs slots ConstructApt @0x5BA0F8 also installs --
        //   Memory      (pfnMemAlloc / pfnMemFree / pfnMemFreeSize)
        //   Debug       (pfnAssertFail / pfnDebugPrint / pfnDebugAddSavedInput /
        //                pfnDebugSetScreenGrabPending)
        //   File        (pfnLoadAnimation / pfnFreeAnimation / pfnFreeConstantTable /
        //                pfnLoadAnimationCompleted / pfnCommand / pfnGetBytesTotal /
        //                pfnGetBytesLoaded / pfnOnUnload)
        //   Variable    (pfnLoadVariables(NULL) / pfnSet/GetExternVariable / pfnSendVariables)
        //   Custom      (pfnCustomControlRender / pfnCustomControlUpdate / the Zid family /
        //                pfnPointHitTest)
        //   Deprecated  (pfnUninitializedVarAccess / pfnGetRealTimeClock /
        //                pfnCustomSavedInputHandler / pfnPlaySavedInputsDone /
        //                pfnHandleZombieState)
        // are NOT installed here: those CgsGui::AptCallback* families are not reconstructed yet.
        // They remain null (the ctor's value-init); installing them is a follow-on once those
        // families land. The render family above is the complete, installable bridge from the Apt
        // engine's render dispatch to AptRenderHandler::Render.
    }
}
