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
//
// Layout/field names are taken verbatim from the DecFIGS DWARF (Apt.h:225..303),
// which is the declaration-shape authority. Field types use the SDK's own
// (non-Hungarian) names per the naming convention's "generated/external API"
// exception. `AptAssetString` is an opaque handle (a void* in the SDK).
// ============================================================================

// Apt.h:136 — opaque per-allocation string handle the SDK hands back.
typedef void* AptAssetString;

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

#endif // SDKS_EATECH_APT_APT_H
