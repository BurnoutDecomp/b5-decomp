#ifndef SDKS_EATECH_APT_APT_H
#define SDKS_EATECH_APT_APT_H

#include "types.hpp"

// ============================================================================
// SDKs/EATech/include/Apt/Apt.h
//
// EA Apt SDK public surface (focused reconstruction). Only the pieces the
// reconstructed game code reaches are recovered here:
//
//   * AptStringAlignment            (Apt.h:166 DWARF enum)
//   * AptAllocateStringParameters   (Apt.h:225 DWARF struct) — the descriptor a
//     caller fills before asking Apt to allocate/measure a text string. It is
//     populated field-by-field by BrnFlapt::TextFieldInstance::SetUpAptStringParams
//     and handed to CgsGui::CgsAptString::Prepare.
//   * AptUserFunctions              (Apt.h:405 DWARF struct) — the global Apt host
//     callback table (gAptFuncs, X360 dword_8324E818). The Apt engine reaches the
//     host (memory / debug / file / variable / RENDER / custom-control) entirely
//     through this single function-pointer table. CgsGui::AptAux::ConstructApt
//     @0x5BA0F8 installs the slots; the engine's render dispatch (AptCharacter::
//     render, the render-context push/pop) and the AptHook_DrawShape boundary call
//     through it. The render-pfn slots (pfnSetBackgroundColour .. pfnDrawRenderingUnit,
//     pfnGetStageHeight/Width, pfnPushRenderFlags/PopRenderFlags) are the family this
//     goal wires to the committed CgsGui::AptCallbackRender free functions.
//
// Layout/field names are taken verbatim from the DecFIGS DWARF (Apt.h:225..303 for
// the string params, Apt.h:405..859 for the user-function table), which is the
// declaration-shape authority. Field types use the SDK's own (non-Hungarian) names
// per the naming convention's "generated/external API" exception. `AptAssetString`
// is an opaque handle (a void* in the SDK).
// ============================================================================

// Apt.h:136 — opaque per-allocation string handle the SDK hands back.
typedef void* AptAssetString;

// ---- Apt opaque asset handles (the SDK's own opaque-handle convention) ----------
// The Apt engine threads these typed-but-opaque handles through the user-function
// table; the host callbacks cast them to the concrete loaded-resource layout they
// read. Modelled as void* (the SDK aliases). NOTE: identical typedefs may also appear
// in the host callback header (CgsAptCallbackRender.h); C++ permits repeated identical
// typedefs, so the two coexist (they name the same SDK type).
typedef void* AptAnimationUserData;       // per-animation user data (+12 -> gui-geometry object)
typedef void* AptAssetRenderingUnit;      // a loaded shape's GuiGeometryFile*
typedef void* AptAssetTexture;            // a GuiTexture id handle
typedef int   AptAssetCustomControlZId;   // a custom-control z-order id

// ---- Apt SDK collaborators referenced only by pointer in the table -------------
// The user-function table references these only as pointers/by-value-handles; full
// homes live in their own SDK headers. Forward-declared so the table is self-
// contained (it never dereferences them here).
struct AptMatrix;                  // SDKs/EATech/include/Apt/AptStd/AptMatrix.h
struct AptCXForm;                  // SDKs/EATech/include/Apt/AptStd/AptCXForm.h
class  AptValue;                   // SDKs/EATech/include/Apt/AptValue/AptValue.h
struct AptFile;                    // the .apt animation file (AptFilePtr referent)
struct AptSavedInputRecord;        // debug saved-input record
struct AptSysClock;                // real-time-clock out-struct
typedef int AptAssetMoiveClip;     // a movie-clip handle (DWARF spelling kept verbatim)
typedef int AptGetBytesEnum;       // GetBytesTotal/Loaded selector
// AptFilePtr is the ref-counted .apt file handle. Its canonical definition is
// AptSharedPtr<AptFile> (AptSharedPtr.h) -- the older `typedef AptFile* AptFilePtr`
// here was a stale duplicate that collided with that counted-handle typedef whenever
// both headers were in scope. Pull the real one in (no cycle; AptSharedPtr.h is light).
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"   // AptFilePtr == AptSharedPtr<AptFile>

// Apt.h:600 — the mask operation the Apt render tree requests for a rendering unit.
// (The full reconstruction of the enum values lives in CgsAptCallbackRender.h, where
// DrawRenderingUnit dispatches on it; declared here as the SDK's int-backed enum so
// the user-function table's render slots are type-correct. Identical re-declaration
// in the host headers is permitted.)
enum AptMaskRenderOperation : int;

// Apt.h:166 — horizontal/box text alignment.
enum AptStringAlignment
{
    AptStringAlignment_Left    = 0,
    AptStringAlignment_Right   = 1,
    AptStringAlignment_Center  = 2,
    AptStringAlignment_None    = 3,
    AptStringAlignment_Justify = 4
};

