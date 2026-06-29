#pragma once

// ===========================================================================
// SDKs/EATech/include/Apt/AptRenderingContext.h
//
// EATech Apt (ActionScript / Flash player) AptRenderingContext -- the per-render
// transform state the Apt player threads through a display-list walk. It holds:
//
//   * a CURRENT 2D affine vertex matrix plus a 16-deep matrix stack, and
//   * a CURRENT colour transform (AptCXForm) plus a 16-deep CXForm stack,
//
// and pushes/pops them as it descends into nested movie clips. The "current"
// values are forwarded to the host renderer through the two render hooks in the
// Apt user-function table (gAptFuncs.pfnSetVertexMatrix / pfnSetColourTransform).
//
// NO DWARF exists for this class in the DecFIGS dump and it is absent from the
// Feb-2007 partial Apt SDK (only forward-declared in AptValue.h:99). All shape
// below is recovered STRICTLY from the X360 ARTIST.XEX, store-for-store:
//     AptRenderingContext::AptRenderingContext            @ 0x82AEB808  (ctor)
//     AptRenderingContext::`scalar deleting destructor'   @ 0x82AE30F8
//     AptRenderingContext::appendVertexMatrix             @ 0x82ADA658
//     AptRenderingContext::multMatrix                     @ 0x82ADA550
//     AptRenderingContext::popVertexMatrix                @ 0x82ADA4F8
//     AptRenderingContext::appendColourTransform          @ 0x82AE0558
//     AptRenderingContext::pushColourTransform            @ 0x82AEF0F0
//     AptRenderingContext::popColourTransform             @ 0x82AEB8F0
//
// LAYOUT (1096 bytes / 0x448, proven by the dtor's Deallocate size):
//     +0x000  mCurrentVertexMatrix     AptMatrix            (24 bytes)
//     +0x018  mpVertexMatrixStackTop   AptMatrix*           (init = &mVertexStack[0])
//     +0x01C  mVertexStack[16]         AptMatrix[16]        (16 * 24 = 0x180)
//     +0x19C  mCurrentColourTransform  AptCXForm            (40 bytes)
//     +0x1C4  mpColourStackTop         AptCXForm*           (init = &mColourStack[0])
//     +0x1C8  mColourStack[16]         AptCXForm[16]        (16 * 40 = 0x280)
//                                                       total = 0x448 (1096)
//
// Member access is BY NAME; the X360 byte offsets above are documentation only.
// On the x64 PC gate the AptCXForm helper vptrs widen, so the struct is larger
// than the X360's 1096 -- correct per the project's semantic-parity-by-named-
// members rule; no sizeof static_assert is asserted against the 32-bit size.
// ===========================================================================

#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"    // AptMatrix
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"    // AptCXForm
#include "SDKs/EATech/include/Apt/AptStd/AptRect.h"      // AptRect (expandBoundingRect)

// ---------------------------------------------------------------------------
// Apt host render hooks — the global AptUserFunctions table gAptFuncs.
//
// The X360 reaches the host renderer through the single global Apt user-function
// table gAptFuncs (X360 dword_8324E818). This TU touches only its two render hooks:
//     gAptFuncs.pfnSetVertexMatrix    (table slot 26 -> X360 dword_8324E880)
//     gAptFuncs.pfnSetColourTransform (table slot 27 -> X360 dword_8324E884)
//
// The full AptUserFunctions table now lives at its SDK home (Apt.h, recovered
// verbatim from the DecFIGS DWARF), so this context reaches the same single table
// by name through it -- the earlier render-hook-only stand-in is unified away.
// The table is homed by the Apt host-install TU (CgsAptAux.cpp); sibling Apt TUs
// reference the same underlying gAptFuncs.
// ---------------------------------------------------------------------------
#include "SDKs/EATech/include/Apt/Apt.h"   // AptUserFunctions gAptFuncs (the real host table)

