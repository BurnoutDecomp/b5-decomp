#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHandler.h"    // CgsGui::AptDataHandler (mAptDataHandler @ +12)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"  // CgsGui::AptRenderHandler (mRenderHandler)
#include "rw/rwcore_structs.h"                                                 // rw::RGBA (Construct alt-colour table)

// Apt SDK opaque/value types the host callback families' signatures reference (so the
// declarations below match the gAptFuncs slot prototypes in Apt.h exactly). Apt.h forward-
// declares AptValue / AptSavedInputRecord / AptSysClock and aliases AptGetBytesEnum / the
// movie-clip handle, which is all these callback signatures need (none deref them here).
#include "SDKs/EATech/include/Apt/Apt.h"

// Forward declarations for AptAux::Construct's collaborators (passed straight through to
// AptRenderHandler::Construct; only pointers are held here).
namespace CgsGraphics    { struct TextRenderer; }
namespace CgsLanguage    { class LanguageManager; }
namespace CgsGui         { struct FontCollection; }
namespace CgsGuiModuleIO { struct ImRendererSet; }   // the active 2D/3D renderer set

// =============================================================================
// CgsGui::AptAux / CgsGui::AptAuxPointer
//
// AptAux is the GUI's per-process Apt (Adobe-Flash-player) host adaptor: it owns
// the render handler the Apt engine's render callbacks draw through, installs the
// host callback table (ConstructApt @0x5BA0F8 -> gAptFuncs), and is reached from
// the render callbacks as a SINGLETON through CgsGui::AptAuxPointer::mpAptAuxInst.
//
// The 16 CgsGui::AptCallbackRender host callbacks (CgsAptCallbackRender.cpp) are the
// free functions the Apt user-function table points at; EVERY one of them dereferences
// CgsGui::AptAuxPointer::mpAptAuxInst->mRenderHandler -- so AptAux's only field in
// scope for the callback family is mRenderHandler, at the guest byte offset 0x420 the
// callbacks address it through (e.g. DrawRenderingUnit @0x5CBA30:
// `r9 = mpAptAuxInst; addi r9, r9, 0x420` == &mRenderHandler).
//
// Recovered from the PS3 External ELF (the callbacks + ConstructApt @0x5BA0F8) and
// AptAux::Construct @0x5C4B6C. LAYOUT NOTE: the real AptAux is large (mRenderHandler
// alone is ~108 KB and sits at guest +0x420 behind the apt-engine bookkeeping AptAux::
// Construct sets up). This slice models ONLY mRenderHandler -- the one member the
// render-callback family touches -- by name; the preceding apt-engine fields are
// out of scope (homed when AptAux::Construct is reconstructed). The gate compiles for a
// 64-bit host, so the guest's +0x420 offset is NOT load-bearing; the member reproduces
// the same observable state the callbacks read/write. FLAG: partial slice -- AptAux's
// apt-engine fields ahead of mRenderHandler are intentionally omitted.
// =============================================================================

namespace CgsGui
{
    class AptAux
    {
    public:
        // PS3 0x5C4B6C (CgsGui::AptAux::Construct). The singleton bring-up the GUI's view
        // module calls (ViewModule::Construct @0x5C4C84 is its only xref): seed the two
        // leading state words, construct the embedded AptDataHandler (+12), construct the
        // embedded render handler (+0x420) -- which is where the render-state seeds land
        // (mpImRenderers / mpTextRenderer / mpFontCollection / the stage resolution from the
        // aspect ratio / the alt-colour AptString pool) -- publish this as the AptAux
        // singleton (AptAuxPointer::mpAptAuxInst), init the data-handler mutex, and install
        // the host callback table (ConstructApt). The render-state field seeding the
        // wave-3 slice deferred is performed by AptRenderHandler::Construct (see below); this
        // forwards its arguments straight through.
        //
        // Arguments mirror the guest prototype exactly:
        //   lpImRenderers         the active ImRendererSet (the 2D/3D renderer set)
        //   lpTextRenderer        the glyph batcher the text path drives
        //   lpLanguageManager     the language manager (held by the render handler)
        //   lpFonts               the font collection AllocateString lays text out against
        //   lfAspectRatio         the display aspect ratio folded into the stage resolution
        //   lpAlternateTextColours the alt-text-colour table the AptString pool is built with
        //   liNumAlternateColours  its entry count
        void Construct(CgsGuiModuleIO::ImRendererSet* lpImRenderers,
                       CgsGraphics::TextRenderer* lpTextRenderer,
                       CgsLanguage::LanguageManager* lpLanguageManager,
                       const FontCollection* lpFonts,
                       f32 lfAspectRatio,
                       const rw::RGBA* lpAlternateTextColours,
                       s32 liNumAlternateColours);

        // X360/PS3 0x5BA0F8 (CgsGui::AptAux::ConstructApt) -- install the Apt host
        // user-function table (gAptFuncs): point every memory / debug / file / variable /
        // render / deprecated callback slot at the matching CgsGui::AptCallback* free
        // function. Reconstructed in CgsAptAux.cpp from the 40-store ConstructApt body; the
        // render-slot installs reference the CgsAptCallbackRender.cpp family this TU defines.
        void ConstructApt();

