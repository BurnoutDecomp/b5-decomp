#include "SDKs/EATech/include/Apt/AptRenderingContext.h"

#include "SDKs/EATech/Apt/DogmaAllocator.h"   // DOGMA_PoolManager (block free)

#include <cstring>   // memcpy

// ===========================================================================
//  AptRenderingContext member functions.
//
//  Store-for-store reconstruction from BURNOUT_X360_ARTIST.XEX; the X360 asm is
//  authoritative. See AptRenderingContext.h for the recovered member layout and
//  the address of each function.
//
//  The render hooks gAptFuncs.pfnSetVertexMatrix (X360 dword_8324E880) and
//  gAptFuncs.pfnSetColourTransform (X360 dword_8324E884) are the indirect calls
//  the X360 makes through the global Apt user-function table. The identity
//  singletons gAptIdentityMatrix (flt_8324E2B0) / gAptNullCXForm (off_82F73388)
//  reproduce the X360's pointer-identity short-circuits.
// ===========================================================================

// ---------------------------------------------------------------------------
// Shared Apt fixed-size pool (X360 off_8324D808). The whole AptRenderingContext
// block (1096 bytes on the X360) is allocated from / freed back to this pool;
// it is the same DOGMA pool the other Apt display-list nodes use (declared under
// this name by AptPseudoCIH.h; the single underlying object is shared).
// ---------------------------------------------------------------------------
extern DOGMA_PoolManager* gpAptPseudoDataPool;   // X360 off_8324D808

// ---- AptRenderingContext @ 0x82AEB808 ------------------------------------
// Reset to the rendering identity: identity 2D vertex matrix, opaque-white
// (scale = 255) / zero-translate colour transform, both stack-top pointers at
// slot 0, and every colour-stack slot default-constructed (the X360 loop writes
// each slot's two helper vtable pointers and zeroes its eight channels -- that
// is exactly the AptCXForm default constructor). The vertex stack region is left
// uninitialised by the X360 ctor, so it is not touched here either.
AptRenderingContext::AptRenderingContext()
{
    // Identity 2D affine vertex matrix (a=1, d=1, rest 0).
    mCurrentVertexMatrix.a  = 1.0f;
    mCurrentVertexMatrix.b  = 0.0f;
    mCurrentVertexMatrix.c  = 0.0f;
    mCurrentVertexMatrix.d  = 1.0f;
    mCurrentVertexMatrix.tx = 0.0f;
    mCurrentVertexMatrix.ty = 0.0f;
    mpVertexMatrixStackTop = &mVertexStack[0];

    // Identity colour transform: scale channels = 255 (opaque white), translate
    // channels = 0. The eight stores after the stack-init loop set these.
    const f32 lafWhite[4] = { 255.0f, 255.0f, 255.0f, 255.0f };   // flt_82010C20
    const f32 lafZero[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };           // flt_82001CC0
    mCurrentColourTransform.scale.CopyFromFloatArray(lafWhite);
    mCurrentColourTransform.translate.CopyFromFloatArray(lafZero);
    mpColourStackTop = &mColourStack[0];

    // The X360 ctor's stack-init loop sets each colour-stack slot's helper vtable
    // pointers and zeroes its channels -- the AptCXForm default ctor does both,
    // and the array members are default-constructed before this body runs, so no
    // explicit per-slot loop is needed for semantic parity.
}

// ---- `scalar deleting destructor' @ 0x82AE30F8 ---------------------------
// The X360 thunk first restores the base AptColorHelper vtable on the current
// colour transform's two helpers and on all 16 colour-stack slots' helpers (the
// trivial helper destructors -- the channels are plain floats, so no member work
// remains), then, when bit0 of the hidden flags arg is set, frees the 1096-byte
// block back to the shared Apt DOGMA pool. The per-helper vtable restores ARE the
// compiler-emitted member-destructor sequence for the AptCXForm members, which
// the implicit ~AptRenderingContext reproduces; only the conditional block free
// remains as explicit code here. Returns `this` (X360 fastcall).
void* AptRenderingContext::ScalarDeletingDestructor(char flags)
{
    // (Implicit member destruction of mCurrentColourTransform + mColourStack[]
    //  restores each helper's base vtable, matching the X360 store loops.)

    if (flags & 1)   // clrlwi. r9, r4, 31; beq -> skip
    {
        gpAptPseudoDataPool->Deallocate(this, sizeof(AptRenderingContext));  // Deallocate(off_8324D808, this, 0x448)
    }
    return this;
}