// ---------------------------------------------------------------------------
// Apt "identity / null" transform singletons (un-homed externs).
//
// The X360 short-circuits a concat against a fixed identity instance by POINTER
// identity (cmplw against a literal .data address), not by value. Modelled as
// extern singletons so the same pointer-equality test reconstructs faithfully:
//     gAptIdentityMatrix  X360 flt_8324E2B0 -- the identity AptMatrix singleton
//     gAptNullCXForm      X360 off_82F73388 -- the "no-op" AptCXForm singleton
// Defined by the Apt runtime; declared extern here.
// ---------------------------------------------------------------------------
extern const AptMatrix  gAptIdentityMatrix;   // X360 flt_8324E2B0
extern const AptCXForm  gAptNullCXForm;       // X360 off_82F73388

class AptRenderingContext
{
public:
    // Stacks are a fixed 16 deep (vertex region +0x1C..+0x19C = 16*24; colour
    // region +0x1C8..+0x448 = 16*40).
    enum { KU_STACK_DEPTH = 16 };

    // ctor @ 0x82AEB808 -- identity vertex matrix + white(255)/zero identity
    // colour transform; both stack-top pointers reset to slot 0; the colour
    // stack slots default-constructed (helper vptrs + zeroed channels).
    AptRenderingContext();

    // `scalar deleting destructor' @ 0x82AE30F8 -- restores the base
    // AptColorHelper vtable on every colour-transform helper (the trivial helper
    // destruct) and, when the delete flag is set, frees the block back to the
    // shared Apt DOGMA pool. Returns `this` in the X360 fastcall convention.
    void* ScalarDeletingDestructor(char flags);

    // appendVertexMatrix @ 0x82ADA658 -- concat pMatrix onto the current vertex
    // matrix (skipped when pMatrix is the identity singleton) and forward the
    // result to the host renderer.
    void appendVertexMatrix(AptMatrix* pMatrix);

    // multMatrix @ 0x82ADA550 -- 2D affine concat: pDst = pSrc concatenated with
    // pAppend (identity-singleton short-circuits each operand to a plain copy).
    static void multMatrix(const AptMatrix* pSrc, const AptMatrix* pAppend, AptMatrix* pDst);

    // expandBoundingRect @0x82ADA6B0 (PS3 External 0x7E1EF8) -- transform pBounds'
    // four corners by the 2D affine pMatrix and grow pAccumulator to include them
    // (the AptCIH/AptDisplayList bounds walk's leaf-accumulate step).
    static void expandBoundingRect(const AptRect* pBounds, const AptMatrix* pMatrix, AptRect* pAccumulator);

    // pushVertexMatrix (PS3 External @0x7E4650) -- save the current vertex matrix
    // onto the stack (the X360 ARTIST reconstruction omitted it; recovered from
    // the PS3 External, where it is a distinct entry point). The matching save for
    // appendVertexMatrix/popVertexMatrix.
    void pushVertexMatrix();

    // popVertexMatrix @ 0x82ADA4F8 -- pop the vertex stack, restore the now-top
    // entry into the current matrix, forward it to the renderer.
    void popVertexMatrix();

    // appendColourTransform @ 0x82AE0558 -- multiply/add pCXForm into the current
    // colour transform with per-channel [-255,255] clamping (skipped when pCXForm
    // is the null singleton) and forward the result to the renderer.
    void appendColourTransform(AptCXForm* pCXForm);

    // pushColourTransform @ 0x82AEF0F0 -- copy the current colour transform's 8
    // channels onto the colour stack, advance the stack-top pointer.
    void pushColourTransform();

    // popColourTransform @ 0x82AEB8F0 -- pop the colour stack, copy the popped
    // slot's 8 channels back into the current colour transform.
    void popColourTransform();

    // The current colour transform (top of the colour-transform stack). The morph
    // render path sets its alpha scale channel directly, then flushes it through
    // the SetColourTransform render hook (AptRenderItemMorph::Render @0x82AFEEC0).
    AptCXForm& GetCurrentColourTransform() { return mCurrentColourTransform; }

private:
    AptMatrix  mCurrentVertexMatrix;                    // [+0x000]
    AptMatrix* mpVertexMatrixStackTop;                  // [+0x018]
    AptMatrix  mVertexStack[KU_STACK_DEPTH];            // [+0x01C]

    AptCXForm  mCurrentColourTransform;                 // [+0x19C]
    AptCXForm* mpColourStackTop;                        // [+0x1C4]
    AptCXForm  mColourStack[KU_STACK_DEPTH];            // [+0x1C8]
};
