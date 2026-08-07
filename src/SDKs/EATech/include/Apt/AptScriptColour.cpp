// ===========================================================================
// EATech Apt -- AptScriptColour: the ActionScript "Color" object.
// Reconstructed from the X360 ARTIST.XEX pseudocode + assembly.
//   AptScriptColour::AptScriptColour      @0x82AF0EE0
//   AptScriptColour::DestroyGCPointers    @0x82AF0FD8
//   AptScriptColour::RegisterReferences   @0x82AE2658
//   AptScriptColour::objectMemberLookup   @0x82AF8C40
//   AptScriptColour::CleanNativeFunctions @0x82AD64D0
//   AptScriptColour::sMethod_getRGB       @0x82AECF48
//   AptScriptColour::sMethod_getTransform @0x82AF5918
//   AptScriptColour::sMethod_setTransform @0x82AE6BA8
//   AptScriptColour::`vector deleting destructor' @0x82AF58B8 (compiler thunk -- dropped)
//
// The colour-transform methods reach the bound clip's AptCXForm through the spine
//   AptCIH (mpTarget) -> AptCharacterInst -> AptRenderItem -> AptCXForm
// using NAMED members (the console hard-codes 32-bit offsets; on x64 the widths
// differ, so access is by name and the meaning is preserved). Flash's Color
// transform is 8 channels: ra/ga/ba/aa (the multiplicative "scale" percents) and
// rb/gb/bb/ab (the additive "translate" offsets), mapped onto AptCXForm.scale /
// AptCXForm.translate per ARGB channel.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptScriptColour.h"

#include "SDKs/EATech/include/Apt/AptCIH.h"               // mpTarget -> char inst
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"     // mpRenderItem / GetRenderItemWritable
#include "SDKs/EATech/include/Apt/AptRenderItem.h"        // mpColorMatrix / GetColorMatrixWritable / gIdentityCXForm
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"     // AptColorHelper channels
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"    // the cached native methods
#include "SDKs/EATech/include/Apt/AptNativeHash.h"        // Lookup / Set on the built Object
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"  // AptInteger::Create

#include <new>       // placement new (the lazily-built method singletons)
#include <string.h>  // _stricmp (the case-insensitive member-name compare)

// The shared "undefined" value the AS Color methods return when there is nothing
// to hand back (X360 global off_8324D814). Defined in AptGlobals.cpp; built by
// AptInit's AptValueInitialize @0x82B02800. (gpUndefinedValue is also declared by
// AptArray.h, but this TU does not include it -- declare it here.)
extern AptValue* gpUndefinedValue;

// The native ActionScript method argument stack. setTransform reads its single
// Object argument off the top of this stack (X360 globals dword_8324E760 = count,
// off_8324E768 = array). Defined in AptGlobals.cpp; the interpreter's native-call
// dispatch (AptActionInterpreterInterpHelpers.cpp) publishes the operand stack
// through them around each native call.
extern AptValue** gppAptNativeArgStack;   // off_8324E768
extern int        gnAptNativeArgCount;    // dword_8324E760

// ---- the four class-wide cached native-method singletons -------------------
AptNativeFunction* AptScriptColour::spMethod_setRGB        = 0;   // off_8324E3BC
AptNativeFunction* AptScriptColour::spMethod_getRGB        = 0;   // off_8324E3C0
AptNativeFunction* AptScriptColour::spMethod_getTransform  = 0;   // off_8324E3C4
AptNativeFunction* AptScriptColour::spMethod_setTransform  = 0;   // off_8324E3C8