// ---- multMatrix @ 0x82ADA550 ---------------------------------------------
// 2D affine concatenation pDst = pSrc concatenated with pAppend. Either operand
// being the identity singleton short-circuits to a plain 24-byte copy of the
// other; otherwise the standard SWF 2x3 matrix product is written into pDst.
void AptRenderingContext::multMatrix(const AptMatrix* pSrc, const AptMatrix* pAppend,
                                     AptMatrix* pDst)
{
    if (pSrc == &gAptIdentityMatrix)
    {
        // Src is identity -> result is just pAppend.
        memcpy(pDst, pAppend, sizeof(AptMatrix));
        return;
    }
    if (pAppend == &gAptIdentityMatrix)
    {
        // Append is identity -> result is just pSrc.
        memcpy(pDst, pSrc, sizeof(AptMatrix));
        return;
    }

    // Snapshot both operands (the X360 memcpys Src and pAppend to stack first so
    // pDst may alias either input safely).
    const AptMatrix lSrc    = *pSrc;
    const AptMatrix lAppend = *pAppend;

    pDst->a  = lSrc.c  * lAppend.b  + lSrc.a  * lAppend.a;
    pDst->b  = lSrc.d  * lAppend.b  + lSrc.b  * lAppend.a;
    pDst->c  = lSrc.c  * lAppend.d  + lSrc.a  * lAppend.c;
    pDst->d  = lSrc.d  * lAppend.d  + lSrc.b  * lAppend.c;
    pDst->tx = lAppend.tx * lSrc.a  + lAppend.ty * lSrc.c + lSrc.tx;
    pDst->ty = lAppend.tx * lSrc.b  + lAppend.ty * lSrc.d + lSrc.ty;
}

// ---- appendVertexMatrix @ 0x82ADA658 -------------------------------------
// Concatenate pMatrix onto the current vertex matrix in place (unless pMatrix is
// the identity singleton, which leaves the current matrix unchanged), then push
// the current matrix to the host renderer.
void AptRenderingContext::appendVertexMatrix(AptMatrix* pMatrix)
{
    if (pMatrix != &gAptIdentityMatrix)
    {
        multMatrix(&mCurrentVertexMatrix, pMatrix, &mCurrentVertexMatrix);
    }
    gAptFuncs.pfnSetVertexMatrix(&mCurrentVertexMatrix);   // dword_8324E880(this)
}

// ---- pushVertexMatrix (PS3 External @0x7E4650) ---------------------------
// Save the current vertex matrix onto the stack and advance the stack-top
// pointer (the matching save for popVertexMatrix). Recovered from the PS3
// External ELF, where it is a distinct entry point.
void AptRenderingContext::pushVertexMatrix()
{
    *mpVertexMatrixStackTop = mCurrentVertexMatrix;       // memcpy(top, this, 24)
    ++mpVertexMatrixStackTop;                             // *(this+24) += 24 bytes
}

// ---- popVertexMatrix @ 0x82ADA4F8 ----------------------------------------
// Pop one entry off the vertex stack and restore the now-top entry into the
// current vertex matrix, then push it to the renderer.
void AptRenderingContext::popVertexMatrix()
{
    --mpVertexMatrixStackTop;                              // *(this+24) -= 24 bytes
    mCurrentVertexMatrix = *mpVertexMatrixStackTop;        // memcpy(this, top, 24)
    gAptFuncs.pfnSetVertexMatrix(&mCurrentVertexMatrix);   // dword_8324E880(this)
}

// ---- appendColourTransform @ 0x82AE0558 ----------------------------------
// Fold pCXForm into the current colour transform: the four scale channels are
// multiplied (and re-normalised by 1/255), the four translate channels are
// added, each result clamped to [-255, 255]. The null singleton leaves the
// current transform untouched. Finally push the current transform to the
// renderer. (flt_82010C1C * flt_82010C20 == (1/255) -- the X360 multiplies the
// scaled product by 1/255 via two constants.)
void AptRenderingContext::appendColourTransform(AptCXForm* pCXForm)
{
    static const f32 KF_CXFORM_CHANNEL_MIN = -255.0f;   // flt_821455F4
    static const f32 KF_CXFORM_CHANNEL_MAX =  255.0f;   // flt_82010C20
    static const f32 KF_INV_255            = 1.0f / 255.0f;  // flt_82010C1C

    if (pCXForm != &gAptNullCXForm)
    {
        // Scale channels: current[i] = clamp(append[i] * current[i] / 255).
        for (int liChannel = AptColorHelper::Alpha; liChannel <= AptColorHelper::Blue; ++liChannel)
        {
            const AptColorHelper::AptColorValue eChannel =
                static_cast<AptColorHelper::AptColorValue>(liChannel);

            f32 lfValue = pCXForm->scale.GetValuef(eChannel)
                        * mCurrentColourTransform.scale.GetValuef(eChannel)
                        * KF_INV_255;
            if (lfValue < KF_CXFORM_CHANNEL_MIN) { lfValue = KF_CXFORM_CHANNEL_MIN; }
            if (lfValue > KF_CXFORM_CHANNEL_MAX) { lfValue = KF_CXFORM_CHANNEL_MAX; }
            mCurrentColourTransform.scale.SetValuef(eChannel, lfValue);
        }

        // Translate channels: current[i] = clamp(append[i] + current[i]).
        for (int liChannel = AptColorHelper::Alpha; liChannel <= AptColorHelper::Blue; ++liChannel)
        {
            const AptColorHelper::AptColorValue eChannel =
                static_cast<AptColorHelper::AptColorValue>(liChannel);

            f32 lfValue = pCXForm->translate.GetValuef(eChannel)
                        + mCurrentColourTransform.translate.GetValuef(eChannel);
            if (lfValue < KF_CXFORM_CHANNEL_MIN) { lfValue = KF_CXFORM_CHANNEL_MIN; }
            if (lfValue > KF_CXFORM_CHANNEL_MAX) { lfValue = KF_CXFORM_CHANNEL_MAX; }
            mCurrentColourTransform.translate.SetValuef(eChannel, lfValue);
        }
    }

    gAptFuncs.pfnSetColourTransform(&mCurrentColourTransform);   // dword_8324E884(this+0x19C)
}

