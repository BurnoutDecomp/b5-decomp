#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCallbackRender.h"  // the render + render-flag callback family
#include "SDKs/EATech/include/Apt/Apt.h"                                         // AptUserFunctions gAptFuncs (the host table)
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (the host-callback asserts)

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

        // ---- the non-render families this TU now homes (their hosts above) -------------------
        gAptFuncs.pfnMemFree               = &AptCallbackMemory::Free;
        gAptFuncs.pfnDebugAddSavedInput    = &AptCallbackDebug::AddSavedInput;
        gAptFuncs.pfnDebugSetScreenGrabPending = &AptCallbackDebug::SetScreenGrabPending;
        gAptFuncs.pfnGetBytesTotal         = &AptCallbackFile::GetBytesTotal;
        gAptFuncs.pfnGetBytesLoaded        = &AptCallbackFile::GetBytesLoaded;
        gAptFuncs.pfnSetExternVariable     = &AptCallbackVariable::SetExternVariable;
        gAptFuncs.pfnGetExternVariable     = &AptCallbackVariable::GetExternVariable;
        gAptFuncs.pfnSendVariables         = &AptCallbackDeprecated::SendVariables;
        gAptFuncs.pfnCommand               = &AptCallbackDeprecated::FsCommand;
        gAptFuncs.pfnLoadVariablesNULL     = &AptCallbackDeprecated::LoadVariablesNULL;
        gAptFuncs.pfnPointHitTest          = &AptCallbackDeprecated::PointHitTest;
        gAptFuncs.pfnGetRealTimeClock      = &AptCallbackDeprecated::GetRealTimeClock;

        // FLAG: the remaining gAptFuncs slots ConstructApt @0x5BA0F8 also installs --
        //   Memory      (pfnMemAlloc / pfnMemFreeSize)
        //   Debug       (pfnAssertFail / pfnDebugPrint)
        //   File        (pfnLoadAnimation / pfnFreeAnimation / pfnFreeConstantTable /
        //                pfnLoadAnimationCompleted / pfnOnUnload)
        //   Variable    (pfnLoadVariables)
        //   Custom      (pfnCustomControlRender / pfnCustomControlUpdate / the Zid family)
        //   Deprecated  (pfnUninitializedVarAccess / pfnCustomSavedInputHandler /
        //                pfnPlaySavedInputsDone / pfnHandleZombieState)
        // are NOT installed here: those hosts are not reconstructed yet (several depend on the
        // undeclared Apt C-API -- AptLoadAnimation / AptPartialGarbageCollection / ... -- or on
        // CgsGui::AptCommunicator, neither of which has a reconstructed home). They remain null
        // (the ctor's value-init); installing them is a follow-on once those families land.
    }

    // =========================================================================
    // The non-render Apt host callback families.
    //
    // AptCallbackMemory::Free is the one trivial REAL callback (forwards a free through the
    // singleton's embedded data-handler allocator). The remaining Debug / File / Variable /
    // Deprecated callbacks are the build's guarded not-yet-implemented entry points: each
    // fires the not-implemented assert and returns a null/zero result -- that assert-and-
    // return is the faithful body the X360 binary executes (NOT a reconstruction stub).
    // =========================================================================

    // ---- Memory -------------------------------------------------------------
    // X360 0x828492A8 (CgsGui::AptCallbackMemory::Free). Free an Apt allocation through the
    // singleton AptAux's embedded data-handler allocator. The guest loads off_8305A6C8
    // (mpAptAuxInst), adds 12 to reach &mAptDataHandler, asserts the singleton is live, then
    // calls AptDataHandler::AptFree(&mAptDataHandler, a1).
    void AptCallbackMemory::Free(void* lpBlock)
    {
        AptAux* lpAptAux = AptAuxPointer::mpAptAuxInst;
        CGS_ASSERT(lpAptAux != nullptr, "Invalid AptDataHandler in AptCallbackMemory::Free");
        lpAptAux->mAptDataHandler.AptFree(lpBlock);
    }

    // ---- Debug --------------------------------------------------------------
    // X360 0x82849528 (CgsGui::AptCallbackDebug::AddSavedInput). Guarded not-yet-implemented.
    void AptCallbackDebug::AddSavedInput(AptSavedInputRecord* /*lpRecord*/, s32 /*liCount*/)
    {
        CGS_ASSERT(false, "AptCallbackDebug::AddSavedInput() has not been implemented but is being used, please implement before utilising.");
    }

    // X360 0x82849568 (CgsGui::AptCallbackDebug::SetScreenGrabPending). Guarded not-yet-implemented.
    void AptCallbackDebug::SetScreenGrabPending(const char* /*lpacName*/)
    {
        CGS_ASSERT(false, "AptCallbackDebug::SetScreenGrabPending() has not been implemented but is being used, please implement before utilising.");
    }

    // ---- File ---------------------------------------------------------------
    // X360 0x828495E0 (CgsGui::AptCallbackFile::GetBytesTotal). Guarded not-yet-implemented.
    s32 AptCallbackFile::GetBytesTotal(const char* /*lpacFileName*/, AptGetBytesEnum /*leWhich*/)
    {
        CGS_ASSERT(false, "AptCallbackFile::GetBytesTotal() has not been implemented but is being used, please implement before utilising.");
        return 0;
    }

    // X360 0x82849620 (CgsGui::AptCallbackFile::GetBytesLoaded). Guarded not-yet-implemented.
    s32 AptCallbackFile::GetBytesLoaded(const char* /*lpacFileName*/, AptGetBytesEnum /*leWhich*/)
    {
        CGS_ASSERT(false, "AptCallbackFile::GetBytesLoaded() has not been implemented but is being used, please implement before utilising.");
        return 0;
    }

    // ---- Variable -----------------------------------------------------------
    // X360 0x82849660 (CgsGui::AptCallbackVariable::SetExternVariable). Guarded not-yet-implemented.
    void AptCallbackVariable::SetExternVariable(const char* /*lpacName*/, const char* /*lpacValue*/)
    {
        CGS_ASSERT(false, "AptCallbackVariable::SetExternVariable() has not been implemented but is being used, please implement before utilising.");
    }

    // X360 0x828496A0 (CgsGui::AptCallbackVariable::GetExternVariable). Guarded not-yet-implemented.
    AptValue* AptCallbackVariable::GetExternVariable(const char* /*lpacName*/)
    {
        CGS_ASSERT(false, "AptCallbackVariable::GetExternVariable() has not been implemented but is being used, please implement before utilising.");
        return nullptr;
    }

    // ---- Deprecated ---------------------------------------------------------
    // X360 0x828496E0 (CgsGui::AptCallbackDeprecated::SendVariables). Guarded not-yet-implemented.
    void AptCallbackDeprecated::SendVariables(const char* /*lpacUrl*/, const char* /*lpacTarget*/,
                                              const char* /*lpacVariables*/, const char* /*lpacMethod*/,
                                              s32 /*liFlags*/)
    {
        CGS_ASSERT(false, "SendVariables() has not been implemented but is being used, please implement before utilising.");
    }

    // X360 0x82849720 (CgsGui::AptCallbackDeprecated::FsCommand). Guarded not-yet-implemented.
    void AptCallbackDeprecated::FsCommand(const char* /*lpacCommand*/, const char* /*lpacArgs*/)
    {
        CGS_ASSERT(false, "Command() has not been implemented but is being used, please implement before utilising.");
    }

    // X360 0x828497A0 (CgsGui::AptCallbackDeprecated::LoadVariablesNULL). Guarded not-yet-implemented.
    AptValue* AptCallbackDeprecated::LoadVariablesNULL()
    {
        CGS_ASSERT(false, "LoadVariablesNULL() has not been implemented but is being used, please implement before utilising.");
        return nullptr;
    }

    // X360 0x828497E0 (CgsGui::AptCallbackDeprecated::PointHitTest). Guarded not-yet-implemented.
    s32 AptCallbackDeprecated::PointHitTest(f32 /*lfX*/, f32 /*lfY*/, AptAssetMoiveClip /*leClip*/)
    {
        CGS_ASSERT(false, "PointHitTest() has not been implemented but is being used, please implement before utilising.");
        return 0;
    }

    // X360 0x82849820 (CgsGui::AptCallbackDeprecated::GetRealTimeClock). Guarded not-yet-implemented.
    void AptCallbackDeprecated::GetRealTimeClock(AptSysClock* /*lpSysClock*/, bool /*lbUseUtc*/)
    {
        CGS_ASSERT(false, "GetRealTimeClock() has not been implemented but is being used, please implement before utilising.");
    }

    // The language-manager accessor CgsAptString::Prepare (CgsAptString.cpp) reaches the manager
    // through. The console Prepare loads the AptAux singleton (off_8305A6C8 == AptAuxPointer::
    // mpAptAuxInst), adds 0x420 to reach the embedded render handler, and reads mpLanguageManager
    // at +99776. This TU owns the full AptAux / AptRenderHandler layout, so it can return the
    // manager by name; the real-CgsAptString TU cannot include those types (the opaque pool
    // CgsAptString stand-in clashes), hence this out-of-line bridge. Asserts mirror the console's.
    CgsLanguage::LanguageManager* GetAptRenderHandlerLanguageManager()
    {
        AptAux* lpAptAux = AptAuxPointer::mpAptAuxInst;
        CGS_ASSERT(lpAptAux != 0, "Invalid Apt Aux instance in AptCallbackRender::AllocateString");
        AptRenderHandler* lpRenderHandler = &lpAptAux->mRenderHandler;
        CGS_ASSERT(lpRenderHandler != 0, "Invalid render handler instance in AptCallbackRender::AllocateString");
        CgsLanguage::LanguageManager* lpLanguage = lpRenderHandler->GetLanguageManager();
        CGS_ASSERT(lpLanguage != 0, "Invalid language manager in CgsAptString::Prepare");
        return lpLanguage;
    }
}