// ---------------------------------------------------------------------------
// ctor @0x82AF0EE0 -- bind the Color to its target movie clip.
//
// The base init is the standard AptObject(AptVFT_ScriptColour, 8) (the X360
// AptValueWithHash(this, 18, 8) + mClassFlags clear + the AptScriptColour vtable).
// The target is stored (and AddRef'd) only when it is a defined CharacterInstHandle
// (or a CIHNone) whose character instance is a movie-clip-like type; otherwise the
// binding is null.
// ---------------------------------------------------------------------------
AptScriptColour::AptScriptColour(AptValue* pTarget)
    : AptObject(AptVFT_ScriptColour, 8)
{
    // Is pTarget a movie-clip handle at all?  (defined CIH, or a CIHNone)
    const bool bTargetIsCIH =
        (pTarget->getVtblIndex() == AptVFT_CharacterInstHandle && pTarget->getIsDefined())
        || pTarget->getVtblIndex() == AptVFT_CIHNone;

    bool bBind = false;
    if (bTargetIsCIH)
    {
        // The character type tag lives in the top 6 bits of the char inst's flags.
        AptCharacterInst* pCharInst = static_cast<AptCIH*>(pTarget)->mpCharacterInst;
        const uint32_t nCharType = pCharInst->mTypeFlags & 0x3Fu;   // x64 low-6-bit tag

        // Accept movie-clip-like characters only (sprite / movieclip / type 2).
        bBind = (nCharType == 5) || (nCharType == 16)
                || (pCharInst->mTypeFlags & 0x3Fu) == 2u;   // dyn-text; x64 low-6-bit tag (X360 form 0x08000000)
    }

    if (bBind)
    {
        mpTarget = pTarget;
        pTarget->AddRef();
    }
    else
    {
        mpTarget = 0;
    }
}

// dtor -- the X360 emits only the deleting-destructor thunk (@0x82AF58B8: restore the
// AptScriptColour vtable -> ~AptObject -> conditional operator delete(this, 36)).
// mpTarget is released by DestroyGCPointers (the GC teardown path), so the user body
// is empty; the AptObject base + the AptValueWithHash hash tear down automatically.
AptScriptColour::~AptScriptColour()
{
}

// ---------------------------------------------------------------------------
// DestroyGCPointers @0x82AF0FD8 -- release the bound target, then tear down the
// property hash (the AptObject/AptValueWithHash base teardown).
// ---------------------------------------------------------------------------
void AptScriptColour::DestroyGCPointers()
{
    if (mpTarget)
        mpTarget->Release();
    mpTarget = 0;

    AptObject::DestroyGCPointers();   // == mHash.DestroyGCPointers() (X360: a1+8)
}

// ---------------------------------------------------------------------------
// RegisterReferences @0x82AE2658 -- GC mark: the property hash, then the bound
// target (registered once, under the debug name "CIH").
// ---------------------------------------------------------------------------
void AptScriptColour::RegisterReferences()
{
    AptObject::RegisterReferences();   // == mHash.RegisterReferences(this) (X360: a1+8, a1)

    if (mpTarget && AptValue::sReferenceRegistrationCb)
        AptValue::sReferenceRegistrationCb(this, &mpTarget, "CIH", 1);
}

// ---------------------------------------------------------------------------
// CleanNativeFunctions @0x82AD64D0 -- Release the four cached native-method
// singletons and clear the slots (Apt shutdown / pool clear).
// ---------------------------------------------------------------------------
void AptScriptColour::CleanNativeFunctions()
{
    if (spMethod_setRGB)       { spMethod_setRGB->Release();       spMethod_setRGB = 0; }
    if (spMethod_getRGB)       { spMethod_getRGB->Release();       spMethod_getRGB = 0; }
    if (spMethod_getTransform) { spMethod_getTransform->Release(); spMethod_getTransform = 0; }
    if (spMethod_setTransform) { spMethod_setTransform->Release(); spMethod_setTransform = 0; }
}