// ---- pushColourTransform @ 0x82AEF0F0 ------------------------------------
// Copy the current colour transform's eight channels (scale[4] then translate[4])
// into the next free colour-stack slot, then advance the stack-top pointer. The
// slot's pre-initialised helper vtable pointers are preserved (only the channels
// are written), so the channel copy is what the X360's two 4-iteration loops do.
void AptRenderingContext::pushColourTransform()
{
    AptCXForm* lpSlot = mpColourStackTop;
    ++mpColourStackTop;   // *(this+452) += 40

    for (int liChannel = AptColorHelper::Alpha; liChannel <= AptColorHelper::Blue; ++liChannel)
    {
        const AptColorHelper::AptColorValue eChannel =
            static_cast<AptColorHelper::AptColorValue>(liChannel);
        lpSlot->scale.SetValuef(eChannel,     mCurrentColourTransform.scale.GetValuef(eChannel));
        lpSlot->translate.SetValuef(eChannel, mCurrentColourTransform.translate.GetValuef(eChannel));
    }
}

// ---- popColourTransform @ 0x82AEB8F0 -------------------------------------
// Decrement the colour stack and copy the popped slot's eight channels back into
// the current colour transform (scale[4] then translate[4]).
void AptRenderingContext::popColourTransform()
{
    --mpColourStackTop;   // *(this+452) -= 40
    AptCXForm* lpSlot = mpColourStackTop;

    for (int liChannel = AptColorHelper::Alpha; liChannel <= AptColorHelper::Blue; ++liChannel)
    {
        const AptColorHelper::AptColorValue eChannel =
            static_cast<AptColorHelper::AptColorValue>(liChannel);
        mCurrentColourTransform.scale.SetValuef(eChannel,     lpSlot->scale.GetValuef(eChannel));
        mCurrentColourTransform.translate.SetValuef(eChannel, lpSlot->translate.GetValuef(eChannel));
    }
}

// ---------------------------------------------------------------------------
// expandBoundingRect @0x82ADA6B0 (body recovered from the PS3 External ELF
// @0x7E1EF8; the X360 form is not in the exports). Transform pBounds' four corners
// by the 2D affine pMatrix (x' = a*x + c*y + tx, y' = b*x + d*y + ty) and grow
// pAccumulator (left/top = min, right/bottom = max) to include them. The console
// keeps a pMatrix==identity-singleton fast-path; it is folded into the general
// affine here (which yields the identical result for the identity matrix).
// ---------------------------------------------------------------------------
void AptRenderingContext::expandBoundingRect(const AptRect* pBounds, const AptMatrix* pMatrix, AptRect* pAccumulator)
{
    const float afCornerX[4] = { pBounds->fLeft,  pBounds->fRight, pBounds->fRight,  pBounds->fLeft };
    const float afCornerY[4] = { pBounds->fTop,   pBounds->fTop,   pBounds->fBottom, pBounds->fBottom };

    for (int i = 0; i < 4; ++i)
    {
        const float fX = pMatrix->a * afCornerX[i] + (pMatrix->c * afCornerY[i] + pMatrix->tx);
        const float fY = pMatrix->b * afCornerX[i] + (pMatrix->d * afCornerY[i] + pMatrix->ty);
        if (fX < pAccumulator->fLeft)   pAccumulator->fLeft   = fX;
        if (fX > pAccumulator->fRight)  pAccumulator->fRight  = fX;
        if (fY < pAccumulator->fTop)    pAccumulator->fTop    = fY;
        if (fY > pAccumulator->fBottom) pAccumulator->fBottom = fY;
    }
}