// Apt.h:225 — the input descriptor for allocating/laying-out a text string.
struct AptAllocateStringParameters
{
    const char*        szFontName;     // Apt.h:227
    float              x0;             // Apt.h:231
    float              y0;             // Apt.h:234
    float              x1;             // Apt.h:237
    float              y1;             // Apt.h:240
    AptStringAlignment eAlignment;     // Apt.h:243
    AptStringAlignment eBoxAlignment;  // Apt.h:245
    int                bMultiline;     // Apt.h:248
    int                bWordWrap;      // Apt.h:250
    unsigned int       nColour;        // Apt.h:252
    unsigned int       nBackColor;     // Apt.h:254
    unsigned int       nBorderColor;   // Apt.h:256
    int                bBackground;    // Apt.h:258
    int                bBorder;        // Apt.h:260
    float              fFontHeight;    // Apt.h:263
    uint32_t           nLineOffset;    // Apt.h:266
    int*               pnNumLines;     // Apt.h:269
    const char*        szString;       // Apt.h:274
    unsigned int       eFlags;         // Apt.h:277
    unsigned int       nFontStyle;     // Apt.h:280
    int                nIndent;        // Apt.h:282
    int                nLeftMargin;    // Apt.h:284
    int                nRightMargin;   // Apt.h:286
    float              fTextWidth;     // Apt.h:290
    float              fTextHeight;    // Apt.h:292
    float              fStrLen;        // Apt.h:294
    uint32_t           nMaxScroll;     // Apt.h:296
    AptAssetString     pCurrString;    // Apt.h:303
};

// ============================================================================
// Apt.h:405 — AptUserFunctions: the global Apt host callback table (gAptFuncs).
//
// The Apt engine never rasterises / allocates / loads anything itself: it routes
// EVERY host interaction through this single function-pointer table. The 47 slots
// are laid out verbatim from the DecFIGS DWARF (Apt.h:417..856), in declaration
// order; on the 32-bit guest each slot is one 4-byte pointer (so e.g.
// pfnSetVertexMatrix is the 27th slot == byte +0x68, pfnDrawRenderingUnit the 29th
// == byte +0x70). On the x64 host the pointers widen to 8 bytes — the byte offsets
// are NOT load-bearing (the host installer is PC-compiled too); the slot ORDER and
// per-slot signature are what make the install + the engine's indirect calls
// type-correct, and those are reproduced exactly.
//
// CgsGui::AptAux::ConstructApt @0x5BA0F8 installs the render family
// (pfnSetBackgroundColour .. pfnDrawRenderingUnit, pfnGetStageHeight/Width,
// pfnPushRenderFlags/pfnPopRenderFlags) by pointing each slot at the matching
// CgsGui::AptCallbackRender free function. The engine then calls them as it walks a
// movie's render tree (the render context's pfnSetVertexMatrix/pfnSetColourTransform,
// AptCharacter::render's pfnDrawRenderingUnit). The non-render families' slots are
// installed by the same body once those host callbacks land (FLAG'd in ConstructApt).
// ============================================================================
struct AptUserFunctions
{
    // ---- Memory (Apt.h:417..427) -------------------------------------------------
    void* (*pfnMemAlloc)(size_t);                                              // +0x00
    void  (*pfnMemFree)(void*);                                               // +0x04
    void  (*pfnMemFreeSize)(void*, size_t);                                   // +0x08

    // ---- Debug (Apt.h:433..459) --------------------------------------------------
    void  (*pfnAssertFail)(const char*, const char*, unsigned int);          // +0x0C
    void  (*pfnSetBackgroundColour)(unsigned int);                            // +0x10 (render)
    void  (*pfnDebugPrint)(const char*, ...);                                 // +0x14
    void  (*pfnDebugAddSavedInput)(AptSavedInputRecord*, int);               // +0x18
    void  (*pfnDebugSetScreenGrabPending)(const char*);                      // +0x1C

    // ---- File (Apt.h:469..490) ---------------------------------------------------
    void  (*pfnLoadAnimation)(const char*, AptFilePtr);                       // +0x20
    void  (*pfnFreeAnimation)(void*);                                         // +0x24
    void  (*pfnFreeConstantTable)(void*);                                     // +0x28
    void  (*pfnLoadAnimationCompleted)(const char*, const char*);            // +0x2C
    void  (*pfnCommand)(const char*, const char*);                           // +0x30

    // ---- Variable (Apt.h:499..548) -----------------------------------------------
    AptValue* (*pfnLoadVariables)(const char*);                              // +0x34
    AptValue* (*pfnLoadVariablesNULL)();                                     // +0x38
    void  (*pfnSetExternVariable)(const char*, const char*);                // +0x3C
    AptValue* (*pfnGetExternVariable)(const char*);                         // +0x40
    void  (*pfnSendVariables)(const char*, const char*, const char*,
                              const char*, int);                            // +0x44