// ---------------------------------------------------------------------------
// objectMemberLookup @0x82AF8C40 -- resolve the four native Color members. Each is
// a class-wide AptNativeFunction singleton built lazily on first request, GC-rooted,
// AddRef'd, and cached; any other name returns null (the base hash resolves it).
//
// The X360 compares the requested name against the interned AS-key globals by
// identity-or-case-insensitive-equality; reconstructed as the case-insensitive name
// compare (the AptObject precedent), which is the same observable test.
// ---------------------------------------------------------------------------
AptValue* AptScriptColour::objectMemberLookup(AptValue* const /*pThis*/,
                                              const AptNativeString* const pName) const
{
    const char* pszName = pName->c_str();

    if (_stricmp(pszName, "setRGB") == 0)
    {
        if (!spMethod_setRGB)
        {
            void* pMem = AptNativeFunction::operator new(sizeof(AptNativeFunction));
            spMethod_setRGB = pMem
                ? ::new (pMem) AptNativeFunction(reinterpret_cast<AptExtFunctionPtr>(&AptScriptColour::sMethod_setRGB))
                : 0;
            spMethod_setRGB->setGCRoot(1);
            spMethod_setRGB->AddRef();
        }
        return spMethod_setRGB;
    }

    if (_stricmp(pszName, "getRGB") == 0)
    {
        if (!spMethod_getRGB)
        {
            void* pMem = AptNativeFunction::operator new(sizeof(AptNativeFunction));
            spMethod_getRGB = pMem
                ? ::new (pMem) AptNativeFunction(reinterpret_cast<AptExtFunctionPtr>(&AptScriptColour::sMethod_getRGB))
                : 0;
            spMethod_getRGB->setGCRoot(1);
            spMethod_getRGB->AddRef();
        }
        return spMethod_getRGB;
    }

    if (_stricmp(pszName, "getTransform") == 0)
    {
        if (!spMethod_getTransform)
        {
            void* pMem = AptNativeFunction::operator new(sizeof(AptNativeFunction));
            spMethod_getTransform = pMem
                ? ::new (pMem) AptNativeFunction(reinterpret_cast<AptExtFunctionPtr>(&AptScriptColour::sMethod_getTransform))
                : 0;
            spMethod_getTransform->setGCRoot(1);
            spMethod_getTransform->AddRef();
        }
        return spMethod_getTransform;
    }

    if (_stricmp(pszName, "setTransform") == 0)
    {
        if (!spMethod_setTransform)
        {
            void* pMem = AptNativeFunction::operator new(sizeof(AptNativeFunction));
            spMethod_setTransform = pMem
                ? ::new (pMem) AptNativeFunction(reinterpret_cast<AptExtFunctionPtr>(&AptScriptColour::sMethod_setTransform))
                : 0;
            spMethod_setTransform->setGCRoot(1);
            spMethod_setTransform->AddRef();
        }
        return spMethod_setTransform;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// sMethod_setRGB @0x82?????? (PS3 EXTERNAL DecFIGS 0xF349AC) -- write the target
// clip's colour transform from a packed 0xRRGGBB integer argument: the additive
// (translate) R/G/B channels take the three bytes (each clamped to [0, 255]) and the
// multiplicative (scale) R/G/B channels are zeroed, so the clip renders the flat
// colour. The clip's render-dirty flag is set. Returns the AS "undefined" value;
// nothing is written when the Color is unbound.
//
// DECOMPILED from the PS3 body: the packed colour is toInteger(top-of-arg-stack);
// the byte extraction is the PS3 v6/BYTE1/BYTE2 (blue = code & 0xFF, green =
// (code >> 8) & 0xFF, red = (code >> 16) & 0xFF); the fsel pair is the [0, 255]
// clamp; the three translate writes + the scale zero + the mFlagsA AS-changed set (x64 bit 0)
// match getRGB's inverse (translate = additive colour, scale = 0).
// ---------------------------------------------------------------------------
AptValue* AptScriptColour::sMethod_setRGB(AptScriptColour* pThis, int /*nArgCount*/)
{
    AptValue* pTarget = pThis->mpTarget;            // PS3: v2 = *(this + 8)
    if (pTarget)
    {
        // The packed 0xRRGGBB colour sits on top of the native-call arg stack.
        const int nCode = gppAptNativeArgStack[gnAptNativeArgCount - 1]->toInteger();

        AptCharacterInst* pCharInst   = static_cast<AptCIH*>(pTarget)->mpCharacterInst;
        AptRenderItem*    pRenderItem = pCharInst->GetRenderItemWritable();
        AptCXForm*        pColorMatrix = pRenderItem->GetColorMatrixWritable();

        // Per-byte channels (the PS3 v6 / BYTE1(v3) / BYTE2(v3)), each clamped [0,255]
        // (the PS3 fsel(255 - c) / fsel(c + 255) pair).
        f32 fRed   = static_cast<f32>((nCode >> 16) & 0xFF);
        f32 fGreen = static_cast<f32>((nCode >>  8) & 0xFF);
        f32 fBlue  = static_cast<f32>( nCode        & 0xFF);
        if (fRed   < 0.0f)   fRed   = 0.0f;   if (fRed   > 255.0f) fRed   = 255.0f;
        if (fGreen < 0.0f)   fGreen = 0.0f;   if (fGreen > 255.0f) fGreen = 255.0f;
        if (fBlue  < 0.0f)   fBlue  = 0.0f;   if (fBlue  > 255.0f) fBlue  = 255.0f;

        // Additive (translate) R/G/B = the colour; multiplicative (scale) R/G/B = 0.
        pColorMatrix->translate.SetValuef(AptColorHelper::Red,   fRed);
        pColorMatrix->translate.SetValuef(AptColorHelper::Green, fGreen);
        pColorMatrix->translate.SetValuef(AptColorHelper::Blue,  fBlue);
        pColorMatrix->scale.SetValuef(AptColorHelper::Red,   0.0f);   // ColorMatrix[2] = 0
        pColorMatrix->scale.SetValuef(AptColorHelper::Green, 0.0f);   // ColorMatrix[3] = 0
        pColorMatrix->scale.SetValuef(AptColorHelper::Blue,  0.0f);   // ColorMatrix[4] = 0

        // Mark the clip's render/colour state AS-changed (x64 mFlagsA bit 0).
        static_cast<AptCIH*>(pTarget)->mFlagsA |= 0x1u;               // X360 reversed form |= 0x80000000
    }

    return gpUndefinedValue;
}

// ---------------------------------------------------------------------------
// sMethod_getRGB @0x82AECF48 -- read the target clip's additive (translate) R/G/B
// channels, clamp each to [0, 255], pack as 0xRRGGBB and box it in an AptInteger.
// Returns the AS "undefined" value when the Color is unbound.
// ---------------------------------------------------------------------------
AptValue* AptScriptColour::sMethod_getRGB(AptScriptColour* pThis)
{
    if (!pThis->mpTarget)
        return gpUndefinedValue;

    // mpTarget -> char inst -> render item -> colour matrix (null -> identity).
    AptCharacterInst* pCharInst   = static_cast<AptCIH*>(pThis->mpTarget)->mpCharacterInst;
    AptRenderItem*    pRenderItem = pCharInst->mpRenderItem;
    const AptCXForm*  pColorMatrix = pRenderItem->mpColorMatrix;
    if (!pColorMatrix)
        pColorMatrix = &gIdentityCXForm;

    // The additive (translate) ARGB run; getRGB takes the R/G/B offsets.
    f32 fRed   = pColorMatrix->translate.GetValuef(AptColorHelper::Red);
    f32 fGreen = pColorMatrix->translate.GetValuef(AptColorHelper::Green);
    f32 fBlue  = pColorMatrix->translate.GetValuef(AptColorHelper::Blue);

    if (fRed   < 0.0f)   fRed   = 0.0f;
    if (fGreen < 0.0f)   fGreen = 0.0f;
    if (fBlue  < 0.0f)   fBlue  = 0.0f;
    if (fRed   > 255.0f) fRed   = 255.0f;
    if (fGreen > 255.0f) fGreen = 255.0f;
    if (fBlue  > 255.0f) fBlue  = 255.0f;

    const int nRed   = static_cast<int>(fRed);
    const int nGreen = static_cast<int>(fGreen);
    const int nBlue  = static_cast<int>(fBlue);

    return AptInteger::Create((nRed << 16) | (nGreen << 8) | nBlue);
}

// ---------------------------------------------------------------------------
// sMethod_getTransform @0x82AF5918 -- build a fresh AS Object holding the target
// clip's 8 colour-transform channels (the multiplicative ra/ga/ba/aa percents +
// the additive rb/gb/bb/ab offsets, each truncated to an int). Returns undefined
// when called with arguments (a getter takes none) or when the Color is unbound.
// ---------------------------------------------------------------------------
AptValue* AptScriptColour::sMethod_getTransform(AptScriptColour* pThis, int nArgCount)
{
    if (nArgCount > 0 || !pThis->mpTarget || !pThis->mpTarget->getIsDefined())
        return gpUndefinedValue;

    AptCharacterInst* pCharInst = static_cast<AptCIH*>(pThis->mpTarget)->mpCharacterInst;

    AptObject* pResult = AptObject::Create(8);

    // The colour matrix (null -> identity).
    const AptCXForm* pColorMatrix = pCharInst->mpRenderItem->mpColorMatrix;
    if (!pColorMatrix)
        pColorMatrix = &gIdentityCXForm;

    // Multiplicative "scale" percents: ra / ga / ba / aa.
    pResult->Set("ra", AptInteger::Create(static_cast<int>(pColorMatrix->scale.GetValuef(AptColorHelper::Red))));
    pResult->Set("ga", AptInteger::Create(static_cast<int>(pColorMatrix->scale.GetValuef(AptColorHelper::Green))));
    pResult->Set("ba", AptInteger::Create(static_cast<int>(pColorMatrix->scale.GetValuef(AptColorHelper::Blue))));
    pResult->Set("aa", AptInteger::Create(static_cast<int>(pColorMatrix->scale.GetValuef(AptColorHelper::Alpha))));

    // Additive "translate" offsets: rb / gb / bb / ab.
    pResult->Set("rb", AptInteger::Create(static_cast<int>(pColorMatrix->translate.GetValuef(AptColorHelper::Red))));
    pResult->Set("gb", AptInteger::Create(static_cast<int>(pColorMatrix->translate.GetValuef(AptColorHelper::Green))));
    pResult->Set("bb", AptInteger::Create(static_cast<int>(pColorMatrix->translate.GetValuef(AptColorHelper::Blue))));
    pResult->Set("ab", AptInteger::Create(static_cast<int>(pColorMatrix->translate.GetValuef(AptColorHelper::Alpha))));

    return pResult;
}

// ---------------------------------------------------------------------------
// sMethod_setTransform @0x82AE6BA8 -- write the target clip's colour transform from
// the channels of the Object argument. Each present channel is coerced to an int,
// clamped to [-255, 255], and written to the matching AptCXForm scale/translate
// channel; if any channel was written the clip's render-dirty flag is set. Always
// returns the AS "undefined" value.
// ---------------------------------------------------------------------------
AptValue* AptScriptColour::sMethod_setTransform(AptScriptColour* pThis, int nArgCount)
{
    if (nArgCount > 0)
    {
        AptValue* pTarget = pThis->mpTarget;
        // The single Object argument sits on top of the native-call arg stack.
        AptValue* pArg = gppAptNativeArgStack[gnAptNativeArgCount - 1];

        if (pTarget
            && pTarget->getIsDefined()
            && pArg->getIsDefined()
            && pArg->getVtblIndex() == AptVFT_Object)
        {
            AptCIH*           pTargetCIH = static_cast<AptCIH*>(pTarget);
            AptRenderItem*    pRenderItem = pTargetCIH->mpCharacterInst->GetRenderItemWritable();
            AptCXForm*        pColorMatrix = pRenderItem->GetColorMatrixWritable();

            AptNativeHash*    pArgHash = pArg->GetNativeHashVirtual();

            // Read one channel from the argument Object (null when absent), clamp to
            // [-255, 255], and write it; track whether anything was set.

            // ra -> scale.Red
            AptValue* pRa = pArgHash->Lookup("ra");
            if (pRa)
            {
                f32 v = static_cast<f32>(pRa->toInteger());
                if (v < -255.0f) v = -255.0f;
                if (v >  255.0f) v =  255.0f;
                pColorMatrix->scale.SetValuef(AptColorHelper::Red, v);
            }

            // rb -> translate.Red
            AptValue* pRb = pArgHash->Lookup("rb");
            if (pRb)
            {
                f32 v = static_cast<f32>(pRb->toInteger());
                if (v < -255.0f) v = -255.0f;
                if (v >  255.0f) v =  255.0f;
                pColorMatrix->translate.SetValuef(AptColorHelper::Red, v);
            }

            // ga -> scale.Green
            AptValue* pGa = pArgHash->Lookup("ga");
            if (pGa)
            {
                f32 v = static_cast<f32>(pGa->toInteger());
                if (v < -255.0f) v = -255.0f;
                if (v >  255.0f) v =  255.0f;
                pColorMatrix->scale.SetValuef(AptColorHelper::Green, v);
            }

            // gb -> translate.Green
            AptValue* pGb = pArgHash->Lookup("gb");
            if (pGb)
            {
                f32 v = static_cast<f32>(pGb->toInteger());
                if (v < -255.0f) v = -255.0f;
                if (v >  255.0f) v =  255.0f;
                pColorMatrix->translate.SetValuef(AptColorHelper::Green, v);
            }

            // ba -> scale.Blue
            AptValue* pBa = pArgHash->Lookup("ba");
            if (pBa)
            {
                f32 v = static_cast<f32>(pBa->toInteger());
                if (v < -255.0f) v = -255.0f;
                if (v >  255.0f) v =  255.0f;
                pColorMatrix->scale.SetValuef(AptColorHelper::Blue, v);
            }

            // bb -> translate.Blue
            AptValue* pBb = pArgHash->Lookup("bb");
            if (pBb)
            {
                f32 v = static_cast<f32>(pBb->toInteger());
                if (v < -255.0f) v = -255.0f;
                if (v >  255.0f) v =  255.0f;
                pColorMatrix->translate.SetValuef(AptColorHelper::Blue, v);
            }

            // aa -> scale.Alpha
            AptValue* pAa = pArgHash->Lookup("aa");
            if (pAa)
            {
                f32 v = static_cast<f32>(pAa->toInteger());
                if (v < -255.0f) v = -255.0f;
                if (v >  255.0f) v =  255.0f;
                pColorMatrix->scale.SetValuef(AptColorHelper::Alpha, v);
            }

            // ab -> translate.Alpha
            AptValue* pAb = pArgHash->Lookup("ab");
            if (pAb)
            {
                f32 v = static_cast<f32>(pAb->toInteger());
                if (v < -255.0f) v = -255.0f;
                if (v >  255.0f) v =  255.0f;
                pColorMatrix->translate.SetValuef(AptColorHelper::Alpha, v);
            }

            const bool bAnySet = pRa || pRb || pGa || pGb || pBa || pBb || pAa || pAb;
            if (bAnySet)
            {
                // Mark the clip's render/colour state AS-changed (x64 mFlagsA bit 0).
                pTargetCIH->mFlagsA |= 0x1u;
            }
        }
    }

    return gpUndefinedValue;
}