        // ---- AptAux head (guest offsets recovered from AptAux::Construct @0x5C4B6C) -------
        // The two leading state words AptAux::Construct seeds (`*a1 = 0; *(a1+4) = 3`). Their
        // meaning is owned by the apt-engine bookkeeping; only that they are set is in scope.
        s32 miState0;   // [guest +0x00] set to 0
        s32 miState4;   // [guest +0x04] set to 3

        // The embedded APT data registry/allocator front-end (Construct builds it). [guest +12]
        AptDataHandler mAptDataHandler;

        // The Apt render bridge the callback family reaches through mpAptAuxInst->mRenderHandler.
        // (Guest byte offset +0x420 == +1056; AptAux::Construct constructs it at a1+1056. The
        // x64 host offset differs because mAptDataHandler widens; the member is addressed by
        // name, so the guest offset is not load-bearing.) [guest +0x420]
        AptRenderHandler mRenderHandler;
    };

    // The AptAux singleton handle the render callbacks resolve through. The guest holds a
    // single static AptAux* (the symbol _ZN6CgsGui13AptAuxPointer12mpAptAuxInstE); the
    // callbacks load it then add 0x420 to reach mRenderHandler. Modelled as a class with a
    // static pointer member so the call sites read exactly
    // `CgsGui::AptAuxPointer::mpAptAuxInst->mRenderHandler`.
    class AptAuxPointer
    {
    public:
        static AptAux* mpAptAuxInst;
    };

    // =========================================================================
    // The Apt host callback FAMILIES (free functions in per-family namespaces).
    //
    // These are the host functions CgsGui::AptAux::ConstructApt @0x5BA0F8 installs into
    // the gAptFuncs user-function table (Apt.h) so the Apt engine can reach the host. The
    // render + render-flag families live in CgsAptCallbackRender.h; declared here are the
    // NON-render families this TU homes (and the one trivial real callback,
    // AptCallbackMemory::Free, which forwards to the singleton's data-handler allocator).
    //
    // Signatures mirror the matching gAptFuncs slot prototypes (Apt.h) exactly, so the
    // ConstructApt install of these slots is type-correct. Declaration shape (namespace +
    // names) is from the DecFIGS DWARF (CgsAptAux.h:48..203). Each body is grounded against
    // the X360 ARTIST binary in CgsAptAux.cpp.
    //
    // NOTE on the "not implemented" callbacks: the X360 build attests these as guarded
    // not-yet-implemented entry points -- each fires an assert ("... has not been
    // implemented but is being used, please implement before utilising.") and returns a
    // null/zero result. That assert-and-return IS the faithful body (it is what the binary
    // does); it is NOT a reconstruction stub.
    // =========================================================================

    namespace AptCallbackMemory
    {
        // X360 0x828492A8. Free an Apt allocation through the singleton AptAux's embedded
        // data-handler allocator: assert the singleton is live, then forward to
        // AptDataHandler::AptFree(&mpAptAuxInst->mAptDataHandler, lpBlock). (matches gAptFuncs
        // pfnMemFree(void*).)
        void Free(void* lpBlock);
    }

    namespace AptCallbackDebug
    {
        // X360 0x82849528 / 0x82849568. Guarded not-yet-implemented debug callbacks.
        void AddSavedInput(AptSavedInputRecord* lpRecord, s32 liCount);
        void SetScreenGrabPending(const char* lpacName);
    }

    namespace AptCallbackFile
    {
        // X360 0x828495E0 / 0x82849620. Guarded not-yet-implemented file-progress callbacks;
        // each fires the not-implemented assert and returns 0. (gAptFuncs pfnGetBytesTotal /
        // pfnGetBytesLoaded(const char*, AptGetBytesEnum).)
        s32 GetBytesTotal(const char* lpacFileName, AptGetBytesEnum leWhich);
        s32 GetBytesLoaded(const char* lpacFileName, AptGetBytesEnum leWhich);
    }

    namespace AptCallbackVariable
    {
        // X360 0x82849660 / 0x828496A0. Guarded not-yet-implemented extern-variable callbacks.
        void      SetExternVariable(const char* lpacName, const char* lpacValue);
        AptValue* GetExternVariable(const char* lpacName);
    }

    namespace AptCallbackDeprecated
    {
        // X360 0x828496E0 / 0x82849720 / 0x828497A0 / 0x828497E0 / 0x82849820. Guarded
        // not-yet-implemented deprecated callbacks (each fires the not-implemented assert and
        // returns a null/zero result). Signatures mirror their gAptFuncs slots.
        void      SendVariables(const char* lpacUrl, const char* lpacTarget,
                                const char* lpacVariables, const char* lpacMethod, s32 liFlags);
        void      FsCommand(const char* lpacCommand, const char* lpacArgs);
        AptValue* LoadVariablesNULL();
        s32       PointHitTest(f32 lfX, f32 lfY, AptAssetMoiveClip leClip);
        void      GetRealTimeClock(AptSysClock* lpSysClock, bool lbUseUtc);
    }
}