    // ---- Render: string allocation + draw (Apt.h:558..580) -----------------------
    AptAssetString (*pfnAllocateString)(AptAllocateStringParameters*);       // +0x48 (render)
    void  (*pfnDeallocateString)(AptAssetString, unsigned int);              // +0x4C (render)
    void  (*pfnDrawString)(AptAssetString, AptMaskRenderOperation, int);     // +0x50 (render)

    // ---- Render: texture binding (Apt.h:625..637) --------------------------------
    AptAssetTexture (*pfnLoadTexture)(AptAnimationUserData, int);            // +0x54 (render)
    void  (*pfnFreeTexture)(AptAssetTexture);                                // +0x58 (render)
    void  (*pfnBindTexture)(AptAnimationUserData, int, AptAssetTexture);     // +0x5C (render)

    // ---- Render: rendering-unit lifetime (Apt.h:650..653) ------------------------
    AptAssetRenderingUnit (*pfnLoadRenderingUnit)(AptAnimationUserData, int);// +0x60 (render)
    void  (*pfnFreeRenderingUnit)(AptAssetRenderingUnit);                    // +0x64 (render)

    // ---- Render: per-batch transform + the shape draw (Apt.h:659..685) -----------
    void  (*pfnSetVertexMatrix)(AptMatrix*);                                 // +0x68 (render; slot 26)
    void  (*pfnSetColourTransform)(AptCXForm*);                              // +0x6C (render; slot 27)
    void  (*pfnDrawRenderingUnit)(AptAssetRenderingUnit,
                                  AptMaskRenderOperation, int);              // +0x70 (render; slot 28)

    // ---- Custom controls (Apt.h:708..740) ----------------------------------------
    void  (*pfnCustomControlRender)(char*, char*, AptAssetRenderingUnit,
                                    const char*, AptMaskRenderOperation, int);// +0x74
    bool  (*pfnCustomControlUpdate)(AptAssetRenderingUnit);                  // +0x78
    AptAssetCustomControlZId (*pfnCreateCustomControlZid)(
        AptAssetRenderingUnit, const char*, AptValue*,
        AptAssetCustomControlZId, bool);                                     // +0x7C
    void  (*pfnDestroyCustomControlZid)(AptAssetCustomControlZId);           // +0x80
    void  (*pfnCustomControlRenderWithZid)(AptAssetCustomControlZId,
        AptAssetRenderingUnit, AptMaskRenderOperation, int);                 // +0x84

    // ---- Misc engine services (Apt.h:747..783) -----------------------------------
    void  (*pfnOnUnload)(AptValue*);                                         // +0x88
    int   (*pfnPointHitTest)(float, float, AptAssetMoiveClip);              // +0x8C
    void  (*pfnGetRealTimeClock)(AptSysClock*, bool);                       // +0x90
    int   (*pfnGetBytesTotal)(const char*, AptGetBytesEnum);               // +0x94
    int   (*pfnGetBytesLoaded)(const char*, AptGetBytesEnum);              // +0x98

    // ---- Deprecated / misc (Apt.h:822..856) --------------------------------------
    void  (*pfnUninitializedVarAccess)(const char*);                        // +0x9C
    float (*pfnGetStageHeight)();                                            // +0xA0 (render)
    float (*pfnGetStageWidth)();                                             // +0xA4 (render)
    void  (*pfnCustomSavedInputHandler)(void*, unsigned int);              // +0xA8
    void  (*pfnPlaySavedInputsDone)(bool, const char*);                    // +0xAC
    void  (*pfnHandleZombieState)(bool, bool, const char*, const char*);   // +0xB0
    void  (*pfnPushRenderFlags)(const char*);                               // +0xB4 (render)
    void  (*pfnPopRenderFlags)(const char*);                                // +0xB8 (render)

    // Apt.h:859 — the table's ctor (zero-installs the slots; the host then overrides
    // each family). Declared for parity; the install path uses gAptFuncs directly.
    AptUserFunctions();
};

// Apt.h:866 — the single global Apt host callback table the engine routes through
// (X360 dword_8324E818). Homed once by the Apt host-install TU (CgsAptAux.cpp,
// alongside AptAux::ConstructApt which fills its render slots).
extern AptUserFunctions gAptFuncs;

// ---- the public engine entries (SDK Apt.h / bodies in Apt/Apt.cpp) -------------
// AptLoadAnimation @0x82B07AC8 -- queue a movie load onto a target path ("_level%d"
// from AptAux::LoadFlashAnimation): resets the BackgroundColour once-per-load latch,
// strips a ".swf" suffix off the name, and hands (name, target) to the live target's
// linker (gpAptTarget->mpLinker->Load). The console's int return is its final
// EAStringC refcount-drop result -- ignored by every caller.
int AptLoadAnimation(const char* pName, const char* pTargetPath);

#endif // SDKS_EATECH_APT_APT_H
