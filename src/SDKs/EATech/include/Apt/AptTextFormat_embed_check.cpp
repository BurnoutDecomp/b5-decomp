// Compile-only embed check for AptTextFormat.h -- verifies the AS TextFormat value
// type is well-formed and its embedded format record's named fields are reachable
// (the shape getNewTextFormat/getTextFormat/setTextFormat + the text render items use).
// Not a runtime TU.

#include "SDKs/EATech/include/Apt/AptTextFormat.h"

namespace
{
    void AptTextFormat_EmbedCheck(AptTextFormat* pFmt)
    {
        TextFormat* pRec = &pFmt->mFormat;     // +0x20 embedded record

        const EAStringC* pFont  = &pRec->mFontName;   // +0x00
        float            fSize  = pRec->mfSize;       // +0x04
        s32              nColor = pRec->mnColor;       // +0x08

        // AptTextFormat IS an AptValue(WithHash) -- usable through the value base.
        AptValue* pAsValue = pFmt;

        (void)pRec; (void)pFont; (void)fSize; (void)nColor; (void)pAsValue;
    }
}
