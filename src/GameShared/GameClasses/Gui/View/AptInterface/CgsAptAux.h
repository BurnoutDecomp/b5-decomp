#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"  // CgsGui::AptRenderHandler (mRenderHandler)

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
        // X360/PS3 0x5BA0F8 (CgsGui::AptAux::ConstructApt) -- install the Apt host
        // user-function table (gAptFuncs): point every memory / debug / file / variable /
        // render / deprecated callback slot at the matching CgsGui::AptCallback* free
        // function. Reconstructed in CgsAptAux.cpp from the 40-store ConstructApt body; the
        // render-slot installs reference the CgsAptCallbackRender.cpp family this TU defines.
        void ConstructApt();

        // The Apt render bridge the callback family reaches through mpAptAuxInst->mRenderHandler.
        // [guest +0x420]
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
}
